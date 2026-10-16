#include "Mods.h"
#include "ItemViewer.h"
#include "PlayerTools.h"
#include <cstring>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>

namespace dk {

// ============================================================ base
bool Mod::enable(const Process& p, std::string& err) {
    if (on_) return true;
    if (!p.attached() || !p.alive()) { err = "Game is not running."; return false; }
    if (!onEnable(p, err)) { onDisable(p); return false; }   // undo anything half-applied
    on_ = true;
    return true;
}

void Mod::disable(const Process& p) {
    if (!on_) return;
    onDisable(p);
    on_ = false;
}

bool Mod::findOne(const Process& p, const char* pattern, uint64_t& addr, std::string& err) {
    Pattern pat = parsePattern(pattern);
    if (!pat.valid) { err = std::string("bad pattern: ") + pattern; return false; }
    auto hits = p.scan(pat, 1);
    if (hits.empty()) { err = "Pattern not found in this game version."; return false; }
    addr = hits[0];
    return true;
}

// ============================================================ Infinite arrows / lives
bool NopMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    if (!findOne(p, pat_, a, err)) return false;
    return patch_.apply(p, a, std::vector<uint8_t>(len_, 0x90), err);
}

// ============================================================ Infinite enchant points
bool InfiniteEnchantMod::onEnable(const Process& p, std::string& err) {
    static const int32_t kBig = -9999990;
    uint64_t a;

    // 1.17:  mov [rbx+29C],edi-ish store  ->  mov dword [rbx+29C], -9999990
    if (findOne(p, "89 83 9C 02 00 00 8B 83", a, err)) {
        hooks_.emplace_back();
        bool ok = hooks_.back().install(p, a, 6, [&](Asm& c, const HookCtx& x) {
            c.b({0xC7, 0x83, 0x9C, 0x02, 0x00, 0x00}).u32((uint32_t)kBig).jmp(x.ret);
        }, err);
        if (!ok) hooks_.pop_back();
        return ok;
    }

    // 1.12+ : three separate stores whose displacement differs per build, so it is read from the match.
    struct Sec { const char* pat; size_t len; uint8_t modrm; size_t dispAt; int32_t value; };
    static const Sec secs[] = {
        {"01 83 ** ** 00 00 48 8D 4D EF 8B 45 F3 F7 D0 21 45 FF", 6, 0x83, 2, -9999990},   // add [rbx+disp],eax
        {"** 89 ** ** ** 00 00 4* 8B ** ** ** ** 89",             7, 0x81, 3, -9999},      // mov [rcx+disp],reg
        {"01 83 ** ** 00 00 48 8* ** 08 4* FF C6",                6, 0x83, 2, -9999990},
    };
    std::string last;
    for (const auto& s : secs) {
        std::string e;
        if (!findOne(p, s.pat, a, e)) { last = e; continue; }
        hooks_.emplace_back();
        bool ok = hooks_.back().install(p, a, s.len, [&](Asm& c, const HookCtx& x) {
            c.b({0xC7, s.modrm});
            c.raw(std::vector<uint8_t>(x.orig.begin() + s.dispAt, x.orig.begin() + s.dispAt + 4));
            c.u32((uint32_t)s.value).jmp(x.ret);
        }, e);
        if (!ok) { hooks_.pop_back(); last = e; }
    }
    if (hooks_.empty()) { err = last.empty() ? "Pattern not found in this game version." : last; return false; }
    err.clear();
    return true;
}

std::string InfiniteEnchantMod::technique() const {
    if (hooks_.empty()) return {};
    std::string s = hooks_.size() == 1 ? "One hook was installed: " : (std::to_string(hooks_.size()) + " hooks were installed (one per enchantment-cost store this build uses): ");
    for (size_t i = 0; i < hooks_.size(); ++i) {
        if (i) s += " ";
        s += hooks_[i].describe();
    }
    s += " Each one replaces the cost it would have subtracted with a large negative number, so the points display never runs out.";
    return s;
}

// ============================================================ No cooldown
NoCooldownMod::NoCooldownMod(std::shared_ptr<UE4> ue) : fallback_(new SlotCooldownMod(std::move(ue))) {}
NoCooldownMod::~NoCooldownMod() = default;

std::string NoCooldownMod::note() const { return fallback_ && fallback_->enabled() ? fallback_->note() : std::string(); }

