#pragma once
// Loads a DLL into a running process (LoadLibraryW through a remote thread). Used by Tools > Console.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace dk {

bool moduleLoaded(DWORD pid, const wchar_t* moduleName);                                    // is a module with this file name inside the process?
bool injectDll(DWORD pid, const std::wstring& dllPath, std::string& err);                   // err is a plain sentence when it fails

} // namespace dk
