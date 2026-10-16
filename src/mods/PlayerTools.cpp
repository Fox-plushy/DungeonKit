#include <cstdio>
#include "PlayerTools.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dk {

// ====================================================================== thread base
bool UEToolMod::onEnable(const Process& p, std::string& err) {
    proc_ = &p;
    if (!ue_->init(p, err)) return false;
    if (!setup(p, err)) return false;
    if (intervalMs() > 0) {
        stop_ = false;
        th_ = CreateThread(nullptr, 0, &UEToolMod::threadMain, this, 0, nullptr);
        if (!th_) { err = "Could not start the background thread."; return false; }
    }
    return true;
}

unsigned long __stdcall UEToolMod::threadMain(void* self) {
    auto* m = static_cast<UEToolMod*>(self);
    while (!m->stop_) {
        m->tick();
        Sleep((DWORD)m->intervalMs());
    }
    return 0;
}

void UEToolMod::stopThread() {
    stop_ = true;
    if (th_) {
        WaitForSingleObject((HANDLE)th_, 5000);
        CloseHandle((HANDLE)th_);
        th_ = nullptr;
    }
}

void UEToolMod::onDisable(const Process& p) {
    stopThread();
    teardown(p);
}

std::string UEToolMod::technique() const {
    const int ms = intervalMs();
    std::string s = "Direct memory read/write: no code is patched at all. DungeonKit walks the game's own object "
                     "tables - the same reflection data Unreal Engine itself uses internally - matching property "
                     "names (like a real one it was built from a class dump of) to the byte offset they live at "
                     "in this build, then reads or writes that offset directly in the game's memory.";
    if (ms > 0) s += " It keeps doing that on its own, roughly every " + std::to_string(ms) + " ms, for as long as this is switched on.";
    return s;
}

static bool gameInForeground(const Process* p) {
    if (!p) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == p->pid();
}

// Finds every copy of the capsule's Z inside the component (moving only one copy would be undone). Used by No Clip and WASD.
static bool capsuleCopies(UE4& ue, const Process& p, uint64_t cap, std::vector<uint32_t>& zOffsets_, std::string& err) {
    UE4::Prop rel;
    if (!ue.find(cap, "RelativeLocation", rel)) { err = "Could not locate the capsule's position."; return false; }

    const uint32_t zOff = rel.offset + 8;                         // FVector: X, Y, Z floats
    uint32_t zBits = 0;
    if (!p.rd(cap + zOff, zBits) || zBits == 0) { err = "Stand somewhere with a non-zero height and try again."; return false; }

    // The position exists in more than one place inside the component; moving only one copy would be undone.
    std::vector<uint8_t> blob;
    for (size_t sz : {size_t(0x1000), size_t(0x800), size_t(0x400)}) {
        blob.assign(sz, 0);
        if (p.read(cap, blob.data(), sz)) break;
        blob.clear();
    }
    if (blob.empty()) { err = "Could not read the character's capsule."; return false; }
    zOffsets_.clear();
    auto okFloat = [](float f) { return std::isfinite(f) && std::fabs(f) < 1.0e7f; };
    for (size_t off = 8; off + 4 <= blob.size(); off += 4) {
        uint32_t v;
        std::memcpy(&v, &blob[off], 4);
        if (v != zBits) continue;
        float x, y, z;
        std::memcpy(&x, &blob[off - 8], 4);
        std::memcpy(&y, &blob[off - 4], 4);
        std::memcpy(&z, &blob[off], 4);
        if (okFloat(x) && okFloat(y) && okFloat(z)) zOffsets_.push_back((uint32_t)off);
    }
    if (zOffsets_.empty() || zOffsets_.size() > 8) {
        err = "Could not pin down your position (found " + std::to_string(zOffsets_.size()) + " matches). Move a little and try again.";
        return false;
    }
    return true;
}

