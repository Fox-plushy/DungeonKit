#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Asm.h"
#include "Process.h"

namespace dk {

// Layout of every cave: 0x00-0x3F data slots (zeroed), 0x40+ code.
struct HookCtx {
    uint64_t cave = 0;     // start of cave (data slot 0)
    uint64_t addr = 0;     // hooked address
    uint64_t ret  = 0;     // addr + overwritten length (where code should jump back to)
    std::vector<uint8_t> orig;   // the bytes that were overwritten
    uint64_t slot(size_t off) const { return cave + off; }
};

class Hook {
public:
    using Emit = std::function<void(Asm&, const HookCtx&)>;

    // Overwrite `len` (>=5) bytes at addr with `jmp cave`; the cave code is produced by `emit`
    // and is responsible for ending with ctx.ret (a.jmp(ctx.ret)) or its own `ret`.
    bool install(const Process& p, uint64_t addr, size_t len, const Emit& emit, std::string& err);
    void remove(const Process& p);
    bool active() const { return active_; }
    const HookCtx& ctx() const { return ctx_; }

    // A short, honest note on how this hook actually reaches into the game: not the exact instructions it
    // injected, but the real mechanism (a detour into an allocated code cave) with the real address this run
    // found. Used to show people roughly how a mod was wired in, without pretending to be a full disassembly.
    std::string describe() const;

private:
    bool active_ = false;
    HookCtx ctx_;
};

// Plain byte patch (e.g. NOP out an instruction) with restore.
class Patch {
public:
    bool apply(const Process& p, uint64_t addr, const std::vector<uint8_t>& bytes, std::string& err);
    void remove(const Process& p);
    bool active() const { return active_; }
    std::string describe() const;    // see Hook::describe() above
private:
    bool active_ = false;
    uint64_t addr_ = 0;
    std::vector<uint8_t> orig_;
    std::vector<uint8_t> bytes_;
};

} // namespace dk
