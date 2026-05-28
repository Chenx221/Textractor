#pragma once
#include <windows.h>
#include <metahost.h>
#include <string>

#pragma comment(lib, "mscoree.lib")

namespace CLRHelper {

struct JitAddressResult {
    void* address;
    bool success;
};

bool GetDotNetMethodAddress(
    const wchar_t* assemblyName,
    const wchar_t* typeName,
    const wchar_t* methodName,
    int parameterCount,
    void** outAddress
);

bool ExecuteManagedCode(
    ICLRRuntimeHost* pClrHost,
    const wchar_t* assemblyPath,
    const wchar_t* typeName,
    const wchar_t* methodName,
    const wchar_t* argument,
    DWORD* returnValue
);

bool ExtractHelperAssembly(const wchar_t* assemblyPath);

} // namespace CLRHelper