void NoCooldownMod::onDisable(const Process& p) {
    hook_.remove(p);
    if (fallback_) fallback_->disable(p);
}

bool NoCooldownMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    std::string perr;
    if (findOne(p, "45 89 79 08 85 C9", a, perr)) {
        return hook_.install(p, a, 6, [&](Asm& c, const HookCtx& x) {
            c.b({0x41, 0x83, 0x79, 0x08, 0x00});                          // cmp dword [r9+8],0
            size_t jne = c.jccShort(0x75);                                // jne original
            c.b({0x41, 0xC7, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00});        // mov dword [r9+8],0
            c.b({0x85, 0xC9}).jmp(x.ret);                                 // test ecx,ecx ; return
            c.bind(jne);
            c.b({0x45, 0x89, 0x79, 0x08, 0x85, 0xC9}).jmp(x.ret);         // original: mov [r9+8],r15d ; test ecx,ecx
        }, err);
    }
    // Code pattern not in this build (Steam): use the object tables instead.
    std::string ferr;
    if (fallback_ && fallback_->enable(p, ferr)) return true;
    err = ferr.empty() ? perr : ferr;
    return false;
}

std::string NoCooldownMod::technique() const {
    if (hook_.active()) return hook_.describe() + " It skips the one check the game makes before starting a cooldown, so artifact and potion cooldowns never begin.";
    if (fallback_ && fallback_->enabled()) return fallback_->technique();
    return {};
}

// ============================================================ pointer capture
uint64_t CaptureMod::object(const Process& p) const {
    if (!hook_.active()) return 0;
    uint64_t v = 0;
    return p.rd(hook_.ctx().slot(0), v) ? v : 0;
}

// ---- currency
bool CurrencyMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    if (!findOne(p, "F8 8B 41 08 C3 CC", a, err)) return false;
    // Hook the `mov eax,[rcx+8]; ret; int3` that follows the 0xF8 byte.
    return hook_.install(p, a + 1, 5, [&](Asm& c, const HookCtx& x) {
        c.ripOp({0x48, 0x83, 0x3D}, x.slot(0), {0x00});               // cmp qword [slot],0
        size_t jne = c.jccShort(0x75);
        c.ripOp({0x48, 0x89, 0x0D}, x.slot(0));                       // mov [slot],rcx
        c.bind(jne);
        c.b({0x8B, 0x41, 0x08, 0xC3});                                // mov eax,[rcx+8] ; ret
    }, err);
}

static int64_t currencyOffset(CurrencyMod::Kind k) {
    switch (k) { case CurrencyMod::Emerald: return 0x8; case CurrencyMod::Gold: return -0x4; default: return 0x14; }
}
bool CurrencyMod::get(const Process& p, Kind k, int32_t& v) const {
    uint64_t o = object(p);
    return o && p.rd(o + currencyOffset(k), v);
}
bool CurrencyMod::set(const Process& p, Kind k, int32_t v) const {
    uint64_t o = object(p);
    return o && p.wr(o + currencyOffset(k), v);
}

// ---- xp
bool XpMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    if (!findOne(p, "48 63 8F ** ** ** ** 48 03 CB", a, err)) return false;
    bool ok = hook_.install(p, a, 7, [&](Asm& c, const HookCtx& x) {
        c.ripOp({0x48, 0x89, 0x3D}, x.slot(0));                       // mov [slot0],rdi   (player xp object)
        c.raw(x.orig);                                                // movsxd rcx,[rdi+disp]  (original)
        c.ripOp({0x48, 0x0F, 0xAF, 0x1D}, x.slot(8));                 // imul rbx,[slot8]  (multiplier)
        c.jmp(x.ret);
    }, err);
    if (ok) ok = setMultiplier(p, mult_);
    return ok;
}
bool XpMod::setMultiplier(const Process& p, int64_t m) {
    mult_ = m;
    return !hook_.active() || p.wr(hook_.ctx().slot(8), m);
}
bool XpMod::total(const Process& p, int32_t& v) const { uint64_t o = object(p); return o && p.rd(o + 0x160, v); }
bool XpMod::setTotal(const Process& p, int32_t v) const { uint64_t o = object(p); return o && p.wr(o + 0x160, v); }

