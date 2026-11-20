#include <Psapi.h>
#include "commandline.h"

inline std::optional<std::wstring> GetModuleFilename(DWORD processId, HMODULE module = NULL)
{
	std::vector<wchar_t> buffer(MAX_PATH);
	if (AutoHandle<> process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId))
		if (GetModuleFileNameExW(process, module, buffer.data(), MAX_PATH)) return buffer.data();
	return {};
}

inline std::optional<std::wstring> GetModuleFilename(HMODULE module = NULL)
{
	std::vector<wchar_t> buffer(MAX_PATH);
	if (GetModuleFileNameW(module, buffer.data(), MAX_PATH)) return buffer.data();
	return {};
}

inline bool IsProcess64Bit(DWORD processId)
{
	static bool isOS64Bit = []() {
		BOOL isWow64 = FALSE;
		IsWow64Process(GetCurrentProcess(), &isWow64);
		return isWow64 || x64;
	}();

	if (!isOS64Bit) return false;

	if (AutoHandle<> process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId))
	{
		BOOL isWow64 = FALSE;
		IsWow64Process(process, &isWow64);
		return !isWow64;
	}
	return false;
}

inline std::optional<std::wstring> GetProcessMainWindowTitle(DWORD processId)
{
	struct EnumData { DWORD targetPid; std::wstring windowTitle; };
	EnumData data{ processId, L"" };

	EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
	{
		EnumData* pData = (EnumData*)lParam;
		DWORD dwProcessId = 0;
		GetWindowThreadProcessId(hwnd, &dwProcessId);
		if (dwProcessId == pData->targetPid && IsWindowVisible(hwnd))
		{
			wchar_t title[1000] = {};
			GetWindowTextW(hwnd, title, sizeof(title) / sizeof(title[0]));
			if (wcslen(title) > 0)
			{
				pData->windowTitle = title;
				return FALSE;
			}
		}
		return TRUE;
	}, (LPARAM)&data);

	if (!data.windowTitle.empty()) return data.windowTitle;
	return {};
}

inline std::vector<std::pair<DWORD, std::optional<std::wstring>>> GetAllProcesses()
{
	std::vector<DWORD> processIds(10000);
	DWORD spaceUsed = 0;
	EnumProcesses(processIds.data(), 10000 * sizeof(DWORD), &spaceUsed);
	std::vector<std::pair<DWORD, std::optional<std::wstring>>> processes;
	for (int i = 0; i < spaceUsed / sizeof(DWORD); ++i) processes.push_back({ processIds[i], GetModuleFilename(processIds[i]) });
	return processes;
}

inline std::optional<std::wstring> GetProcessCommandLine(DWORD processId)
{
	struct UnicodeString { USHORT Length; USHORT MaximumLength; PWSTR Buffer; };
	using NtQueryInformationProcessFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

	static auto ntQueryInformationProcess = (NtQueryInformationProcessFn)
		GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
	if (!ntQueryInformationProcess) return {};

	AutoHandle<> process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (!process) return {};

	std::vector<BYTE> buffer(sizeof(UnicodeString) + 0x8000);
	ULONG returned = 0;
	if (ntQueryInformationProcess(process, 60 /*ProcessCommandLineInformation*/, buffer.data(),
		(ULONG)buffer.size(), &returned) < 0) return {};

	auto* info = (UnicodeString*)buffer.data();
	if (!info->Buffer || !info->Length) return {};

	std::wstring commandLine;
	const BYTE* begin = buffer.data();
	const BYTE* end = begin + buffer.size();
	if ((BYTE*)info->Buffer >= begin && (BYTE*)info->Buffer < end)
		commandLine.assign(info->Buffer, info->Length / sizeof(wchar_t));
	else
	{
		AutoHandle<> reader = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, processId);
		std::wstring remote(info->Length / sizeof(wchar_t), L'\0');
		SIZE_T read = 0;
		if (reader && ReadProcessMemory(reader, info->Buffer, remote.data(), info->Length, &read) &&
			read == info->Length)
			commandLine = std::move(remote);
	}
	if (commandLine.empty()) return {};
	return commandLine;
}

inline bool IsRendererProcess(DWORD processId)
{
	auto commandLine = GetProcessCommandLine(processId);
	return commandLine && IsRendererCommandLine(*commandLine);
}
