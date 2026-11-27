#pragma once

#include <cstdint>
#include <string>

namespace TextPipe {

// Unique pipe name for this injection, e.g.
//   \\.\pipe\TextractorRenpy_<pid>_<tick>
std::string MakePipeName(const std::string& prefix);

void SetThreadIdentity(uintptr_t address);

void StartPipe(const std::string& pipeName);

void StopPipe();

// Feeds already-serialised JSON lines ("{\"s\":\"..\",\"t\":\"..\"}\n") in
// directly. Used by engines that cannot use the pipe because the page has no
// Node integration: they pull the queued lines out of the page instead.
void PushLines(const char* data, size_t length);

} // namespace TextPipe

// EOF
