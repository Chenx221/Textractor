// engine/common/text_transport.cc
//
// Shared named-pipe transport for the V8 engines.

#include "text_transport.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

#include <nlohmann/json.hpp>

#include "main.h"
#include "types.h"

namespace TextPipe {

namespace {

std::atomic<bool> stopping{ false };
std::mutex readerMutex;
std::vector<HANDLE> readerThreads;
HANDLE listenerThread = nullptr;
uintptr_t threadIdentity = 0;

void EmitLine(const std::string& line)
{
    try {
        auto j = nlohmann::json::parse(line);

        std::string text = j.value("t", "");
        if (text.empty()) return;

        std::string speaker = j.value("s", "");

        std::string full;
        if (!speaker.empty()) { full = speaker; full += '\n'; }
        full += text;

        int wlen = MultiByteToWideChar(CP_UTF8, 0, full.c_str(), (int)full.size(), nullptr, 0);
        if (wlen <= 0) return;
        std::wstring wide(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, full.c_str(), (int)full.size(), wide.data(), wlen);

        static thread_local BYTE buffer[PIPE_BUFFER_SIZE];
        int bytes = (int)(wide.size() * sizeof(wchar_t));
        const int limit = (int)(PIPE_BUFFER_SIZE - sizeof(ThreadParam));
        if (bytes > limit) bytes = limit;
        if (bytes <= 0) return;
        memcpy(buffer + sizeof(ThreadParam), wide.data(), bytes);

        TextOutput(ThreadParam{ GetCurrentProcessId(), (uint64_t)threadIdentity, 0, 0 },
                   (BYTE(*)[PIPE_BUFFER_SIZE]) & buffer, bytes);
    }
    catch (...) {
    }
}

void EmitLineSafe(const std::string& line)
{
    __try { EmitLine(line); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

} // unnamed namespace

void PushLines(const char* data, size_t length)
{
    if (!data || !length) return;

    const char* begin = data;
    const char* end = data + length;
    for (const char* p = data; p < end; ++p)
    {
        if (*p != '\n') continue;
        if (p > begin) EmitLineSafe(std::string(begin, p - begin));
        begin = p + 1;
    }
    if (begin < end) EmitLineSafe(std::string(begin, end - begin));
}

namespace {

DWORD WINAPI ReadLoop(LPVOID param)
{
    HANDLE pipe = (HANDLE)param;
    std::string accumulator;
    char chunk[4096];
    DWORD read = 0;

    while (!stopping.load(std::memory_order_acquire) &&
           ReadFile(pipe, chunk, sizeof(chunk), &read, nullptr) && read)
    {
        accumulator.append(chunk, read);
        size_t newline;
        while ((newline = accumulator.find('\n')) != std::string::npos)
        {
            std::string line = accumulator.substr(0, newline);
            accumulator.erase(0, newline + 1);
            EmitLineSafe(line);
        }
        if (accumulator.size() > (1 << 20)) accumulator.clear();
    }

    CloseHandle(pipe);
    return 0;
}

DWORD WINAPI ListenLoop(LPVOID param)
{
    std::string pipeName = *(std::string*)param;
    delete (std::string*)param;

    while (!stopping.load(std::memory_order_acquire))
    {
        HANDLE pipe = CreateNamedPipeA(pipeName.c_str(), PIPE_ACCESS_INBOUND,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       PIPE_UNLIMITED_INSTANCES, 0, 1 << 16, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            static int logged = 0;
            if (logged++ < 3) ConsoleOutput("Textractor: v8 pipe CreateNamedPipe failed: %lu", GetLastError());
            Sleep(1000);
            continue;
        }

        if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
        {
            static int logged = 0;
            if (logged++ < 3) ConsoleOutput("Textractor: v8 pipe ConnectNamedPipe failed: %lu", GetLastError());
            CloseHandle(pipe);
            if (stopping.load(std::memory_order_acquire)) break;
            Sleep(200);
            continue;
        }

        HANDLE reader = CreateThread(nullptr, 0, ReadLoop, pipe, 0, nullptr);
        if (reader)
        {
            std::lock_guard<std::mutex> guard(readerMutex);
            if (readerThreads.size() < 64) readerThreads.push_back(reader);
            else CloseHandle(reader);
        }
    }
    return 0;
}

} // unnamed namespace

std::string MakePipeName(const std::string& prefix)
{
    static const unsigned int instance = GetTickCount();
    return "\\\\.\\pipe\\" + prefix + std::to_string(GetCurrentProcessId()) +
           "_" + std::to_string(instance);
}

void SetThreadIdentity(uintptr_t address)
{
    threadIdentity = address;
}

void StartPipe(const std::string& pipeName)
{
    if (listenerThread) return;

    stopping.store(false, std::memory_order_release);

    auto* copy = new std::string(pipeName);
    listenerThread = CreateThread(nullptr, 0, ListenLoop, copy, 0, nullptr);
    if (!listenerThread)
    {
        delete copy;
        ConsoleOutput("Textractor: v8 pipe thread could not be created");
    }
}

void StopPipe()
{
    stopping.store(true, std::memory_order_release);

    if (listenerThread)
    {
        CancelSynchronousIo(listenerThread);
        WaitForSingleObject(listenerThread, 3000);
        CloseHandle(listenerThread);
        listenerThread = nullptr;
    }

    {
        std::lock_guard<std::mutex> guard(readerMutex);
        for (HANDLE thread : readerThreads) CancelSynchronousIo(thread);
        for (HANDLE thread : readerThreads) { WaitForSingleObject(thread, 1000); CloseHandle(thread); }
        readerThreads.clear();
    }
}

} // namespace TextPipe

// EOF`n