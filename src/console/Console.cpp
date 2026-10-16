// DungeonKit console DLL. Loaded into Minecraft Dungeons by Experiments > Console DLL (test).
// Shipping builds of the game never create the Unreal console, so the console key does nothing. This DLL creates it:
// it makes one Console object (the game's own Console class) and puts it in the game viewport's ViewportConsole slot,
// the same thing the engine does when it is not a shipping build. After that the game handles the console key itself.
// It installs no hooks and patches no code. Inspired by the Universal UE4 Unlocker (UUU) by Otis_Inf.
//
// Only builds it can verify are supported: the address of the engine's object-creation function is known for
// Minecraft Dungeons 1.17.0.0 (Xbox app / Minecraft Launcher) and is checked against a call site before it is used.
// On a build where that known address doesn't check out (Steam, so far), it falls back to finding the same function
// dynamically through the game's own reflection data instead - the same technique Minecraft Dungeons II's console DLL
// uses, which is why that one already works on both the Microsoft Store/Xbox build and Steam. See findSpawnObjectDynamic
// below: like the rest of this DLL, it refuses to guess on a build it hasn't confirmed, and logs the candidate bytes
// instead so a new build can be added once they're sent over.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include "../core/Process.h"
#include "../core/UE4.h"
#include "Toast.h"

using namespace dk;

namespace {

// Minecraft Dungeons 1.17.0.0: StaticConstructObject_Internal and a place in the engine that calls it (offsets from the module base).
const uint64_t kCreateObjectRva = 0x12498A0;
const uint64_t kCreateObjectCallSiteRva = 0x34523D;

HMODULE g_self = nullptr;
FILE* g_log = nullptr;
std::wstring g_dir;

void openLog() {
    wchar_t path[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameW(g_self, path, MAX_PATH * 2);
    std::wstring dir(path, n);
    const size_t slash = dir.find_last_of(L"\\/");
    dir = slash == std::wstring::npos ? L"." : dir.substr(0, slash);
    g_dir = dir;
    g_log = _wfopen((dir + L"\\DungeonKitConsole.log").c_str(), L"ab");
    if (!g_log) {
        wchar_t tmp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tmp);
        g_log = _wfopen((std::wstring(tmp) + L"DungeonKitConsole.log").c_str(), L"ab");
    }
}

void say(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

std::string hex(uint64_t v) {
    char b[32];
    snprintf(b, sizeof b, "0x%llX", (unsigned long long)v);
    return b;
}

LONG CALLBACK onException(PEXCEPTION_POINTERS e) {          // only installed around the one call, so a crash leaves a note in the log
    const DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code == 0xC0000005 || code == 0xC0000094 || code == 0xC000001D || code == 0xC00000FD)
        say("The game raised exception 0x%lX at %s while the console was being created.", (unsigned long)code, hex((uint64_t)(uintptr_t)e->ExceptionRecord->ExceptionAddress).c_str());
    return EXCEPTION_CONTINUE_SEARCH;
}

// Is the creation function really where we think? An E8 call near the known call site must lead straight to it.
bool verifyCreateFunction(const Process& p, uint64_t& fn, std::string& why) {
    const uint64_t base = p.base();
    fn = base + kCreateObjectRva;
    uint8_t buf[64] = {};
    const uint64_t from = base + kCreateObjectCallSiteRva - 16;
    if (!p.read(from, buf, sizeof buf)) { why = "could not read the call site"; return false; }
    bool linked = false;
    for (int i = 0; i + 5 <= (int)sizeof buf; ++i) {
        if (buf[i] != 0xE8) continue;
        int32_t rel = 0;
        memcpy(&rel, buf + i + 1, 4);
        if (from + (uint64_t)i + 5 + (int64_t)rel == fn) { linked = true; break; }
    }
    if (!linked) { why = "no call to the expected address was found at the known call site, so this is a different game build"; return false; }
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery((LPCVOID)(uintptr_t)fn, &mbi, sizeof mbi) ||
        !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) { why = "the address is not executable code"; return false; }
    return true;
}

typedef void* (*CreateObjectFn)(void* cls, void* outer, uint64_t name, uint32_t flags, uint32_t internalFlags,
                                void* tmpl, bool copyTransients, void* instanceGraph, bool assumeArchetype);