// ---- threat (1.17 "FFH" hook)
bool ThreatMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    if (!findOne(p, "48 89 41 08 48 83 C1 10 E8 *7", a, err)) return false;
    return hook_.install(p, a, 8, [&](Asm& c, const HookCtx& x) {
        c.b({0x53});                                                  // push rbx
        c.b({0x48, 0x8B, 0x59, 0x50});                                // mov rbx,[rcx+50]
        c.b({0x48, 0x39, 0x59, 0x08});                                // cmp [rcx+8],rbx
        size_t jne = c.jccShort(0x75);
        c.ripOp({0x48, 0x89, 0x0D}, x.slot(0));                       // mov [slot],rcx
        c.bind(jne);
        c.b({0x5B});                                                  // pop rbx
        c.b({0x48, 0x89, 0x41, 0x08});                                // mov [rcx+8],rax   (original)
        c.b({0x48, 0x83, 0xC1, 0x10});                                // add rcx,10        (original)
        c.jmp(x.ret);
    }, err);
}
bool ThreatMod::get(const Process& p, int32_t& v) const { uint64_t o = object(p); return o && p.rd(o + 8, v); }
bool ThreatMod::set(const Process& p, int32_t v) const { uint64_t o = object(p); return o && p.wr(o + 8, v); }

// ---- position
bool PositionMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    if (!findOne(p, "0F 10 89 A0 01 00 00 0F 11 4B", a, err)) return false;
    return hook_.install(p, a, 7, [&](Asm& c, const HookCtx& x) {
        c.ripOp({0x48, 0x89, 0x0D}, x.slot(0));                       // mov [slot],rcx
        c.raw(x.orig);                                                // movups xmm1,[rcx+1A0]
        c.jmp(x.ret);
    }, err);
}
bool PositionMod::get(const Process& p, float xyz[3]) const { uint64_t o = object(p); return o && p.read(o + 0x1A0, xyz, 12); }
bool PositionMod::set(const Process& p, const float xyz[3]) const { uint64_t o = object(p); return o && p.write(o + 0x1A0, xyz, 12); }
bool PositionMod::save(const Process& p) {
    float v[3];
    if (!get(p, v)) return false;
    std::memcpy(saved_, v, sizeof v);
    hasSaved_ = true;
    return true;
}
bool PositionMod::teleportSaved(const Process& p) const { return hasSaved_ && set(p, saved_); }

// ---- model scale
bool ScaleMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    if (!findOne(p, "F2 0F 11 83 7C 01 00 00 8B 47", a, err)) return false;
    return hook_.install(p, a, 8, [&](Asm& c, const HookCtx& x) {
        c.ripOp({0x48, 0x89, 0x1D}, x.slot(0));                       // mov [slot],rbx
        c.raw(x.orig);                                                // movsd [rbx+17C],xmm0
        c.jmp(x.ret);
    }, err);
}
bool ScaleMod::get(const Process& p, float& w, float& s) const {
    uint64_t o = object(p); float v[2];
    if (!o || !p.read(o + 0x17C, v, 8)) return false;
    w = v[0]; s = v[1]; return true;
}
bool ScaleMod::set(const Process& p, float w, float s) const {
    uint64_t o = object(p); float v[2] = {w, s};
    return o && p.write(o + 0x17C, v, 8);
}


// ============================================================ item editor
bool ItemEditorMod::onEnable(const Process& p, std::string& err) {
    uint64_t a;
    if (!findOne(p,
        "C3 CC CC CC CC 48 89 5C 24 10 57 48 83 EC 20 48 83 C1 28 E8 ** ** ** ** 48 8B D8 E8 ** ** ** ** "
        "4C 8B 03 48 8D 54 24 30 48 8B C8 48 8B F8 E8 ** ** ** FF 48 63 4C 24 30 83 F9 FF 74 0D 48 8B 07 "
        "48 8D 0C 49 48 8D 0C C8 EB 02 33 C9 48 8B 49 08 48 8B 5C 24 38 48 83 C4 20 5F E9 ** ** ** ** "
        "CC CC CC CC CC CC 48 89 5C 24 10 57 48 83 EC 20 E8 ** ** ** ** 48 8B D8 E8 ** ** ** ** "
        "4C 8B 03 48 8D 54 24 30 48 8B C8 48 8B F8 E8 ** ** ** ** 48 63 4C 24 30 83 F9 FF 74 0D 48 8B 07 "
        "48 8D 0C 49 48 8D 0C C8 EB 02 33 C9 48 8B 49 08 48 8B 5C 24 38 48 83 C4 20 5F E9 ** ** ** ** "
        "CC CC CC CC CC CC CC CC CC CC", a, err)) {
        // Legacy Mode's pattern wasn't found (a different build compiled this bit of code differently - Steam,
        // most likely). That's fine as long as Modern Mode can still do the job through the reflection tables
        // instead: let the mod switch on anyway, just without a working Legacy capture.
        if (modernAvailable()) { err.clear(); return true; }
        return false;
    }
    // +0x20:  mov r8,[rbx] ; lea rdx,[rsp+30]   (rbx = item object)
    if (hook_.install(p, a + 0x20, 8, [&](Asm& c, const HookCtx& x) {
        c.ripOp({0x48, 0x89, 0x1D}, x.slot(0));                       // mov [slot],rbx
        c.raw(x.orig);                                                // original 8 bytes
        c.jmp(x.ret);
    }, err)) return true;
    return modernAvailable();      // the hook failed to install, but Modern Mode can still carry the mod
}

