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

} // namespace TextPipe

// EOF
