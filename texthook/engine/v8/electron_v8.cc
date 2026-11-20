#include "electron_v8.h"

#include <atomic>
#include <string>

#include "main.h"
#include "types.h"
#include "MinHook.h"
#include "commandline.h"
#include "v8_bootstrap.h"
#include "../common/text_transport.h"

#include <Psapi.h>

namespace {

using Handle = void*;

struct V8Api
{
    void* (*GetCurrent)();
    void (*GetCurrentContext)(void* isolate, Handle* out); // member: this, sret
    void (*ScopeCtor)(void* scope, void* isolate);
    void (*ScopeDtor)(void* scope);
    void (*NewStr)(Handle* out, void* isolate, const char* utf8, int type, int len); // static
    void (*Compile)(Handle* out, Handle context, Handle source, void* origin);      // static
    void (*Run)(Handle script, Handle* out, Handle context);                        // member
    void (*TryCatchCtor)(void* tryCatch, void* isolate);
    void (*TryCatchDtor)(void* tryCatch);
    bool (*HasCaught)(void* tryCatch);
    void* functionCall; // v8::Function::Call, our detour target
} v8{};

#define V8_SYM_GETCURRENT  "?GetCurrent@Isolate@v8@@SAPEAV12@XZ"
#define V8_SYM_GETCONTEXT  "?GetCurrentContext@Isolate@v8@@QEAA?AV?$Local@VContext@v8@@@2@XZ"
#define V8_SYM_SCOPECTOR   "??0HandleScope@v8@@QEAA@PEAVIsolate@1@@Z"
#define V8_SYM_SCOPEDTOR   "??1HandleScope@v8@@QEAA@XZ"
#define V8_SYM_NEWUTF8     "?NewFromUtf8@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PEAVIsolate@2@PEBDW4NewStringType@2@H@Z"
#define V8_SYM_COMPILE     "?Compile@Script@v8@@SA?AV?$MaybeLocal@VScript@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VString@v8@@@2@PEAVScriptOrigin@2@@Z"
#define V8_SYM_RUN         "?Run@Script@v8@@QEAA?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@@Z"
#define V8_SYM_TCCTOR      "??0TryCatch@v8@@QEAA@PEAVIsolate@1@@Z"
#define V8_SYM_TCDTOR      "??1TryCatch@v8@@QEAA@XZ"
#define V8_SYM_TCCATCH     "?HasCaught@TryCatch@v8@@QEBA_NXZ"
#define V8_SYM_FUNCCALL    "?Call@Function@v8@@QEAA?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VValue@v8@@@2@HQEAV52@@Z"

template <typename T>
bool Resolve(HMODULE module, const char* name, T& out)
{
    out = reinterpret_cast<T>(GetProcAddress(module, name));
    return out != nullptr;
}

bool ResolveV8(HMODULE module)
{
    return Resolve(module, V8_SYM_GETCURRENT, v8.GetCurrent)
        && Resolve(module, V8_SYM_GETCONTEXT, v8.GetCurrentContext)
        && Resolve(module, V8_SYM_SCOPECTOR, v8.ScopeCtor)
        && Resolve(module, V8_SYM_SCOPEDTOR, v8.ScopeDtor)
        && Resolve(module, V8_SYM_NEWUTF8, v8.NewStr)
        && Resolve(module, V8_SYM_COMPILE, v8.Compile)
        && Resolve(module, V8_SYM_RUN, v8.Run)
        && Resolve(module, V8_SYM_TCCTOR, v8.TryCatchCtor)
        && Resolve(module, V8_SYM_TCDTOR, v8.TryCatchDtor)
        && Resolve(module, V8_SYM_TCCATCH, v8.HasCaught)
        && Resolve(module, V8_SYM_FUNCCALL, v8.functionCall);
}

std::string EscapeForJs(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

std::atomic<int> bootState{ 0 }; // 0 idle, 1 booting, 2 done, -1 failed
std::string scriptSource;
std::atomic<bool> stopping{ false };
std::atomic<long> inFlight{ 0 };

bool RunScript(void* isolate, Handle context, const char* js, size_t len)
{
    alignas(16) BYTE tryCatch[0x200] = {};
    alignas(16) BYTE scope[0x100] = {};

    bool ok = false, scoped = false;
    v8.TryCatchCtor(tryCatch, isolate);
    __try
    {
        v8.ScopeCtor(scope, isolate);
        scoped = true;

        Handle source = nullptr;
        v8.NewStr(&source, isolate, js, 0, static_cast<int>(len));
        if (!v8.HasCaught(tryCatch) && source)
        {
            Handle script = nullptr;
            v8.Compile(&script, context, source, nullptr);
            if (!v8.HasCaught(tryCatch) && script)
            {
                Handle result = nullptr;
                v8.Run(script, &result, context);
                ok = !v8.HasCaught(tryCatch);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    if (scoped) v8.ScopeDtor(scope);
    v8.TryCatchDtor(tryCatch);
    return ok;
}

bool BootCore(void* isolate, Handle contextFromCall)
{
    bool ok = false;
    __try
    {
        Handle context = contextFromCall;
        if (!context)
        {
            v8.GetCurrentContext(isolate, &context);
        }
        if (context) ok = RunScript(isolate, context, scriptSource.c_str(), scriptSource.size());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    return ok;
}

struct PokeData { DWORD selfPid; std::wstring exePath; int posted; };

BOOL CALLBACK PokeWindow(HWND hwnd, LPARAM param)
{
    auto* data = reinterpret_cast<PokeData*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || pid == data->selfPid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    AutoHandle<> process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) return TRUE;

    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameExW(process, nullptr, path, MAX_PATH)) return TRUE;
    if (_wcsicmp(path, data->exePath.c_str()) != 0) return TRUE;

    PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(8, 8));
    data->posted++;
    return TRUE;
}

void PokeAppWindow()
{
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    PokeData data{ GetCurrentProcessId(), self, 0 };
    EnumWindows(PokeWindow, reinterpret_cast<LPARAM>(&data));
}

DWORD WINAPI PokerThread(LPVOID)
{
    for (int i = 0; i < 8; ++i)
    {
        Sleep(1500);
        if (stopping.load(std::memory_order_acquire)) return 0;
        if (bootState.load(std::memory_order_acquire) != 0) return 0;
        PokeAppWindow();
    }
    return 0;
}

} // unnamed namespace

// v8::Function::Call

namespace {

using FunctionCallFn = void (*)(void* sret, void* self, Handle context, void* receiver, int argc, void* argv);
FunctionCallFn originalFunctionCall = nullptr;

void DetourFunctionCall(void* sret, void* self, Handle context, void* receiver, int argc, void* argv)
{
    inFlight.fetch_add(1, std::memory_order_relaxed);
    if (!stopping.load(std::memory_order_relaxed) &&
        bootState.load(std::memory_order_acquire) == 0)
    {
        int expected = 0;
        if (bootState.compare_exchange_strong(expected, 1))
        {
            bool ok = false;
            void* isolate = nullptr;
            __try { isolate = v8.GetCurrent(); }
            __except (EXCEPTION_EXECUTE_HANDLER) { isolate = nullptr; }
            if (isolate) ok = BootCore(isolate, context);
            bootState.store(ok ? 2 : -1);
            ConsoleOutput("Textractor: electron V8 bootstrap %s", ok ? "ok" : "failed");
        }
    }
    originalFunctionCall(sret, self, context, receiver, argc, argv);
    inFlight.fetch_sub(1, std::memory_order_relaxed);
}

} // unnamed namespace

namespace Engine {

std::atomic<bool> electronInstalled{ false };

bool InsertElectronV8Hook()
{
    if (electronInstalled.exchange(true)) return true;

    // Only renderers host a page.
    if (!IsRendererCommandLine(GetCommandLineW()))
    {
        electronInstalled = false;
        return false;
    }
    if (GetModuleHandleW(L"nw.dll"))
    {
        electronInstalled = false;
        return false;
    }

    HMODULE exe = GetModuleHandleW(nullptr);
    if (!exe || !ResolveV8(exe))
    {
        electronInstalled = false;
        return false;
    }

    static wchar_t identity[8] = {};
    TextPipe::SetThreadIdentity((uintptr_t)identity);

    HookParam hp = {};
    hp.address = (uint64_t)identity;
    hp.type = USING_UNICODE | DIRECT_READ | FULL_STRING;
    NewHook(hp, "electron_v8");

    const std::string pipeName = TextPipe::MakePipeName("TextractorElectronV8_");
    scriptSource = "var __TT_PIPE = \"" + EscapeForJs(pipeName) + "\";\n";
    scriptSource += kBootstrapCore;
    scriptSource += kBootstrapRpgMaker;
    scriptSource += kBootstrapTyrano;
    scriptSource += kBootstrapTail;

    TextPipe::StartPipe(pipeName);
    CloseHandle(CreateThread(nullptr, 0, PokerThread, nullptr, 0, nullptr));

    if (MH_CreateHook(v8.functionCall, (LPVOID)DetourFunctionCall, (LPVOID*)&originalFunctionCall) != MH_OK
        || MH_EnableHook(v8.functionCall) != MH_OK)
    {
        ConsoleOutput("Textractor: electron V8 engine: failed to hook Function::Call");
        electronInstalled = false;
        return false;
    }

    ConsoleOutput("Textractor: electron V8 engine armed, waiting for a JS call");
    return true;
}

void ShutdownElectronV8Hook()
{
    if (!electronInstalled.exchange(false)) return;
    stopping.store(true, std::memory_order_release);

    if (v8.functionCall && originalFunctionCall)
    {
        MH_DisableHook(v8.functionCall);
        Sleep(50);
        for (int i = 0; i < 2000 && inFlight.load() > 0; ++i) Sleep(1);
        MH_RemoveHook(v8.functionCall);
        originalFunctionCall = nullptr;
    }

    TextPipe::StopPipe();

    ConsoleOutput("Textractor: electron V8 engine detached cleanly");
}

} // namespace Engine

// EOF