std::string ItemEditorMod::technique() const {
    if (modernTarget_)
        return "Modern Mode: no code is patched or hooked. DungeonKit found the item through the game's own "
               "reflection tables (your character -> its ItemStashComponent -> an inventory or storage slot -> "
               "the item), the same name-based lookup the Minecraft Dungeons II side uses, and reads/writes its "
               "fields directly. Works the same way on every platform, Steam included.";
    return CaptureMod::technique();
}

namespace {
struct FieldInfo { uint32_t off; uint32_t size; };
const FieldInfo kFields[] = {
    {0x00, 4},   // ItemId
    {0x38, 4},   // Rarity
    {0x60, 1},   // Gilded
    {0x64, 1},   // GildedEnchant
    {0x68, 4},   // GildedLevel
    {0x20, 4},   // TotalEnchants
    {0x24, 4},   // MaxEnchants
    {0x30, 4},   // TotalStats
    {0x34, 4},   // MaxStats
    {0x79, 1},   // Cloned: InventoryItemMetaData.Cloned (the item sits at InventoryItem+0x28, its Meta at +0xA0)
};
}

bool ItemEditorMod::getInt(const Process& p, Field f, int32_t& v) const {
    uint64_t o = object(p);
    if (!o) return false;
    const FieldInfo& fi = kFields[f];
    int32_t tmp = 0;
    if (!p.read(o + fi.off, &tmp, fi.size)) return false;   // little endian: low bytes first
    if (fi.size == 1) tmp &= 0xFF;
    v = tmp;
    return true;
}
bool ItemEditorMod::setInt(const Process& p, Field f, int32_t v) const {
    uint64_t o = object(p);
    return o && p.write(o + kFields[f].off, &v, kFields[f].size);
}
bool ItemEditorMod::getPower(const Process& p, float& v) const { uint64_t o = object(p); return o && p.rd(o + 0x14, v); }
bool ItemEditorMod::setPower(const Process& p, float v) const { uint64_t o = object(p); return o && p.wr(o + 0x14, v); }

int ItemEditorMod::enchantCapacity(const Process& p) const {
    int32_t total = 0, mx = 0;
    if (!getInt(p, TotalEnchants, total) || !getInt(p, MaxEnchants, mx)) return 0;
    int cap = (mx > 0 && mx <= 64) ? (mx > total ? mx : total) : total;
    if (cap < 0) cap = 0;
    return cap > 64 ? 0 : cap;      // garbage values -> refuse to touch the array
}

bool ItemEditorMod::enchantAddr(const Process& p, int group, int slot, uint64_t& addr) const {
    if (group < 0 || group >= kGroups || slot < 0 || slot >= kSlots) return false;
    uint64_t o = object(p);
    if (!o || group >= enchantCapacity(p)) return false;
    uint64_t arr = 0;
    if (!p.rd(o + 0x18, arr) || !arr) return false;
    static const uint32_t slotOff[kSlots] = {0x20, 0x00, 0x10};     // slot 1, 2, 3
    addr = arr + 0x30ull * group + slotOff[slot];
    return true;
}

