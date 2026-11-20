//   Isolate::GetCurrent -> Isolate::GetCurrentContext -> HandleScope
//   -> String::NewFromUtf8 -> Script::Compile -> Script::Run

#include "nwjs_v8.h"

#include <atomic>
#include <initializer_list>
#include <string>

#include "main.h"
#include "types.h"
#include "MinHook.h"
#include "commandline.h"
#include "v8_bootstrap.h"
#include "../common/text_transport.h"

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
    void (V8_THISCALL *GetEnteredContext)(void* isolate, Handle* out);
    void (V8_THISCALL *ScopeCtor)(void* scope, void* isolate);
    void (V8_THISCALL *ScopeDtor)(void* scope);
    void (*NewStr)(Handle* out, void* isolate, const char* utf8, int type, int len);
    void (*Compile)(Handle* out, Handle context, Handle source, void* origin);
    void (V8_THISCALL *Run)(Handle script, Handle* out, Handle context);
    void (V8_THISCALL *TryCatchCtor)(void* tryCatch, void* isolate);
    void (V8_THISCALL *TryCatchDtor)(void* tryCatch);
    bool (V8_THISCALL *HasCaught)(void* tryCatch);
    void* executeCall;
} v8{};

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
        "?GetEnteredOrMicrotaskContext@Isolate@v8@@QEAA?AV?$Local@VContext@v8@@@2@XZ",
        "?GetEnteredOrMicrotaskContext@Isolate@v8@@QAE?AV?$Local@VContext@v8@@@2@XZ" }, v8.GetEnteredContext)
    && ResolveAny(module, {
        "??0HandleScope@v8@@QEAA@PEAVIsolate@1@@Z",
        "??0HandleScope@v8@@QAE@PAVIsolate@1@@Z" }, v8.ScopeCtor)
    && ResolveAny(module, {
        "??1HandleScope@v8@@QEAA@XZ",
        "??1HandleScope@v8@@QAE@XZ" }, v8.ScopeDtor)
    && ResolveAny(module, {
        "?NewFromUtf8@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PEAVIsolate@2@PEBDW4NewStringType@2@H@Z",
        "?NewFromUtf8@String@v8@@SA?AV?$MaybeLocal@VString@v8@@@2@PAVIsolate@2@PBDW4NewStringType@2@H@Z",
        "?NewFromUtf8@String@v8@@SA?AV?$Local@VString@v8@@@2@PAVIsolate@2@PBDW4NewStringType@2@H@Z" }, v8.NewStr)
    && ResolveAny(module, {
        "?Compile@Script@v8@@SA?AV?$MaybeLocal@VScript@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VString@v8@@@2@PEAVScriptOrigin@2@@Z",
        "?Compile@Script@v8@@SA?AV?$MaybeLocal@VScript@v8@@@2@V?$Local@VContext@v8@@@2@V?$Local@VString@v8@@@2@PAVScriptOrigin@2@@Z",
        "?Compile@Script@v8@@SA?AV?$Local@VString@v8@@@2@0@Z" }, v8.Compile)
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
        "?Call@Execution@internal@v8@@SA?AV?$MaybeHandle@VObject@internal@v8@@@23@PEAVIsolate@23@V?$Handle@VObject@internal@v8@@@23@1HQEAV623@@Z",
        "?Call@Execution@internal@v8@@SA?AV?$MaybeHandle@VObject@internal@v8@@@23@PAVIsolate@23@V?$Handle@VObject@internal@v8@@@23@1HQAV623@@Z" }, v8.executeCall);
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


std::atomic<int> bootState{ 0 }; // 0 = idle, 1 = booting, 2 = done, -1 = failed
std::string scriptSource;        // built once, before booting

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
        v8.NewStr(&source, isolate, js, 0 /* kNormal */, static_cast<int>(len));
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