// ====================================================================== No Clip & Fly
bool NoClipMod::setup(const Process& p, std::string& err) {
    uint64_t ch = 0, cap = 0, mov = 0;
    if (!ue_->character(ch)) { err = "Can't find your character. Load into a level first (not a menu)."; return false; }
    if (!ue_->readPtr(ch, "CapsuleComponent", cap)) { err = "The character has no capsule component here."; return false; }
    if (!capsuleCopies(*ue_, p, cap, zOffsets_, err)) return false;

    if (!ue_->readPtr(ch, "CharacterMovement", mov) || !ue_->find(mov, "MovementMode", moveMode_)) {
        err = "Could not find the movement mode.";
        return false;
    }
    haveCollision_ = ue_->find(ch, "bActorEnableCollision", collision_) && collision_.isBool && collision_.mask;
    last_ = std::chrono::steady_clock::now();
    armed_ = true;
    return true;
}

void NoClipMod::tick() {
    uint64_t ch = 0, cap = 0, mov = 0;
    if (!ue_->character(ch)) return;                               // between levels
    const Process& p = *proc_;
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last_).count();
    last_ = now;
    if (dt <= 0.f || dt > 0.25f) dt = 0.008f;
    const float step = speed_ * dt;

    if (gameInForeground(&p) && ue_->readPtr(ch, "CapsuleComponent", cap)) {
        auto key = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
        float dx = 0, dy = 0, dz = 0;
        if (key(VK_SPACE)) dz += step;
        if (key(VK_LCONTROL)) dz -= step;
        if (key('S')) dx += step;                                  // same axes as the original table
        if (key('W')) dx -= step;
        if (key('A')) dy += step;
        if (key('D')) dy -= step;
        if (dx != 0 || dy != 0 || dz != 0) {
            for (uint32_t zo : zOffsets_) {
                float v[3];
                if (!p.read(cap + zo - 8, v, sizeof v)) continue;
                v[0] += dx; v[1] += dy; v[2] += dz;
                p.write(cap + zo - 8, v, sizeof v);
            }
        }
    }

    if (ue_->readPtr(ch, "CharacterMovement", mov)) {
        const uint8_t flying = 5;
        p.write(mov + moveMode_.offset, &flying, 1);
    }
    if (haveCollision_) {
        uint8_t b = 0;
        const uint64_t a = ch + collision_.offset + collision_.byteOffset;
        if (p.read(a, &b, 1) && (b & collision_.mask)) {
            b &= (uint8_t)~collision_.mask;
            p.write(a, &b, 1);
        }
    }
}

void NoClipMod::teardown(const Process& p) {
    uint64_t ch = 0, mov = 0;
    if (!armed_) return;
    armed_ = false;
    if (!ue_->ready() || !ue_->character(ch)) return;
    if (haveCollision_) {
        uint8_t b = 0;
        const uint64_t a = ch + collision_.offset + collision_.byteOffset;
        if (p.read(a, &b, 1)) { b |= collision_.mask; p.write(a, &b, 1); }
    }
    if (ue_->readPtr(ch, "CharacterMovement", mov)) {
        const uint8_t walking = 1;
        p.write(mov + moveMode_.offset, &walking, 1);
    }
}

// ====================================================================== WASD movement (experimental)
// Feeds the game its own movement input. The pawn has a ControlInputVector (the sum of the "move this way" requests of the
// frame); the character movement component turns it into acceleration and then clears it. While a key is held we write
// a unit vector there many times a second, so you walk the normal way: gravity, collision and walk speed all still apply.
// Same axes as the original table (W = -X, S = +X, A = +Y, D = -Y), with an optional rotation.
bool WasdMod::setup(const Process&, std::string& err) {
    uint64_t ch = 0;
    if (!ue_->character(ch)) { err = "Can't find your character. Load into a level first (not a menu)."; return false; }
    UE4::Prop pr;
    if (!ue_->find(ch, "ControlInputVector", pr)) { err = "This game build has no movement input field to use."; return false; }
    inputOffset_ = pr.offset;
    return true;
}

void WasdMod::teardown(const Process&) {}