bool ItemEditorMod::getEnchant(const Process& p, int g, int s, int32_t& id, int32_t& tier) const {
    uint64_t a;
    if (!enchantAddr(p, g, s, a)) return false;
    int32_t i = 0, t = 0;
    if (!p.read(a, &i, 4) || !p.read(a + 4, &t, 4)) return false;
    id = i & 0xFF;                               // enchant IDs fit in one byte (highest is 161)
    tier = t;
    return true;
}
bool ItemEditorMod::setEnchantId(const Process& p, int g, int s, int32_t id) const {
    uint64_t a;
    if (!enchantAddr(p, g, s, a)) return false;
    uint8_t b = (uint8_t)id;                     // write one byte so neighbouring data is never touched
    return p.write(a, &b, 1);
}
bool ItemEditorMod::setEnchantTier(const Process& p, int g, int s, int32_t tier) const {
    uint64_t a;
    return enchantAddr(p, g, s, a) && p.write(a + 4, &tier, 4);
}

int ItemEditorMod::statCapacity(const Process& p) const {
    int32_t total = 0, mx = 0;
    if (!getInt(p, TotalStats, total) || !getInt(p, MaxStats, mx)) return 0;
    if (total < 0 || total > kMaxStats) return 0;                     // garbage: refuse to touch the array
    int cap = (mx > total && mx <= kMaxStats) ? mx : total;
    return cap;
}
bool ItemEditorMod::statInfo(const Process& p, uint64_t& arrayPtr, int32_t& used, int32_t& max) const {
    uint64_t o = object(p);
    arrayPtr = 0; used = 0; max = 0;
    return o && p.rd(o + 0x28, arrayPtr) && getInt(p, TotalStats, used) && getInt(p, MaxStats, max);
}
bool ItemEditorMod::getStat(const Process& p, int i, uint16_t& v) const {
    uint64_t o = object(p), arr = 0;
    if (!o || i < 0 || i >= statCapacity(p) || !p.rd(o + 0x28, arr) || !arr) return false;
    return p.rd(arr + 2ull * i, v);
}
bool ItemEditorMod::getStats(const Process& p, std::vector<uint16_t>& out) const {
    out.clear();
    uint64_t o = object(p), arr = 0;
    const int n = statCapacity(p);
    if (!o || n <= 0 || !p.rd(o + 0x28, arr) || !arr) return false;
    out.assign(n, 0);
    if (p.read(arr, out.data(), size_t(n) * 2)) return true;
    // one big read failed (array shorter than reported?): fall back to reading entry by entry
    for (int i = 0; i < n; ++i) { uint16_t v = 0; if (!p.rd(arr + 2ull * i, v)) { out.resize(i); break; } out[i] = v; }
    return !out.empty();
}

bool ItemEditorMod::setStat(const Process& p, int i, uint16_t v) const {
    uint64_t o = object(p), arr = 0;
    if (!o || i < 0 || i >= statCapacity(p) || !p.rd(o + 0x28, arr) || !arr) return false;
    return p.wr(arr + 2ull * i, v);
}

bool ItemEditorMod::readPreset(const Process& p, Preset& s) const {
    s = Preset();
    if (!getInt(p, ItemId, s.id) || !getPower(p, s.power) || !getInt(p, Rarity, s.rarity) ||
        !getInt(p, Gilded, s.gilded) || !getInt(p, GildedEnchant, s.gildedEnchant) || !getInt(p, GildedLevel, s.gildedLevel)) return false;
    s.levels = enchantCapacity(p);
    for (int g = 0; g < kGroups && g < s.levels; ++g)
        for (int sl = 0; sl < kSlots; ++sl) getEnchant(p, g, sl, s.enchId[g][sl], s.enchTier[g][sl]);
    return true;
}

bool ItemEditorMod::applyPreset(const Process& p, const Preset& s, int& levelsWritten) const {
    levelsWritten = 0;
    if (!object(p)) return false;
    setInt(p, ItemId, s.id);
    setPower(p, s.power);
    setInt(p, Rarity, s.rarity);
    setInt(p, Gilded, s.gilded ? 1 : 0);
    setInt(p, GildedEnchant, s.gildedEnchant);
    setInt(p, GildedLevel, s.gildedLevel);
    const int room = enchantCapacity(p);                                   // the item may have changed type: read the room after the ID was written
    for (int g = 0; g < kGroups && g < room && g < s.levels; ++g) {
        for (int sl = 0; sl < kSlots; ++sl) {
            setEnchantId(p, g, sl, s.enchId[g][sl]);
            setEnchantTier(p, g, sl, s.enchTier[g][sl]);
        }
        ++levelsWritten;
    }
    return true;
}