bool BootCore(void* isolate)
{
    bool ok = false;
    __try
    {
        Handle context = nullptr;
        v8.GetCurrentContext(isolate, &context);
        if (!context)
        {
            Handle alt = nullptr;
            v8.GetEnteredContext(isolate, &alt);
            context = alt;
        }
        if (context) ok = RunScript(isolate, context, scriptSource.c_str(), scriptSource.size());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    return ok;
}

} // unnamed namespace

namespace {

std::atomic<bool> stopping{ false };
std::atomic<long> inFlight{ 0 };

// v8::internal::Execution::Call

using ExecCallFn = void (*)(void* sret, void* isolate, Handle callable, Handle receiver, int argc, void* argv);
ExecCallFn originalExecCall = nullptr;

void DetourExecCall(void* sret, void* isolate, Handle callable, Handle receiver, int argc, void* argv)
{
    inFlight.fetch_add(1, std::memory_order_relaxed);
    if (!stopping.load(std::memory_order_relaxed) &&
        bootState.load(std::memory_order_acquire) == 0)
    {
        int expected = 0;
        if (bootState.compare_exchange_strong(expected, 1))
        {
            bool ok = BootCore(isolate);
            bootState.store(ok ? 2 : -1);
            ConsoleOutput("Textractor: nw.js V8 bootstrap %s", ok ? "ok" : "failed");
        }
    }
    originalExecCall(sret, isolate, callable, receiver, argc, argv);
    inFlight.fetch_sub(1, std::memory_order_relaxed);
}

} // unnamed namespace

namespace Engine {

std::atomic<bool> installed{ false };

bool InsertNwjsV8Hook(HMODULE module)
{
    if (installed.exchange(true)) return true;
    if (!module) return installed = false;

    // Only renderers host a page.
    if (!IsRendererCommandLine(GetCommandLineW()))
    {
        ConsoleOutput("Textractor: nw.js V8 engine needs the renderer process "
                      "(attach to the process whose command line contains --type=renderer)");
        installed = false;
        return false;
    }

    if (!ResolveV8(module))
    {
        ConsoleOutput("Textractor: nw.js V8 engine: this build does not export V8 symbols");
        installed = false;
        return false;
    }

    static wchar_t identity[8] = {};
    TextPipe::SetThreadIdentity((uintptr_t)identity);

    HookParam hp = {};
    hp.address = (uint64_t)identity;
    hp.type = USING_UNICODE | DIRECT_READ | FULL_STRING;
    NewHook(hp, "nwjs_v8");

    const std::string pipeName = TextPipe::MakePipeName("TextractorNwjsV8_");
    scriptSource = "var __TT_PIPE = \"" + EscapeForJs(pipeName) + "\";\n";
    scriptSource += kBootstrapCore;
    scriptSource += kBootstrapRpgMaker;
    scriptSource += kBootstrapTyrano;
    scriptSource += kBootstrapTail;

    TextPipe::StartPipe(pipeName);

    if (MH_CreateHook(v8.executeCall, (LPVOID)DetourExecCall, (LPVOID*)&originalExecCall) != MH_OK
        || MH_EnableHook(v8.executeCall) != MH_OK)
    {
        ConsoleOutput("Textractor: nw.js V8 engine: failed to hook Execution::Call");
        installed = false;
        return false;
    }

    ConsoleOutput("Textractor: nw.js V8 engine armed, waiting for the first JS call");
    return true;
}

void ShutdownNwjsV8Hook()
{
    if (!installed.exchange(false)) return;
    stopping.store(true, std::memory_order_release);

    if (v8.executeCall && originalExecCall)
    {
        MH_DisableHook(v8.executeCall);
        Sleep(50);
        for (int i = 0; i < 2000 && inFlight.load() > 0; ++i) Sleep(1);
        MH_RemoveHook(v8.executeCall);
        originalExecCall = nullptr;
    }

    TextPipe::StopPipe();

    ConsoleOutput("Textractor: nw.js V8 engine detached cleanly");
}

} // namespace Engine

// EOF
