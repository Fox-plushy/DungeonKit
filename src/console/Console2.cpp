// DungeonKit console DLL for Minecraft Dungeons II. Loaded by Minecraft Dungeons II > Tools > Console.
// Shipping builds never create the Unreal console, so the console key does nothing. This DLL makes one Console object and puts it in the game
// viewport's ViewportConsole slot (what the engine does in non-shipping builds), and then makes a cheat manager for each player controller so commands
// like summon work. It installs no hooks and patches no code.
// The object is made with the game's own GameplayStatics.SpawnObject function. Its address is found from the game's own function table and checked
// against the code bytes of a build this DLL has confirmed (currently Microsoft Store/Xbox 1.1.2.0 - Steam's 1.2.0 build happens to compile to the
// identical bytes at this function, so it's covered too); on a build whose bytes don't match, it does nothing rather than guess.
// It also fixes up freshly summoned mobs: the stock "summon" command skips the game's own spawn-in logic, which normally makes the mob
// visible. This flips that flag back on by name through the game's reflection data (see fixSummonedMob below) - still just the console key's
// summon command plus reflection writes, nothing new installed into the game.
// Inspired by the Universal UE4 Unlocker (UUU) by Otis_Inf.
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
#include <unordered_map>
#include <vector>
#include "../core/Process.h"
#include "../core/UE4.h"
#include "Toast.h"

using namespace dk;

namespace {

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
    g_log = _wfopen((dir + L"\\DungeonKitConsole2.log").c_str(), L"ab");
    if (!g_log) {
        wchar_t tmp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tmp);
        g_log = _wfopen((std::wstring(tmp) + L"DungeonKitConsole2.log").c_str(), L"ab");
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


// ---- Console key. DungeonKit writes DungeonKitConsole2.ini (key=F2) next to this DLL. The game's console key list (InputSettings.ConsoleKeys)
// is a plain array of keys, each 0x18 bytes: an FName (index + number), then a pointer the engine fills in again by itself.
struct KeyList { uint64_t arrayAddr = 0; std::vector<uint32_t> orig; };

std::string readKeySetting() {
    char buf[64] = {};
    const std::wstring ini = g_dir + L"\\DungeonKitConsole2.ini";
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

bool isExecutable(uint64_t addr) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery((LPCVOID)(uintptr_t)addr, &mbi, sizeof mbi)) return false;
    return (mbi.State == MEM_COMMIT) && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}

LONG CALLBACK onException(PEXCEPTION_POINTERS e) {          // only installed around the one call, so a crash leaves a note in the log
    const DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code == 0xC0000005 || code == 0xC0000094 || code == 0xC000001D || code == 0xC00000FD)
        say("The game raised exception 0x%lX at %s while an object was being created.", (unsigned long)code, hex((uint64_t)(uintptr_t)e->ExceptionRecord->ExceptionAddress).c_str());
    return EXCEPTION_CONTINUE_SEARCH;
}

// SpawnObject in this build: UObject* UGameplayStatics::SpawnObject(TSubclassOf<UObject>& ObjectClass, UObject* Outer). Found from the Blueprint function's native
// entry point (the call whose result is stored with "mov [rsi],rax"), then the start of the function is compared with the bytes seen in the log of this build.
// Builds known so far are listed in kKnownPrologues; a build not in that list is refused rather than guessed at, since calling a native function
// pointer this DLL hasn't confirmed the shape of could crash the game. If every candidate call site is logged as "not a known build" below,
// that log (DungeonKitConsole2.log) has exactly the bytes needed to add the new build here - send it over.
std::string hexBytes(const unsigned char* b, size_t n) {
    std::string s;
    char tmp[4];
    for (size_t i = 0; i < n; ++i) { snprintf(tmp, sizeof tmp, "%02X ", b[i]); s += tmp; }
    return s;
}