int ItemEditorMod::mismatches(const Process& p, const Preset& w, int levelsWritten) const {
    Preset r;
    if (!readPreset(p, r)) return -1;
    int bad = 0;
    if (r.id != w.id) ++bad;
    if (r.rarity != w.rarity) ++bad;
    if ((r.gilded != 0) != (w.gilded != 0)) ++bad;
    if (r.gildedEnchant != w.gildedEnchant) ++bad;
    if (r.gildedLevel != w.gildedLevel) ++bad;
    if (std::fabs(r.power - w.power) > 0.05f) ++bad;
    for (int g = 0; g < kGroups && g < levelsWritten; ++g)
        for (int sl = 0; sl < kSlots; ++sl)
            if (r.enchId[g][sl] != w.enchId[g][sl] || r.enchTier[g][sl] != w.enchTier[g][sl]) ++bad;
    return bad;
}

bool ItemEditorMod::copy(const Process& p) {
    // Copied: item ID, power level, rarity, gilded, gilded enchant, gilded level, and the enchantments with their levels (tiers).
    // Not copied: the stats / armor properties.
    Preset s;
    if (!readPreset(p, s)) return false;
    snap_ = s;
    snapValid_ = true;
    return true;
}

bool ItemEditorMod::paste(const Process& p, int* levelsWritten, int* wrong) const {
    if (!snapValid_ || !object(p)) return false;
    int written = 0;
    if (!applyPreset(p, snap_, written)) return false;
    if (levelsWritten) *levelsWritten = written;
    if (wrong) *wrong = mismatches(p, snap_, written);
    return true;
}

// ---- Modern Mode
uint64_t ItemEditorMod::object(const Process& p) const {
    if (modernTarget_) return UE4::plausible(modernTarget_) ? modernTarget_ : 0;
    return CaptureMod::object(p);
}

bool ItemEditorMod::selectModern(const Process& p, uint64_t itemStruct) {
    if (!UE4::plausible(itemStruct)) { modernTarget_ = 0; return false; }
    int32_t id = 0;
    if (!p.read(itemStruct, &id, 4)) { modernTarget_ = 0; return false; }   // confirm it's readable before committing to it
    modernTarget_ = itemStruct;
    return true;
}

bool ItemEditorMod::listInventory(const Process& p, std::vector<InventoryEntry>& out, std::string& err) const {
    out.clear();
    if (!ue_) { err = "Modern Mode is not available for this game."; return false; }
    if (!ue_->init(p, err)) return false;
    uint64_t ch = 0;
    if (!ue_->character(ch)) { err = "Can't find your character. Load into a level first (not a menu)."; return false; }
    uint64_t stash = 0;
    if (!ue_->readPtr(ch, "ItemStashComponent", stash) || !UE4::plausible(stash)) {
        err = "Can't find your inventory. Open your inventory in the game, then try again.";
        return false;
    }
    auto scan = [&](const char* prop, bool storage) {
        uint64_t arr = 0;
        int32_t num = 0;
        if (!ue_->readArray(stash, prop, arr, num)) return;
        for (int32_t i = 0; i < num && i < 512; ++i) {               // 512 is far beyond any real inventory/storage size
            const uint64_t slot = ue_->rd64(arr + (uint64_t)i * 8);
            if (!UE4::plausible(slot)) continue;
            uint64_t itemObj = 0;
            if (!ue_->readPtr(slot, "Item", itemObj) || !UE4::plausible(itemObj)) continue;    // empty slot
            const uint64_t itemStruct = itemObj + 0x28;               // InventoryItem.Item (confirmed via the game's own reflection dump)
            int32_t id = 0;
            if (!p.read(itemStruct, &id, 4)) continue;
            InventoryEntry e;
            e.itemStruct = itemStruct;
            e.itemId = id;
            e.slotIndex = (int)i;
            e.storage = storage;
            out.push_back(e);
        }
    };
    scan("InventorySlots", false);
    scan("StorageChestSlots", true);
    if (out.empty() && err.empty()) err = "Your inventory and storage chest look empty right now.";
    return !out.empty();
}

