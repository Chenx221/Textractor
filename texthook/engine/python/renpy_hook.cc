#include "renpy_hook.h"

#include <atomic>
#include <string>

#include <tlhelp32.h>

#include "main.h"
#include "types.h"
#include "../common/text_transport.h"

namespace {

struct PyApi
{
    int (*IsInitialized)();
    int (*GilEnsure)();
    void (*GilRelease)(int);
    int (*RunString)(const char* code, void* flags);
    void (*ErrPrint)();
    const char* (*GetVersion)();
} py{};

template <typename T>
bool Resolve(HMODULE module, const char* name, T& out)
{
    out = reinterpret_cast<T>(GetProcAddress(module, name));
    return out != nullptr;
}

bool ResolvePy(HMODULE module)
{
    return Resolve(module, "Py_IsInitialized", py.IsInitialized)
        && Resolve(module, "PyGILState_Ensure", py.GilEnsure)
        && Resolve(module, "PyGILState_Release", py.GilRelease)
        && Resolve(module, "PyRun_SimpleStringFlags", py.RunString)
        && Resolve(module, "PyErr_Print", py.ErrPrint)
        && Resolve(module, "Py_GetVersion", py.GetVersion);
}

HMODULE FindPythonModule()
{
    static const wchar_t* known[] = {
        L"python27.dll",      // Ren'Py 6.99.14.3 ~ 7.3.x, 32 bit only 
        L"libpython2.7.dll",  // Ren'Py 7.4.x ~ 7.8.x, 32 and 64 bit
        L"libpython3.9.dll",  // Ren'Py 8.0.x ~ 8.3.x, 64 bit only
        L"libpython3.12.dll", // Ren'Py >= 8.4, 64 bit only
    };
    for (const wchar_t* name : known)
        if (HMODULE module = GetModuleHandleW(name)) return module;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return nullptr;

    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    HMODULE found = nullptr;
    if (Module32FirstW(snapshot, &entry))
        do {
            if (wcsstr(entry.szModule, L"python")) { found = (HMODULE)entry.modBaseAddr; break; }
        } while (Module32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return found;
}


const char kBootstrap[] = R"PY(
import json as _tt_json
import os as _tt_os

try:
    _tt_unicode = unicode            # Python 2 (Ren'Py 7.x)
    _tt_strings = (str, unicode)
except NameError:
    _tt_unicode = str                # Python 3 (Ren'Py 8.x)
    _tt_strings = (str,)


def _tt_as_text(value):
    if isinstance(value, _tt_unicode):
        return value
    if isinstance(value, bytes):
        try:
            return value.decode('utf-8')
        except Exception:
            return value.decode('latin-1')
    try:
        return _tt_unicode(value)
    except Exception:
        return _tt_unicode('')


# Re-attaching to the same game re-runs this file: keep the state, drop the
# connection that belonged to the previous pipe.
_tt_state = globals().get('_tt_state')
if _tt_state is None:
    _tt_state = {'fd': None, 'last': None}
_tt_state['last'] = None
try:
    if _tt_state['fd'] is not None:
        _tt_os.close(_tt_state['fd'])
except Exception:
    pass
_tt_state['fd'] = None


def _tt_open():
    if _tt_state['fd'] is not None:
        return _tt_state['fd']
    try:
        flags = _tt_os.O_WRONLY | getattr(_tt_os, 'O_BINARY', 0)
        _tt_state['fd'] = _tt_os.open(_tt_pipe, flags)
    except OSError:
        _tt_state['fd'] = None
    return _tt_state['fd']


def _tt_emit(who, what):
    try:
        text = _tt_as_text(what)
        if not text:
            return
        fd = _tt_open()
        if fd is None:
            return
        payload = _tt_json.dumps({u's': _tt_as_text(who), u't': text}, ensure_ascii=False)
        _tt_os.write(fd, (payload + _tt_unicode('\n')).encode('utf-8'))
    except OSError:
        _tt_state['fd'] = None
    except Exception:
        pass


def _tt_who_name(who):
    # display_say receives the already rendered name: a plain string for the
    # usual Character("Name") case, a Character object for some subclasses,
    # None for the narrator.
    if who is None:
        return _tt_unicode('')
    if isinstance(who, _tt_strings):
        return _tt_as_text(who)
    try:
        name = getattr(who, 'name', None)
        if isinstance(name, _tt_strings):
            return _tt_as_text(name)
    except Exception:
        pass
    try:
        text = _tt_as_text(who)
    except Exception:
        return _tt_unicode('')
    if text.startswith(u'<') and u' object at ' in text:
        return _tt_unicode('')
    return text


def _tt_clean(text):
    # Ren'Py hands us the text with its tags still in place ({b}, {color=...},
    # {p} ...). Tags have to be removed by a scanner rather than a regex: {{
    # is an escaped literal brace and must survive as a single {.
    # Backslashes are left alone: by the time text reaches display_say the
    # Python-level escapes are already gone, so anything left is literal.
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == u'{':
            if text.startswith(u'{{', i):
                out.append(u'{')
                i += 2
                continue
            j = text.find(u'}', i)
            if j < 0:
                out.append(c)
                i += 1
                continue
            name = text[i + 1:j].split(u'=')[0].lstrip(u'/')
            i = j + 1
            if name == u'p':
                out.append(u'\n')
            continue
        if c == u'[':
            if text.startswith(u'[[', i):
                out.append(u'[')
                i += 2
                continue
            j = text.find(u']', i)
            if j < 0:
                out.append(c)
                i += 1
                continue
            out.append(text[i:j + 1])
            i = j + 1
            continue
        out.append(c)
        i += 1
    return u''.join(out)


def _tt_say(*args, **kwargs):
    try:
        who = args[0] if len(args) > 0 else kwargs.get('who')
        what = args[1] if len(args) > 1 else kwargs.get('what')
        if what is not None:
            name = _tt_who_name(who)
            text = _tt_clean(_tt_as_text(what))
            if text.strip():
                key = (name, text)
                if key != _tt_state['last']:
                    _tt_state['last'] = key
                    _tt_emit(name, text)
    except Exception:
        pass
    return _tt_orig_say(*args, **kwargs)


import renpy.character as _tt_ch
if not getattr(_tt_ch, '_tt_hooked', False):
    _tt_ch._tt_hooked = True
    _tt_orig_say = _tt_ch.display_say
    _tt_ch.display_say = _tt_say


import renpy.exports as _tt_ex
if not getattr(_tt_ex, '_tt_menu_hooked', False):
    _tt_ex._tt_menu_hooked = True
    _tt_orig_menu = _tt_ex.menu

    def _tt_menu(items, *args, **kwargs):
        try:
            labels = []
            for item in items:
                try:
                    labels.append(_tt_as_text(item[0]))
                except Exception:
                    labels.append(_tt_as_text(item))
            if labels:
                _tt_emit(_tt_unicode(''), u'\n'.join(labels))
        except Exception:
            pass
        return _tt_orig_menu(items, *args, **kwargs)

    _tt_ex.menu = _tt_menu
    try:
        _tt_ex.menuexports.menu = _tt_menu
    except Exception:
        pass
)PY";


std::atomic<bool> installed{ false };
std::atomic<bool> running{ false };
std::string scriptSource;

DWORD WINAPI BootThread(LPVOID)
{
    for (int i = 0; i < 600 && running.load(); ++i)
    {
        if (py.IsInitialized()) break;
        Sleep(100);
    }
    if (!running.load() || !py.IsInitialized()) return 0;

    Sleep(300);
    if (!running.load()) return 0;

    int gil = py.GilEnsure();
    int rc = py.RunString(scriptSource.c_str(), nullptr);
    if (rc != 0) py.ErrPrint();
    py.GilRelease(gil);

    ConsoleOutput("Textractor: Ren'Py hooks installed");
    return 0;
}

} // unnamed namespace

