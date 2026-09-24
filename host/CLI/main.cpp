// TextractorCLI: headless front-end for Textractor, sharing the host library with the GUI.
// chenx221: 这个cli使用vibe coding增强，只做了简单测试，祝好运（
// 
#include "common.h"
#include "defs.h"
#include "../host.h"
#include "../hookcode.h"
#include "../../texthook/texthook.h"
#include <io.h>
#include <fcntl.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
	enum ExitCode
	{
		EXIT_OK = 0,             // success
		EXIT_USAGE = 1,          // bad argument or command
		EXIT_ATTACH_FAILED = 2,  // injection failed
		EXIT_NO_TARGET = 3,      // no matching target process
		EXIT_DETACH_FAILED = 4,  // detach failed or timed out
		EXIT_REMOVE_FAILED = 5,  // hook could not be removed
	};

	enum class Encoding { Auto, Utf8, Utf16, Ansi };

	int g_exitCode = EXIT_OK;
	bool g_quiet = false;
	bool g_showConsole = true;    // false = --no-console
	bool g_showClipboard = false; // off by default (clipboard text would pollute the text stream), --clipboard enables it
	bool g_textOnly = false;      // --text-only: print bare text without the [handle:pid:...] prefix
	bool g_crlf = false;          // --crlf: use CRLF for redirected output
	bool g_quit = false;          // set by the "gui" command, which hands over to Textractor.exe

	std::map<std::pair<DWORD, uint64_t>, int> g_hookThreads;

	struct Sink
	{
		HANDLE handle = INVALID_HANDLE_VALUE;
		bool console = false;
		Encoding encoding = Encoding::Utf8;
		bool bomWritten = false;
	};

	Sink g_out, g_err;
	std::mutex g_writeMutex;

	bool IsConsoleHandle(HANDLE handle)
	{
		DWORD mode = 0;
		return handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != 0;
	}

	std::string WideToCodePage(const std::wstring& text, UINT codepage)
	{
		if (text.empty()) return {};
		int length = WideCharToMultiByte(codepage, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
		if (length <= 0) return {};
		std::string bytes(length, '\0');
		WideCharToMultiByte(codepage, 0, text.data(), (int)text.size(), bytes.data(), length, nullptr, nullptr);
		return bytes;
	}

	void WriteSink(Sink& sink, const std::wstring& text)
	{
		if (sink.handle == INVALID_HANDLE_VALUE) return;
		std::scoped_lock lock(g_writeMutex);
		DWORD written = 0;
		if (sink.console)
		{
			std::wstring line = text + L"\r\n";
			WriteConsoleW(sink.handle, line.c_str(), (DWORD)line.size(), &written, nullptr);
			return;
		}
		std::wstring line = text + (g_crlf ? L"\r\n" : L"\n");
		std::string bytes;
		if (sink.encoding == Encoding::Utf16)
		{
			if (!sink.bomWritten)
			{
				sink.bomWritten = true;
				bytes = "\xff\xfe";
			}
			bytes.append((const char*)line.data(), line.size() * sizeof(wchar_t));
		}
		else if (sink.encoding == Encoding::Ansi) bytes = WideToCodePage(line, CP_ACP);
		else bytes = WideStringToString(line);
		WriteFile(sink.handle, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
	}

	void Out(const std::wstring& text) { WriteSink(g_out, text); }
	void Info(const std::wstring& text) { if (!g_quiet) WriteSink(g_err, L"[info] " + text); }
	void Error(const std::wstring& text) { WriteSink(g_err, L"error: " + text); }

	void StripLineEnding(std::wstring& line)
	{
		if (!line.empty() && line.back() == L'\r') line.pop_back();
		if (!line.empty() && line.front() == L'\uFEFF') line.erase(0, 1);
	}

	void PrintVersion()
	{
#ifdef VERSION
		Out(L"TextractorCLI " + StringToWideString(VERSION) + (x64 ? L" (x64)" : L" (x86)"));
#else
		Out(x64 ? L"TextractorCLI (x64)" : L"TextractorCLI (x86)");
#endif
	}

	std::mutex g_connMutex;
	std::condition_variable g_connCv;
	std::set<DWORD> g_connected;

	std::vector<DWORD> ConnectedProcesses()
	{
		std::scoped_lock lock(g_connMutex);
		return { g_connected.begin(), g_connected.end() };
	}

	bool IsConnected(DWORD processId)
	{
		std::scoped_lock lock(g_connMutex);
		return g_connected.count(processId) != 0;
	}

	bool WaitForConnection(DWORD processId, std::chrono::milliseconds timeout)
	{
		std::unique_lock lock(g_connMutex);
		return g_connCv.wait_for(lock, timeout, [processId] { return g_connected.count(processId) != 0; });
	}

	void WaitForAllDisconnected()
	{
		std::unique_lock lock(g_connMutex);
		g_connCv.wait(lock, [] { return g_connected.empty(); });
	}

	bool WaitForHookThreadsGone(DWORD processId, uint64_t address, std::chrono::milliseconds timeout)
	{
		std::unique_lock lock(g_connMutex);
		return g_connCv.wait_for(lock, timeout, [processId, address]
		{
			auto found = g_hookThreads.find({ processId, address });
			return found == g_hookThreads.end() || found->second <= 0;
		});
	}

	void OnConnect(DWORD processId)
	{
		{
			std::scoped_lock lock(g_connMutex);
			g_connected.insert(processId);
		}
		g_connCv.notify_all();
		Info(FormatString(L"attached to process %u", processId));
	}

	void OnDisconnect(DWORD processId)
	{
		{
			std::scoped_lock lock(g_connMutex);
			g_connected.erase(processId);
		}
		g_connCv.notify_all();
		Info(FormatString(L"detached from process %u", processId));
	}

	bool IsSystemThread(const TextThread& thread)
	{
		return thread.tp == Host::console || thread.tp == Host::clipboard;
	}

	void OnThreadCreate(TextThread& thread)
	{
		if (IsSystemThread(thread)) return;
		{
			std::scoped_lock lock(g_connMutex);
			++g_hookThreads[{ thread.tp.processId, thread.tp.addr }];
		}
		Info(FormatString(L"thread %I64X created for process %u: %s addr %I64X (%s)",
			thread.handle, thread.tp.processId, thread.name.c_str(), thread.tp.addr,
			HookCode::Generate(thread.hp, thread.tp.processId).c_str()));
	}

	void OnThreadDestroy(TextThread& thread)
	{
		if (IsSystemThread(thread)) return;
		{
			std::scoped_lock lock(g_connMutex);
			auto found = g_hookThreads.find({ thread.tp.processId, thread.tp.addr });
			if (found != g_hookThreads.end() && --found->second <= 0) g_hookThreads.erase(found);
		}
		g_connCv.notify_all();
		Info(FormatString(L"thread %I64X destroyed", thread.handle));
	}

	bool OnTextOutput(TextThread& thread, std::wstring& output)
	{
		if (!g_showConsole && thread.tp == Host::console) return false;
		if (!g_showClipboard && thread.tp == Host::clipboard) return false;
		if (g_textOnly && !IsSystemThread(thread))
		{
			Out(output);
			return false;
		}
		Out(FormatString(L"[%I64X:%I32X:%I64X:%I64X:%I64X:%s:%s] %s",
			thread.handle,
			thread.tp.processId,
			thread.tp.addr,
			thread.tp.ctx,
			thread.tp.ctx2,
			thread.name.c_str(),
			HookCode::Generate(thread.hp, thread.tp.processId).c_str(),
			output.c_str()));
		return false;
	}

	std::optional<Encoding> ParseEncoding(const std::wstring& value)
	{
		if (_wcsicmp(value.c_str(), L"auto") == 0) return Encoding::Auto;
		if (_wcsicmp(value.c_str(), L"utf8") == 0 || _wcsicmp(value.c_str(), L"utf-8") == 0) return Encoding::Utf8;
		if (_wcsicmp(value.c_str(), L"utf16") == 0 || _wcsicmp(value.c_str(), L"utf-16") == 0 || _wcsicmp(value.c_str(), L"utf16le") == 0) return Encoding::Utf16;
		if (_wcsicmp(value.c_str(), L"ansi") == 0 || _wcsicmp(value.c_str(), L"acp") == 0 || _wcsicmp(value.c_str(), L"mbcs") == 0) return Encoding::Ansi;
		return std::nullopt;
	}

	bool IsValidUtf8(const std::string& bytes)
	{
		if (bytes.empty()) return true;
		return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), (int)bytes.size(), nullptr, 0) != 0;
	}

	bool IsIncompleteUtf8Head(const std::string& bytes)
	{
		if (bytes.empty()) return false;
		BYTE first = (BYTE)bytes[0];
		size_t needed = 0;
		if (first >= 0xC2 && first <= 0xDF) needed = 2;
		else if (first >= 0xE0 && first <= 0xEF) needed = 3;
		else if (first >= 0xF0 && first <= 0xF4) needed = 4;
		if (needed == 0 || bytes.size() >= needed) return false;
		for (size_t i = 1; i < bytes.size(); ++i) if (((BYTE)bytes[i] & 0xC0) != 0x80) return false;
		return true;
	}

	std::wstring DecodeBytes(const std::string& bytes)
	{
		size_t offset = 0;
		bool utf16 = false, bigEndian = false;
		if (bytes.size() >= 3 && (BYTE)bytes[0] == 0xEF && (BYTE)bytes[1] == 0xBB && (BYTE)bytes[2] == 0xBF) offset = 3;
		else if (bytes.size() >= 2 && (BYTE)bytes[0] == 0xFF && (BYTE)bytes[1] == 0xFE) { utf16 = true; offset = 2; }
		else if (bytes.size() >= 2 && (BYTE)bytes[0] == 0xFE && (BYTE)bytes[1] == 0xFF) { utf16 = true; bigEndian = true; offset = 2; }

		if (utf16)
		{
			std::wstring text;
			for (size_t i = offset; i + 1 < bytes.size(); i += 2)
				text.push_back((wchar_t)(bigEndian ? ((BYTE)bytes[i] << 8) | (BYTE)bytes[i + 1]
					: (BYTE)bytes[i] | ((BYTE)bytes[i + 1] << 8)));
			return text;
		}
		std::string body = bytes.substr(offset);
		if (IsValidUtf8(body)) return StringToWideString(body);
		if (auto wide = StringToWideString(body, CP_ACP)) return wide.value();
		return {};
	}

	class LineReader
	{
	public:
		void ForceEncoding(Encoding encoding) { forced = encoding; }
		bool Init();
		bool ReadLine(std::wstring& line);

	private:
		bool ReadConsoleLine(std::wstring& line);
		bool ReadStreamLine(std::wstring& line);
		bool Fill();
		void Decode();

		bool console = false;
		Encoding forced = Encoding::Auto;
		bool sniffed = false;
		bool utf16 = false, utf16be = false, ansi = false, utf8Broken = false, eof = false;
		std::string bytes;
		std::wstring text;
	};

	bool LineReader::Init()
	{
		console = IsConsoleHandle(GetStdHandle(STD_INPUT_HANDLE));
		_setmode(_fileno(stdin), console ? _O_U16TEXT : _O_BINARY);
		return true;
	}

	bool LineReader::Fill()
	{
		char buffer[4096];
		int count = _read(_fileno(stdin), buffer, (int)sizeof(buffer));
		if (count <= 0)
		{
			eof = true;
			return false;
		}
		bytes.append(buffer, count);
		return true;
	}

	void LineReader::Decode()
	{
		if (!sniffed)
		{
			bool possibleBom = !bytes.empty() && ((BYTE)bytes[0] == 0xEF || (BYTE)bytes[0] == 0xFF || (BYTE)bytes[0] == 0xFE);
			if (!eof && possibleBom && bytes.size() < 3) return;
			sniffed = true;
			if ((forced == Encoding::Auto || forced == Encoding::Utf8) && bytes.size() >= 3
				&& (BYTE)bytes[0] == 0xEF && (BYTE)bytes[1] == 0xBB && (BYTE)bytes[2] == 0xBF) bytes.erase(0, 3);
			else if ((forced == Encoding::Auto || forced == Encoding::Utf16) && bytes.size() >= 2
				&& (BYTE)bytes[0] == 0xFF && (BYTE)bytes[1] == 0xFE) { utf16 = true; bytes.erase(0, 2); }
			else if ((forced == Encoding::Auto || forced == Encoding::Utf16) && bytes.size() >= 2
				&& (BYTE)bytes[0] == 0xFE && (BYTE)bytes[1] == 0xFF) { utf16 = true; utf16be = true; bytes.erase(0, 2); }
			else if (forced == Encoding::Utf16) utf16 = true;
			else if (forced == Encoding::Ansi) ansi = true;
		}

		if (utf16)
		{
			size_t i = 0;
			for (; i + 1 < bytes.size(); i += 2)
			{
				unsigned value = utf16be ? (((unsigned)(BYTE)bytes[i] << 8) | (BYTE)bytes[i + 1])
					: ((unsigned)(BYTE)bytes[i] | ((unsigned)(BYTE)bytes[i + 1] << 8));
				text.push_back((wchar_t)value);
			}
			bytes.erase(0, i);
			return;
		}

		while (!bytes.empty())
		{
			if (!ansi)
			{
				if (!utf8Broken)
				{
					size_t usable = bytes.size();
					int wideLength = 0;
					std::vector<wchar_t> buffer(bytes.size() + 2);
					while (usable > 0 && (wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
						bytes.data(), (int)usable, buffer.data(), (int)buffer.size())) == 0) --usable;
					if (usable > 0)
					{
						text.append(buffer.data(), wideLength);
						bytes.erase(0, usable);
						continue;
					}
					if (!eof && IsIncompleteUtf8Head(bytes)) return; // wait for more data
					if (forced != Encoding::Utf8)
					{
						ansi = true;
						continue;
					}

					utf8Broken = true;
				}
				std::vector<wchar_t> wide(bytes.size() + 2);
				int length = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), wide.data(), (int)wide.size());
				if (length > 0) text.append(wide.data(), length);
				bytes.clear();
				continue;
			}
			size_t keep = (!eof && IsDBCSLeadByteEx(CP_ACP, (BYTE)bytes.back())) ? 1 : 0;
			size_t take = bytes.size() - keep;
			if (take == 0) return;
			std::vector<wchar_t> buffer(take + 2);
			int wideLength = MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)take, buffer.data(), (int)buffer.size());
			if (wideLength > 0) text.append(buffer.data(), wideLength);
			bytes.erase(0, take);
		}
	}

	bool LineReader::ReadConsoleLine(std::wstring& line)
	{
		wchar_t buffer[4096];
		line.clear();
		for (;;)
		{
			if (!fgetws(buffer, (int)(sizeof(buffer) / sizeof(buffer[0])), stdin))
			{
				if (line.empty()) return false;
				break;
			}
			size_t length = wcslen(buffer);
			bool complete = length > 0 && buffer[length - 1] == L'\n';
			if (complete) --length;
			line.append(buffer, length);
			if (complete) break;
		}
		StripLineEnding(line);
		return true;
	}

	bool LineReader::ReadStreamLine(std::wstring& line)
	{
		for (;;)
		{
			size_t end = text.find(L'\n');
			if (end != std::wstring::npos)
			{
				line.assign(text, 0, end);
				text.erase(0, end + 1);
				StripLineEnding(line);
				return true;
			}
			if (eof)
			{
				if (text.empty()) return false;
				line.clear();
				line.swap(text);
				StripLineEnding(line);
				return true;
			}
			Fill();
			Decode();
		}
	}

	bool LineReader::ReadLine(std::wstring& line)
	{
		return console ? ReadConsoleLine(line) : ReadStreamLine(line);
	}

	struct ProcessInfo
	{
		DWORD processId = 0;
		std::wstring name;
		std::wstring path;
		std::wstring arch = L"?";
		bool is64 = false;
	};

	bool OsIs64Bit()
	{
		static const bool result = []
		{
			SYSTEM_INFO info = {};
			GetNativeSystemInfo(&info);
			return info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
				|| info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64;
		}();
		return result;
	}

	std::optional<bool> IsProcess64Bit(DWORD processId)
	{
		if (!OsIs64Bit()) return false;
		if (AutoHandle<> process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId))
		{
			BOOL wow64 = FALSE;
			if (IsWow64Process(process, &wow64)) return !wow64;
		}
		return std::nullopt;
	}

	std::wstring GetProcessPath(DWORD processId)
	{
		if (AutoHandle<> process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId))
		{
			std::vector<wchar_t> buffer(MAX_PATH * 2);
			DWORD size = (DWORD)buffer.size();
			if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size)) return std::wstring(buffer.data(), size);
		}
		return {};
	}

	std::vector<ProcessInfo> GetProcesses()
	{
		std::vector<ProcessInfo> result;
		AutoHandle<> snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (!snapshot) return result;
		PROCESSENTRY32W entry = {};
		entry.dwSize = sizeof(entry);
		if (Process32FirstW(snapshot, &entry))
		{
			do
			{
				ProcessInfo info;
				info.processId = entry.th32ProcessID;
				info.name = entry.szExeFile;
				info.path = GetProcessPath(info.processId);
				if (!info.path.empty()) info.name = std::filesystem::path(info.path).filename().wstring();
				if (auto is64 = IsProcess64Bit(info.processId))
				{
					info.is64 = is64.value();
					info.arch = is64.value() ? L"x64" : L"x86";
				}
				result.push_back(std::move(info));
			} while (Process32NextW(snapshot, &entry));
		}
		return result;
	}

	// Get a module base address inside the target process
	std::optional<uint64_t> ResolveModuleBase(DWORD processId, const std::wstring& moduleName)
	{
		if (moduleName.empty()) return std::nullopt;
		AutoHandle<> snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
		if (!snapshot) return std::nullopt;
		MODULEENTRY32W entry = {};
		entry.dwSize = sizeof(entry);
		if (Module32FirstW(snapshot, &entry))
		{
			do
			{
				if (_wcsicmp(entry.szModule, moduleName.c_str()) == 0) return (uint64_t)entry.modBaseAddr;
			} while (Module32NextW(snapshot, &entry));
		}
		return std::nullopt;
	}

	std::wstring ExecutableDirectory()
	{
		std::vector<wchar_t> buffer(4096);
		DWORD length = GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());
		if (length == 0 || length >= buffer.size()) return {};
		return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path().wstring();
	}

	std::optional<DWORD> LaunchProgram(const std::wstring& path, std::wstring& error, const std::wstring& arguments = L"")
	{
		std::error_code code;
		std::filesystem::path executable = std::filesystem::absolute(path, code);
		if (code || !std::filesystem::exists(executable, code))
		{
			error = L"file not found: " + executable.wstring();
			return std::nullopt;
		}
		std::wstring commandLine = L"\"" + executable.wstring() + L"\"" + arguments;
		std::wstring directory = executable.parent_path().wstring();
		STARTUPINFOW startup = {};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process = {};
		if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
			directory.empty() ? nullptr : directory.c_str(), &startup, &process))
		{
			error = FormatString(L"cannot start '%s' (error %u)", executable.wstring().c_str(), GetLastError());
			return std::nullopt;
		}
		DWORD processId = process.dwProcessId;
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return processId;
	}

	bool EqualsNoCase(const std::wstring& a, const std::wstring& b)
	{
		return a.size() == b.size() && _wcsicmp(a.c_str(), b.c_str()) == 0;
	}

	bool ContainsNoCase(const std::wstring& text, const std::wstring& part)
	{
		if (part.empty()) return true;
		if (part.size() > text.size()) return false;
		return std::search(text.begin(), text.end(), part.begin(), part.end(),
			[](wchar_t a, wchar_t b) { return towlower(a) == towlower(b); }) != text.end();
	}

	std::vector<DWORD> FindProcessesByWindowTitle(const std::wstring& title)
	{
		std::vector<DWORD> processIds;
		struct Data { const std::wstring* title; std::vector<DWORD>* processIds; };
		Data data{ &title, &processIds };
		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
		{
			Data* data = (Data*)lParam;
			if (!IsWindowVisible(hwnd)) return TRUE;
			DWORD processId = 0;
			GetWindowThreadProcessId(hwnd, &processId);
			if (!processId) return TRUE;
			wchar_t caption[512] = {};
			GetWindowTextW(hwnd, caption, (int)(sizeof(caption) / sizeof(caption[0])));
			if (*caption && ContainsNoCase(caption, *data->title))
				if (std::find(data->processIds->begin(), data->processIds->end(), processId) == data->processIds->end())
					data->processIds->push_back(processId);
			return TRUE;
		}, (LPARAM)&data);
		return processIds;
	}

	struct TargetQuery
	{
		std::vector<DWORD> processIds; // -P
		std::wstring name;             // -N
		std::wstring title;            // -W
		bool all = false;              // --all
	};

	bool ParseInteger(const std::wstring& text, unsigned& value)
	{
		if (text.empty() || !iswdigit(text[0])) return false;
		wchar_t* end = nullptr;
		unsigned long parsed = wcstoul(text.c_str(), &end, 10);
		if (end == text.c_str() || *end != 0 || parsed == 0 || parsed > 0xFFFFFFFFul) return false;
		value = (unsigned)parsed;
		return true;
	}

	std::optional<std::wstring> TakeOptionValue(const std::vector<std::wstring>& args, size_t& index, const wchar_t* shortName, const wchar_t* longName = nullptr)
	{
		const std::wstring& arg = args[index];
		for (const wchar_t* name : { shortName, longName })
		{
			if (!name) continue;
			size_t length = wcslen(name);
			if (_wcsicmp(arg.c_str(), name) == 0)
			{
				if (index + 1 < args.size()) return args[++index];
				return std::wstring();
			}
			if (_wcsnicmp(arg.c_str(), name, length) == 0 && arg.size() > length)
			{
				std::wstring rest = arg.substr(length);
				if (!rest.empty() && rest[0] == L'=') return rest.substr(1);
				if (name == shortName) return rest;
			}
		}
		return std::nullopt;
	}

	bool ParseTargets(const std::vector<std::wstring>& args, TargetQuery& query, std::wstring& error)
	{
		for (size_t i = 0; i < args.size(); ++i)
		{
			const std::wstring& arg = args[i];

			if (auto value = TakeOptionValue(args, i, L"-P"))
			{
				unsigned processId = 0;
				if (!ParseInteger(*value, processId))
				{
					error = L"invalid process id '" + *value + L"'";
					return false;
				}
				query.processIds.push_back(processId);
			}
			else if (auto value = TakeOptionValue(args, i, L"-N")) query.name = *value;
			else if (auto value = TakeOptionValue(args, i, L"-W")) query.title = *value;
			else if (_wcsicmp(arg.c_str(), L"--all") == 0 || _wcsicmp(arg.c_str(), L"-a") == 0) query.all = true;
			else
			{
				error = L"unknown option '" + arg + L"'";
				return false;
			}
		}
		return true;
	}

	std::vector<DWORD> ResolveTargets(const TargetQuery& query, std::wstring& error)
	{
		std::vector<DWORD> processIds = query.processIds;
		auto add = [&processIds](DWORD processId)
		{
			if (std::find(processIds.begin(), processIds.end(), processId) == processIds.end()) processIds.push_back(processId);
		};

		std::vector<DWORD> matched;
		auto addMatched = [&matched](DWORD processId)
		{
			if (std::find(matched.begin(), matched.end(), processId) == matched.end()) matched.push_back(processId);
		};
		if (!query.name.empty())
		{
			std::wstring queryStem = std::filesystem::path(query.name).stem().wstring();
			for (const auto& info : GetProcesses())
			{
				std::wstring infoStem = std::filesystem::path(info.name).stem().wstring();
				if (EqualsNoCase(info.name, query.name)        // game.exe
					|| EqualsNoCase(infoStem, query.name)      // name given without the extension
					|| EqualsNoCase(info.name, queryStem))
				{
					add(info.processId);
					addMatched(info.processId);
				}
			}
		}
		if (!query.title.empty())
			for (DWORD processId : FindProcessesByWindowTitle(query.title))
			{
				add(processId);
				addMatched(processId);
			}

		processIds.erase(std::remove(processIds.begin(), processIds.end(), GetCurrentProcessId()), processIds.end());
		processIds.erase(std::remove(processIds.begin(), processIds.end(), 0), processIds.end());

		if (processIds.empty())
		{
			error = query.name.empty() && query.title.empty() ? L"no process specified" : L"no matching process found";
			return {};
		}
		if (!query.all && query.processIds.empty() && matched.size() > 1)
		{
			error = FormatString(L"matched %d processes, use -P <pid> or --all:", (int)matched.size());
			for (DWORD processId : matched) error += FormatString(L"\n         %5u  %s", processId, GetProcessPath(processId).c_str());
			return {};
		}
		return processIds;
	}

	std::optional<HookParam> TryParseHookCode(const std::wstring& code)
	{
		try
		{
			return HookCode::Parse(code);
		}
		catch (const std::exception&)
		{
			return std::nullopt;
		}
	}

	bool IsHookCode(const std::wstring& token)
	{
		return TryParseHookCode(token).has_value();
	}

	// -A accepts a plain absolute address (hex, 0x prefix optional) as well as a hook code;
	// module-relative hook codes are resolved to an absolute address inside the target process
	std::optional<uint64_t> ResolveHookAddress(DWORD processId, const std::wstring& value, std::wstring& error)
	{
		std::wstring text = value;
		if (text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X')) text.erase(0, 2);
		wchar_t* end = nullptr;
		unsigned long long address = wcstoull(text.c_str(), &end, 16);
		if (!text.empty() && end == text.c_str() + text.size() && address != 0) return (uint64_t)address;

		if (auto hook = TryParseHookCode(value))
		{
			HookParam hp = hook.value();
			if (hp.type & FUNCTION_OFFSET)
			{
				error = L"hook code '" + value + L"' is function-relative; use the absolute address (3rd field of the text output lines)";
				return std::nullopt;
			}
			if (hp.type & MODULE_OFFSET)
			{
				if (auto base = ResolveModuleBase(processId, hp.module)) return base.value() + hp.address;
				error = L"cannot resolve module '" + std::wstring(hp.module) + L"' in process " + std::to_wstring(processId)
					+ L" (try the absolute address from the 3rd field of the text output lines)";
				return std::nullopt;
			}
			return hp.address;
		}
		error = L"invalid address '" + value + L"' (expected hex like 140154B3A, or a hook code like HQ@1A2B3C4)";
		return std::nullopt;
	}

	// Read the hook list maintained by texthook.dll inside the target process.
	std::vector<HookParam> GetInstalledHooks(DWORD processId)
	{
		std::vector<HookParam> result;
		AutoHandle<> mappedFile = OpenFileMappingW(FILE_MAP_READ, FALSE, (ITH_SECTION_ + std::to_wstring(processId)).c_str());
		if (!mappedFile) return result;
		LPVOID view = MapViewOfFile(mappedFile, FILE_MAP_READ, 0, 0, HOOK_BUFFER_SIZE);
		if (!view) return result;
		AutoHandle<Functor<UnmapViewOfFile>> unmap = view;

		WinMutex viewMutex(ITH_HOOKMAN_MUTEX_ + std::to_wstring(processId)); // same mutex the DLL writes under, so no half-cleared entry is read
		std::scoped_lock lock(viewMutex);
		const TextHook(*hooks)[MAX_HOOK] = (const TextHook(*)[MAX_HOOK])view;
		for (const auto& hook : *hooks)
			if (hook.address) result.push_back(hook.hp);
		return result;
	}

	int CommandList(const std::vector<std::wstring>& args)
	{
		std::wstring filter = args.empty() ? L"" : args[0];
		auto processes = GetProcesses();
		std::sort(processes.begin(), processes.end(),
			[](const ProcessInfo& a, const ProcessInfo& b) { return a.processId < b.processId; });
		Out(L"   PID  ARCH  ATTACHED  INJECTABLE  NAME");
		for (const auto& info : processes)
		{
			if (!filter.empty() && !ContainsNoCase(info.name, filter) && !ContainsNoCase(info.path, filter)) continue;
			// arch is "?" when the architecture could not be queried (no rights), so it is not injectable
			bool injectable = info.arch != L"?" && info.is64 == x64 && info.processId != GetCurrentProcessId();
			Out(FormatString(L"%6u  %-4s  %-8s  %-10s  %s",
				info.processId,
				info.arch.c_str(),
				IsConnected(info.processId) ? L"yes" : L"",
				injectable ? L"yes" : L"",
				info.name.empty() ? info.path.c_str() : info.name.c_str()));
		}
		return EXIT_OK;
	}

	// Inject into one process and wait until it connects through the pipe
	int AttachToProcess(DWORD processId)
	{
		if (auto is64 = IsProcess64Bit(processId))
			if (is64.value() != x64)
			{
				Error(FormatString(L"cannot inject process %u: architecture mismatch (target is %s, this build is %s)",
					processId, is64.value() ? L"x64" : L"x86", x64 ? L"x64" : L"x86"));
				return EXIT_ATTACH_FAILED;
			}
		Info(FormatString(L"injecting into process %u", processId));
		Host::InjectProcess(processId);
		if (WaitForConnection(processId, std::chrono::seconds(5)))
		{
			Info(FormatString(L"process %u connected", processId));
			return EXIT_OK;
		}
		// the host layer reports the actual reason (already injected / injection failed) as Console text
		Error(FormatString(L"failed to attach to process %u (see console output above)", processId));
		return EXIT_ATTACH_FAILED;
	}

	int CommandAttach(const std::vector<std::wstring>& args)
	{
		TargetQuery query;
		std::wstring error;
		if (!ParseTargets(args, query, error))
		{
			Error(error);
			return EXIT_USAGE;
		}
		std::vector<DWORD> processIds = ResolveTargets(query, error);
		if (processIds.empty())
		{
			Error(error);
			return EXIT_NO_TARGET;
		}

		int result = EXIT_OK;
		for (DWORD processId : processIds)
			if (int code = AttachToProcess(processId)) result = code;
		return result;
	}

	int DetachProcesses(const std::vector<DWORD>& processIds)
	{
		int failures = 0;
		size_t sent = 0;
		for (DWORD processId : processIds)
		{
			try
			{
				Host::DetachProcess(processId);
				++sent;
				Info(FormatString(L"detaching from process %u", processId));
			}
			catch (const std::out_of_range&)
			{
				Error(FormatString(L"process %u is not attached", processId));
				++failures;
			}
		}
		if (sent)
		{
			std::unique_lock lock(g_connMutex);
			if (!g_connCv.wait_for(lock, std::chrono::seconds(5), [&processIds]
				{
					return std::all_of(processIds.begin(), processIds.end(), [](DWORD processId)
					{
						return g_connected.find(processId) == g_connected.end();
					});
				}))
			{
				lock.unlock();
				Error(L"timed out waiting for detach");
				++failures;
			}
		}
		return failures ? EXIT_DETACH_FAILED : EXIT_OK;
	}

	int DetachEverything()
	{
		std::vector<DWORD> connected = ConnectedProcesses();
		if (connected.empty()) return EXIT_OK;
		Info(FormatString(L"detaching from %d process(es)", (int)connected.size()));
		return DetachProcesses(connected);
	}

	// Hand over to the GUI. Everything is detached first, because Textractor.exe cannot attach to
	// the same game while our hook DLL is still loaded in it.
	int CommandGui()
	{
		std::wstring path = ExecutableDirectory() + L"\\Textractor.exe";
		std::error_code code;
		if (!std::filesystem::exists(path, code))
		{
			Error(L"cannot find Textractor.exe next to TextractorCLI.exe (" + path + L")");
			return EXIT_USAGE;
		}

		std::wstring arguments;
		for (DWORD processId : ConnectedProcesses()) arguments += FormatString(L" /p0x%X", processId);
		DetachEverything();
		std::wstring error;
		if (!LaunchProgram(path, error, arguments))
		{
			Error(error);
			return EXIT_USAGE;
		}
		Info(L"started Textractor.exe" + arguments + L", exiting");
		g_quit = true;
		return EXIT_OK;
	}

	int CommandDetach(const std::vector<std::wstring>& args)
	{
		TargetQuery query;
		std::wstring error;
		if (!ParseTargets(args, query, error))
		{
			Error(error);
			return EXIT_USAGE;
		}
		if (query.all && query.processIds.empty() && query.name.empty() && query.title.empty())
		{
			std::vector<DWORD> connected = ConnectedProcesses();
			if (connected.empty())
			{
				Info(L"nothing to detach");
				return EXIT_OK;
			}
			return DetachProcesses(connected);
		}
		if (query.processIds.empty() && query.name.empty() && query.title.empty())
		{
			Error(L"no process specified (use -P <pid>, -N <name>, -W <title> or --all)");
			return EXIT_USAGE;
		}
		std::vector<DWORD> processIds = ResolveTargets(query, error);
		if (processIds.empty())
		{
			Error(error);
			return EXIT_NO_TARGET;
		}
		return DetachProcesses(processIds);
	}

	int CommandRemove(const std::vector<std::wstring>& args)
	{
		std::vector<std::wstring> addresses, targetArgs;
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (auto value = TakeOptionValue(args, i, L"-A", L"--address"))
			{
				if (value->empty())
				{
					Error(L"option '-A' requires an address");
					return EXIT_USAGE;
				}
				addresses.push_back(value.value());
			}
			else targetArgs.push_back(args[i]);
		}
		if (addresses.empty())
		{
			Error(L"no address specified (use -A <hex addr|HookCode>)");
			return EXIT_USAGE;
		}

		TargetQuery query;
		std::wstring error;
		if (!ParseTargets(targetArgs, query, error))
		{
			Error(error);
			return EXIT_USAGE;
		}
		if (query.all && query.processIds.empty() && query.name.empty() && query.title.empty())
		{
			query.processIds = ConnectedProcesses();
			if (query.processIds.empty())
			{
				Error(L"no attached process (run 'attach' first)");
				return EXIT_NO_TARGET;
			}
		}
		std::vector<DWORD> processIds = ResolveTargets(query, error);
		if (processIds.empty())
		{
			Error(error);
			return EXIT_NO_TARGET;
		}

		int result = EXIT_OK;
		for (DWORD processId : processIds)
		{
			for (const std::wstring& value : addresses)
			{
				auto address = ResolveHookAddress(processId, value, error);
				if (!address)
				{
					Error(error);
					result = EXIT_USAGE;
					continue;
				}
				size_t before = 0;
				{
					std::scoped_lock lock(g_connMutex);
					auto found = g_hookThreads.find({ processId, address.value() });
					if (found != g_hookThreads.end()) before = found->second;
				}
				try
				{
					Host::RemoveHook(processId, address.value());
				}
				catch (const std::out_of_range&)
				{
					Error(FormatString(L"process %u is not attached (run 'attach' first)", processId));
					result = EXIT_NO_TARGET;
					continue;
				}
				if (before == 0)
				{
					Info(FormatString(L"remove sent for address %I64X in process %u (no text thread seen for it yet, cannot verify)",
						address.value(), processId));
					continue;
				}
				if (WaitForHookThreadsGone(processId, address.value(), std::chrono::seconds(2)))
					Info(FormatString(L"removed hook %I64X from process %u (%d text thread(s))", address.value(), processId, (int)before));
				else
				{
					Error(FormatString(L"hook %I64X in process %u did not go away (wrong address?)", address.value(), processId));
					result = EXIT_REMOVE_FAILED;
				}
			}
		}
		return result;
	}

	int CommandHooks(const std::vector<std::wstring>& args)
	{
		TargetQuery query;
		std::wstring error;
		if (!ParseTargets(args, query, error))
		{
			Error(error);
			return EXIT_USAGE;
		}
		if (query.processIds.empty() && query.name.empty() && query.title.empty())
		{
			query.processIds = ConnectedProcesses();
			if (query.processIds.empty())
			{
				Error(L"no attached process (run 'attach' first)");
				return EXIT_NO_TARGET;
			}
		}
		std::vector<DWORD> processIds = ResolveTargets(query, error);
		if (processIds.empty())
		{
			Error(error);
			return EXIT_NO_TARGET;
		}
		std::sort(processIds.begin(), processIds.end());

		Out(L"   PID  ADDR              KIND  CP     TEXT  NAME                      HOOKCODE");
		size_t total = 0;
		for (DWORD processId : processIds)
		{
			auto hooks = GetInstalledHooks(processId);
			if (hooks.empty())
			{
				Info(FormatString(L"no hooks found in process %u (not attached?)", processId));
				continue;
			}
			for (const auto& hook : hooks)
			{
				bool producesText = false;
				{
					std::scoped_lock lock(g_connMutex);
					producesText = g_hookThreads.find({ processId, hook.address }) != g_hookThreads.end();
				}
				Out(FormatString(L"%6u  %-16I64X  %-4s  %-5u  %-5s  %-24s  %s",
					processId,
					hook.address,
					(hook.type & DIRECT_READ) ? L"R" : L"H",
					hook.codepage,
					producesText ? L"yes" : L"",
					StringToWideString(hook.name).c_str(),
					HookCode::Generate(hook, processId).c_str()));
				++total;
			}
		}
		if (total) Info(L"use 'remove -P <pid> -A <ADDR>' to remove a hook");
		return EXIT_OK;
	}

	int CommandInsertHook(const HookParam& hook, const std::vector<std::wstring>& args)
	{
		TargetQuery query;
		std::wstring error;
		if (!ParseTargets(args, query, error))
		{
			Error(error);
			return EXIT_USAGE;
		}

		if (query.processIds.empty() && query.name.empty() && query.title.empty())
		{
			query.processIds = ConnectedProcesses();
			if (query.processIds.empty())
			{
				Error(L"no attached process (run 'attach' first)");
				return EXIT_NO_TARGET;
			}
		}
		std::vector<DWORD> processIds = ResolveTargets(query, error);
		if (processIds.empty())
		{
			Error(error);
			return EXIT_NO_TARGET;
		}

		int result = EXIT_OK;
		for (DWORD processId : processIds)
		{
			try
			{
				Host::InsertHook(processId, hook);
				Info(FormatString(L"inserted hook into process %u", processId));
			}
			catch (const std::out_of_range&)
			{
				Error(FormatString(L"process %u is not attached (run 'attach' first)", processId));
				result = EXIT_NO_TARGET;
			}
		}
		return result;
	}

	void PrintHelp()
	{
		Out(L"TextractorCLI - headless Textractor (" + std::wstring(x64 ? L"x64" : L"x86") + L")");
		Out(L"");
		Out(L"Usage:");
		Out(L"  TextractorCLI                          interactive mode, reads commands from stdin");
		Out(L"  TextractorCLI [options] [command...]   run commands; a successful attach keeps streaming");
		Out(L"  TextractorCLI attach -P 1234 H@1A2B3C4   attach + insert hook, then stream until Ctrl+C");
		Out(L"  TextractorCLI --launch game.exe --text-only   start the game, attach, stream bare text");
		Out(L"  Global options may appear anywhere; command options (-P/-N/-W/-A/--all) follow the command.");
		Out(L"");
		Out(L"Commands:");
		Out(L"  list [filter] | ps [filter]         list processes (PID / arch / attached / injectable)");
		Out(L"  attach -P <pid>                     inject into a process");
		Out(L"  attach -N <name> [--all]            inject by process name (e.g. game.exe)");
		Out(L"  attach -W <title> [--all]           inject by window title");
		Out(L"  detach -P <pid>                     detach from a process");
		Out(L"  detach --all                        detach from every attached process");
		Out(L"  remove -P <pid> -A <addr|HookCode>  remove a hook (addr = 3rd field of a text line)");
		Out(L"  hooks [--all|-P <pid>]              list installed hooks (ADDR can be passed to remove -A)");
		Out(L"  gui                                 start Textractor.exe (attached processes are handed over) and exit");
		Out(L"  <HookCode> -P <pid>                 insert a hook into an already attached process");
		Out(L"  help | ?                            show this help");
		Out(L"  version                             show version");
		Out(L"  quit | exit                         detach everything and exit");
		Out(L"");
		Out(L"Options:");
		Out(L"  -i, --interactive                   keep reading stdin after the command line/batch finished");
		Out(L"  --no-wait                           do not stay attached: run commands, detach, exit (smoke test)");
		Out(L"  --launch <path>                     start a game (relative or absolute path) and attach to it");
		Out(L"  --delay <ms>                        delay before injecting a launched game (default 3000; raise it");
		Out(L"                                      if the game loads its engine DLLs slowly)");
		Out(L"  --batch <file>                      read commands from a file (UTF-8/BOM, UTF-16 BOM, ANSI)");
		Out(L"  -q, --quiet                         suppress attach/detach and other notices");
		Out(L"  -e, --encoding <utf8|utf16|ansi>    encoding of redirected output (default utf8; utf16 writes a BOM)");
		Out(L"  --input-encoding <auto|utf8|utf16|ansi>");
		Out(L"                                      encoding of stdin (default auto: BOM -> UTF-8 -> system ANSI)");
		Out(L"  -o, --output <file>                 write the text to a file (\"-\" means stdout)");
		Out(L"  --no-console                        do not print the Console thread (incl. host error messages)");
		Out(L"  --clipboard                         also print the Clipboard thread (off by default)");
		Out(L"  --text-only                         print bare text, without the [handle:pid:...:hookcode] prefix");
		Out(L"  --crlf                              use CRLF for redirected output (default LF)");
		Out(L"  --dedup                             filter repeated text (TextThread::filterRepetition)");
		Out(L"  --flush-delay <ms>                  text flush interval (default 500)");
		Out(L"  --limit-length <chars>              max sentence length, 0 = unlimited (default 1000, longer ones are dropped)");
		Out(L"  --codepage <cp>                     default code page for hooks that do not specify one (default 932)");
		Out(L"");
		Out(L"Output:");
		Out(L"  stdout carries text lines only: [handle:pid:addr:ctx:ctx2:thread:hookcode] text");
		Out(L"  with --text-only it is the bare text; notices and errors always go to stderr: [info] ... / error: ...");
		Out(L"  the console is written as Unicode (independent of the code page); redirected output uses --encoding");
		Out(L"");
		Out(L"HookCode syntax (same as the \"hook code\" box of the GUI):");
		Out(L"  H...  classic hook (e.g. HQ@1A2B3C4, HB4@4A0123:gdi.dll:GetTextOutA)");
		Out(L"  R...  direct read");
		Out(L"  first letter: A=big endian pointer B=byte W=Unicode (length first) H=Unicode+hex dump");
		Out(L"                S=string Q=Unicode string V=UTF-8 string M=Unicode+hex dump");
		Out(L"  modifiers: F=full string N=no context X=custom function");
		Out(L"             <null_length>\\<  <codepage>#  <padding>+  <offset>  *deref  :split");
		Out(L"  address: @addr[:module[:function]]");
		Out(L"");
		Out(L"Exit codes: 0 ok  1 bad argument/command  2 injection failed  3 no matching process  4 detach failed/timed out  5 remove failed");
	}

	int ExecuteCommand(const std::vector<std::wstring>& tokens)
	{
		if (tokens.empty()) return EXIT_OK;
		const std::wstring& command = tokens[0];
		std::vector<std::wstring> args(tokens.begin() + 1, tokens.end());

		if (_wcsicmp(command.c_str(), L"list") == 0 || _wcsicmp(command.c_str(), L"ps") == 0) return CommandList(args);
		if (_wcsicmp(command.c_str(), L"attach") == 0 || _wcsicmp(command.c_str(), L"inject") == 0) return CommandAttach(args);
		if (_wcsicmp(command.c_str(), L"detach") == 0) return CommandDetach(args);
		if (_wcsicmp(command.c_str(), L"remove") == 0) return CommandRemove(args);
		if (_wcsicmp(command.c_str(), L"hooks") == 0) return CommandHooks(args);
		if (_wcsicmp(command.c_str(), L"gui") == 0) return CommandGui();
		if (_wcsicmp(command.c_str(), L"help") == 0 || command == L"?") { PrintHelp(); return EXIT_OK; }
		if (_wcsicmp(command.c_str(), L"version") == 0) { PrintVersion(); return EXIT_OK; }

		if (auto hook = TryParseHookCode(command)) return CommandInsertHook(hook.value(), args);

		Error(L"unknown command '" + command + L"' (type 'help' for usage)");
		return EXIT_USAGE;
	}

	bool StartsCommand(const std::wstring& token)
	{
		static const wchar_t* commands[] = { L"list", L"ps", L"hooks", L"gui", L"attach", L"inject", L"detach", L"remove", L"help", L"version", L"quit", L"exit", L"?" };
		for (const wchar_t* command : commands) if (_wcsicmp(token.c_str(), command) == 0) return true;
		return token.size() > 1 && IsHookCode(token);
	}

	bool IsQuit(const std::vector<std::wstring>& tokens)
	{
		return !tokens.empty() && (_wcsicmp(tokens[0].c_str(), L"quit") == 0 || _wcsicmp(tokens[0].c_str(), L"exit") == 0);
	}

	std::vector<std::wstring> Tokenize(const std::wstring& line)
	{
		std::vector<std::wstring> tokens;
		std::wstring current;
		bool quoted = false, hasQuotes = false;
		for (wchar_t character : line)
		{
			if (character == L'"')
			{
				quoted = !quoted;
				hasQuotes = true;
				continue;
			}
			if (!quoted && iswspace(character))
			{
				if (!current.empty() || hasQuotes) // hasQuotes keeps an empty quoted argument like "" from being dropped
				{
					tokens.push_back(current);
					current.clear();
				}
				hasQuotes = false;
				continue;
			}
			current.push_back(character);
		}
		if (!current.empty() || hasQuotes) tokens.push_back(current);
		return tokens;
	}

	std::optional<std::vector<std::vector<std::wstring>>> ReadCommandFile(const std::wstring& path)
	{
		AutoHandle<> file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (!file) return std::nullopt;
		std::string bytes;
		char buffer[8192];
		DWORD read = 0;
		while (ReadFile(file, buffer, (DWORD)sizeof(buffer), &read, nullptr) && read > 0) bytes.append(buffer, read);

		std::vector<std::vector<std::wstring>> commands;
		std::wstring text = DecodeBytes(bytes);
		size_t begin = 0;
		while (begin <= text.size())
		{
			size_t end = text.find(L'\n', begin);
			std::wstring line = end == std::wstring::npos ? text.substr(begin) : text.substr(begin, end - begin);
			begin = end == std::wstring::npos ? text.size() + 1 : end + 1;
			StripLineEnding(line);
			Trim(line);
			if (line.empty() || line[0] == L'#') continue;
			commands.push_back(Tokenize(line));
		}
		return commands;
	}

	BOOL WINAPI ControlHandler(DWORD type)
	{
		if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT
			|| type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT)
		{
			Info(L"interrupted, cleaning up");
			DetachEverything();
			ExitProcess(g_exitCode);
		}
		return FALSE;
	}
}

