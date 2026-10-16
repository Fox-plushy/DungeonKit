#include "Injector.h"
#include <tlhelp32.h>
#include <cwchar>

namespace dk {

bool moduleLoaded(DWORD pid, const wchar_t* moduleName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, moduleName) == 0) { found = true; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

bool injectDll(DWORD pid, const std::wstring& dllPath, std::string& err) {
    if (dllPath.empty() || GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) { err = "The DLL file was not found."; return false; }
    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!h) { err = "Could not open the game (error " + std::to_string(GetLastError()) + "). Run DungeonKit as administrator."; return false; }

    BOOL wow = FALSE;
    if (IsWow64Process(h, &wow) && wow) { CloseHandle(h); err = "The game is a 32-bit process, but this DLL is 64-bit."; return false; }

    const SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { CloseHandle(h); err = "Could not reserve memory in the game."; return false; }
    if (!WriteProcessMemory(h, remote, dllPath.c_str(), bytes, nullptr)) {
        VirtualFreeEx(h, remote, 0, MEM_RELEASE);
        CloseHandle(h);
        err = "Could not write to the game's memory.";
        return false;
    }
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC load = k32 ? GetProcAddress(k32, "LoadLibraryW") : nullptr;
    if (!load) { VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); err = "Could not find LoadLibraryW."; return false; }
    HANDLE th = CreateRemoteThread(h, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<void*>(load)), remote, 0, nullptr);
    if (!th) {
        VirtualFreeEx(h, remote, 0, MEM_RELEASE);
        CloseHandle(h);
        err = "Could not start the loader in the game (error " + std::to_string(GetLastError()) + ").";
        return false;
    }
    const DWORD waited = WaitForSingleObject(th, 15000);
    DWORD code = 0;
    GetExitCodeThread(th, &code);
    CloseHandle(th);
    if (waited == WAIT_OBJECT_0) VirtualFreeEx(h, remote, 0, MEM_RELEASE);      // leave it if the loader is somehow still running
    CloseHandle(h);
    if (waited != WAIT_OBJECT_0) { err = "The game took too long to load the DLL."; return false; }
    if (code == 0) { err = "Windows refused to load the DLL. Antivirus, a missing file next to it, or the folder's permissions can cause this."; return false; }
    return true;
}

} // namespace dk