bool findSpawnObject(UE4& ue, Process& proc, const UE4::ObjArray& arr, uint64_t& fn, std::string& why) {
    const uint64_t base = proc.base(), end = base + proc.moduleSize();
    uint64_t fnObj = 0;
    const int limit = arr.num < 3000000 ? arr.num : 3000000;
    for (int i = 0; i < limit && !fnObj; ++i) {
        const uint64_t o = ue.objectAt(arr, i);
        if (!UE4::plausible(o) || ue.objectName(o) != "SpawnObject" || ue.className(o) != "Function") continue;
        const uint64_t outer = ue.rd64(o + 0x20);
        if (UE4::plausible(outer) && ue.objectName(outer) == "GameplayStatics") fnObj = o;
    }
    if (!fnObj) { why = "the SpawnObject function was not found"; return false; }
    uint64_t thunk = 0;
    for (uint64_t off = 0x30; off < 0x180 && !thunk; off += 8) {
        const uint64_t q = ue.rd64(fnObj + off);
        if (q >= base && q < end && isExecutable(q)) thunk = q;
    }
    if (!thunk) { why = "the SpawnObject entry point was not found"; return false; }
    const size_t n = 0x140;
    std::string buf(n, '\0');
    if (!proc.read(thunk, &buf[0], n)) { why = "could not read the SpawnObject entry point"; return false; }
    // Known SpawnObject prologues, one per build this DLL has been confirmed against.
    static const unsigned char kGdk1120[15] =  {0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x48, 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00};
    static const unsigned char* const kKnownPrologues[] = {kGdk1120};
    static const char* const kKnownNames[] = {"Microsoft Store/Xbox 1.1.2.0"};
    bool sawAnyCandidate = false;
    std::string candidateLog;
    for (size_t i = 0; i + 8 <= n; ++i) {
        if ((unsigned char)buf[i] != 0xE8 || (unsigned char)buf[i + 5] != 0x48 || (unsigned char)buf[i + 6] != 0x89 || (unsigned char)buf[i + 7] != 0x06) continue;
        int32_t rel = 0;
        memcpy(&rel, &buf[i + 1], 4);
        const uint64_t t = thunk + i + 5 + (int64_t)rel;
        if (t < base || t >= end || !isExecutable(t)) continue;
        unsigned char head[15] = {};
        if (!proc.read(t, head, sizeof head)) continue;
        sawAnyCandidate = true;
        bool matched = false;
        for (size_t k = 0; k < sizeof(kKnownPrologues) / sizeof(kKnownPrologues[0]); ++k) {
            if (memcmp(head, kKnownPrologues[k], sizeof head) == 0) { fn = t; say("SpawnObject matches the known %s build.", kKnownNames[k]); return true; }
        }
        candidateLog += "  candidate at module+" + hex(t - base) + ": " + hexBytes(head, sizeof head) + "\n";
    }
    if (!sawAnyCandidate) { why = "no call to SpawnObject was found in its entry point, so this is a different game build"; return false; }
    say("SpawnObject's entry point did not match any known build. Candidate bytes seen:\n%s", candidateLog.c_str());
    why = "the start of SpawnObject is not a build this DLL knows yet - this log now has the bytes needed to add it, please send it over";
    return false;
}

typedef void* (*SpawnObjectFn)(void** objectClass, void* outer);

uint64_t construct(uint64_t fn, uint64_t cls, uint64_t outer) {
    void* c = (void*)(uintptr_t)cls;
    void* handler = AddVectoredExceptionHandler(1, onException);
    void* made = reinterpret_cast<SpawnObjectFn>(fn)(&c, (void*)(uintptr_t)outer);
    if (handler) RemoveVectoredExceptionHandler(handler);
    return (uint64_t)(uintptr_t)made;
}

// ---- Fix for summoned mobs. The stock "summon" console command only calls the engine's generic SpawnActor, which skips
// Minecraft Dungeons II's own mob spawn-in logic. Two things are missing as a result: MobCharacter.mPlayerVisible stays
// false (so the mob never renders), and its AbilitySystemComponent (Unreal's Gameplay Ability System, which this game
// uses for health and combat abilities) never has its attribute sets granted (which is almost certainly why it can't be
// killed and never attacks). The visibility part is fixed below the same safe way the rest of DungeonKit edits values,
// by name through the game's own reflection data. The attribute sets are not: the real game code picks which attribute
// set to grant from the mob's own type/level data, and nothing in the reflection table says which one that should be, so
// guessing and writing one in blind risks corrupting memory. Instead this logs how many attribute sets a freshly
// summoned mob ends up with, which confirms whether that is really the cause and gives a real log to work from next.
//
// This has to look at every loaded object, every pass, rather than only newly created ones: Unreal reuses object-table
// slots that older, destroyed objects (projectiles, pickups, effects) free up, so a freshly summoned mob can easily land
// at an index this DLL already looked at before. To keep a full pass cheap, which class a given object belongs to is
// looked up with the game's reflection data only once per class (not once per object) and cached by class pointer.
struct MobVisProp { bool has = false; uint32_t absOffset = 0; uint8_t mask = 0; bool bitmask = false; };
std::unordered_map<uint64_t, MobVisProp> g_mobVisByClass;

