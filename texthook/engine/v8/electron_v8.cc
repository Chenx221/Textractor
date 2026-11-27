#include "electron_v8.h"

#include <atomic>
#include <initializer_list>
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

#ifdef _M_IX86
#define V8_THISCALL __thiscall
#else
#define V8_THISCALL
#endif

struct V8Api
{
    void* (*GetCurrent)();
    void (V8_THISCALL *GetCurrentContext)(void* isolate, Handle* out);
    void (V8_THISCALL *ScopeCtor)(void* scope, void* isolate);
    void (V8_THISCALL *ScopeDtor)(void* scope);
    void (*NewStr)(Handle* out, void* isolate, const char* utf8, int type, int len);
    void (*Compile)(Handle* out, Handle context, Handle source, void* origin);
    void (V8_THISCALL *Run)(Handle script, Handle* out, Handle context);
    void (V8_THISCALL *TryCatchCtor)(void* tryCatch, void* isolate);
    void (V8_THISCALL *TryCatchDtor)(void* tryCatch);
    bool (V8_THISCALL *HasCaught)(void* tryCatch);
    void* utf8Ctor;
    bool utf8CtorWantsIsolate;
    void (V8_THISCALL *Utf8Dtor)(void* self);
    void* functionCall; // v8::Function::Call, our detour target
} v8{};

using Utf8Ctor1 = void (V8_THISCALL *)(void* self, Handle value);
using Utf8Ctor2 = void (V8_THISCALL *)(void* self, void* isolate, Handle value);

template <typename T>
bool ResolveAny(HMODULE module, std::initializer_list<const char*> names, T& out)
{
    for (const char* name : names)
        if (FARPROC address = GetProcAddress(module, name))
        {
            out = reinterpret_cast<T>(address);
            return true;
        }
    return false;
}

bool ResolveV8(HMODULE module)
{
    return ResolveAny(module, {
        "?GetCurrent@Isolate@v8@@SAPEAV12@XZ",
        "?GetCurrent@Isolate@v8@@SAPAV12@XZ" }, v8.GetCurrent)
    && ResolveAny(module, {
        "?GetCurrentContext@Isolate@v8@@QEAA?AV?$Local@VContext@v8@@@2@XZ",
        "?GetCurrentContext@Isolate@v8@@QAE?AV?$Local@VContext@v8@@@2@XZ" }, v8.GetCurrentContext)
    && ResolveAny(module, {
        "??0HandleScope@v8@@QEAA@PEAVIsolate@1@@Z",
        "??0HandleScope@v8@@QAE@PAVIsolate@1@@Z" }, v8.ScopeCtor)
    && ResolveAny(module, {
        "??1HandleScope@v8@@QEAA@XZ",
        "??1HandleScope@v8@@QAE@XZ" }, v8.ScopeDtor)
    && ResolveAny(module, {
        "?NewFromUtf8@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PEAVIsolate@2@PEBDW4NewStringType@2@H@Z",
        "?NewFromUtf8@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PAVIsolate@2@PBDW4NewStringType@2@H@Z" }, v8.NewStr)
    && ResolveAny(module, {
        "?Compile@Script@v8@@SA?AV?$MaybeLocal@VScript@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VString@v8@@@2@PEAVScriptOrigin@2@@Z",
        "?Compile@Script@v8@@SA?AV?$MaybeLocal@VScript@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VString@v8@@@2@PAVScriptOrigin@2@@Z" }, v8.Compile)
    && ResolveAny(module, {
        "?Run@Script@v8@@QEAA?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@@Z",
        "?Run@Script@v8@@QAE?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@@Z" }, v8.Run)
    && ResolveAny(module, {
        "??0TryCatch@v8@@QEAA@PEAVIsolate@1@@Z",
        "??0TryCatch@v8@@QAE@PAVIsolate@1@@Z" }, v8.TryCatchCtor)
    && ResolveAny(module, {
        "??1TryCatch@v8@@QEAA@XZ",
        "??1TryCatch@v8@@QAE@XZ" }, v8.TryCatchDtor)
    && ResolveAny(module, {
        "?HasCaught@TryCatch@v8@@QEBA_NXZ",
        "?HasCaught@TryCatch@v8@@QBE_NXZ" }, v8.HasCaught)
    && ResolveAny(module, {
        "?Call@Function@v8@@QEAA?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VValue@v8@@@2@HQEAV52@@Z",
        "?Call@Function@v8@@QAE?AV?$MaybeLocal@VValue@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VValue@v8@@@2@HQAV52@@Z" }, v8.functionCall);
}

