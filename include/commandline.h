#pragma once

// commandline.h -- switch parsing that is not fooled by Chromium's decoys.
//
// Chromium appends this to the GPU process command line:
//     --ignored=" --type=renderer " /prefetch:2
// so a plain find(L"--type=renderer") marks the GPU process as a renderer.
// Walk the string while tracking double quotes and only accept a switch that
// starts outside quotes and whose value is exactly "renderer".

#include <string>
#include <wchar.h>

inline bool IsRendererCommandLine(const std::wstring& commandLine)
{
	const size_t n = commandLine.size();
	bool inQuotes = false;

	for (size_t i = 0; i < n; ++i)
	{
		const wchar_t c = commandLine[i];
		if (c == L'"')
		{
			if (i > 0 && commandLine[i - 1] == L'\\') continue; // escaped quote
			inQuotes = !inQuotes;
			continue;
		}
		if (inQuotes) continue;

		if (i + 7 > n || commandLine.compare(i, 7, L"--type=") != 0) continue;
		// a switch must start the command line or follow whitespace
		if (i > 0 && commandLine[i - 1] != L' ' && commandLine[i - 1] != L'\t') continue;

		size_t j = i + 7;
		if (j < n && commandLine[j] == L'"') ++j;      // --type="renderer"
		const size_t start = j;
		while (j < n && commandLine[j] != L' ' && commandLine[j] != L'\t' && commandLine[j] != L'"') ++j;
		if (j - start == 8 && _wcsnicmp(commandLine.c_str() + start, L"renderer", 8) == 0)
			return true;
	}
	return false;
}