// ---- Fallback for builds where kCreateObjectRva/kCreateObjectCallSiteRva don't check out (Steam, so far): find
// GameplayStatics::SpawnObject's native entry point dynamically through the game's own reflection data, the same way
// Minecraft Dungeons II's console DLL does (see Console2.cpp). Builds known so far are listed in kKnownPrologues; on a
// build not in that list this refuses to use what it found (calling an unconfirmed function pointer could crash the
// game) and instead logs the candidate bytes, so a new build can be added once this log is sent over.
bool isExecutable(const Process& p, uint64_t addr) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery((LPCVOID)(uintptr_t)addr, &mbi, sizeof mbi)) return false;
    return (mbi.State == MEM_COMMIT) && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}

std::string hexBytes(const unsigned char* b, size_t n) {
    std::string s;
    char tmp[4];
    for (size_t i = 0; i < n; ++i) { snprintf(tmp, sizeof tmp, "%02X ", b[i]); s += tmp; }
    return s;
}

bool findSpawnObjectDynamic(UE4& ue, Process& proc, const UE4::ObjArray& arr, uint64_t& fn, std::string& why) {
    const uint64_t base = proc.base(), end = base + proc.moduleSize();
    uint64_t fnObj = 0;
    const int limit = arr.num < 3000000 ? arr.num : 3000000;
    for (int i = 0; i < limit && !fnObj; ++i) {
        const uint64_t o = ue.objectAt(arr, i);
        if (!UE4::plausible(o) || ue.objectName(o) != "SpawnObject" || ue.className(o) != "Function") continue;
        const uint64_t outer = ue.rd64(o + 0x20);
        if (UE4::plausible(outer) && ue.objectName(outer) == "GameplayStatics") fnObj = o;
    }
    if (!fnObj) { why = "the SpawnObject function was not found in this build's reflection data"; return false; }
    uint64_t thunk = 0;
    for (uint64_t off = 0x30; off < 0x180 && !thunk; off += 8) {
        const uint64_t q = ue.rd64(fnObj + off);
        if (q >= base && q < end && isExecutable(proc, q)) thunk = q;
    }
    if (!thunk) { why = "the SpawnObject entry point was not found"; return false; }
    const size_t n = 0x140;
    std::string buf(n, '\0');
    if (!proc.read(thunk, &buf[0], n)) { why = "could not read the SpawnObject entry point"; return false; }
    // Known SpawnObject prologues, one per build this DLL has been confirmed against. None right now: a candidate at
    // module+0x1F92210 (bytes: 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 50) was tried for Steam 1.17.0.0 based on it
    // looking the most like a real function among the three call-site candidates seen - it instantly crashed the game
    // on the user's own test (access violation while the console was being created), so it is RULED OUT, not pending.
    // Do not add it back. The other two candidates from that same log (module+0x124DFB0 and +0x124DFE0) still look like
    // small field/flag-check helpers rather than the function itself and have not been tried.
    static const unsigned char* const kKnownPrologues[] = {nullptr};
    static const char* const kKnownNames[] = {nullptr};
    static const int kKnownCount = 0;
    bool sawAnyCandidate = false;
    std::string candidateLog;
    for (size_t i = 0; i + 8 <= n; ++i) {
        if ((unsigned char)buf[i] != 0xE8) continue;
        int32_t rel = 0;
        memcpy(&rel, &buf[i + 1], 4);
        const uint64_t t = thunk + i + 5 + (int64_t)rel;
        if (t < base || t >= end || !isExecutable(proc, t)) continue;
        unsigned char head[15] = {};
        if (!proc.read(t, head, sizeof head)) continue;
        sawAnyCandidate = true;
        for (int k = 0; k < kKnownCount; ++k) {
            if (memcmp(head, kKnownPrologues[k], sizeof head) == 0) { fn = t; say("SpawnObject matches the known %s build.", kKnownNames[k]); return true; }
        }
        candidateLog += "  candidate at module+" + hex(t - base) + ": " + hexBytes(head, sizeof head) + "\n";
    }
    if (!sawAnyCandidate) { why = "no call to an executable address was found in SpawnObject's entry point, so this is a different game build"; return false; }
    say("SpawnObject's entry point did not match any known build. Candidate bytes seen:\n%s", candidateLog.c_str());
    why = "the start of SpawnObject is not a build this DLL knows yet - this log now has the bytes needed to add it, please send it over";
    return false;
}


// ---- Console key. DungeonKit writes DungeonKitConsole.ini (key=F2) next to this DLL. The game's console key list (InputSettings.ConsoleKeys)
// is a plain array of keys, each 0x18 bytes: an FName (index + number), then a pointer the engine fills in again by itself.
struct KeyList { uint64_t arrayAddr = 0; std::vector<uint32_t> orig; };