void WasdMod::tick() {
    if (exclusive_ && exclusive_->enabled()) return;                 // No Clip already moves you with these keys
    const Process& p = *proc_;
    if (!gameInForeground(&p)) return;
    auto key = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    float fx = 0, fy = 0;
    if (key('W')) fx += 1;                                           // W = +X, S = -X, A = -Y, D = +Y
    if (key('S')) fx -= 1;
    if (key('A')) fy -= 1;
    if (key('D')) fy += 1;
    if (fx == 0 && fy == 0) return;                                  // leave the game's own input alone
    const float stickX = (key('D') ? 1.f : 0.f) - (key('A') ? 1.f : 0.f), stickY = (key('W') ? 1.f : 0.f) - (key('S') ? 1.f : 0.f);
    const float len = std::sqrt(fx * fx + fy * fy);
    fx /= len; fy /= len;
    const float rad = rotation_ * 3.14159265f / 180.f;
    const float c = std::cos(rad), sn = std::sin(rad);
    const float v[3] = {fx * c - fy * sn, fx * sn + fy * c, 0.f};
    uint64_t ch = 0;
    if (!ue_->character(ch)) return;                                 // between levels
    p.write(ch + inputOffset_, v, sizeof v);
    // Also tell the player controller which way the "stick" is pushed (mLastMoveX / mLastMoveY). The game's camera and
    // facing look at it, so this is what lets the camera react to WASD like it does to a real move input.
    uint64_t pc = 0;
    if (ue_->readPtr(ch, "Controller", pc) && UE4::plausible(pc)) {
        UE4::Prop px, py;
        if (ue_->find(pc, "mLastMoveX", px) && ue_->find(pc, "mLastMoveY", py)) {
            p.write(pc + px.offset, &stickX, 4);
            p.write(pc + py.offset, &stickY, 4);
        }
    }
}

// ====================================================================== Slot cooldown (works without the code pattern)
static const float kBigReduction = 600.0f;

int SlotCooldownMod::apply(bool restore) {
    uint64_t ch = 0, ec = 0, data = 0;
    int32_t num = 0;
    if (!ue_->ready() || !ue_->character(ch)) return -1;
    if (!ue_->readPtr(ch, "EquipmentComponent", ec) || !UE4::plausible(ec)) return -1;
    if (!ue_->readArray(ec, "EquipmentSlots", data, num) || num <= 0 || num > 64) return -1;
    int touched = 0;
    for (int i = 0; i < num; ++i) {
        const uint64_t slot = ue_->rd64(data + (uint64_t)i * 8);
        if (!UE4::plausible(slot)) continue;
        UE4::Prop pr;
        if (!ue_->find(slot, "CooldownReduction", pr)) continue;
        const uint64_t a = slot + pr.offset;
        float cur = 0;
        if (!proc_->read(a, &cur, sizeof(cur))) continue;
        if (restore) {
            auto it = original_.find(slot);
            if (it != original_.end() && cur == kBigReduction) { proc_->write(a, &it->second, sizeof(float)); ++touched; }
            continue;
        }
        if (cur == kBigReduction) { ++touched; continue; }
        if (!original_.count(slot)) original_[slot] = cur;
        if (proc_->write(a, &kBigReduction, sizeof(float))) ++touched;
    }
    return touched;
}

bool SlotCooldownMod::setup(const Process&, std::string& err) {
    original_.clear();
    const int n = apply(false);
    if (n < 0) { err = "Can't find your equipment. Load into a level first (not a menu)."; return false; }
    if (n == 0) { err = "Found your equipment but none of its slots has a cooldown value in this game build."; return false; }
    std::string vals;
    for (auto& kv : original_) { char b[32]; snprintf(b, sizeof(b), "%s%g", vals.empty() ? "" : ", ", kv.second); vals += b; }
    note_ = "Slot cooldown mode: " + std::to_string(n) + " slots changed (values before: " + vals + ").";
    return true;
}

void SlotCooldownMod::tick() { apply(false); }

void SlotCooldownMod::teardown(const Process&) {
    apply(true);
    original_.clear();
}