void ResolveUtf8(HMODULE module)
{
    const char* withIsolate[] = {
        "??0Utf8Value@String@v8@@QEAA@PEAVIsolate@2@V?$Local@VValue@v8@@@2@@Z",
        "??0Utf8Value@String@v8@@QAE@PAVIsolate@2@V?$Local@VValue@v8@@@2@@Z" };
    const char* withoutIsolate[] = {
        "??0Utf8Value@String@v8@@QEAA@V?$Local@VValue@v8@@@2@@Z",
        "??0Utf8Value@String@v8@@QAE@V?$Local@VValue@v8@@@2@@Z" };

    for (const char* name : withIsolate)
        if (FARPROC p = GetProcAddress(module, name))
        {
            v8.utf8Ctor = p;
            v8.utf8CtorWantsIsolate = true;
            break;
        }
    if (!v8.utf8Ctor)
        for (const char* name : withoutIsolate)
            if (FARPROC p = GetProcAddress(module, name))
            {
                v8.utf8Ctor = p;
                break;
            }

    for (const char* name : { "??1Utf8Value@String@v8@@QEAA@XZ", "??1Utf8Value@String@v8@@QAE@XZ" })
        if (FARPROC p = GetProcAddress(module, name))
        {
            v8.Utf8Dtor = reinterpret_cast<decltype(v8.Utf8Dtor)>(p);
            break;
        }
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

static const char kDrainScript[] =
    "(function(){var a=window.__ttNwjsV8;return (a&&a.drain)?a.drain():'';})()";

bool RunScriptOut(void* isolate, Handle context, const char* js, size_t len, std::string& out)
{
    alignas(16) BYTE tryCatch[0x200] = {};
    alignas(16) BYTE scope[0x100] = {};

    bool ok = false, scoped = false;
    v8.TryCatchCtor(tryCatch, isolate);
    __try
    {
        v8.ScopeCtor(scope, isolate);
        scoped = true;

        Handle source = nullptr, script = nullptr, value = nullptr;
        v8.NewStr(&source, isolate, js, 0, static_cast<int>(len));
        if (!v8.HasCaught(tryCatch) && source)
        {
            v8.Compile(&script, context, source, nullptr);
            if (!v8.HasCaught(tryCatch) && script)
            {
                v8.Run(script, &value, context);
                ok = !v8.HasCaught(tryCatch);
            }
        }
        if (ok && value && v8.utf8Ctor && v8.Utf8Dtor)
        {
            alignas(8) BYTE utf8[0x40] = {};
            if (v8.utf8CtorWantsIsolate)
                reinterpret_cast<Utf8Ctor2>(v8.utf8Ctor)(utf8, isolate, value);
            else
                reinterpret_cast<Utf8Ctor1>(v8.utf8Ctor)(utf8, value);

            const char* text = *reinterpret_cast<const char**>(utf8);
            if (text) out.assign(text);
            v8.Utf8Dtor(utf8);
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

void MaybeDrain(void* isolate, Handle context)
{
    static std::atomic<uint64_t> lastDrain{ 0 };

    uint64_t now = GetTickCount64();
    uint64_t last = lastDrain.load(std::memory_order_relaxed);
    if (now - last < 100) return;
    if (!lastDrain.compare_exchange_strong(last, now)) return;

    std::string text;
    if (RunScriptOut(isolate, context, kDrainScript, sizeof(kDrainScript) - 1, text) && !text.empty())
        TextPipe::PushLines(text.data(), text.size());
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

#ifdef _M_IX86
using FunctionCallFn = void (__fastcall *)(void* self, void* unused, void* sret,
                                          Handle context, void* receiver, int argc, void* argv);
#else
using FunctionCallFn = void (*)(void* self, void* sret,
                                Handle context, void* receiver, int argc, void* argv);
#endif

FunctionCallFn originalFunctionCall = nullptr;

void BootOnce(void* isolate, Handle context)
{
    if (stopping.load(std::memory_order_relaxed) ||
        bootState.load(std::memory_order_acquire) != 0) return;

    int expected = 0;
    if (!bootState.compare_exchange_strong(expected, 1)) return;

    bool ok = BootCore(isolate, context);
    bootState.store(ok ? 2 : -1);
    ConsoleOutput("Textractor: electron V8 bootstrap %s", ok ? "ok" : "failed");
}

void OnJsCall(Handle context)
{
    if (stopping.load(std::memory_order_relaxed)) return;

    void* isolate = nullptr;
    __try { isolate = v8.GetCurrent(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { isolate = nullptr; }
    if (!isolate) return;

    int state = bootState.load(std::memory_order_acquire);
    if (state == 0) BootOnce(isolate, context);
    else if (state == 2) MaybeDrain(isolate, context);
}

#ifdef _M_IX86
void __fastcall DetourFunctionCall(void* self, void* unused, void* sret,
                                   Handle context, void* receiver, int argc, void* argv)
{
    inFlight.fetch_add(1, std::memory_order_relaxed);
    OnJsCall(context);
    originalFunctionCall(self, unused, sret, context, receiver, argc, argv);
    inFlight.fetch_sub(1, std::memory_order_relaxed);
}
#else
void DetourFunctionCall(void* self, void* sret,
                        Handle context, void* receiver, int argc, void* argv)
{
    inFlight.fetch_add(1, std::memory_order_relaxed);
    OnJsCall(context);
    originalFunctionCall(self, sret, context, receiver, argc, argv);
    inFlight.fetch_sub(1, std::memory_order_relaxed);
}
#endif

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
    ResolveUtf8(exe);

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
