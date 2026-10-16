#pragma once
// Tiny x64 byte emitter for code caves (no real assembler; every instruction is spelled out as bytes).
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace dk {

class Asm {
public:
    explicit Asm(uint64_t base) : base_(base) {}

    uint64_t here() const { return base_ + buf_.size(); }
    const std::vector<uint8_t>& bytes() const { return buf_; }

    Asm& b(std::initializer_list<uint8_t> v) { buf_.insert(buf_.end(), v.begin(), v.end()); return *this; }
    Asm& raw(const std::vector<uint8_t>& v)  { buf_.insert(buf_.end(), v.begin(), v.end()); return *this; }
    Asm& u32(uint32_t v) { for (int i = 0; i < 4; ++i) buf_.push_back((uint8_t)(v >> (8 * i))); return *this; }

    // E9 rel32
    Asm& jmp(uint64_t target) {
        int64_t d = (int64_t)target - (int64_t)(here() + 5);
        if (d > INT32_MAX || d < INT32_MIN) throw std::runtime_error("jmp out of rel32 range");
        buf_.push_back(0xE9);
        return u32((uint32_t)(int32_t)d);
    }

    // opcode bytes + disp32 (rip-relative, pointing at `target`) + trailing immediate bytes
    Asm& ripOp(std::initializer_list<uint8_t> opcode, uint64_t target, std::initializer_list<uint8_t> trailing = {}) {
        buf_.insert(buf_.end(), opcode.begin(), opcode.end());
        int64_t next = (int64_t)here() + 4 + (int64_t)trailing.size();
        int64_t d = (int64_t)target - next;
        if (d > INT32_MAX || d < INT32_MIN) throw std::runtime_error("rip-relative out of range");
        u32((uint32_t)(int32_t)d);
        buf_.insert(buf_.end(), trailing.begin(), trailing.end());
        return *this;
    }

    // Short conditional jump (opcode 0x70-0x7F or 0xEB) with a label patched later.
    size_t jccShort(uint8_t op) { buf_.push_back(op); buf_.push_back(0); return buf_.size() - 1; }
    void bind(size_t pos) {
        size_t d = buf_.size() - (pos + 1);
        if (d > 127) throw std::runtime_error("short jump too far");
        buf_[pos] = (uint8_t)d;
    }

private:
    uint64_t base_;
    std::vector<uint8_t> buf_;
};

} // namespace dk
