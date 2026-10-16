#pragma once
// Platform-independent byte pattern with wildcards (Cheat Engine AOB syntax).
//   "29 BB FC 01 00 00"   exact bytes
//   "**" / "??" / "*"     any byte
//   "4*" / "*7"           nibble wildcards (high / low nibble must match)
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dk {

struct Pattern {
    std::vector<uint8_t> val;
    std::vector<uint8_t> mask;   // 0xFF exact, 0xF0 / 0x0F nibble, 0x00 wildcard
    bool valid = false;

    size_t size() const { return val.size(); }
    bool matchAt(const uint8_t* p) const {
        for (size_t i = 0; i < val.size(); ++i)
            if ((p[i] & mask[i]) != val[i]) return false;
        return true;
    }
};

Pattern parsePattern(const std::string& text);

// Offsets of matches inside [data, data+len). Stops after `limit` hits.
std::vector<size_t> findAll(const Pattern& pat, const uint8_t* data, size_t len, size_t limit = 4);

} // namespace dk
