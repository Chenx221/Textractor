#pragma once

#include <windows.h>

namespace Engine {

bool InsertNwjsV8Hook(HMODULE module);

void ShutdownNwjsV8Hook();

} // namespace Engine

// EOF