// ====================================================================== Enemies ignore you
bool NoTargetMod::setup(const Process& p, std::string& err) {
    uint64_t ch = 0;
    if (!ue_->character(ch)) { err = "Can't find your character. Load into a level first (not a menu)."; return false; }
    if (!ue_->find(ch, "targetable", flag_) || !flag_.isBool || !flag_.mask) { err = "Could not find the targetable flag in this game build."; return false; }
    const uint64_t a = ch + flag_.offset + flag_.byteOffset;
    uint8_t b = 0;
    if (!p.read(a, &b, 1)) { err = "Could not read your character."; return false; }
    orig_ = (b & flag_.mask) != 0;
    b &= (uint8_t)~flag_.mask;
    p.write(a, &b, 1);
    armed_ = true;
    return true;
}

void NoTargetMod::tick() {
    uint64_t ch = 0;
    if (!ue_->character(ch)) return;
    const uint64_t a = ch + flag_.offset + flag_.byteOffset;
    uint8_t b = 0;
    if (proc_->read(a, &b, 1) && (b & flag_.mask)) { b &= (uint8_t)~flag_.mask; proc_->write(a, &b, 1); }
}

void NoTargetMod::teardown(const Process& p) {
    if (!armed_) return;
    armed_ = false;
    uint64_t ch = 0;
    if (!ue_->ready() || !ue_->character(ch)) return;
    const uint64_t a = ch + flag_.offset + flag_.byteOffset;
    uint8_t b = 0;
    if (p.read(a, &b, 1)) { if (orig_) b |= flag_.mask; p.write(a, &b, 1); }
}

// ====================================================================== God Mode
// ====================================================================== Overkill
static const float kOverkill = 1000000.0f;

bool OverkillMod::collect(std::vector<uint64_t>& addrs) {
    addrs.clear();
    uint64_t ch = 0, as = 0, data = 0;
    int32_t num = 0;
    if (!ue_->character(ch) || !ue_->readPtr(ch, "AbilitySystem", as) || !ue_->readArray(as, "SpawnedAttributes", data, num)) return false;
    static const char* props[] = {"MeleeAttackDamageMultiplier", "RangedAttackDamageMultipler", "DealDamageMul"};
    for (int i = 0; i < num && i < 64; ++i) {
        const uint64_t o = ue_->rd64(data + (uint64_t)i * 8);
        if (!UE4::plausible(o)) continue;
        for (const char* name : props) {
            UE4::Prop pr;
            if (ue_->find(o, name, pr)) addrs.push_back(o + pr.offset);
        }
    }
    return true;
}

bool OverkillMod::setup(const Process&, std::string& err) {
    std::vector<uint64_t> a;
    if (!collect(a) || a.empty()) { err = "Can't find your character's damage values. Load into a level first."; return false; }
    original_.clear();
    return true;
}

void OverkillMod::tick() {
    std::vector<uint64_t> addrs;
    if (!collect(addrs)) return;                              // between levels
    for (uint64_t a : addrs) {
        float cur = 0;
        if (!ue_->rd(a, cur) || !std::isfinite(cur)) continue;
        if (cur != kOverkill) {
            original_[a] = cur;                                // the game's own (latest) value
            const float big = kOverkill;
            proc_->write(a, &big, sizeof big);
        }
    }
}

void OverkillMod::teardown(const Process& p) {
    std::vector<uint64_t> addrs;
    if (ue_->ready() && collect(addrs))
        for (uint64_t a : addrs) {                             // only objects that exist right now; old ones may be gone
            auto it = original_.find(a);
            float cur = 0;
            if (it != original_.end() && ue_->rd(a, cur) && cur == kOverkill) p.write(a, &it->second, sizeof(float));
        }
    original_.clear();
}

bool GodModeMod::setup(const Process&, std::string& err) {
    uint64_t ch = 0;
    if (!ue_->character(ch)) { err = "Can't find your character. Load into a level first (not a menu)."; return false; }
    if (!ue_->find(ch, "bCanBeDamaged", flag_) || !flag_.isBool || !flag_.mask) { err = "Could not find the damage flag."; return false; }
    armed_ = true;
    return true;
}

