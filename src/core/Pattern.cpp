#include "Pattern.h"
#include <cctype>
#include <cstring>
#include <sstream>

namespace dk {

static int nib(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)std::toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;   // wildcard ('*' or '?') or invalid
}

Pattern parsePattern(const std::string& text) {
    Pattern p;
    std::istringstream ss(text);
    std::string tok;
    while (ss >> tok) {
        uint8_t v = 0, m = 0;
        if (tok.size() == 1) {
            if (tok[0] != '*' && tok[0] != '?') return Pattern{};   // single hex digit is invalid
        } else if (tok.size() == 2) {
            bool bad = false;
            for (int i = 0; i < 2; ++i) {
                char c = tok[i];
                int n = nib(c);
                if (n >= 0) { v |= (uint8_t)(n << (i == 0 ? 4 : 0)); m |= (uint8_t)(0xF << (i == 0 ? 4 : 0)); }
                else if (c != '*' && c != '?') bad = true;
            }
            if (bad) return Pattern{};
        } else {
            return Pattern{};
        }
        p.val.push_back(v);
        p.mask.push_back(m);
    }
    p.valid = !p.val.empty();
    return p;
}

std::vector<size_t> findAll(const Pattern& pat, const uint8_t* data, size_t len, size_t limit) {
    std::vector<size_t> out;
    const size_t n = pat.size();
    if (!pat.valid || len < n) return out;

    // Use the first fully-known byte as a memchr anchor.
    size_t anchor = n;
    for (size_t i = 0; i < n; ++i) if (pat.mask[i] == 0xFF) { anchor = i; break; }

    if (anchor == n) {   // no exact byte at all: brute force
        for (size_t i = 0; i + n <= len && out.size() < limit; ++i)
            if (pat.matchAt(data + i)) out.push_back(i);
        return out;
    }

    const size_t last = len - n;                       // last valid start
    const uint8_t* cur = data + anchor;
    const uint8_t* end = data + last + anchor + 1;
    while (cur < end && out.size() < limit) {
        const uint8_t* hit = (const uint8_t*)std::memchr(cur, pat.val[anchor], (size_t)(end - cur));
        if (!hit) break;
        size_t start = (size_t)(hit - data) - anchor;
        if (pat.matchAt(data + start)) out.push_back(start);
        cur = hit + 1;
    }
    return out;
}

} // namespace dk