bool writeBoolProp(UE4& ue, Process& proc, uint64_t obj, const char* prop, bool v) {
    UE4::Prop pr;
    if (!UE4::plausible(obj) || !ue.find(obj, prop, pr)) return false;
    const uint64_t a = obj + pr.offset + (pr.isBool ? pr.byteOffset : 0);
    uint8_t b = 0;
    if (!ue.rd(a, b)) return false;
    if (pr.isBool && pr.mask) b = v ? (b | pr.mask) : (b & ~pr.mask); else b = v ? 1 : 0;
    return proc.write(a, &b, 1);
}

void fixSummonedMob(UE4& ue, Process& proc, uint64_t obj) {
    const uint64_t cls = ue.rd64(obj + 0x10);
    if (!UE4::plausible(cls)) return;
    auto it = g_mobVisByClass.find(cls);
    if (it == g_mobVisByClass.end()) {
        MobVisProp mv;
        UE4::Prop pr;
        if (ue.find(obj, "mPlayerVisible", pr)) {
            mv.has = true;
            mv.absOffset = pr.offset + (pr.isBool ? pr.byteOffset : 0);
            mv.mask = pr.mask;
            mv.bitmask = pr.isBool && pr.mask;
        }
        it = g_mobVisByClass.emplace(cls, mv).first;
    }
    if (!it->second.has) return;                                        // this class has no mPlayerVisible property - not a mob, nothing to do
    uint8_t b = 0;
    if (!ue.rd(obj + it->second.absOffset, b)) return;
    if (it->second.bitmask ? (b & it->second.mask) != 0 : b != 0) return;   // already visible - nothing left to fix here
    const uint8_t nb = it->second.bitmask ? (uint8_t)(b | it->second.mask) : (uint8_t)1;
    if (!proc.write(obj + it->second.absOffset, &nb, 1)) return;
    const std::string clsName = ue.className(obj);
    writeBoolProp(ue, proc, obj, "mPlayerDistanceCheck", false);         // stop it from hiding itself again based on distance to the player
    int32_t sets = -1;
    uint64_t asc = 0;
    if (ue.readPtr(obj, "AbilitySystemComponent", asc) && UE4::plausible(asc)) {
        uint64_t data = 0;
        int32_t num = 0;
        if (ue.readArray(asc, "SpawnedAttributes", data, num)) sets = num;
    }
    const char* note = sets == 0 ? "likely why it can't be killed or attack - the game never granted it any, and there's no safe way to add that from outside"
                      : sets > 0  ? "it may actually behave correctly in combat"
                                  : "its ability system component could not be read";
    say("Made a freshly summoned %s visible (object %s). Attribute sets on its ability system: %d (%s).", clsName.c_str(), hex(obj).c_str(), (int)sets, note);
}

uint64_t findClass(UE4& ue, const UE4::ObjArray& arr, const char* name) {
    const int limit = arr.num < 3000000 ? arr.num : 3000000;
    for (int i = 0; i < limit; ++i) {
        const uint64_t o = ue.objectAt(arr, i);
        if (UE4::plausible(o) && ue.className(o) == "Class" && ue.objectName(o) == name) return o;
    }
    return 0;
}

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

void notifyFail() {
    toast::show(g_self, L"DungeonKit console could not start", L"Open the Console card in DungeonKit and press Refresh log to see why.", false, 0);
}