// ============================================================ profile
// ============================================================ Apocalypse level
namespace {
struct ApoPath { bool physx; uint64_t base; std::vector<uint32_t> offs; };   // offsets in the order the table lists them (first = last step)
const std::vector<ApoPath>& apoPaths() {
    static const std::vector<ApoPath> v = {
        {true,  0x1EB3C0, {0xAC, 0xC30, 0x198, 0x20, 0x10, 0x440, 0x30}},
        {false, 0x4743088, {0xAC, 0xC30, 0x198, 0x20, 0x10, 0x440, 0x50}},
        {true,  0x1E8878, {0xAC, 0xC30, 0x198, 0x20, 0x10, 0x440, 0x50}},
        {true,  0x1EB3C0, {0xAC, 0xC30, 0x198, 0x20, 0x50, 0x9E8, 0x30}},
        {false, 0x4743088, {0xAC, 0xC30, 0x198, 0x20, 0x50, 0x9E8, 0x50}},
        {true,  0x1E8878, {0xAC, 0xC30, 0x198, 0x20, 0x50, 0x9E8, 0x50}},
        {false, 0x428FC60, {0xAC, 0xC30, 0x198, 0x40, 0x70}},
        {false, 0x4655918, {0xAC, 0xC30, 0x198, 0x380, 0x28, 0x68}},
        {false, 0x4652C50, {0xAC, 0xC30, 0x198, 0x380, 0x28}},
        {false, 0x45501F0, {0xAC, 0xC30, 0x198, 0x380, 0x88, 0x0}},
        {false, 0x4550EF8, {0xAC, 0xC30, 0x55C, 0x18, 0x830, 0x8}},
        {false, 0x4550EF8, {0xAC, 0xC30, 0x55C, 0x38, 0x850, 0x8}},
        {false, 0x4550EF8, {0xAC, 0xC30, 0x55C, 0x58, 0x870, 0x8}},
        {false, 0x47515A8, {0xAC, 0xC30, 0x20, 0xB60, 0x8}},
        {false, 0x4550EF8, {0xAC, 0xC30, 0x20, 0xB60, 0x28, 0x10, 0x100}},
    };
    return v;
}
}

bool ApocalypseMod::resolve(const Process& p, int index, uint64_t& addr) const {
    const auto& v = apoPaths();
    if (index < 0 || index >= (int)v.size()) return false;
    const ApoPath& path = v[index];
    const uint64_t mod = path.physx ? p.moduleBase(L"PhysX3_x64.dll") : p.base();
    if (!mod) return false;
    uint64_t cur = 0;
    if (!p.rd(mod + path.base, cur)) return false;
    for (int i = (int)path.offs.size() - 1; i >= 1; --i) {
        if (cur < 0x10000 || cur > 0x7FFFFFFFFFFFull) return false;
        if (!p.rd(cur + path.offs[i], cur)) return false;
    }
    if (cur < 0x10000 || cur > 0x7FFFFFFFFFFFull) return false;
    addr = cur + path.offs[0];
    return true;
}

// Name-based route for builds where no saved pointer path matches: the mission map's threat slider widget keeps the
// selected Apocalypse+ level (EndlessStruggle.Value) in "selectedStruggleLevel". Found through the object tables.
bool ApocalypseMod::sliderValid() const {
    return slider_ && UE4::plausible(slider_) && ue_->className(slider_) == "UMG_MissionThreatSlider_C";
}

bool ApocalypseMod::findSlider(const Process& p, std::string& err) {
    slider_ = 0;
    if (!ue_->init(p, err)) return false;
    UE4::ObjArray oa;
    if (!ue_->objectArray(oa, err)) return false;
    std::map<uint64_t, bool> isTarget;
    std::vector<uint8_t> items((size_t)oa.perChunk * 0x18);
    int32_t index = 0;
    for (int32_t ch = 0; ch < oa.numChunks && index < oa.num; ++ch) {
        const uint64_t chunk = ue_->rd64(oa.chunks + (uint64_t)ch * 8);
        if (!UE4::plausible(chunk)) { index += oa.perChunk; continue; }
        const int32_t inChunk = std::min(oa.perChunk, oa.num - index);
        if (!p.read(chunk, items.data(), (size_t)inChunk * 0x18)) { index += inChunk; continue; }
        for (int32_t i = 0; i < inChunk; ++i, ++index) {
            uint64_t o;
            std::memcpy(&o, &items[(size_t)i * 0x18], 8);
            if (!UE4::plausible(o)) continue;
            uint64_t cls = 0;
            if (!p.read(o + 0x10, &cls, 8) || !UE4::plausible(cls)) continue;
            auto it = isTarget.find(cls);
            if (it == isTarget.end()) it = isTarget.emplace(cls, ue_->objectName(cls) == "UMG_MissionThreatSlider_C").first;
            if (!it->second) continue;
            const std::string nm = ue_->objectName(o);
            if (nm.compare(0, 9, "Default__") == 0 || nm.compare(0, 5, "SKEL_") == 0 || nm.compare(0, 6, "REINST") == 0) continue;
            UE4::Prop ps, ph;
            if (!ue_->find(o, "selectedStruggleLevel", ps)) continue;
            slider_ = o;
            selOff_ = ps.offset;
            hiOff_ = ue_->find(o, "highlightedStruggleLevel", ph) ? ph.offset : 0;
        }
    }
    if (!slider_) { err = "The mission map is not open. Open the mission map (where you pick the threat level), then switch this on again."; return false; }
    return true;
}