int main()
{
	int argc = 0;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	SetConsoleCtrlHandler(ControlHandler, TRUE);

	g_out.handle = GetStdHandle(STD_OUTPUT_HANDLE);
	g_out.console = IsConsoleHandle(g_out.handle);
	g_err.handle = GetStdHandle(STD_ERROR_HANDLE);
	g_err.console = IsConsoleHandle(g_err.handle);

	bool interactive = false, version = false, help = false, noWait = false;
	Encoding outputEncoding = Encoding::Utf8;
	Encoding forcedInput = Encoding::Auto;
	std::wstring outputFile, batchFile;
	bool outputFileSet = false;
	int flushDelay = -1, limitLength = -1;
	unsigned codepage = 0;
	bool dedup = false;
	std::wstring launchPath;
	int launchDelay = 3000; // ms to wait before injecting a game started by --launch
	std::vector<std::vector<std::wstring>> commands;
	std::vector<std::wstring> current;

	auto optionValue = [&](int& index, const std::wstring& token, const wchar_t* shortName, const wchar_t* longName) -> std::optional<std::wstring>
	{
		for (const wchar_t* name : { shortName, longName })
		{
			if (!name) continue;
			size_t length = wcslen(name);
			if (_wcsicmp(token.c_str(), name) == 0)
			{
				if (index + 1 < argc) return std::wstring(argv[++index]);
				return std::wstring();
			}
			if (_wcsnicmp(token.c_str(), name, length) == 0 && token.size() > length)
			{
				std::wstring rest = token.substr(length);
				if (!rest.empty() && rest[0] == L'=') return rest.substr(1);
				if (name == shortName) return rest;
			}
		}
		return std::nullopt;
	};

	for (int index = 1; index < argc; ++index)
	{
		std::wstring token = argv[index];
		auto isFlag = [&](const wchar_t* shortName, const wchar_t* longName)
		{
			return (shortName && _wcsicmp(token.c_str(), shortName) == 0) || (longName && _wcsicmp(token.c_str(), longName) == 0);
		};

		if (!token.empty() && token[0] == L'-' && !IsHookCode(token))
		{
			if (isFlag(L"-h", L"--help")) { help = true; continue; }
			if (isFlag(L"-v", L"--version")) { version = true; continue; }
			if (auto value = optionValue(index, token, L"-e", L"--encoding"))
			{
				auto encoding = ParseEncoding(*value);
				if (!encoding || *encoding == Encoding::Auto)
				{
					Error(L"invalid output encoding '" + *value + L"' (utf8|utf16|ansi)");
					return EXIT_USAGE;
				}
				outputEncoding = *encoding;
				continue;
			}
			if (auto value = optionValue(index, token, nullptr, L"--input-encoding"))
			{
				auto encoding = ParseEncoding(*value);
				if (!encoding)
				{
					Error(L"invalid input encoding '" + *value + L"' (auto|utf8|utf16|ansi)");
					return EXIT_USAGE;
				}
				forcedInput = *encoding;
				continue;
			}
			if (auto value = optionValue(index, token, L"-o", L"--output"))
			{
				if (value->empty())
				{
					Error(L"option '" + token + L"' requires a file path");
					return EXIT_USAGE;
				}
				outputFile = *value;
				outputFileSet = true;
				continue;
			}
			if (auto value = optionValue(index, token, nullptr, L"--batch"))
			{
				if (value->empty())
				{
					Error(L"option '" + token + L"' requires a file path");
					return EXIT_USAGE;
				}
				batchFile = *value;
				continue;
			}
			if (auto value = optionValue(index, token, nullptr, L"--flush-delay"))
			{
				wchar_t* end = nullptr;
				unsigned long parsed = value->empty() ? 0 : wcstoul(value->c_str(), &end, 10);
				if (!value->empty() && (end == value->c_str() || *end != 0))
				{
					Error(L"invalid flush delay '" + *value + L"'");
					return EXIT_USAGE;
				}
				flushDelay = (int)parsed;
				continue;
			}
			if (auto value = optionValue(index, token, nullptr, L"--limit-length"))
			{
				if (value->empty()) { limitLength = 0; continue; }
				wchar_t* end = nullptr;
				unsigned long parsed = wcstoul(value->c_str(), &end, 10);
				if (end == value->c_str() || *end != 0)
				{
					Error(L"invalid limit length '" + *value + L"'");
					return EXIT_USAGE;
				}
				limitLength = (int)parsed;
				continue;
			}
			if (auto value = optionValue(index, token, nullptr, L"--codepage"))
			{
				if (!ParseInteger(*value, codepage))
				{
					Error(L"invalid codepage '" + *value + L"'");
					return EXIT_USAGE;
				}
				continue;
			}
			if (auto value = optionValue(index, token, nullptr, L"--launch"))
			{
				if (value->empty())
				{
					Error(L"option '" + token + L"' requires a path");
					return EXIT_USAGE;
				}
				launchPath = *value;
				continue;
			}
			if (auto value = optionValue(index, token, nullptr, L"--delay"))
			{
				wchar_t* end = nullptr;
				unsigned long parsed = value->empty() ? 0 : wcstoul(value->c_str(), &end, 10);
				if (!value->empty() && (end == value->c_str() || *end != 0))
				{
					Error(L"invalid delay '" + *value + L"'");
					return EXIT_USAGE;
				}
				launchDelay = (int)parsed;
				continue;
			}
			if (_wcsicmp(token.c_str(), L"-i") == 0 || _wcsicmp(token.c_str(), L"--interactive") == 0) { interactive = true; continue; }
			if (_wcsicmp(token.c_str(), L"--no-wait") == 0) { noWait = true; continue; }
			if (_wcsicmp(token.c_str(), L"-q") == 0 || _wcsicmp(token.c_str(), L"--quiet") == 0) { g_quiet = true; continue; }
			if (_wcsicmp(token.c_str(), L"--no-console") == 0) { g_showConsole = false; continue; }
			if (_wcsicmp(token.c_str(), L"--clipboard") == 0) { g_showClipboard = true; continue; }
			if (_wcsicmp(token.c_str(), L"--no-clipboard") == 0) { g_showClipboard = false; continue; } // legacy spelling (already the default)
			if (_wcsicmp(token.c_str(), L"--text-only") == 0) { g_textOnly = true; continue; }
			if (_wcsicmp(token.c_str(), L"--crlf") == 0) { g_crlf = true; continue; }
			if (_wcsicmp(token.c_str(), L"--dedup") == 0) { dedup = true; continue; }

			if (current.empty())
			{
				Error(L"unknown option '" + token + L"' (command options like -P/-N/-W/-A go after the command; type 'help' for usage)");
				return EXIT_USAGE;
			}
		}

		if (!current.empty() && StartsCommand(token))
		{
			commands.push_back(current);
			current.clear();
		}
		if (current.empty())
		{
			if (!StartsCommand(token))
			{
				Error(L"unknown command '" + token + L"' (type 'help' for usage)");
				return EXIT_USAGE;
			}
			current.push_back(token);
		}
		else current.push_back(token);
	}
	if (!current.empty()) commands.push_back(current);

	if (help)
	{
		PrintHelp();
		return EXIT_OK;
	}
	if (version)
	{
		PrintVersion();
		return EXIT_OK;
	}

	g_out.encoding = outputEncoding;
	AutoHandle<> outputHandle = INVALID_HANDLE_VALUE;
	if (outputFileSet && outputFile != L"-")
	{
		outputHandle = CreateFileW(outputFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (!outputHandle)
		{
			Error(L"cannot open output file '" + outputFile + L"'");
			return EXIT_USAGE;
		}
		g_out.handle = outputHandle;
		g_out.console = false;
	}

	std::vector<std::vector<std::wstring>> batchCommands;
	if (!batchFile.empty())
	{
		auto parsed = ReadCommandFile(batchFile);
		if (!parsed)
		{
			Error(L"cannot read batch file '" + batchFile + L"'");
			return EXIT_USAGE;
		}
		batchCommands = parsed.value();
	}

	if (dedup) TextThread::filterRepetition = true;
	if (flushDelay >= 0) TextThread::flushDelay = flushDelay;
	if (limitLength >= 0) TextThread::limitStringLength = limitLength;
	if (codepage > 0) Host::defaultCodepage = (int)codepage;

	Host::Start(OnConnect, OnDisconnect, OnThreadCreate, OnThreadDestroy, OnTextOutput);

	if (!launchPath.empty())
	{
		std::wstring error;
		auto processId = LaunchProgram(launchPath, error);
		if (!processId)
		{
			Error(error);
			return EXIT_USAGE;
		}
		Info(FormatString(L"launched process %u (%s)", processId.value(), launchPath.c_str()));
		if (launchDelay > 0)
		{
			Info(FormatString(L"waiting %d ms before injecting (raise --delay if the game needs longer)", launchDelay));
			Sleep(launchDelay);
		}
		if (int code = AttachToProcess(processId.value())) g_exitCode = code;
	}

	auto run = [](const std::vector<std::wstring>& tokens)
	{
		int code = ExecuteCommand(tokens);
		if (code != EXIT_OK) g_exitCode = code;
	};

	// The "gui" command detaches everything and hands over to Textractor.exe, so stop after it.
	for (const auto& command : batchCommands)
	{
		if (IsQuit(command) || g_quit) break;
		run(command);
	}
	for (const auto& command : commands)
	{
		if (IsQuit(command) || g_quit) break;
		run(command);
	}

	// ---- interactive / stay attached
	const bool hasCommands = !commands.empty() || !batchCommands.empty();
	// In one-shot mode a successful attach keeps running by default (attaching and immediately
	// detaching again would be pointless); --no-wait disables that.
	bool keepRunning = interactive || !hasCommands || (!noWait && !ConnectedProcesses().empty());
	bool quitRequested = false;
	if (keepRunning)
	{
		LineReader reader;
		reader.ForceEncoding(forcedInput);
		reader.Init();

		Info(L"TextractorCLI " + std::wstring(x64 ? L"(x64)" : L"(x86)") + L" - type 'help' for usage, 'list' to list processes, 'quit' to detach and exit");
		if (forcedInput == Encoding::Auto && !IsConsoleHandle(GetStdHandle(STD_INPUT_HANDLE)))
			Info(L"reading commands from redirected input (UTF-8 / UTF-16 BOM / ANSI auto-detected)");
		std::wstring line;
		while (reader.ReadLine(line))
		{
			Trim(line);
			if (line.empty() || line[0] == L'#') continue;
			std::vector<std::wstring> tokens = Tokenize(line);
			if (tokens.empty()) continue;
			if (IsQuit(tokens))
			{
				quitRequested = true;
				break;
			}
			run(tokens);
			if (g_quit) break; // the "gui" command took over
		}

		if (!quitRequested && !interactive && !noWait && hasCommands && !ConnectedProcesses().empty())
		{
			Info(L"commands finished, still attached - press Ctrl+C to detach and exit");
			WaitForAllDisconnected();
		}
	}

	DetachEverything();
	ExitProcess(g_exitCode);
}