void GodModeMod::tick() {
    uint64_t ch = 0;
    if (!ue_->character(ch)) return;
    const uint64_t a = ch + flag_.offset + flag_.byteOffset;
    uint8_t b = 0;
    if (proc_->read(a, &b, 1) && (b & flag_.mask)) { b &= (uint8_t)~flag_.mask; proc_->write(a, &b, 1); }
}

void GodModeMod::teardown(const Process& p) {
    uint64_t ch = 0;
    if (!armed_) return;
    armed_ = false;
    if (!ue_->ready() || !ue_->character(ch)) return;
    const uint64_t a = ch + flag_.offset + flag_.byteOffset;
    uint8_t b = 0;
    if (p.read(a, &b, 1)) { b |= flag_.mask; p.write(a, &b, 1); }
}

// ====================================================================== Player stats
// group index -> attribute class name and the slot the original table used as a fallback
static const struct { const char* name; int fallback; } kGroups[5] = {
    {"Melee", 2}, {"Ranged", 3}, {"Health", 7}, {"Resistance", 0}, {"Movement", 1}};

const char* StatsMod::groupName(int g) { return kGroups[g].name; }

const std::vector<StatsMod::Def>& StatsMod::defs() {
    static const std::vector<Def> d = {
        {"Melee", "Item power",            "MeleeAttackItemPowerFactor"},
        {"Melee", "Damage multiplier",     "MeleeAttackDamageMultiplier"},
        {"Melee", "Attack speed",          "MeleeAttackSpeedMultiplier"},
        {"Melee", "Attack range",          "MeleeAttackRangeMultiplier"},
        {"Melee", "Pushback",              "MeleeAttackPushbackMultiplier"},
        {"Melee", "Life steal",            "MeleeAttackLifeStealAmount"},
        {"Melee", "Hit chance",            "MeleeAttackHitChanceMultiplier"},
        {"Melee", "Damage increase",       "MeleeAttackDamageIncrease"},
        {"Ranged", "Item power",           "RangedAttackItemPowerFactor"},
        {"Ranged", "Damage multiplier",    "RangedAttackDamageMultipler"},
        {"Ranged", "Attack speed",         "RangedAttackSpeedMultiplier"},
        {"Ranged", "Pushback",             "RangedAttackPushbackMultiplier"},
        {"Ranged", "Life steal",           "RangedAttackLifeStealAmount"},
        {"Ranged", "Hit chance",           "RangedAttackHitChanceMultiplier"},
        {"Ranged", "Damage increase",      "RangedAttackDamageIncrease"},
        {"Health", "Shield",               "Shield"},
        {"Health", "Health",               "Health"},
        {"Health", "Max health",           "MaxHealth"},
        {"Health", "Death resistance",     "ResistDeath"},
        {"Health", "Endurance",            "Endurance"},
        {"Health", "Max endurance",        "MaxEndurance"},
        {"Health", "Endurance recovery",   "EnduranceRecoveryPerSecond"},
        {"Resistance", "Slow resistance",  "SlowResistanceMagnitude"},
        {"Movement", "Speed multiplier",   "SpeedMultiplier"},
        {"Movement", "Friction multiplier","FrictionMultiplier"},
        {"Movement", "Rotation multiplier","RotationMultiplier"},
        {"Movement", "Dodge cooldown",     "DodgeCooldown"},
        {"Movement", "Dodge speed",        "DodgeSpeed"},

        // --- Candidate names below: same "XxxYyyMultiplier" naming convention as the attributes above,
        // mined from the equivalent, verified Minecraft Dungeons 2 attribute list. Not independently
        // confirmed to exist on Minecraft Dungeons' class, but that's safe to try: a name that doesn't
        // exist here simply fails to read/write (shown as unavailable), it can't crash the game.
        {"Melee", "Critical hit chance",      "MeleeAttackCriticalChanceMultiplier"},
        {"Melee", "Critical hit damage",      "MeleeAttackCriticalDamageMultiplier"},
        {"Melee", "Knockback",                "MeleeAttackKnockbackMultiplier"},
        {"Melee", "Stagger",                  "MeleeAttackStaggerMultiplier"},
        {"Melee", "Splash damage",            "MeleeAttackSplashDamageMultiplier"},

        {"Ranged", "Critical hit chance",     "RangedAttackCriticalChanceMultiplier"},
        {"Ranged", "Critical hit damage",     "RangedAttackCriticalDamageMultiplier"},
        {"Ranged", "Recharge speed",          "RangedAttackRechargeMultiplier"},
        {"Ranged", "Projectile velocity",     "RangedAttackVelocityMultiplier"},
        {"Ranged", "Multi-shot chance",       "RangedAttackMultiShotChance"},
        {"Ranged", "Max ammo",                "RangedAttackAmmoCountMax"},

        {"Health", "Health regeneration",     "HealthRegenerationPerSecond"},
        {"Health", "Potion healing",          "HealthPotionHealingMultiplier"},
        {"Health", "Potion cooldown",         "HealthPotionBaseCooldown"},
        {"Health", "Passive healing",         "PassiveHealingPercentage"},

        {"Resistance", "Damage resistance",        "BaseDamageResistanceMultiplier"},
        {"Resistance", "Melee damage resistance",  "MeleeDamageResistanceMultiplier"},
        {"Resistance", "Ranged damage resistance", "RangedDamageResistanceMultiplier"},
        {"Resistance", "Elemental resistance",     "ElementalDamageResistanceMultiplier"},
        {"Resistance", "Fire resistance",          "FireDamageResistanceMultiplier"},
        {"Resistance", "Knockback resistance",     "KnockbackResistanceMultiplier"},
        {"Resistance", "Armor multiplier",         "ArmorMultiplier"},
        {"Resistance", "Poise",                    "Poise"},
        {"Resistance", "Max poise",                "PoiseMax"},

        {"Movement", "Gravity scale",         "GravityScale"},
        {"Movement", "Jump height",           "JumpVerticalMultiplier"},
        {"Movement", "Jump distance",         "JumpHorizontalMultiplier"},
        {"Movement", "Roll cooldown",         "RollCooldown"},
        {"Movement", "Interaction range",     "InteractionRange"},
        {"Movement", "Air control",           "AirControl"},
    };
    return d;
}

