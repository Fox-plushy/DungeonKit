#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "GameVersions.h"
#include "Pattern.h"

namespace dk {

class Process {
public:
    Process() = default;
    ~Process() { detach(); }
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // Finds Dungeons-Win64-Shipping.exe (preferred) or Dungeons.exe and opens it. For Dungeons2, also accepts the Microsoft
    // Store/Xbox build's Dungeons-WinGDK-Shipping.exe (preferred when present) or the plain Steam names, since Steam's
    // Minecraft Dungeons II build happens to use the same Dungeons-Win64-Shipping.exe/Dungeons.exe names as the original game.
    enum class Target { Dungeons, Dungeons2, Dungeons2Launcher, None };   // Launcher = Dungeons.exe next to the Dungeons 2 game
    bool attach(std::string& err, Target t = Target::Dungeons);
    Target target() const { return target_; }
    void detach();
    // Ends every Minecraft Dungeons / Minecraft Dungeons II game process that is still running (Dungeons.exe,
    // Dungeons-Win64-Shipping.exe, Dungeons-WinGDK-Shipping.exe). Returns how many were ended; `failed` counts the ones
    // that could not be ended (usually access denied).
    static int terminateGameProcesses(int& failed);
    bool attached() const { return h_ != nullptr; }
    unsigned long exitCode() const;           // exit code once the game has exited (259 while running)
    bool alive() const;                       // false once the game has exited

    DWORD pid() const { return pid_; }
    uint64_t base() const { return base_; }
    uint64_t moduleSize() const { return size_; }
    const std::string& exeName() const { return exe_; }
    int dungeonsProcessCount() const { return procCount_; }
    std::wstring exePath() const;                              // full path of the game exe ("" if unknown)
    uint64_t moduleBase(const wchar_t* name) const;          // base address of any loaded module (0 if not found)
    // The game's biggest visible top-level window (its actual play area, whether windowed, borderless or maximized).
    // Returns false if the game isn't running or has no visible window right now (e.g. minimized).
    bool mainWindowRect(long& x, long& y, long& w, long& h) const;
    std::optional<GameVersion> version() const { return identifyVersion(size_, procCount_); }

    bool read(uint64_t addr, void* buf, size_t n) const;
    bool write(uint64_t addr, const void* buf, size_t n) const;
    bool writeCode(uint64_t addr, const void* buf, size_t n) const;   // handles page protection + icache flush

    template <class T> bool rd(uint64_t a, T& v) const { return read(a, &v, sizeof(T)); }
    template <class T> bool wr(uint64_t a, const T& v) const { return write(a, &v, sizeof(T)); }

    // Scan executable pages of the main module. Returns up to `limit` absolute addresses.
    std::vector<uint64_t> scan(const Pattern& pat, size_t limit = 4) const;

    // Allocate RWX memory within +-2GB of `target` so a 5-byte rel32 jmp can reach it.
    uint64_t allocNear(uint64_t target, size_t size) const;
    void freeMem(uint64_t addr) const;

    // Suspends every thread of the game for the lifetime of the object (used while patching code).
    class Freeze {
    public:
        explicit Freeze(const Process& p);
        ~Freeze();
    private:
        std::vector<HANDLE> threads_;
    };

private:
    HANDLE h_ = nullptr;
    DWORD pid_ = 0;
    uint64_t base_ = 0, size_ = 0;
    std::string exe_;
    int procCount_ = 0;
    Target target_ = Target::Dungeons;
};

} // namespace dk
