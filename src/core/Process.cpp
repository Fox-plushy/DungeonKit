#include "Process.h"
#include <tlhelp32.h>
#include <wchar.h>
#include <algorithm>
#include <cctype>

namespace dk {

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string narrow(const wchar_t* w) {
    std::string s;
    for (; *w; ++w) s.push_back(*w < 128 ? (char)*w : '?');
    return s;
}

bool Process::attach(std::string& err, Target target) {
    detach();
    if (target == Target::None) { err = "No game selected."; return false; }

    DWORD shippingPid = 0, launcherPid = 0, gdkPid = 0;
    int count = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) { err = "Could not list processes."; return false; }
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            std::string n = lower(narrow(pe.szExeFile));
            if (n == "dungeons-win64-shipping.exe") { shippingPid = pe.th32ProcessID; ++count; }
            else if (n == "dungeons.exe")           { launcherPid = pe.th32ProcessID; ++count; }
            else if (n == "dungeons-wingdk-shipping.exe") { gdkPid = pe.th32ProcessID; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    // Same rule as the old table: if both exist the game is the Shipping exe.
    DWORD pid = shippingPid ? shippingPid : launcherPid;
    std::string name = shippingPid ? "Dungeons-Win64-Shipping.exe" : "Dungeons.exe";
    if (target == Target::Dungeons2Launcher) {
        pid = launcherPid;
        name = "Dungeons.exe";
        count = 1;
        if (!pid) { err = "Dungeons.exe is not running."; return false; }
    } else if (target == Target::Dungeons2) {
        // The Microsoft Store/Xbox build uses Dungeons-WinGDK-Shipping.exe. The Steam build uses the same plain
        // Dungeons-Win64-Shipping.exe (and Dungeons.exe launcher) that the original Minecraft Dungeons uses - which one
        // is actually running is down to which game you picked on Home, same as it already is for Minecraft Dungeons.
        pid = gdkPid ? gdkPid : (shippingPid ? shippingPid : launcherPid);
        name = gdkPid ? "Dungeons-WinGDK-Shipping.exe" : (shippingPid ? "Dungeons-Win64-Shipping.exe" : "Dungeons.exe");
        count = 1;
        if (!pid) { err = "Minecraft Dungeons II is not running."; return false; }
    } else if (!pid) { err = "Minecraft Dungeons is not running."; return false; }

    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) h = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                            PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!h) { err = "OpenProcess failed (error " + std::to_string(GetLastError()) + "). Run DungeonKit as administrator."; return false; }

    // Main module = first module in the snapshot.
    HANDLE ms = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (ms == INVALID_HANDLE_VALUE) { CloseHandle(h); err = "Could not list modules (error " + std::to_string(GetLastError()) + ")."; return false; }
    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    bool ok = Module32FirstW(ms, &me) != FALSE;
    CloseHandle(ms);
    if (!ok) { CloseHandle(h); err = "Could not read the main module."; return false; }

    h_ = h; pid_ = pid; exe_ = name; procCount_ = count; target_ = target;
    base_ = (uint64_t)(uintptr_t)me.modBaseAddr;
    size_ = me.modBaseSize;
    return true;
}

std::wstring Process::exePath() const {
    if (!h_) return {};
    wchar_t buf[1024];
    DWORD n = 1024;
    if (QueryFullProcessImageNameW(h_, 0, buf, &n)) return std::wstring(buf, n);
    return {};
}

uint64_t Process::moduleBase(const wchar_t* name) const {
    if (!h_) return 0;
    HANDLE ms = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
    if (ms == INVALID_HANDLE_VALUE) return 0;
    uint64_t base = 0;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    for (BOOL ok = Module32FirstW(ms, &me); ok; ok = Module32NextW(ms, &me)) {
        if (_wcsicmp(me.szModule, name) == 0) { base = (uint64_t)(uintptr_t)me.modBaseAddr; break; }
    }
    CloseHandle(ms);
    return base;
}

namespace {
struct FindCtx { DWORD pid; HWND best; };
BOOL CALLBACK findMainWindow(HWND h, LPARAM param) {
    FindCtx* ctx = reinterpret_cast<FindCtx*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != ctx->pid || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r;
    if (!GetWindowRect(h, &r)) return TRUE;
    const long area = (r.right - r.left) * (r.bottom - r.top);
    RECT br{};
    if (ctx->best) GetWindowRect(ctx->best, &br);
    if (!ctx->best || area > (br.right - br.left) * (br.bottom - br.top)) ctx->best = h;
    return TRUE;
}
}

bool Process::mainWindowRect(long& x, long& y, long& w, long& h) const {
    if (!h_ || !pid_) return false;
    FindCtx ctx{pid_, nullptr};
    EnumWindows(findMainWindow, reinterpret_cast<LPARAM>(&ctx));
    if (!ctx.best) return false;
    RECT r;
    if (!GetWindowRect(ctx.best, &r)) return false;
    x = r.left; y = r.top; w = r.right - r.left; h = r.bottom - r.top;
    return w > 0 && h > 0;
}

void Process::detach() {
    if (h_) CloseHandle(h_);
    h_ = nullptr; pid_ = 0; base_ = size_ = 0; exe_.clear(); procCount_ = 0;
}

unsigned long Process::exitCode() const {
    DWORD code = 259;
    if (h_ && GetExitCodeProcess(h_, &code)) return code;
    return 259;
}

bool Process::alive() const {
    if (!h_) return false;
    DWORD code = 0;
    return GetExitCodeProcess(h_, &code) && code == STILL_ACTIVE;
}