void run() {
    say("---- DungeonKit console DLL for Minecraft Dungeons II loaded. ----");
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    char exeA[MAX_PATH] = {};
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, exeA, MAX_PATH - 1, nullptr, nullptr);
    say("Process: %s (pid %lu)", exeA, (unsigned long)GetCurrentProcessId());
    std::string lower = exeA;
    for (char& c : lower) c = (char)tolower((unsigned char)c);
    // Microsoft Store/Xbox uses Dungeons-WinGDK-Shipping.exe; Steam uses the plain Dungeons-Win64-Shipping.exe that the
    // original Minecraft Dungeons also uses. DungeonKit only ever injects this DLL into whichever process it already
    // confirmed is running Minecraft Dungeons II's engine data (see MainWindow::engineProc), so seeing the plain name
    // here still means Minecraft Dungeons II, not the original game.
    if (lower.find("wingdk") == std::string::npos && lower.find("win64-shipping") == std::string::npos) {
        say("This is not Minecraft Dungeons II. Use the Minecraft Dungeons console instead. Nothing was changed.");
        notifyFail();
        return;
    }

    Process proc;
    UE4 ue;
    std::string err, lastErr;
    bool ready = false;
    for (int attempt = 0; attempt < 30 && !ready; ++attempt) {
        if (!proc.attach(err, Process::Target::Dungeons2)) { if (err != lastErr) say("Attach: %s", err.c_str()); lastErr = err; Sleep(2000); continue; }
        if (ue.init(proc, err)) { ready = true; break; }
        if (err != lastErr) say("Engine not ready yet: %s", err.c_str());
        lastErr = err;
        Sleep(2000);
    }
    if (!ready) { say("Gave up: the engine could not be found. %s", lastErr.c_str()); notifyFail(); return; }
    say("Engine found. Module base %s.", hex(proc.base()).c_str());

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
        const int lim = arr.num < 3000000 ? arr.num : 3000000;
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
    std::string why;
    if (!findSpawnObject(ue, proc, arr, fn, why)) { say("Not creating anything: %s. Nothing was changed. Please send this log to Plushy.", why.c_str()); notifyFail(); return; }
    say("SpawnObject found at module+%s.", hex(fn - proc.base()).c_str());

    if (UE4::plausible(existing)) {
        say("A console already exists (%s). Press the console key in the game.", hex(existing).c_str());
        { const std::wstring m = L"The console is already on. Press " + keyLabel(appliedKey) + L" in the game to open it."; toast::show(g_self, L"DungeonKit is connected", m.c_str(), true, 0); }
    } else if (!consoleClass) {
        say("The Console class was not found. No console was made.");
        notifyFail();
    } else {
        say("Creating the console now...");
        const uint64_t obj = construct(fn, consoleClass, vp);
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

    // Cheat manager: shipping builds never make one, so cheat commands have nowhere to go. A new PlayerController is made on every level load,
    // so this keeps watching and makes one for each.
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
        {   // newly summoned mobs: a full pass over every loaded object, since Unreal reuses freed object-table slots
            // (from destroyed projectiles, pickups, effects and so on), so a summoned mob is not reliably at a new,
            // never-seen-before index. fixSummonedMob keeps this cheap and skips anything already fixed or not a mob.
            UE4::ObjArray arr2;
            std::string aerr;
            if (ue.objectArray(arr2, aerr)) {
                const int32_t lim = arr2.num < 3000000 ? arr2.num : 3000000;
                for (int32_t i = 0; i < lim; ++i) {
                    const uint64_t o = ue.objectAt(arr2, i);
                    if (UE4::plausible(o)) fixSummonedMob(ue, proc, o);
                }
            }
        }
        const uint64_t pc = localController(ue);
        if (!pc) { lastPc = 0; continue; }
        if (pc != lastPc) { lastPc = pc; continue; }          // wait until the same controller is seen twice
        if (pc == failedPc) continue;
        UE4::Prop cm;
        if (!ue.find(pc, "CheatManager", cm)) { say("The CheatManager slot was not found on the player controller."); failedPc = pc; continue; }
        if (UE4::plausible(ue.rd64(pc + cm.offset))) continue;
        UE4::Prop cam;
        uint64_t camMgr = 0;
        if (ue.find(pc, "PlayerCameraManager", cam)) camMgr = ue.rd64(pc + cam.offset);
        if (!UE4::plausible(camMgr)) continue;
        uint64_t cls = 0;
        if (ue.readPtr(pc, "CheatClass", cls) && UE4::plausible(cls) && ue.objectName(cls).find("CheatManager") == std::string::npos) cls = 0;
        if (!UE4::plausible(cls)) cls = cheatClassScanned;
        if (!cls) { say("The CheatManager class was not found."); failedPc = pc; continue; }
        say("New player controller %s (%s). Creating a cheat manager...", hex(pc).c_str(), ue.className(pc).c_str());
        const uint64_t obj = construct(fn, cls, pc);
        if (!UE4::plausible(obj) || ue.className(obj).find("CheatManager") == std::string::npos) {
            say("The game did not return a cheat manager. Will not retry for this controller.");
            failedPc = pc;
            continue;
        }
        if (!proc.write(pc + cm.offset, &obj, sizeof obj)) { say("Could not store the cheat manager."); failedPc = pc; continue; }
        say("Cheat manager %s (%s) is in place.", hex(obj).c_str(), ue.className(obj).c_str());
        if (!announced) {
            announced = true;
            toast::show(g_self, L"Cheat commands on", L"Cheat commands can now be typed in the console.", true, 1);
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