std::string readKeySetting() {
    char buf[64] = {};
    const std::wstring ini = g_dir + L"\\DungeonKitConsole.ini";
    wchar_t w[64] = {};
    GetPrivateProfileStringW(L"console", L"key", L"Tilde", w, 63, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, 63, nullptr, nullptr);
    std::string k = buf;
    while (!k.empty() && (k.back() == ' ' || k.back() == '\r')) k.pop_back();
    return k.empty() ? "Tilde" : k;
}

bool sameText(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

uint32_t findNameIndex(UE4& ue, const std::string& text) {
    for (uint32_t i = 1; i < 400000; ++i) if (sameText(ue.nameAt(i), text)) return i;     // key names are registered early, so this ends quickly
    return 0;
}

void setConsoleKeys(UE4& ue, Process& proc, std::vector<KeyList>& lists, const std::string& key) {
    const bool restore = sameText(key, "Tilde");
    uint32_t idx = 0;
    if (!restore) {
        idx = findNameIndex(ue, key);
        if (!idx) { say("The key name '%s' was not found in the game, so the console key was not changed.", key.c_str()); return; }
    }
    for (const KeyList& kl : lists) {
        const uint64_t data = ue.rd64(kl.arrayAddr);
        int32_t num = 0;
        ue.rd(kl.arrayAddr + 8, num);
        if (!UE4::plausible(data) || num < 1 || num > 8) continue;
        for (int k = 0; k < num && k < (int)kl.orig.size(); ++k) {
            uint8_t entry[0x18] = {};                                   // name index, number 0, and a cleared pointer the engine looks up again
            const uint32_t use = restore ? kl.orig[k] : idx;
            memcpy(entry, &use, 4);
            proc.write(data + (uint64_t)k * 0x18, entry, sizeof entry);
        }
    }
    say("Console key is now %s.", restore ? "Tilde (default)" : key.c_str());
}

std::wstring keyLabel(const std::string& key) {
    if (sameText(key, "Tilde")) return L"~";
    return std::wstring(key.begin(), key.end());
}

void notifyFail() {
    toast::show(g_self, L"DungeonKit console could not start", L"Open the Console DLL card in DungeonKit and press Refresh log to see why.", false, 0);
}

uint64_t findClass(UE4& ue, const UE4::ObjArray& arr, const char* name) {
    const int limit = arr.num < 1000000 ? arr.num : 1000000;
    for (int i = 0; i < limit; ++i) {
        const uint64_t o = ue.objectAt(arr, i);
        if (UE4::plausible(o) && ue.className(o) == "Class" && ue.objectName(o) == name) return o;
    }
    return 0;
}

// Calls the engine's object-creation function with the crash note installed. Returns the new object or 0.
// Two different native functions can end up here depending on which path found fn (see run() below), and they take
// different arguments, so each gets its own typedef and wrapper - calling one with the other's signature would read
// garbage off the stack/registers and likely crash the game.
uint64_t construct(uint64_t fn, uint64_t cls, uint64_t outer) {
    void* handler = AddVectoredExceptionHandler(1, onException);
    void* made = reinterpret_cast<CreateObjectFn>(fn)((void*)(uintptr_t)cls, (void*)(uintptr_t)outer, 0, 0, 0, nullptr, false, nullptr, false);
    if (handler) RemoveVectoredExceptionHandler(handler);
    return (uint64_t)(uintptr_t)made;
}

// UObject* UGameplayStatics::SpawnObject(TSubclassOf<UObject>& ObjectClass, UObject* Outer) - found by findSpawnObjectDynamic.
typedef void* (*SpawnObjectFn)(void** objectClass, void* outer);

uint64_t constructDynamic(uint64_t fn, uint64_t cls, uint64_t outer) {
    void* c = (void*)(uintptr_t)cls;
    void* handler = AddVectoredExceptionHandler(1, onException);
    void* made = reinterpret_cast<SpawnObjectFn>(fn)(&c, (void*)(uintptr_t)outer);
    if (handler) RemoveVectoredExceptionHandler(handler);
    return (uint64_t)(uintptr_t)made;
}

// The local PlayerController, found the same way the camera editor does it.
uint64_t localController(UE4& ue) {
    uint64_t eng = 0, gi = 0, pc = 0;
    if (!ue.engine(eng) || !ue.readPtr(eng, "GameInstance", gi) || !UE4::plausible(gi)) return 0;
    uint64_t players = 0;
    int num = 0;
    if (!ue.readArray(gi, "LocalPlayers", players, num) || num < 1 || !UE4::plausible(players)) return 0;
    const uint64_t lp = ue.rd64(players);
    if (!UE4::plausible(lp) || !ue.readPtr(lp, "PlayerController", pc) || !UE4::plausible(pc)) return 0;
    return pc;
}

void run() {
    say("---- DungeonKit console DLL loaded. ----");
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    char exeA[MAX_PATH] = {};
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, exeA, MAX_PATH - 1, nullptr, nullptr);
    say("Process: %s (pid %lu)", exeA, (unsigned long)GetCurrentProcessId());
    std::string lower = exeA;
    for (char& c : lower) c = (char)tolower((unsigned char)c);
    if (lower.find("wingdk") != std::string::npos) { say("This is Minecraft Dungeons II. The console DLL only supports Minecraft Dungeons for now. Nothing was changed."); notifyFail(); return; }

    Process proc;
    UE4 ue;
    std::string err, lastErr;
    bool ready = false;
    for (int attempt = 0; attempt < 30 && !ready; ++attempt) {
        if (!proc.attach(err, Process::Target::Dungeons)) { if (err != lastErr) say("Attach: %s", err.c_str()); lastErr = err; Sleep(2000); continue; }
        if (ue.init(proc, err)) { ready = true; break; }
        if (err != lastErr) say("Engine not ready yet: %s", err.c_str());
        lastErr = err;
        Sleep(2000);
    }
    if (!ready) { say("Gave up: the engine could not be found. %s", lastErr.c_str()); notifyFail(); return; }
    say("Engine found.");

    uint64_t eng = 0, vp = 0;
    if (!ue.engine(eng) || !ue.readPtr(eng, "GameViewport", vp) || !UE4::plausible(vp)) { say("The game viewport was not found. Load into the game and try again. Nothing was changed."); notifyFail(); return; }
    UE4::Prop slot;
    if (!ue.find(vp, "ViewportConsole", slot)) { say("The ViewportConsole slot was not found. Nothing was changed."); notifyFail(); return; }
    const uint64_t slotAddr = vp + slot.offset;
    const uint64_t existing = ue.rd64(slotAddr);

    UE4::ObjArray arr;
    if (!ue.objectArray(arr, err)) { say("Object table: %s", err.c_str()); notifyFail(); return; }
    const uint64_t consoleClass = findClass(ue, arr, "Console");
    const uint64_t cheatClassScanned = findClass(ue, arr, "CheatManager");
    say("Console class: %s, CheatManager class: %s", hex(consoleClass).c_str(), hex(cheatClassScanned).c_str());

    std::vector<KeyList> keyLists;
    {
        const int lim = arr.num < 1000000 ? arr.num : 1000000;
        for (int i = 0; i < lim && keyLists.size() < 4; ++i) {
            const uint64_t o = ue.objectAt(arr, i);
            if (!UE4::plausible(o) || ue.className(o) != "InputSettings") continue;
            UE4::Prop pr;
            if (!ue.find(o, "ConsoleKeys", pr)) continue;
            KeyList kl;
            kl.arrayAddr = o + pr.offset;
            const uint64_t data = ue.rd64(kl.arrayAddr);
            int32_t num = 0;
            ue.rd(kl.arrayAddr + 8, num);
            if (!UE4::plausible(data) || num < 1 || num > 8) continue;
            for (int k = 0; k < num; ++k) { uint32_t n = 0; ue.rd(data + (uint64_t)k * 0x18, n); kl.orig.push_back(n); }
            keyLists.push_back(kl);
        }
        say("Found %d input settings object(s) with a console key list.", (int)keyLists.size());
    }
    std::string appliedKey = "Tilde";
    {
        const std::string want = readKeySetting();
        if (!keyLists.empty() && !sameText(want, "Tilde")) { setConsoleKeys(ue, proc, keyLists, want); appliedKey = want; }
    }

    uint64_t fn = 0;
    bool usingDynamicSpawn = false;
    std::string why;
    if (verifyCreateFunction(proc, fn, why)) {
        say("Creation function verified at %s.", hex(fn).c_str());
    } else {
        std::string why2;
        say("The known build's creation function was not found (%s). Trying to find it dynamically instead...", why.c_str());
        if (!findSpawnObjectDynamic(ue, proc, arr, fn, why2)) {
            say("Not creating anything: %s. Nothing was changed. Please send this log to Plushy.", why2.c_str());
            notifyFail();
            return;
        }
        usingDynamicSpawn = true;
        say("SpawnObject found dynamically at module+%s.", hex(fn - proc.base()).c_str());
    }
    auto createObject = [fn, usingDynamicSpawn](uint64_t cls, uint64_t outer) -> uint64_t {
        return usingDynamicSpawn ? constructDynamic(fn, cls, outer) : construct(fn, cls, outer);
    };

    if (UE4::plausible(existing)) {
        say("A console already exists (%s). Press the console key in the game.", hex(existing).c_str());
        { const std::wstring m = L"The console is already on. Press " + keyLabel(appliedKey) + L" in the game to open it."; toast::show(g_self, L"DungeonKit is connected", m.c_str(), true, 0); }
    } else if (!consoleClass) {
        say("The Console class was not found. No console was made.");
        notifyFail();
    } else {
        say("Creating the console now...");
        const uint64_t obj = createObject(consoleClass, vp);
        bool ok = false;
        if (!UE4::plausible(obj)) say("The game did not return a console object. Nothing was changed.");
        else {
            const std::string cls = ue.className(obj);
            say("Console object: %s (%s)", hex(obj).c_str(), cls.c_str());
            if (cls != "Console") say("That object is not a Console, so it was not used.");
            else if (!proc.write(slotAddr, &obj, sizeof obj)) say("Could not store the console in the viewport.");
            else ok = true;
        }
        if (ok) {
            say("Console ready. Press the console key (default ~) in the game to open it.");
            { const std::wstring m = L"Console ready. Press " + keyLabel(appliedKey) + L" in the game to open it."; toast::show(g_self, L"DungeonKit is connected", m.c_str(), true, 0); }
        } else notifyFail();
    }

    // Cheat manager: shipping builds never make one, so slomo, summon and similar commands have nowhere to go.
    // A new PlayerController is made on every level load, so this keeps watching and makes one for each.
    uint64_t lastPc = 0, failedPc = 0;
    bool announced = false;
    for (;;) {
        Sleep(1500);
        if (!keyLists.empty()) {                                  // the key can be changed from DungeonKit at any time
            const std::string want = readKeySetting();
            if (!sameText(want, appliedKey)) {
                setConsoleKeys(ue, proc, keyLists, want);
                appliedKey = want;
                const std::wstring m = L"Press " + keyLabel(want) + L" to open the console.";
                toast::show(g_self, L"Console key changed", m.c_str(), true, 0);
            }
        }
        const uint64_t pc = localController(ue);
        if (!pc) { lastPc = 0; continue; }
        if (pc != lastPc) { lastPc = pc; continue; }          // wait until the same controller is seen twice
        if (pc == failedPc) continue;
        UE4::Prop cm;
        if (!ue.find(pc, "CheatManager", cm)) { say("The CheatManager slot was not found on the player controller."); failedPc = pc; continue; }
        if (UE4::plausible(ue.rd64(pc + cm.offset))) {
            if (!announced) { announced = true; }
            continue;
        }
        UE4::Prop cam;
        uint64_t camMgr = 0;
        if (ue.find(pc, "PlayerCameraManager", cam)) camMgr = ue.rd64(pc + cam.offset);
        if (!UE4::plausible(camMgr)) continue;
        uint64_t cls = 0;
        if (ue.readPtr(pc, "CheatClass", cls) && UE4::plausible(cls) && ue.objectName(cls).find("CheatManager") == std::string::npos) cls = 0;
        if (!UE4::plausible(cls)) cls = cheatClassScanned;
        if (!cls) { say("The CheatManager class was not found."); failedPc = pc; continue; }
        say("New player controller %s. Creating a cheat manager...", hex(pc).c_str());
        const uint64_t obj = createObject(cls, pc);
        if (!UE4::plausible(obj) || ue.className(obj).find("CheatManager") == std::string::npos) {
            say("The game did not return a cheat manager. Will not retry for this controller.");
            failedPc = pc;
            continue;
        }
        if (!proc.write(pc + cm.offset, &obj, sizeof obj)) { say("Could not store the cheat manager."); failedPc = pc; continue; }
        say("Cheat manager %s (%s) is in place.", hex(obj).c_str(), ue.className(obj).c_str());
        if (!announced) {
            announced = true;
            toast::show(g_self, L"Cheat commands on", L"slomo, summon and other cheat commands can now be typed in the console.", true, 1);
        }
    }
}

DWORD WINAPI worker(LPVOID) {
    try { openLog(); run(); }
    catch (...) { say("The console DLL stopped on an unexpected error."); }
    if (g_log) { fclose(g_log); g_log = nullptr; }
    return 0;
}

} // namespace

extern "C" BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