bool Process::read(uint64_t a, void* buf, size_t n) const {
    SIZE_T got = 0;
    return h_ && ReadProcessMemory(h_, (LPCVOID)(uintptr_t)a, buf, n, &got) && got == n;
}

bool Process::write(uint64_t a, const void* buf, size_t n) const {
    SIZE_T got = 0;
    return h_ && WriteProcessMemory(h_, (LPVOID)(uintptr_t)a, buf, n, &got) && got == n;
}

bool Process::writeCode(uint64_t a, const void* buf, size_t n) const {
    if (!h_) return false;
    DWORD old = 0;
    VirtualProtectEx(h_, (LPVOID)(uintptr_t)a, n, PAGE_EXECUTE_READWRITE, &old);
    bool ok = write(a, buf, n);
    DWORD tmp = 0;
    if (old) VirtualProtectEx(h_, (LPVOID)(uintptr_t)a, n, old, &tmp);
    FlushInstructionCache(h_, (LPCVOID)(uintptr_t)a, n);
    return ok;
}

std::vector<uint64_t> Process::scan(const Pattern& pat, size_t limit) const {
    std::vector<uint64_t> out;
    if (!h_ || !pat.valid) return out;

    const uint64_t modEnd = base_ + size_;
    const size_t overlap = pat.size() - 1;
    std::vector<uint8_t> buf;
    uint64_t addr = base_;

    while (addr < modEnd && out.size() < limit) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQueryEx(h_, (LPCVOID)(uintptr_t)addr, &mbi, sizeof(mbi))) break;
        uint64_t rBeg = (uint64_t)(uintptr_t)mbi.BaseAddress;
        uint64_t rEnd = std::min<uint64_t>(rBeg + mbi.RegionSize, modEnd);
        constexpr DWORD kExec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        bool scannable = mbi.State == MEM_COMMIT && (mbi.Protect & kExec) && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));
        if (scannable) {
            const size_t chunk = 4u << 20;
            for (uint64_t p = std::max(rBeg, addr); p < rEnd && out.size() < limit; ) {
                size_t n = (size_t)std::min<uint64_t>(chunk, rEnd - p);
                buf.resize(n);
                if (read(p, buf.data(), n)) {
                    for (size_t off : findAll(pat, buf.data(), n, limit - out.size()))
                        out.push_back(p + off);
                }
                if (p + n >= rEnd) break;
                p += n - overlap;   // overlap so matches across chunk edges are not missed
            }
        }
        if (rEnd <= addr) break;
        addr = rEnd;
    }
    // chunk overlap can report the same hit twice
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

uint64_t Process::allocNear(uint64_t target, size_t size) const {
    if (!h_) return 0;
    SYSTEM_INFO si; GetSystemInfo(&si);
    const uint64_t gran = si.dwAllocationGranularity;
    const uint64_t reach = 0x7FFF0000ull - 0x10000;           // keep both jmp directions in rel32 range
    uint64_t lo = target > reach ? target - reach : (uint64_t)(uintptr_t)si.lpMinimumApplicationAddress;
    uint64_t hi = std::min<uint64_t>(target + reach, (uint64_t)(uintptr_t)si.lpMaximumApplicationAddress);

    struct Cand { uint64_t addr; uint64_t dist; };
    std::vector<Cand> cands;
    uint64_t a = lo;
    while (a < hi) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQueryEx(h_, (LPCVOID)(uintptr_t)a, &mbi, sizeof(mbi))) break;
        uint64_t rBeg = (uint64_t)(uintptr_t)mbi.BaseAddress, rEnd = rBeg + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            uint64_t s = (std::max(rBeg, lo) + gran - 1) / gran * gran;
            for (uint64_t c : {s, (rEnd > size + gran ? (rEnd - size) / gran * gran : s)}) {
                if (c >= s && c + size <= rEnd && c >= lo && c + size <= hi)
                    cands.push_back({c, c > target ? c - target : target - c});
            }
        }
        if (rEnd <= a) break;
        a = rEnd;
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) { return x.dist < y.dist; });
    for (const auto& c : cands) {
        void* p = VirtualAllocEx(h_, (LPVOID)(uintptr_t)c.addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (p) return (uint64_t)(uintptr_t)p;
    }
    return 0;
}

void Process::freeMem(uint64_t addr) const {
    if (h_ && addr) VirtualFreeEx(h_, (LPVOID)(uintptr_t)addr, 0, MEM_RELEASE);
}

Process::Freeze::Freeze(const Process& p) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{}; te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != p.pid()) continue;
            HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
            if (!t) continue;
            if (SuspendThread(t) == (DWORD)-1) { CloseHandle(t); continue; }
            threads_.push_back(t);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

Process::Freeze::~Freeze() {
    for (HANDLE t : threads_) { ResumeThread(t); CloseHandle(t); }
}

int Process::terminateGameProcesses(int& failed) {
    failed = 0;
    int ended = 0;
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            const std::string n = lower(narrow(pe.szExeFile));
            if (n == "dungeons.exe" || n == "dungeons-win64-shipping.exe" || n == "dungeons-wingdk-shipping.exe")
                pids.push_back(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    for (DWORD pid : pids) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!h) { ++failed; continue; }
        if (TerminateProcess(h, 0)) {
            WaitForSingleObject(h, 3000);
            ++ended;
        } else {
            ++failed;
        }
        CloseHandle(h);
    }
    return ended;
}

} // namespace dk
