#include "Hook.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace dk {

static std::string hexBytes(const std::vector<uint8_t>& b) {
    std::string s;
    char h[4];
    for (size_t i = 0; i < b.size() && i < 16; ++i) {
        std::snprintf(h, sizeof h, "%02X ", b[i]);
        s += h;
    }
    if (b.size() > 16) s += "...";
    if (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

bool Hook::install(const Process& p, uint64_t addr, size_t len, const Emit& emit, std::string& err) {
    if (active_) return true;
    if (len < 5) { err = "hook length < 5"; return false; }

    std::vector<uint8_t> orig(len);
    if (!p.read(addr, orig.data(), len)) { err = "could not read target code"; return false; }

    const size_t kCaveSize = 0x1000;
    uint64_t cave = p.allocNear(addr, kCaveSize);
    if (!cave) { err = "could not allocate memory near the game code"; return false; }

    HookCtx ctx;
    ctx.cave = cave; ctx.addr = addr; ctx.ret = addr + len; ctx.orig = orig;

    std::vector<uint8_t> image(kCaveSize, 0);
    try {
        Asm a(cave + 0x40);
        emit(a, ctx);
        if (a.bytes().size() > kCaveSize - 0x40) throw std::runtime_error("cave code too large");
        std::copy(a.bytes().begin(), a.bytes().end(), image.begin() + 0x40);

        Asm j(addr);
        j.jmp(cave + 0x40);
        std::vector<uint8_t> patch = j.bytes();
        patch.resize(len, 0x90);   // pad with NOPs

        if (!p.write(cave, image.data(), image.size())) throw std::runtime_error("could not write cave");
        {
            Process::Freeze freeze(p);   // no thread is mid-instruction while we swap bytes
            if (!p.writeCode(addr, patch.data(), patch.size())) throw std::runtime_error("could not write hook");
        }
    } catch (const std::exception& e) {
        p.freeMem(cave);
        err = e.what();
        return false;
    }
    ctx_ = ctx;
    active_ = true;
    return true;
}

std::string Hook::describe() const {
    if (!active_) return "Not installed.";
    char buf[512];
    std::snprintf(buf, sizeof buf,
        "Code hook: the %zu original byte(s) at 0x%llX (%s) were overwritten with a jump into a small block of "
        "memory DungeonKit allocated nearby (0x%llX). That block runs a few extra instructions of its own - usually "
        "saving a pointer or value the game was about to use into a reserved slot DungeonKit can read - then runs "
        "the original bytes it replaced before jumping back to 0x%llX to resume the game's own code exactly where "
        "it left off. Turning this off restores those original bytes and frees the memory.",
        ctx_.orig.size(), (unsigned long long)ctx_.addr, hexBytes(ctx_.orig).c_str(),
        (unsigned long long)ctx_.cave, (unsigned long long)ctx_.ret);
    return buf;
}

void Hook::remove(const Process& p) {
    if (!active_) return;
    if (p.alive()) {
        {
            Process::Freeze freeze(p);
            p.writeCode(ctx_.addr, ctx_.orig.data(), ctx_.orig.size());
        }
        Sleep(100);               // let any thread that is already inside the cave leave it
        p.freeMem(ctx_.cave);
    }
    active_ = false;
}

bool Patch::apply(const Process& p, uint64_t addr, const std::vector<uint8_t>& bytes, std::string& err) {
    if (active_) return true;
    orig_.assign(bytes.size(), 0);
    if (!p.read(addr, orig_.data(), orig_.size())) { err = "could not read target code"; return false; }
    Process::Freeze freeze(p);
    if (!p.writeCode(addr, bytes.data(), bytes.size())) { err = "could not write patch"; return false; }
    addr_ = addr;
    bytes_ = bytes;
    active_ = true;
    return true;
}

void Patch::remove(const Process& p) {
    if (!active_) return;
    if (p.alive()) {
        Process::Freeze freeze(p);
        p.writeCode(addr_, orig_.data(), orig_.size());
    }
    active_ = false;
}

std::string Patch::describe() const {
    if (!active_) return "Not applied.";
    char buf[384];
    std::snprintf(buf, sizeof buf,
        "Byte patch: %zu byte(s) at 0x%llX were overwritten with %s so that code never runs as written. "
        "The original bytes (%s) are saved and written back exactly when this is turned off.",
        bytes_.size(), (unsigned long long)addr_, hexBytes(bytes_).c_str(), hexBytes(orig_).c_str());
    return buf;
}

} // namespace dk