namespace Engine {

bool InsertRenpyCpythonHook()
{
    if (installed.exchange(true)) return true;

    if (!(GetModuleHandleW(L"librenpython.dll") || GetModuleHandleW(L"_renpy.pyd")))
    {
        installed = false;
        return false;
    }

    HMODULE python = FindPythonModule();
    if (!python || !ResolvePy(python))
    {
        installed = false;
        return false;
    }

    const char* version = py.GetVersion();
    if (!version || (version[0] != '2' && version[0] != '3'))
    {
        ConsoleOutput("Textractor: Ren'Py engine skipped (unsupported Python %s)",
                      version ? version : "?");
        installed = false;
        return false;
    }
    ConsoleOutput("Textractor: Ren'Py engine using Python %s", version);

    static wchar_t identity[8] = {};
    TextPipe::SetThreadIdentity((uintptr_t)identity);

    HookParam hp = {};
    hp.address = (uint64_t)identity;
    hp.type = USING_UNICODE | DIRECT_READ | FULL_STRING;
    NewHook(hp, "renpy");

    const std::string pipeName = TextPipe::MakePipeName("TextractorRenpy_");
    scriptSource = "_tt_pipe = r'" + pipeName + "'\n";
    scriptSource += kBootstrap;

    TextPipe::StartPipe(pipeName);

    running.store(true);
    CloseHandle(CreateThread(nullptr, 0, BootThread, nullptr, 0, nullptr));

    ConsoleOutput("Textractor: Ren'Py engine armed");
    return true;
}

void ShutdownRenpyCpythonHook()
{
    if (!installed.exchange(false)) return;
    running.store(false);
    TextPipe::StopPipe();
    ConsoleOutput("Textractor: Ren'Py engine detached cleanly");
}

} // namespace Engine

// EOF