static uint64_t nowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool StatsMod::resolve(bool force) {
    if (!force && resolvedAt_ && nowMs() - resolvedAt_ < 250 && groups_[0] + groups_[1] + groups_[2] + groups_[3] + groups_[4]) return true;
    uint64_t ch = 0, as = 0, data = 0;
    int32_t num = 0;
    for (auto& g : groups_) g = 0;
    for (auto& s : groupClass_) s.clear();
    if (!ue_->character(ch)) return false;
    if (!ue_->readPtr(ch, "AbilitySystem", as)) return false;
    if (!ue_->readArray(as, "SpawnedAttributes", data, num)) return false;

    std::vector<std::pair<std::string, uint64_t>> objs;
    for (int i = 0; i < num && i < 64; ++i) {
        const uint64_t o = ue_->rd64(data + (uint64_t)i * 8);
        if (UE4::plausible(o)) objs.emplace_back(ue_->className(o), o);
    }
    for (int g = 0; g < 5; ++g) {
        const std::string want = kGroups[g].name;
        uint64_t pick = 0;
        std::string cn;
        for (auto& o : objs) if (o.first == want) { pick = o.second; cn = o.first; break; }
        if (!pick) for (auto& o : objs) if (o.first.compare(0, want.size(), want) == 0) { pick = o.second; cn = o.first; break; }
        if (!pick && kGroups[g].fallback < (int)objs.size()) { pick = objs[kGroups[g].fallback].second; cn = objs[kGroups[g].fallback].first + " (by slot)"; }
        groups_[g] = pick;
        groupClass_[g] = cn;
    }
    resolvedAt_ = nowMs();
    return true;
}

bool StatsMod::setup(const Process&, std::string& err) {
    if (!resolve(true)) { err = "Can't find your character's attributes. Load into a level first."; return false; }
    return true;
}

