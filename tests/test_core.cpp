// Platform-independent checks: pattern parser/scanner and the byte emitter. Build: see CMake option DK_BUILD_TESTS.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../src/core/Asm.h"
#include "../src/core/Pattern.h"
#include "../src/data/IdTables.h"
using namespace dk;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

int main() {
    // parser
    CHECK(parsePattern("29 BB FC 01 00 00").size() == 6);
    CHECK(!parsePattern("29 B").valid);
    CHECK(!parsePattern("29 ZZ").valid);
    CHECK(!parsePattern("").valid);
    auto w = parsePattern("4* *7 ** ? 8B");
    CHECK(w.valid && w.size() == 5);
    CHECK(w.mask[0] == 0xF0 && w.val[0] == 0x40);
    CHECK(w.mask[1] == 0x0F && w.val[1] == 0x07);
    CHECK(w.mask[2] == 0 && w.mask[3] == 0 && w.mask[4] == 0xFF);

    // scanner
    uint8_t mem[64]; std::memset(mem, 0xCC, sizeof mem);
    const uint8_t arrows[] = {0x29, 0xBB, 0xFC, 0x01, 0x00, 0x00};
    std::memcpy(mem + 10, arrows, 6);
    auto hits = findAll(parsePattern("29 BB FC 01 00 00"), mem, sizeof mem);
    CHECK(hits.size() == 1 && hits[0] == 10);

    // wildcard + nibble, and pattern at very start/end of buffer
    const uint8_t lives[] = {0x29, 0x91, 0x12, 0x34, 0x00, 0x00, 1,2,3,4,5,6,7, 0x33, 0xD2, 0xE8};
    std::memcpy(mem, lives, sizeof lives);
    hits = findAll(parsePattern("29 91 ** ** 00 00 ** ** ** ** ** ** ** 33 D2 E*"), mem, sizeof mem);
    CHECK(hits.size() == 1 && hits[0] == 0);
    mem[63] = 0x42; mem[62] = 0x41;
    hits = findAll(parsePattern("41 4*"), mem, sizeof mem);
    CHECK(hits.size() == 1 && hits[0] == 62);
    CHECK(findAll(parsePattern("DE AD"), mem, sizeof mem).empty());
    CHECK(findAll(parsePattern("** **"), mem, 3).size() == 2);   // all-wildcard path
    CHECK(findAll(parsePattern("01 02 03"), mem, 2).empty());    // buffer smaller than pattern

    // Asm: jmp, rip-relative, short jump
    Asm a(0x1000);
    a.jmp(0x2000);
    CHECK(a.bytes().size() == 5 && a.bytes()[0] == 0xE9);
    int32_t d; std::memcpy(&d, &a.bytes()[1], 4);
    CHECK(d == 0x2000 - (0x1000 + 5));

    Asm r(0x5040);
    r.ripOp({0x48, 0x83, 0x3D}, 0x5000, {0x00});                // cmp qword [rip+d],0  (8 bytes)
    CHECK(r.bytes().size() == 8);
    std::memcpy(&d, &r.bytes()[3], 4);
    CHECK(d == 0x5000 - (0x5040 + 8));                           // relative to end of instruction

    Asm s(0);
    size_t j = s.jccShort(0x75);
    s.b({0x90, 0x90, 0x90});
    s.bind(j);
    CHECK(s.bytes()[1] == 3);

    // The No Cooldown cave must have a jne that lands on the "original" branch
    Asm nc(0x10000);
    nc.b({0x41, 0x83, 0x79, 0x08, 0x00});
    size_t jne = nc.jccShort(0x75);
    nc.b({0x41, 0xC7, 0x41, 0x08, 0, 0, 0, 0});
    nc.b({0x85, 0xC9}).jmp(0x20000);
    nc.bind(jne);
    CHECK(nc.bytes()[6] == 0x0F);   // 8 + 2 + 5

    try { Asm f(0); f.jmp(0x100000000ull); CHECK(false); } catch (const std::exception&) {}

    // ID tables
    auto l29 = listsForVersion(29);
    CHECK(l29.items.count > 250 && l29.enchants.count > 100 && l29.armor.count > 50);
    CHECK(listsForVersion(1).items.count < l29.items.count);          // older build has a shorter list
    CHECK(listsForVersion(9999).items.count == l29.items.count);      // unknown -> newest
    bool sword = false;
    for (size_t i = 0; i < l29.items.count; ++i) if (l29.items.items[i].id == 3089 && std::strcmp(l29.items.items[i].name, "Sword") == 0) sword = true;
    CHECK(sword);
    CHECK(rarityList().count == 24 && tierList().count == 4);

    std::printf(fails ? "%d FAILED\n" : "all core tests passed\n", fails);
    return fails != 0;
}