bool ApocalypseMod::onEnable(const Process& p, std::string& err) {
    for (int i = 0; i < (int)apoPaths().size(); ++i) {
        uint64_t a;
        int32_t v;
        if (resolve(p, i, a) && p.rd(a, v) && v >= 0 && v <= 100000) { path_ = i; return true; }
    }
    std::string e;
    if (findSlider(p, e)) return true;
    err = e;
    return false;
}

bool ApocalypseMod::get(const Process& p, int32_t& v) const {
    if (path_ >= 0) {
        uint64_t a;
        return resolve(p, path_, a) && p.rd(a, v);
    }
    return sliderValid() && p.rd(slider_ + selOff_, v);
}

bool ApocalypseMod::set(const Process& p, int32_t v) const {
    if (v < 0) return false;
    if (path_ >= 0) {
        uint64_t a;
        int32_t cur;
        if (!resolve(p, path_, a) || !p.rd(a, cur)) return false;
        return p.wr(a, v);
    }
    if (!sliderValid()) return false;
    bool ok = p.wr(slider_ + selOff_, v);
    if (hiOff_) p.wr(slider_ + hiOff_, v);
    return ok;
}

std::unique_ptr<GameProfile> makeMinecraftDungeons() {
    auto g = std::make_unique<GameProfile>();
    g->name = "Minecraft Dungeons";
    g->subtitle = "Launcher / Xbox App / Steam  -  1.0.5 to 1.17";

    auto add = [&](auto* m) { g->owned.emplace_back(m); g->mods.all.push_back(m); return m; };

    g->mods.arrows = add(new NopMod("inf_arrows", "Infinite Arrows", "Arrows of every type are never used up.",
                                    "29 BB FC 01 00 00", 6));
    g->mods.lives = add(new NopMod("inf_lives", "Infinite Lives", "You never lose a life.",
                                   "29 91 ** ** 00 00 ** ** ** ** ** ** ** 33 D2 E*", 6));
    g->mods.enchant  = add(new InfiniteEnchantMod());
    g->mods.currency = add(new CurrencyMod());
    g->mods.xp       = add(new XpMod());
    g->mods.threat   = add(new ThreatMod());
    g->mods.position = add(new PositionMod());
    g->mods.scale    = add(new ScaleMod());
    auto ue = std::make_shared<UE4>();
    g->mods.items    = add(new ItemEditorMod(ue));   // ue powers its Modern Mode (reflection-based, works on every platform)
    g->mods.cooldown = add(new NoCooldownMod(ue));
    g->mods.apocalypse = add(new ApocalypseMod(ue));
    g->mods.viewer   = add(new ItemViewerMod(ue));
    g->mods.ue4      = ue;
    g->mods.noclip   = add(new NoClipMod(ue));
    g->mods.god      = add(new GodModeMod(ue));
    g->mods.notarget = add(new NoTargetMod(ue));
    g->mods.overkill = add(new OverkillMod(ue));
    g->mods.wasd = add(new WasdMod(ue));
    g->mods.wasd->setExclusive(g->mods.noclip);
    g->mods.stats    = add(new StatsMod(ue));
    auto tc = std::make_shared<TimeControl>(ue);
    g->mods.timer    = add(new TimerMod(ue, tc));
    g->mods.pause    = add(new PauseMod(ue, tc));
    return g;
}

} // namespace dk