bool StatsMod::locate(int def, uint64_t& addr) {
    if (!enabled() || !ue_->ready() || def < 0 || def >= (int)defs().size()) return false;
    if (!resolve(false)) return false;
    int g = 0;
    for (; g < 5; ++g) if (std::string(defs()[def].group) == kGroups[g].name) break;
    if (g >= 5 || !groups_[g]) return false;
    UE4::Prop pr;
    if (!ue_->find(groups_[g], defs()[def].prop, pr)) return false;
    addr = groups_[g] + pr.offset;
    return true;
}

bool StatsMod::get(int def, float& v) {
    uint64_t a;
    return locate(def, a) && ue_->rd(a, v);
}

bool StatsMod::set(int def, float v) {
    uint64_t a;
    return locate(def, a) && ue_->proc()->write(a, &v, sizeof v);
}

std::string StatsMod::summary() {
    if (!enabled()) return {};
    resolve(false);
    std::string s;
    for (int g = 0; g < 5; ++g) {
        s += std::string(kGroups[g].name) + ": ";
        s += groups_[g] ? groupClass_[g] : std::string("not found");
        s += g < 4 ? "   " : "";
    }
    return s;
}

// ====================================================================== Timer / Pause
bool TimeControl::worldSettings(uint64_t& ws) {
    uint64_t eng = 0, vp = 0, world = 0, level = 0;
    if (ue_->engine(eng) && ue_->readPtr(eng, "GameViewport", vp) && ue_->readPtr(vp, "World", world)) {
        if (ue_->readPtr(world, "PersistentLevel", level) && ue_->readPtr(level, "WorldSettings", ws)) return true;
        if (ue_->readPtr(world, "WorldSettings", ws)) return true;
    }
    uint64_t ch = 0;
    if (ue_->character(ch)) {                              // an actor's Outer is its level
        const uint64_t outer = ue_->rd64(ch + 0x20);
        if (UE4::plausible(outer) && ue_->readPtr(outer, "WorldSettings", ws)) return true;
    }
    return false;
}

bool TimeControl::locate(std::string& err) {
    uint64_t ws = 0;
    UE4::Prop pr;
    if (!worldSettings(ws) || !ue_->find(ws, "TimeDilation", pr)) {
        err = "Can't reach the game's time settings. Load into a level first (not a menu).";
        return false;
    }
    return true;
}

void TimeControl::apply() {
    uint64_t ws = 0;
    UE4::Prop pr;
    if (!worldSettings(ws) || !ue_->find(ws, "TimeDilation", pr)) return;     // between levels
    float want = 1.0f;
    if (pauseOn) want = 0.0001f;
    else if (timerOn) want = std::min(20.0f, std::max(0.01f, speed.load()));
    float cur = 0;
    const uint64_t a = ws + pr.offset;
    if (ue_->rd(a, cur) && std::fabs(cur - want) > 1e-6f) ue_->proc()->write(a, &want, sizeof want);
}

bool TimeControl::readRaw(float& v) {
    uint64_t ws = 0;
    UE4::Prop pr;
    return worldSettings(ws) && ue_->find(ws, "TimeDilation", pr) && ue_->rd(ws + pr.offset, v) && std::isfinite(v);
}

bool TimeControl::writeRaw(float v) {
    uint64_t ws = 0;
    UE4::Prop pr;
    return worldSettings(ws) && ue_->find(ws, "TimeDilation", pr) && ue_->proc()->write(ws + pr.offset, &v, sizeof v);
}

bool TimerMod::setup(const Process&, std::string& err) {
    if (!tc_->locate(err)) return false;
    tc_->timerOn = true;
    tc_->apply();
    return true;
}
void TimerMod::teardown(const Process&) { tc_->timerOn = false; if (ue_->ready()) tc_->apply(); }

bool PauseMod::setup(const Process&, std::string& err) {
    if (!tc_->locate(err)) return false;
    tc_->pauseOn = true;
    tc_->apply();
    return true;
}
void PauseMod::teardown(const Process&) { tc_->pauseOn = false; if (ue_->ready()) tc_->apply(); }

} // namespace dk
