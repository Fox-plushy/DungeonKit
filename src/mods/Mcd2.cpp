#include "Mcd2.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

namespace dk {

namespace {
uint64_t nowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
bool startsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
}

void Mcd2::reset() {
    scanned_ = false;
    inv_.clear(); expl_.clear(); cams_.clear(); arms_.clear(); quests_.clear(); actors_.clear();
    slotSize_ = entrySize_ = 0;
    sets_.clear(); setsAt_ = setsPawn_ = 0;
    posOffs_.clear();
}

bool Mcd2::player(Player& p) {
    p = Player();
    if (!ok()) return false;
    if (!ue_->engine(p.eng)) return false;
    uint64_t gi = 0, arr = 0, vp = 0;
    int32_t n = 0;
    if (ue_->readPtr(p.eng, "GameViewport", vp)) ue_->readPtr(vp, "World", p.world);
    if (!ue_->readPtr(p.eng, "GameInstance", gi) || !ue_->readArray(gi, "LocalPlayers", arr, n)) return false;
    const uint64_t lp = ue_->rd64(arr);
    if (!UE4::plausible(lp) || !ue_->readPtr(lp, "PlayerController", p.controller)) return false;
    if (!ue_->readPtr(p.controller, "Pawn", p.pawn) && !ue_->readPtr(p.controller, "Character", p.pawn))
        ue_->readPtr(p.controller, "AcknowledgedPawn", p.pawn);
    ue_->readPtr(p.controller, "PlayerState", p.state);
    ue_->readPtr(p.controller, "PlayerCameraManager", p.camMgr);
    if (UE4::plausible(p.pawn)) ue_->readPtr(p.pawn, "RootComponent", p.root);
    return UE4::plausible(p.pawn);
}

std::string Mcd2::worldName() {
    Player p;
    if (!player(p) || !UE4::plausible(p.world)) return {};
    return ue_->objectName(p.world);
}

bool Mcd2::floatAt(uint64_t obj, const char* prop, uint64_t& addr) {
    UE4::Prop pr;
    if (!UE4::plausible(obj) || !ue_->find(obj, prop, pr)) return false;
    addr = obj + pr.offset;
    return true;
}

// ------------------------------------------------------------------ location
std::vector<uint64_t> Mcd2::worldPosAddrs(uint64_t root, const Vec3& rel) {
    // The game moves a character by its cached world transform, not by RelativeLocation. Find the copies of the
    // position inside the component (three doubles equal to the relative location) so both can be kept in step.
    auto it = posOffs_.find(root);
    if (it == posOffs_.end()) {
        std::vector<uint32_t> offs;
        uint8_t buf[0x280];
        if (ue_->proc()->read(root + 0x180, buf, sizeof buf)) {
            for (size_t i = 0; i + 24 <= sizeof buf; i += 8) {
                double d[3];
                std::memcpy(d, buf + i, 24);
                auto same = [](double a, double b) { return std::fabs(a - b) <= 1e-3 * std::max(1.0, std::fabs(b)); };
                if (same(d[0], rel.x) && same(d[1], rel.y) && same(d[2], rel.z)) offs.push_back((uint32_t)(0x180 + i));
            }
        }
        it = posOffs_.emplace(root, offs).first;
    }
    std::vector<uint64_t> out;
    for (uint32_t o : it->second) out.push_back(root + o);
    return out;
}

bool Mcd2::getLocation(Vec3& v, double* yaw) {
    Player p;
    if (!player(p) || !UE4::plausible(p.root)) return false;
    uint64_t a = 0;
    if (!floatAt(p.root, "RelativeLocation", a)) return false;
    double d[3];
    if (!ue_->proc()->read(a, d, sizeof d)) return false;
    if (!std::isfinite(d[0]) || !std::isfinite(d[1]) || !std::isfinite(d[2])) return false;
    v = {d[0], d[1], d[2]};
    if (yaw) {
        *yaw = 0;
        uint64_t ra = 0;
        double r[3];
        if (floatAt(p.controller, "ControlRotation", ra) && ue_->proc()->read(ra, r, sizeof r) && std::isfinite(r[1])) *yaw = r[1];
    }
    return true;
}

bool Mcd2::addMoveInput(double x, double y) {
    Player p;
    uint64_t a = 0;
    if (!player(p) || !UE4::plausible(p.pawn) || !floatAt(p.pawn, "ControlInputVector", a)) return false;
    const double v[3] = {x, y, 0.0};
    return ue_->proc()->write(a, v, sizeof v);
}

bool Mcd2::gameForeground() const {
    if (!ue_ || !ue_->proc()) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid != 0 && pid == ue_->proc()->pid();
}

bool Mcd2::setLocation(const Vec3& v) {
    Player p;
    if (!player(p) || !UE4::plausible(p.root)) return false;
    uint64_t a = 0;
    if (!floatAt(p.root, "RelativeLocation", a)) return false;
    double cur[3];
    if (!ue_->proc()->read(a, cur, sizeof cur)) return false;
    const double nv[3] = {v.x, v.y, v.z};
    for (uint64_t w : worldPosAddrs(p.root, {cur[0], cur[1], cur[2]})) ue_->proc()->write(w, nv, sizeof nv);
    return ue_->proc()->write(a, nv, sizeof nv);
}



// ------------------------------------------------------------------ enemies ignore you
int Mcd2::applyIgnore() {
    int changed = 0;
    auto zero = [&](uint64_t setObj, const char* prop) {
        UE4::Prop pr;
        if (!ue_->find(setObj, prop, pr)) return;
        const uint64_t a = setObj + pr.offset;
        float base = 0, cur = 0;
        if (!ue_->rd(a + 8, base) || !ue_->rd(a + 0xC, cur) || !std::isfinite(base) || !std::isfinite(cur)) return;
        bool known = false;
        for (auto& sv : ign_) if (sv.addr == a) { known = true; break; }
        if (!known) {
            if (ign_.size() >= 4000) return;
            IgnoreSave sv;
            sv.obj = setObj; sv.cls = ue_->rd64(setObj + 0x10); sv.addr = a; sv.base = base; sv.cur = cur;
            ign_.push_back(sv);
        }
        if (base == 0.f && cur == 0.f) return;
        const float z = 0.f;
        if (ue_->proc()->write(a + 8, &z, sizeof z) | ue_->proc()->write(a + 0xC, &z, sizeof z)) ++changed;
    };
    Player p;
    if (player(p) && resolveSets(p))
        for (auto& s : sets_)
            if (startsWith(s.first, "ATR_AggroScoreModifiers")) zero(s.second, "AggroMultiplier");
    int mobs = 0;
    for (auto& a : actors_) {
        if (a.kind != 2 || mobs >= 400) continue;
        if (!UE4::plausible(a.obj) || ue_->className(a.obj).empty()) continue;      // the enemy is gone
        uint64_t asc = 0, data = 0;
        int32_t num = 0;
        if (!ue_->readPtr(a.obj, "AbilitySystemComponent", asc) || !UE4::plausible(asc) || !ue_->readArray(asc, "SpawnedAttributes", data, num)) continue;
        ++mobs;
        for (int i = 0; i < num && i < 96; ++i) {
            const uint64_t so = ue_->rd64(data + (uint64_t)i * 8);
            if (!UE4::plausible(so)) continue;
            if (!startsWith(ue_->className(so), "ATR_AIBehavior")) continue;
            zero(so, "SightPerceptionRadius");
            zero(so, "AggroEventRadius");
        }
    }
    return changed;
}

int Mcd2::enemyIgnoreOn(std::string& note) {
    if (!ok()) { note = "Not ready."; return -1; }
    Player p;
    if (!player(p)) { note = "Player not found - load into a level first."; return -1; }
    ign_.clear();
    ignOn_ = true;
    const int n = applyIgnore();
    if (ign_.empty()) { ignOn_ = false; note = "Could not find the aggro or sight values in this game build."; return -1; }
    note.clear();
    return n;
}

void Mcd2::enemyIgnoreTick() {
    if (ignOn_) applyIgnore();
}

void Mcd2::enemyIgnoreOff() {
    if (!ignOn_) return;
    ignOn_ = false;
    for (auto& sv : ign_) {
        if (!UE4::plausible(sv.obj) || ue_->rd64(sv.obj + 0x10) != sv.cls) continue;      // the object is gone or reused: never write there
        ue_->proc()->write(sv.addr + 8, &sv.base, sizeof sv.base);
        ue_->proc()->write(sv.addr + 0xC, &sv.cur, sizeof sv.cur);
    }
    ign_.clear();
}

// ------------------------------------------------------------------ item rarity table
bool Mcd2::rarityRows(std::vector<RarityRow>& out, std::string& note) {
    out.clear();
    if (!ok()) { note = "Not ready."; return false; }
    UE4::ObjArray oa;
    std::string err;
    if (!ue_->objectArray(oa, err)) { note = err; return false; }
    std::map<uint64_t, bool> isTable;
    std::vector<uint8_t> items((size_t)oa.perChunk * 0x18);
    int32_t index = 0;
    int tables = 0;
    for (int32_t c = 0; c < oa.numChunks && index < oa.num; ++c) {
        const uint64_t chunk = ue_->rd64(oa.chunks + (uint64_t)c * 8);
        if (!UE4::plausible(chunk)) { index += oa.perChunk; continue; }
        const int32_t inChunk = std::min(oa.perChunk, oa.num - index);
        if (!ue_->proc()->read(chunk, items.data(), (size_t)inChunk * 0x18)) { index += inChunk; continue; }
        for (int32_t i = 0; i < inChunk; ++i, ++index) {
            uint64_t o;
            std::memcpy(&o, &items[(size_t)i * 0x18], 8);
            if (!UE4::plausible(o)) continue;
            uint64_t cls = ue_->rd64(o + 0x10);
            if (!UE4::plausible(cls)) continue;
            auto it = isTable.find(cls);
            if (it == isTable.end()) {
                const std::string cn = ue_->objectName(cls);
                it = isTable.emplace(cls, cn.size() >= 9 && cn.compare(cn.size() - 9, 9, "DataTable") == 0).first;
            }
            if (!it->second) continue;
            UE4::Prop rs, rm;
            uint64_t rowStruct = 0, mapOff = 0x30;
            if (ue_->find(o, "RowStruct", rs)) rowStruct = ue_->rd64(o + rs.offset); else rowStruct = ue_->rd64(o + 0x28);
            if (!UE4::plausible(rowStruct) || ue_->objectName(rowStruct) != "RarityTableRow") continue;
            if (ue_->find(o, "RowMap", rm)) mapOff = rm.offset;
            uint64_t defOff = 0x30;
            std::vector<UE4::Field> fl;
            if (ue_->listFields(rowStruct, fl))
                for (auto& f : fl) if (f.name == "RarityDefinition") { defOff = f.offset; break; }
            ++tables;
            const std::string tname = ue_->nameAt((uint32_t)ue_->rd64(o + 0x18));
            const uint64_t data = ue_->rd64(o + mapOff);
            int32_t num = 0;
            ue_->rd(o + mapOff + 8, num);
            if (!UE4::plausible(data) || num <= 0 || num > 512) continue;
            for (int32_t k = 0; k < num; ++k) {
                uint8_t e[24];
                if (!ue_->proc()->read(data + (uint64_t)k * 24, e, sizeof e)) continue;
                uint32_t nameIdx;
                uint64_t row;
                std::memcpy(&nameIdx, e, 4);
                std::memcpy(&row, e + 8, 8);
                if (!UE4::plausible(row)) continue;
                const std::string nm = ue_->nameAt(nameIdx);
                if (nm.empty()) continue;
                int32_t w = 0;
                if (!ue_->rd(row + defOff + 0x58, w)) continue;
                RarityRow r;
                r.addr = row + defOff + 0x58;
                r.tagAddr = row + 8;
                r.table = tname;
                r.name = nm;
                r.tag = tagAt(row + 8);
                r.weight = w;
                out.push_back(r);
            }
        }
    }
    if (!tables) { note = "The rarity table was not found in the game's memory."; return false; }
    if (out.empty()) { note = "The rarity table is empty or could not be read."; return false; }
    note.clear();
    return true;
}

bool Mcd2::setRarityWeight(const RarityRow& r, int weight, std::string& err) {
    if (!UE4::plausible(r.addr) || weight < 0) { err = "Bad value."; return false; }
    if (tagAt(r.tagAddr) != r.tag) { err = "The table changed. Read the chances again."; return false; }
    const int32_t w = weight;
    if (!ue_->proc()->write(r.addr, &w, sizeof w)) { err = "Could not write."; return false; }
    return true;
}

// ------------------------------------------------------------------ attributes
const std::vector<Mcd2::AttrDef>& Mcd2::attrDefs() {
    static const std::vector<AttrDef> d = {
        {"Health", "ATR_Health", "Health", nullptr},
        {"Max health", "ATR_Health", "HealthMax", nullptr},
        {"Health potion charges", "ATR_Health", "HealthPotionCharges", nullptr},
        {"Emeralds", "ATR_Currency", "Emeralds", nullptr},
        {"Spring stones", "ATR_Currency", "SpringStone", nullptr},
        {"XP", "ATR_XP", "XP", nullptr},
        {"Level", "ATR_XP", "Level", nullptr},
        {"Enchantment points", "ATR_XP", "EnchantmentPoints", nullptr},
        {"Souls", "ATR_Soul", "Souls", nullptr},
        {"Max souls", "ATR_Soul", "SoulsMax", nullptr},
        {"Arrows (ammo)", "ATR_RangedAttack", "RangedAttackAmmoCount", nullptr},
        {"Max arrows", "ATR_RangedAttack", "RangedAttackAmmoCountMax", nullptr},
        {"Threat level (current)", "ATR_ThreatLevel", "ThreatLevelCurrent", "Threat"},
        {"Threat level offset", "ATR_ThreatLevel", "ThreatLevelOffset", "Threat"},
        {"Threat level (area)", "ATR_ThreatLevel", "ThreatLevelArea", "Threat"},
        {"Threat level (recommended)", "ATR_ThreatLevel", "ThreatLevelRecommended", "Threat"},
        {"Rarity bonus chance", "ATR_Loot", "RarityBonusChance", "Loot"},
        {"Drop chance increase", "ATR_Loot", "DropChanceIncrease", "Loot"},
        {"Drop duplication chance", "ATR_Loot", "DropDuplicationChance", "Loot"},
        {"Looting multiplier", "ATR_Loot", "LootingMultiplier", "Loot"},
        {"Base damage multiplier", "ATR_Damage", "BaseDamageMultiplier", "General combat"},
        {"Base damage bonus", "ATR_Damage", "BaseDamageBonus", "General combat"},
        {"Critical hit chance", "ATR_Damage", "CriticalHitChance", "General combat"},
        {"Critical hit multiplier", "ATR_Damage", "CriticalHitMultiplier", "General combat"},
        {"Knockback multiplier", "ATR_Damage", "KnockbackMultiplier", "General combat"},
        {"Stagger multiplier", "ATR_Damage", "StaggerMultiplier", "General combat"},
        {"Movement damage multiplier", "ATR_Damage", "MovementDamageMultiplier", "General combat"},
        {"Primary target damage multiplier", "ATR_Damage", "PrimaryTargetDamageMultiplier", "General combat"},
        {"Full health target damage multiplier", "ATR_Damage", "FullHealthTargetDamageMultiplier", "General combat"},
        {"Hurt target damage multiplier", "ATR_Damage", "HurtTargetDamageMultiplier", "General combat"},
        {"Desperation multiplier", "ATR_Damage", "DesperationMultiplier", "General combat"},
        {"Opportunity multiplier", "ATR_Damage", "OpportunityMultiplier", "General combat"},
        {"Attack empowerment", "ATR_Damage", "AttackEmpowerment", "General combat"},
        {"Attack empowerment max", "ATR_Damage", "AttackEmpowermentMax", "General combat"},
        {"Melee damage multiplier", "ATR_Damage", "MeleeDamageMultiplier", "Melee"},
        {"Melee weapon multiplier", "ATR_Damage", "MeleeAttackWeaponMultiplier", "Melee"},
        {"Jumping melee damage multiplier", "ATR_Damage", "JumpingMeleeDamageMultiplier", "Melee"},
        {"Critical hit chance (melee)", "ATR_Damage", "CriticalHitChanceMelee", "Melee"},
        {"Critical hit multiplier (melee)", "ATR_Damage", "CriticalHitMultiplierMelee", "Melee"},
        {"Melee attack speed", "ATR_MeleeAttack", "MeleeAttackSpeed", "Melee"},
        {"Melee attack range", "ATR_MeleeAttack", "MeleeAttackRange", "Melee"},
        {"Melee vertical range", "ATR_MeleeAttack", "MeleeVerticalRange", "Melee"},
        {"Melee splash damage multiplier", "ATR_MeleeAttack", "MeleeSplashDamageMultiplier", "Melee"},
        {"Negate melee attack chance", "ATR_MeleeAttack", "NegateMeleeAttackChance", "Melee"},
        {"Ranged damage multiplier", "ATR_Damage", "RangedDamageMultiplier", "Ranged"},
        {"Ranged weapon multiplier", "ATR_Damage", "RangedAttackWeaponMultiplier", "Ranged"},
        {"Critical hit chance (ranged)", "ATR_Damage", "CriticalHitChanceRanged", "Ranged"},
        {"Critical hit multiplier (ranged)", "ATR_Damage", "CriticalHitMultiplierRanged", "Ranged"},
        {"Charged attack multiplier", "ATR_Damage", "RangedChargedAttackMultiplier", "Ranged"},
        {"Long distance bonus multiplier", "ATR_Damage", "RangedLongDistanceAttackBonusMultiplier", "Ranged"},
        {"Point blank multiplier", "ATR_Damage", "RangedPointBlankAttackMultiplier", "Ranged"},
        {"Ranged attack speed", "ATR_RangedAttack", "RangedAttackSpeed", "Ranged"},
        {"Projectile speed multiplier", "ATR_RangedAttack", "RangedVelocityMultiplier", "Ranged"},
        {"Ammo recharge time", "ATR_RangedAttack", "RangedAttackRechargeTime", "Ranged"},
        {"Ammo recharge delay", "ATR_RangedAttack", "RangedAttackRechargeDelay", "Ranged"},
        {"Ammo recharge multiplier", "ATR_RangedAttack", "RangedAttackRechargeMultiplier", "Ranged"},
        {"Multishot chance", "ATR_RangedAttack", "MultiShotChance", "Ranged"},
        {"Multishot extra projectiles", "ATR_RangedAttack", "MultiShotAdditionalProjectiles", "Ranged"},
        {"Double ammo regen chance", "ATR_RangedAttack", "DoubleAmmoRegenChance", "Ranged"},
        {"Extra penetration targets", "ATR_RangedAttack", "AdditionalPenetrationTargets", "Ranged"},
        {"Extra ricochet targets", "ATR_RangedAttack", "AdditionalRicochetTargets", "Ranged"},
        {"Ricochet damage multiplier", "ATR_RangedAttack", "RicochetDamageMultplier", "Ranged"},
        {"Chain reaction chance", "ATR_RangedAttack", "ChainReactionChance", "Ranged"},
        {"Chain reaction projectiles", "ATR_RangedAttack", "ChainReactionProjectiles", "Ranged"},
        {"Artifact damage multiplier", "ATR_Damage", "ArtifactDamageMultiplier", "Artifacts"},
        {"Artifact weapon multiplier", "ATR_Damage", "ArtifactAttackWeaponMultiplier", "Artifacts"},
        {"Artifact 1 weapon multiplier", "ATR_Damage", "Artifact0AttackWeaponMultiplier", "Artifacts"},
        {"Artifact 2 weapon multiplier", "ATR_Damage", "Artifact1AttackWeaponMultiplier", "Artifacts"},
        {"Artifact 3 weapon multiplier", "ATR_Damage", "Artifact2AttackWeaponMultiplier", "Artifacts"},
        {"Artifact cooldown multiplier", "ATR_Artifact", "ArtifactCooldownMultiplier", "Artifacts"},
        {"Artifact healing percentage", "ATR_Health", "ArtifactHealingPercentage", "Artifacts"},
        {"TNT damage multiplier", "ATR_Damage", "ThrowableTntMultiplier", "Artifacts"},
        {"Elemental damage multiplier", "ATR_Damage", "ElementalDamageMultiplier", "Elemental damage"},
        {"Fire damage multiplier", "ATR_Damage", "FireDamageMultiplier", "Elemental damage"},
        {"Ice damage multiplier", "ATR_Damage", "IceDamageMultiplier", "Elemental damage"},
        {"Lightning damage multiplier", "ATR_Damage", "LightningDamageMultiplier", "Elemental damage"},
        {"Poison damage multiplier", "ATR_Damage", "PoisonDamageMultiplier", "Elemental damage"},
        {"Soul damage multiplier", "ATR_Damage", "SoulDamageMultiplier", "Elemental damage"},
        {"Blast damage multiplier", "ATR_Damage", "BlastDamageMultiplier", "Elemental damage"},
        {"Movement speed", "ATR_Movement", "MovementSpeed", "Movement and rolling"},
        {"Movement speed multiplier", "ATR_Movement", "MovementSpeedMultiplier", "Movement and rolling"},
        {"Movement friction", "ATR_Movement", "MovementFriction", "Movement and rolling"},
        {"Movement friction multiplier", "ATR_Movement", "MovementFrictionMultiplier", "Movement and rolling"},
        {"Turn speed", "ATR_Movement", "MovementRotation", "Movement and rolling"},
        {"Turn speed multiplier", "ATR_Movement", "MovementRotationMultiplier", "Movement and rolling"},
        {"Gravity scale", "ATR_Movement", "GravityScale", "Movement and rolling"},
        {"Air control", "ATR_Movement", "AirControl", "Movement and rolling"},
        {"Mass", "ATR_Movement", "Mass", "Movement and rolling"},
        {"Roll cooldown", "ATR_Movement", "RollCooldown", "Movement and rolling"},
        {"Roll cooldown modifier", "ATR_Movement", "RollCooldownModifier", "Movement and rolling"},
        {"Roll charges", "ATR_Movement", "RollCharges", "Movement and rolling"},
        {"Roll charges max", "ATR_Movement", "RollChargesMax", "Movement and rolling"},
        {"Interaction range", "ATR_Movement", "InteractionRange", "Movement and rolling"},
        {"Jump vertical multiplier", "ATR_Jump", "JumpVerticalMultiplier", "Movement and rolling"},
        {"Jump horizontal multiplier", "ATR_Jump", "JumpHorizontalMultiplier", "Movement and rolling"},
        {"Base damage resistance", "ATR_Resistance", "BaseDamageResistanceMultiplier", "Defense"},
        {"Melee damage resistance", "ATR_Resistance", "MeleeDamageResistanceMultiplier", "Defense"},
        {"Ranged damage resistance", "ATR_Resistance", "RangedDamageResistanceMultiplier", "Defense"},
        {"Elemental damage resistance", "ATR_Resistance", "ElementalDamageResistanceMultiplier", "Defense"},
        {"Fire damage resistance", "ATR_Resistance", "FireDamageResistanceMultiplier", "Defense"},
        {"Ice damage resistance", "ATR_Resistance", "IceDamageResistanceMultiplier", "Defense"},
        {"Lightning damage resistance", "ATR_Resistance", "LightningDamageResistanceMultiplier", "Defense"},
        {"Poison damage resistance", "ATR_Resistance", "PoisonDamageResistanceMultiplier", "Defense"},
        {"Soul damage resistance", "ATR_Resistance", "SoulDamageResistanceMultiplier", "Defense"},
        {"Blast damage resistance", "ATR_Resistance", "BlastDamageResistanceMultiplier", "Defense"},
        {"TNT damage resistance", "ATR_Resistance", "TNTDamageResistanceMultiplier", "Defense"},
        {"Knockback resistance", "ATR_Resistance", "KnockbackResistanceMultiplier", "Defense"},
        {"Armor multiplier", "ATR_Resistance", "ArmorMultiplier", "Defense"},
        {"Shield", "ATR_Resistance", "Shield", "Defense"},
        {"Shield max", "ATR_Resistance", "ShieldMax", "Defense"},
        {"Projectile deflect chance", "ATR_Resistance", "ProjectileDeflectChance", "Defense"},
        {"Poise", "ATR_Poise", "Poise", "Defense"},
        {"Poise max", "ATR_Poise", "PoiseMax", "Defense"},
        {"Poise recovery", "ATR_Poise", "PoiseRecovery", "Defense"},
        {"Thorns activation chance", "ATR_Thorns", "ThornsActivationChance", "Defense"},
        {"Thorns damage multiplier", "ATR_Thorns", "ThornsDamageMultiplier", "Defense"},
        {"Passive healing percentage", "ATR_Health", "PassiveHealingPercentage", "Healing"},
        {"Life leech (melee)", "ATR_Health", "LifeLeechMeleePercentage", "Healing"},
        {"Life leech (ranged)", "ATR_Health", "LifeLeechRangedPercentage", "Healing"},
        {"Healing received multiplier", "ATR_Health", "ReceiveHealingMultiplier", "Healing"},
        {"Healing given multiplier", "ATR_Health", "ApplyHealingMultiplier", "Healing"},
        {"Health potion healing multiplier", "ATR_Health", "HealthPotionHealingMultiplier", "Healing"},
        {"Health potion cooldown", "ATR_Health", "HealthPotionBaseCooldown", "Healing"},
        {"Health potion cooldown multiplier", "ATR_Health", "HealthPotionCooldownMultiplier", "Healing"},
        {"Health potion charges max", "ATR_Health", "HealthPotionChargesMax", "Healing"},
        {"Companion damage multiplier", "ATR_Companion", "CompanionDamageMultiplier", "Companions and status"},
        {"Companion resistance multiplier", "ATR_Companion", "CompanionResistanceMultiplier", "Companions and status"},
        {"Companion cooldown multiplier", "ATR_Companion", "CompanionCooldownMultiplier", "Companions and status"},
        {"Positive effects you give: duration", "ATR_Status", "GivePositiveStatusDurationMultiplier", "Companions and status"},
        {"Negative effects you give: duration", "ATR_Status", "GiveNegativeStatusDurationMultiplier", "Companions and status"},
        {"Positive effects you receive: duration", "ATR_Status", "ReceivePositiveStatusDurationMultiplier", "Companions and status"},
        {"Negative effects you receive: duration", "ATR_Status", "ReceiveNegativeStatusDurationMultiplier", "Companions and status"},
        {"Soul gathering", "ATR_Soul", "SoulGathering", "Companions and status"},
        {"Souls passive regeneration", "ATR_Soul", "SoulsPassivePlayerRegenerationAmountPerPeriod", "Companions and status"},
    };
    return d;
}

bool Mcd2::resolveSets(const Player& p) {
    if (setsAt_ && nowMs() - setsAt_ < 400 && setsPawn_ == p.pawn && !sets_.empty()) return true;
    sets_.clear();
    uint64_t owners[2] = {p.pawn, p.state};
    for (uint64_t owner : owners) {
        uint64_t asc = 0, data = 0;
        int32_t num = 0;
        if (!UE4::plausible(owner) || !ue_->readPtr(owner, "AbilitySystemComponent", asc)) continue;
        if (!ue_->readArray(asc, "SpawnedAttributes", data, num)) continue;
        for (int i = 0; i < num && i < 96; ++i) {
            const uint64_t o = ue_->rd64(data + (uint64_t)i * 8);
            if (UE4::plausible(o)) sets_.emplace_back(ue_->className(o), o);
        }
        if (!sets_.empty()) break;
    }
    setsAt_ = nowMs();
    setsPawn_ = p.pawn;
    return !sets_.empty();
}

bool Mcd2::getAttr(int def, float& cur) {
    Player p;
    if (def < 0 || def >= (int)attrDefs().size() || !player(p) || !resolveSets(p)) return false;
    const AttrDef& d = attrDefs()[def];
    for (auto& s : sets_) {
        if (!startsWith(s.first, d.set)) continue;
        UE4::Prop pr;
        if (!ue_->find(s.second, d.prop, pr)) continue;
        return ue_->rd(s.second + pr.offset + 0xC, cur) && std::isfinite(cur);
    }
    return false;
}

bool Mcd2::setAttr(int def, float v) {
    Player p;
    if (def < 0 || def >= (int)attrDefs().size() || !player(p) || !resolveSets(p)) return false;
    const AttrDef& d = attrDefs()[def];
    for (auto& s : sets_) {
        if (!startsWith(s.first, d.set)) continue;
        UE4::Prop pr;
        if (!ue_->find(s.second, d.prop, pr)) continue;
        const uint64_t a = s.second + pr.offset;
        const bool b = ue_->proc()->write(a + 0x8, &v, sizeof v);       // BaseValue
        const bool c = ue_->proc()->write(a + 0xC, &v, sizeof v);       // CurrentValue
        return b || c;
    }
    return false;
}

std::string Mcd2::attrSummary() {
    Player p;
    if (!player(p)) return "Player not found - load into a level.";
    if (!resolveSets(p)) return "No attribute sets found on the player.";
    std::string s = "Attribute sets: ";
    for (size_t i = 0; i < sets_.size(); ++i) s += (i ? ", " : "") + sets_[i].first;
    return s;
}

// ------------------------------------------------------------------ movement component
const std::vector<Mcd2::MoveDef>& Mcd2::moveDefs() {
    static const std::vector<MoveDef> d = {
        {"Walk speed", "MaxWalkSpeed"},
        {"Max acceleration", "MaxAcceleration"},
        {"Fly speed", "MaxFlySpeed"},
        {"Jump velocity", "JumpZVelocity"},
        {"Gravity scale", "GravityScale"},
        {"Air control", "AirControl"},
    };
    return d;
}

bool Mcd2::getMove(int def, float& v) {
    Player p;
    uint64_t mv = 0, a = 0;
    if (def < 0 || def >= (int)moveDefs().size() || !player(p) || !ue_->readPtr(p.pawn, "CharacterMovement", mv)) return false;
    return floatAt(mv, moveDefs()[def].prop, a) && ue_->rd(a, v) && std::isfinite(v);
}

bool Mcd2::setMove(int def, float v) {
    Player p;
    uint64_t mv = 0, a = 0;
    if (def < 0 || def >= (int)moveDefs().size() || !player(p) || !ue_->readPtr(p.pawn, "CharacterMovement", mv)) return false;
    return floatAt(mv, moveDefs()[def].prop, a) && ue_->proc()->write(a, &v, sizeof v);
}

// ------------------------------------------------------------------ camera
bool Mcd2::ownedBy(uint64_t obj, uint64_t owner) {
    for (int d = 0; d < 12 && UE4::plausible(obj); ++d, obj = ue_->rd64(obj + 0x20))
        if (obj == owner) return true;
    return false;
}

bool Mcd2::camOwned(uint64_t obj, const Player& p) {
    return ownedBy(obj, p.pawn) || (UE4::plausible(p.controller) && ownedBy(obj, p.controller)) || (UE4::plausible(p.camMgr) && ownedBy(obj, p.camMgr));
}

// The camera / spring arm objects that belong to this player. When the list of scanned objects is stale (a new level made new
// ones) and nothing matches, the game is scanned again, at most every 15 seconds.
std::vector<uint64_t> Mcd2::camTargets(int kind, const Player& p) {
    auto collect = [&]() {
        std::vector<uint64_t> out;
        const auto& list = kind == Fov ? cams_ : arms_;
        for (auto& o : list) if (camOwned(o.obj, p)) out.push_back(o.obj);
        return out;
    };
    std::vector<uint64_t> out = collect();
    if (out.empty() && nowMs() - camScanAt_ > 15000) {
        camScanAt_ = nowMs();
        std::string err;
        if (rescan(err)) out = collect();
    }
    return out;
}

bool Mcd2::getCam(int kind, float& v) {
    Player p;
    if (!player(p)) return false;
    uint64_t a = 0;
    if (kind == DefaultFov) return floatAt(p.camMgr, "DefaultFOV", a) && ue_->rd(a, v) && std::isfinite(v);
    const char* prop = kind == Fov ? "FieldOfView" : kind == ArmLength ? "TargetArmLength" : "SeekArmLength";
    for (uint64_t o : camTargets(kind, p))
        if (floatAt(o, prop, a) && ue_->rd(a, v) && std::isfinite(v)) return true;
    return false;
}

bool Mcd2::setCam(int kind, float v) {
    Player p;
    if (!player(p)) return false;
    uint64_t a = 0;
    if (kind == DefaultFov) return floatAt(p.camMgr, "DefaultFOV", a) && ue_->proc()->write(a, &v, sizeof v);
    const char* prop = kind == Fov ? "FieldOfView" : kind == ArmLength ? "TargetArmLength" : "SeekArmLength";
    bool any = false;
    for (uint64_t o : camTargets(kind, p))
        if (floatAt(o, prop, a)) any |= ue_->proc()->write(a, &v, sizeof v);
    return any;
}

bool Mcd2::getCamOffset(Vec3& v) {
    Player p;
    if (!player(p)) return false;
    uint64_t a = 0;
    for (uint64_t o : camTargets(ArmLength, p)) {      // SocketOffset lives on the same spring arm as arm length
        if (!floatAt(o, "SocketOffset", a)) continue;
        double d[3];
        if (ue_->proc()->read(a, d, sizeof d) && std::isfinite(d[0]) && std::isfinite(d[1]) && std::isfinite(d[2])) {
            v = {d[0], d[1], d[2]};
            return true;
        }
    }
    return false;
}

bool Mcd2::setCamOffset(const Vec3& v) {
    Player p;
    if (!player(p)) return false;
    uint64_t a = 0;
    bool any = false;
    const double nv[3] = {v.x, v.y, v.z};
    for (uint64_t o : camTargets(ArmLength, p))
        if (floatAt(o, "SocketOffset", a)) any |= ue_->proc()->write(a, nv, sizeof nv);
    return any;
}


// ------------------------------------------------------------------ object scan
bool Mcd2::rescan(std::string& err) {
    scanned_ = false;
    inv_.clear(); expl_.clear(); cams_.clear(); arms_.clear(); actors_.clear();
    std::map<uint64_t, int> kinds;
    UE4::ObjArray oa;
    if (!ue_->objectArray(oa, err)) return false;
    std::map<uint64_t, std::string> classNames;
    std::vector<uint8_t> items((size_t)oa.perChunk * 0x18);
    int32_t index = 0;
    uint64_t slotStruct = 0, entryStruct = 0;
    for (int32_t c = 0; c < oa.numChunks && index < oa.num; ++c) {
        const uint64_t chunk = ue_->rd64(oa.chunks + (uint64_t)c * 8);
        if (!UE4::plausible(chunk)) { index += oa.perChunk; continue; }
        const int32_t inChunk = std::min(oa.perChunk, oa.num - index);
        if (!ue_->proc()->read(chunk, items.data(), (size_t)inChunk * 0x18)) { index += inChunk; continue; }
        for (int32_t i = 0; i < inChunk; ++i, ++index) {
            uint64_t o;
            std::memcpy(&o, &items[(size_t)i * 0x18], 8);
            if (!UE4::plausible(o)) continue;
            uint8_t h[0x28];
            if (!ue_->proc()->read(o, h, sizeof h)) continue;
            uint64_t cls, outer;
            uint32_t nameIdx;
            std::memcpy(&cls, h + 0x10, 8);
            std::memcpy(&nameIdx, h + 0x18, 4);
            std::memcpy(&outer, h + 0x20, 8);
            if (!UE4::plausible(cls)) continue;
            auto it = classNames.find(cls);
            if (it == classNames.end()) it = classNames.emplace(cls, ue_->objectName(cls)).first;
            const std::string& cn = it->second;
            const bool wantInv = contains(cn, "InventoryManagerComponent") && !contains(cn, "Mock");
            const bool wantExpl = contains(cn, "WorldExplorationComponent");
            const bool wantCam = contains(cn, "CameraComponent") && !contains(cn, "Cine");
            const bool wantArm = contains(cn, "SpringArmComponent");
            const bool wantStruct = cn == "ScriptStruct";
            const bool wantQuest = cn == "LiveQuest";
            auto kit = kinds.find(cls);
            if (kit == kinds.end()) {                       // is this class a chest or a mob? walk up its parent classes once
                int k = 0;
                uint64_t cc = cls;
                // Both checks used to require an exact name ("MobCharacter"), which breaks the moment a game update
                // reparents mobs under a renamed or new intermediate base class (e.g. a seasonal variant base).
                // Matching the substring anywhere in the chain, the same way ChestActor is already matched, is far
                // more resilient and is still effectively certain to only hit real mob classes. The walk also now
                // goes one class further (20 instead of 14) in case a patch added an extra base class in between.
                for (int d = 0; d < 20 && UE4::plausible(cc); ++d, cc = ue_->rd64(cc + 0x40)) {
                    const std::string n = d == 0 ? cn : ue_->objectName(cc);
                    if (contains(n, "ChestActor")) { k = 1; break; }
                    if (contains(n, "MobCharacter")) { k = 2; break; }
                }
                kit = kinds.emplace(cls, k).first;
            }
            const int actorKind = kit->second;
            if (!wantInv && !wantExpl && !wantCam && !wantArm && !wantStruct && !wantQuest && !actorKind) continue;
            const std::string nm = ue_->nameAt(nameIdx);
            if (wantStruct) {
                if (nm == "SlotEntry") slotStruct = o;
                else if (nm == "InventoryEntry") entryStruct = o;
                continue;
            }
            if (contains(nm, "Default__") || startsWith(nm, "REINST") || startsWith(nm, "SKEL")) continue;
            if (actorKind && !wantInv && !wantExpl && !wantCam && !wantArm) {
                if (actors_.size() < 20000) actors_.push_back({o, actorKind, nm});
                continue;
            }
            ObjRef r{o, outer, cn, nm};
            if (wantQuest) { if (quests_.size() < 5000) quests_.push_back(r); continue; }
            if (wantInv) inv_.push_back(r);
            else if (wantExpl) expl_.push_back(r);
            else if (wantArm) arms_.push_back(r);
            else cams_.push_back(r);
        }
    }
    auto sizeOf = [&](uint64_t st, uint32_t fallback) {
        uint32_t sz = 0;
        if (UE4::plausible(st) && ue_->rd(st + 0x58, sz) && sz >= 0x20 && sz <= 0x400 && sz % 8 == 0) return sz;
        return fallback;
    };
    slotSize_ = sizeOf(slotStruct, 0x50);
    entrySize_ = sizeOf(entryStruct, 0xE0);
    scanned_ = true;
    return true;
}

// ------------------------------------------------------------------ missions
const char* Mcd2::questStateName(int s) {
    switch (s) {
    case 0: return "Not set";
    case 1: return "Locked";
    case 2: return "Active";
    case 3: return "Available";
    case 4: return "Completed";
    case 5: return "Failed";
    case 6: return "Unavailable";
    default: return "?";
    }
}

bool Mcd2::quests(std::vector<QuestRow>& out, std::string& note) {
    out.clear();
    Player p;
    if (!player(p)) { note = "Player not found - load into a level first."; return false; }
    if (!scanned_) { note = "Press Scan missions to look through the game."; return false; }
    for (auto& q : quests_) {
        UE4::Prop st, idp, refp;
        if (!ue_->find(q.obj, "State", st)) continue;
        uint8_t state = 0;
        if (!ue_->rd(q.obj + st.offset, state) || state > 7) continue;
        QuestRow r;
        r.obj = q.obj;
        r.state = state;
        if (ue_->find(q.obj, "ID", idp)) {                                   // FString: pointer + length (with the terminator)
            const uint64_t chars = ue_->rd64(q.obj + idp.offset);
            int32_t n = 0;
            ue_->rd(q.obj + idp.offset + 8, n);
            if (UE4::plausible(chars) && n > 1 && n < 200) {
                std::vector<uint16_t> w((size_t)n, 0);
                if (ue_->proc()->read(chars, w.data(), (size_t)n * 2))
                    for (int k = 0; k < n - 1; ++k) r.id += (w[k] >= 32 && w[k] < 127) ? (char)w[k] : '?';
            }
        }
        if (ue_->find(q.obj, "ReferenceQuest", refp)) {                      // the quest descriptor holds the readable name
            const uint64_t desc = ue_->rd64(q.obj + refp.offset);
            UE4::Prop np;
            if (UE4::plausible(desc) && ue_->find(desc, "Name", np)) {
                uint32_t ni = 0;
                if (ue_->rd(desc + np.offset, ni)) r.name = ue_->nameAt(ni);
            }
        }
        if (r.name.empty()) r.name = r.id.empty() ? q.name : r.id;
        out.push_back(r);
    }
    note = "Quests found: " + std::to_string(out.size());
    return true;
}

bool Mcd2::setQuestState(uint64_t obj, const std::string& expectId, int state) {
    if (!UE4::plausible(obj) || state < 1 || state > 6) return false;
    UE4::Prop st, idp;
    if (!ue_->find(obj, "State", st)) return false;
    if (!expectId.empty() && ue_->find(obj, "ID", idp)) {                    // same check as when listing: the ID must still match
        const uint64_t chars = ue_->rd64(obj + idp.offset);
        int32_t n = 0;
        ue_->rd(obj + idp.offset + 8, n);
        std::string id;
        if (UE4::plausible(chars) && n > 1 && n < 200) {
            std::vector<uint16_t> w((size_t)n, 0);
            if (ue_->proc()->read(chars, w.data(), (size_t)n * 2))
                for (int k = 0; k < n - 1; ++k) id += (w[k] >= 32 && w[k] < 127) ? (char)w[k] : '?';
        }
        if (id != expectId) return false;
    }
    const uint8_t v = (uint8_t)state;
    return ue_->proc()->write(obj + st.offset, &v, 1);
}

std::string Mcd2::regionName() {
    Player p;
    if (!ok() || !player(p) || !UE4::plausible(p.world)) return {};
    if (mapsWorld_ != p.world) { mapsWorld_ = p.world; mapsSub_ = 0; }
    if (mapsSub_) {                                                          // still the same live object?
        const std::string cn = ue_->objectName(ue_->rd64(mapsSub_ + 0x10));
        if (cn != "ClientMapsSubsystem" && cn != "MapsSubsystem") mapsSub_ = 0;
    }
    if (!mapsSub_) {                                                         // find the maps subsystem once per world
        UE4::ObjArray oa;
        std::string err;
        if (!ue_->objectArray(oa, err)) return {};
        std::vector<uint8_t> items((size_t)oa.perChunk * 0x18);
        int32_t index = 0;
        std::map<uint64_t, bool> good;
        uint64_t fallback = 0;
        for (int32_t c = 0; c < oa.numChunks && index < oa.num && !mapsSub_; ++c) {
            const uint64_t chunk = ue_->rd64(oa.chunks + (uint64_t)c * 8);
            if (!UE4::plausible(chunk)) { index += oa.perChunk; continue; }
            const int32_t inChunk = std::min(oa.perChunk, oa.num - index);
            if (!ue_->proc()->read(chunk, items.data(), (size_t)inChunk * 0x18)) { index += inChunk; continue; }
            for (int32_t i = 0; i < inChunk; ++i, ++index) {
                uint64_t o;
                std::memcpy(&o, &items[(size_t)i * 0x18], 8);
                if (!UE4::plausible(o)) continue;
                const uint64_t cls = ue_->rd64(o + 0x10);
                if (!UE4::plausible(cls)) continue;
                auto it = good.find(cls);
                if (it == good.end()) {
                    const std::string cn = ue_->objectName(cls);
                    it = good.emplace(cls, cn == "ClientMapsSubsystem" || cn == "MapsSubsystem").first;
                }
                if (!it->second) continue;
                if (ue_->nameAt((uint32_t)ue_->rd64(o + 0x18)).compare(0, 9, "Default__") == 0) continue;
                if (ue_->rd64(o + 0x20) != p.world) { if (!fallback) fallback = o; continue; }   // prefer the one that belongs to this world
                mapsSub_ = o;
                break;
            }
        }
        if (!mapsSub_) mapsSub_ = fallback;
        if (!mapsSub_) return {};
    }
    UE4::Prop pr;
    uint64_t off = 0x88;
    if (ue_->find(mapsSub_, "PlayerRegions", pr)) off = pr.offset;
    const uint64_t data = ue_->rd64(mapsSub_ + off);
    int32_t num = 0;
    ue_->rd(mapsSub_ + off + 8, num);
    if (!UE4::plausible(data) || num <= 0 || num > 64) { return {}; }
    // The element layout (key size) is not known, so look through its first bytes for a region tag.
    uint8_t buf[0x100];
    if (!ue_->proc()->read(data, buf, sizeof buf)) return {};
    for (size_t i = 0; i + 4 <= sizeof buf; i += 4) {
        uint32_t idx;
        std::memcpy(&idx, buf + i, 4);
        if (idx == 0 || idx > 0x3FFFFFF) continue;
        const std::string n = ue_->nameAt(idx);
        if (n.compare(0, 10, "SW.Region.") == 0) return n.substr(10);
    }
    return {};
}

int Mcd2::unlockQuests(std::string& note) {
    std::vector<QuestRow> rows;
    if (!quests(rows, note)) return -1;
    int changed = 0;
    for (auto& r : rows) {
        if (r.state != 1 && r.state != 6) continue;
        UE4::Prop st;
        if (!ue_->find(r.obj, "State", st)) continue;
        uint8_t cur = 0;
        if (!ue_->rd(r.obj + st.offset, cur) || (cur != 1 && cur != 6)) continue;     // re-check right before writing
        const uint8_t avail = 3;
        if (ue_->proc()->write(r.obj + st.offset, &avail, 1)) ++changed;
    }
    return changed;
}

// ------------------------------------------------------------------ tags and the two raw structures
std::string Mcd2::tagAt(uint64_t addr) {
    uint32_t idx = 0;
    if (!ue_->rd(addr, idx)) return {};
    return ue_->nameAt(idx);
}

bool Mcd2::tagList(uint64_t arrAddr, std::vector<std::string>& out) {
    out.clear();
    const uint64_t data = ue_->rd64(arrAddr);
    int32_t num = 0;
    ue_->rd(arrAddr + 8, num);
    if (!UE4::plausible(data) || num < 0 || num > 5000) return num == 0;
    for (int32_t i = 0; i < num; ++i) {
        const std::string t = tagAt(data + (uint64_t)i * 8);
        if (!t.empty()) out.push_back(t);
    }
    return true;
}

bool Mcd2::inventory(std::vector<InvRow>& rows, std::string& note) {
    rows.clear();
    Player p;
    if (!player(p)) { note = "Player not found - load into a level first."; return false; }
    if (!scanned_) { note = "Press Refresh to scan the game for your inventory."; return false; }
    // the player's own inventory component, else the one with the most slots
    const ObjRef* pick = nullptr;
    int bestCount = -1;
    UE4::Prop pr;
    for (auto& r : inv_) {
        if (!ue_->find(r.obj, "ReplicatedItems", pr)) continue;
        int32_t n = 0;
        ue_->rd(r.obj + pr.offset + 0x108 + 8, n);
        const bool mine = ownedBy(r.obj, p.pawn) || ownedBy(r.obj, p.controller) || ownedBy(r.obj, p.state);
        const int score = (mine ? 100000 : 0) + n;
        if (score > bestCount) { bestCount = score; pick = &r; }
    }
    if (!pick) { note = "No inventory component found (" + std::to_string(inv_.size()) + " candidates)."; return false; }
    if (!ue_->find(pick->obj, "ReplicatedItems", pr)) { note = "ReplicatedItems not found."; return false; }
    const uint64_t cont = pick->obj + pr.offset;
    const uint64_t slots = ue_->rd64(cont + 0x108);
    int32_t slotNum = 0;
    ue_->rd(cont + 0x108 + 8, slotNum);
    if (!UE4::plausible(slots) || slotNum < 1 || slotNum > 512) { note = "Inventory list is empty or unreadable (slots: " + std::to_string(slotNum) + ")."; return false; }
    for (int32_t s = 0; s < slotNum; ++s) {
        const uint64_t sl = slots + (uint64_t)s * slotSize_;
        const std::string slotName = tagAt(sl + 0x0C);
        const uint64_t ents = ue_->rd64(sl + 0x40);
        int32_t en = 0;
        ue_->rd(sl + 0x48, en);
        if (!UE4::plausible(ents) || en < 1 || en > 2048) continue;
        for (int32_t e = 0; e < en; ++e) {
            const uint64_t ea = ents + (uint64_t)e * entrySize_;
            InvRow r;
            r.slot = slotName;
            r.addr = ea;
            r.item = tagAt(ea + 0x00);
            r.rarity = tagAt(ea + 0x14);
            ue_->rd(ea + 0xC0, r.stack);
            r.equipped = tagAt(ea + 0xC4);
            int32_t lvl = 0;
            ue_->rd(ea + 0x68, lvl);
            r.level = lvl;
            ue_->rd(ea + 0x5C, r.powerBase);                    // ItemData.GeneratorData.PowerGeneratorValues.ItemPowerOriginal
            ue_->rd(ea + 0x60, r.power);                        // ... .ItemPower
            if (!std::isfinite(r.power)) r.power = 0;
            if (!std::isfinite(r.powerBase)) r.powerBase = 0;
            ue_->rd(ea + 0x6C, r.xp);                           // ItemData.ItemProgression.CurrentXP
            if (!std::isfinite(r.xp)) r.xp = 0;
            uint8_t rr = 0;
            ue_->rd(ea + 0x30, rr);                             // ItemData.EffectRerolls
            r.rerolls = rr;
            tagList(ea + 0x80, r.props);                        // ItemData.DynamicPropertyTags (first array of the tag container)
            auto readEffects = [&](uint64_t arr, int group) {
                const uint64_t fx = ue_->rd64(arr);
                int32_t fn = 0;
                ue_->rd(arr + 8, fn);
                if (!UE4::plausible(fx) || fn <= 0 || fn >= 64) return;
                for (int32_t k = 0; k < fn; ++k) {
                    const uint64_t f = fx + (uint64_t)k * 0x18;
                    float inten = 0;
                    uint8_t pts = 0;
                    ue_->rd(f + 8, inten);
                    ue_->rd(f + 0xC, pts);
                    char b[64];
                    std::snprintf(b, sizeof b, " x%.2f (%d pts)", inten, (int)pts);
                    const std::string t = tagAt(f);
                    r.effects += std::string(r.effects.empty() ? "" : "; ") + (group >= 0 ? "L" + std::to_string(group + 1) + ": " : std::string()) + t + b;
                    EffRow er;
                    er.addr = f; er.tag = t; er.intensity = inten; er.points = pts; er.group = group;
                    r.fx.push_back(er);
                }
            };
            readEffects(ea + 0x20, -1);
            {   // ItemProgression.ItemLevels: one collection (0x30 bytes) per item level, each with its own effect array
                const uint64_t lv = ue_->rd64(ea + 0x70);
                int32_t ln = 0;
                ue_->rd(ea + 0x78, ln);
                if (UE4::plausible(lv) && ln > 0 && ln <= 64)
                    for (int32_t g = 0; g < ln; ++g) readEffects(lv + (uint64_t)g * 0x30, g);
            }
            if (r.item.empty()) continue;
            rows.push_back(r);
        }
    }
    note = "Slots: " + std::to_string(slotNum) + ", items: " + std::to_string(rows.size()) +
           " (slot struct " + std::to_string(slotSize_) + " bytes, entry " + std::to_string(entrySize_) + " bytes)";
    return true;
}

static bool writeTag(UE4& ue, uint32_t idx, uint64_t addr) {
    const uint32_t v[2] = {idx, 0};                          // FName: index, number
    return ue.proc()->write(addr, v, sizeof v);
}

bool Mcd2::editItem(uint64_t addr, const std::string& expect, const ItemEdit& e, std::string& err) {
    if (!UE4::plausible(addr) || expect.empty()) { err = "No item selected."; return false; }
    if (tagAt(addr) != expect) { err = "The inventory changed since it was listed. Press Refresh and select the item again."; return false; }
    bool any = false;
    if (e.level >= 0) { int32_t v = e.level; any |= ue_->proc()->write(addr + 0x68, &v, sizeof v); }
    if (e.power >= 0) {
        const float v = (float)e.power;                      // both copies, so the game's recalculation keeps it
        any |= ue_->proc()->write(addr + 0x5C, &v, sizeof v);
        any |= ue_->proc()->write(addr + 0x60, &v, sizeof v);
    }
    if (e.stack >= 0) { int32_t v = e.stack; any |= ue_->proc()->write(addr + 0xC0, &v, sizeof v); }
    if (e.xp >= 0) { const float v = (float)e.xp; any |= ue_->proc()->write(addr + 0x6C, &v, sizeof v); }
    if (e.rerolls >= 0 && e.rerolls < 256) { const uint8_t v = (uint8_t)e.rerolls; any |= ue_->proc()->write(addr + 0x30, &v, 1); }
    if (!e.rarity.empty()) {
        const uint32_t idx = findNameIndex(e.rarity);
        if (idx == 0xFFFFFFFFu) { err = "The game has no name '" + e.rarity + "'."; return false; }
        any |= writeTag(*ue_, idx, addr + 0x14);
    }
    if (!e.item.empty() && e.item != expect) {
        const uint32_t idx = findNameIndex(e.item);
        if (idx == 0xFFFFFFFFu) { err = "The game has no item named '" + e.item + "'."; return false; }
        any |= writeTag(*ue_, idx, addr);
    }
    if (!any) err = "Nothing was written.";
    return any;
}

bool Mcd2::editEffect(uint64_t entryAddr, const std::string& expect, uint64_t f, const std::string& tag, float intensity, int points, std::string& err) {
    if (!UE4::plausible(entryAddr) || tagAt(entryAddr) != expect) { err = "The inventory changed. Press Refresh and select the item again."; return false; }
    // the effect must still sit inside one of this item's effect arrays
    auto inside = [&](uint64_t arr) {
        const uint64_t fx = ue_->rd64(arr);
        int32_t n = 0;
        ue_->rd(arr + 8, n);
        return UE4::plausible(fx) && n > 0 && n < 64 && f >= fx && f < fx + (uint64_t)n * 0x18 && (f - fx) % 0x18 == 0;
    };
    bool found = inside(entryAddr + 0x20);
    if (!found) {
        const uint64_t lv = ue_->rd64(entryAddr + 0x70);
        int32_t ln = 0;
        ue_->rd(entryAddr + 0x78, ln);
        if (UE4::plausible(lv) && ln > 0 && ln <= 64)
            for (int32_t g = 0; g < ln && !found; ++g) found = inside(lv + (uint64_t)g * 0x30);
    }
    if (!found) { err = "That effect no longer exists. Press Refresh and select the item again."; return false; }
    bool any = false;
    if (!tag.empty() && tag != tagAt(f)) {
        const uint32_t idx = findNameIndex(tag);
        if (idx == 0xFFFFFFFFu) { err = "The game has no name '" + tag + "'."; return false; }
        any |= writeTag(*ue_, idx, f);
    }
    // only touch what actually changes: fewer writes into live game data means fewer chances to upset it
    float curI = 0;
    uint8_t curP = 0;
    ue_->rd(f + 8, curI);
    ue_->rd(f + 0xC, curP);
    if (std::isfinite(intensity) && std::fabs(intensity - curI) > 1e-6f) any |= ue_->proc()->write(f + 8, &intensity, sizeof intensity);
    if (points >= 0 && points < 256 && (uint8_t)points != curP) { uint8_t b = (uint8_t)points; any |= ue_->proc()->write(f + 0xC, &b, 1); }
    if (!any) err = "Nothing was written.";
    return any;
}

std::vector<std::string> Mcd2::namesWithPrefix(const std::string& prefix, size_t cap) {
    std::vector<std::string> out;
    if (!ue_->modernNames()) return out;
    const uint64_t var = ue_->namesVar();
    std::vector<uint8_t> buf(0x20000 + 4);
    for (uint32_t block = 0; block < 512 && out.size() < cap; ++block) {
        const uint64_t base = ue_->rd64(var + 0x10 + (uint64_t)block * 8);
        if (!UE4::plausible(base)) break;
        if (!ue_->proc()->read(base, buf.data(), 0x20000)) continue;
        uint32_t off = 0;
        while (off + 2 <= 0x20000 && out.size() < cap) {
            uint16_t hd;
            std::memcpy(&hd, &buf[off], 2);
            const uint32_t len = hd >> 6;
            if (len == 0) break;
            const uint32_t bytes = len * ((hd & 1) ? 2 : 1);
            if (!(hd & 1) && len >= prefix.size() && off + 2 + bytes <= 0x20000 && std::memcmp(&buf[off + 2], prefix.data(), prefix.size()) == 0)
                out.emplace_back((const char*)&buf[off + 2], len);
            off += (2 + bytes + 1) & ~1u;
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool Mcd2::world(WorldInfo& w, std::string& note) {
    w = WorldInfo();
    Player p;
    if (!player(p)) { note = "Player not found - load into a level first."; return false; }
    w.map = worldName();
    if (!scanned_) { note = "Press Refresh to scan the game."; return false; }
    const ObjRef* pick = nullptr;
    for (auto& r : expl_)
        if (!pick || ownedBy(r.obj, p.pawn) || ownedBy(r.obj, p.controller) || ownedBy(r.obj, p.state)) pick = &r;
    if (!pick) { note = "No world exploration component found."; return false; }
    UE4::Prop pr;
    if (ue_->find(pick->obj, "DiscoveredMinecartStations", pr)) tagList(pick->obj + pr.offset, w.stations);
    if (ue_->find(pick->obj, "ActivatedGimmicks", pr)) tagList(pick->obj + pr.offset, w.gimmicks);
    if (ue_->find(pick->obj, "PlayedCutscenes", pr)) tagList(pick->obj + pr.offset, w.cutscenes);
    if (ue_->find(pick->obj, "DiscoveredDungeonDoors", pr)) {
        int32_t n = 0;
        ue_->rd(pick->obj + pr.offset + 8, n);
        w.doors = n;
    }
    note = "Read from " + pick->name;
    return true;
}

// ------------------------------------------------------------------ small helpers
bool Mcd2::readBool(uint64_t obj, const char* prop, bool& v) {
    UE4::Prop pr;
    if (!UE4::plausible(obj) || !ue_->find(obj, prop, pr)) return false;
    uint8_t b = 0;
    if (!ue_->rd(obj + pr.offset + (pr.isBool ? pr.byteOffset : 0), b)) return false;
    v = (pr.isBool && pr.mask) ? (b & pr.mask) != 0 : b != 0;
    return true;
}

bool Mcd2::writeBool(uint64_t obj, const char* prop, bool v) {
    UE4::Prop pr;
    if (!UE4::plausible(obj) || !ue_->find(obj, prop, pr)) return false;
    const uint64_t a = obj + pr.offset + (pr.isBool ? pr.byteOffset : 0);
    uint8_t b = 0;
    if (!ue_->rd(a, b)) return false;
    if (pr.isBool && pr.mask) b = v ? (b | pr.mask) : (b & ~pr.mask); else b = v ? 1 : 0;
    return ue_->proc()->write(a, &b, 1);
}

bool Mcd2::worldSettings(uint64_t& ws) {
    Player p;
    uint64_t level = 0;
    if (!player(p) || !UE4::plausible(p.world)) return false;
    if (ue_->readPtr(p.world, "PersistentLevel", level) && ue_->readPtr(level, "WorldSettings", ws)) return true;
    return ue_->readPtr(p.world, "WorldSettings", ws);
}

bool Mcd2::gameState(uint64_t& gs) {
    Player p;
    return player(p) && UE4::plausible(p.world) && ue_->readPtr(p.world, "GameState", gs);
}

// ------------------------------------------------------------------ fly and time scale
bool Mcd2::getMoveMode(int& mode) {
    Player p;
    uint64_t mv = 0, a = 0;
    if (!player(p) || !ue_->readPtr(p.pawn, "CharacterMovement", mv) || !floatAt(mv, "MovementMode", a)) return false;
    uint8_t b = 0;
    if (!ue_->rd(a, b)) return false;
    mode = b;
    return true;
}

bool Mcd2::setMoveMode(int mode) {
    Player p;
    uint64_t mv = 0, a = 0;
    if (!player(p) || !ue_->readPtr(p.pawn, "CharacterMovement", mv) || !floatAt(mv, "MovementMode", a)) return false;
    const uint8_t b = (uint8_t)mode;
    return ue_->proc()->write(a, &b, 1);
}

bool Mcd2::setCollision(bool on) {
    Player p;
    return player(p) && UE4::plausible(p.pawn) && writeBool(p.pawn, "bActorEnableCollision", on);
}

bool Mcd2::getCollision(bool& on) {
    Player p;
    return player(p) && UE4::plausible(p.pawn) && readBool(p.pawn, "bActorEnableCollision", on);
}

bool Mcd2::getTimeScale(float& v) {
    uint64_t ws = 0, a = 0;
    return worldSettings(ws) && floatAt(ws, "TimeDilation", a) && ue_->rd(a, v) && std::isfinite(v);
}

bool Mcd2::setTimeScale(float v) {
    uint64_t ws = 0, a = 0;
    return worldSettings(ws) && floatAt(ws, "TimeDilation", a) && ue_->proc()->write(a, &v, sizeof v);
}

// ------------------------------------------------------------------ difficulty
uint32_t Mcd2::findNameIndex(const std::string& target) {
    auto it = nameIdx_.find(target);
    if (it != nameIdx_.end()) return it->second;
    uint32_t found = 0xFFFFFFFFu;
    if (ue_->modernNames()) {
        const uint64_t var = ue_->namesVar();
        std::vector<uint8_t> buf(0x20000 + 4);
        for (uint32_t block = 0; block < 512 && found == 0xFFFFFFFFu; ++block) {
            const uint64_t base = ue_->rd64(var + 0x10 + (uint64_t)block * 8);
            if (!UE4::plausible(base)) break;
            if (!ue_->proc()->read(base, buf.data(), 0x20000)) continue;
            uint32_t off = 0;
            while (off + 2 <= 0x20000) {
                uint16_t hd;
                std::memcpy(&hd, &buf[off], 2);
                const uint32_t len = hd >> 6;
                if (len == 0) break;
                const uint32_t bytes = len * ((hd & 1) ? 2 : 1);
                if (!(hd & 1) && len == target.size() && off + 2 + bytes <= 0x20000 && std::memcmp(&buf[off + 2], target.data(), len) == 0) {
                    found = (block << 16) | (off / 2);
                    break;
                }
                off += (2 + bytes + 1) & ~1u;
            }
        }
    }
    nameIdx_[target] = found;
    return found;
}

bool Mcd2::getDifficulty(std::string& tag, bool& locked) {
    uint64_t gs = 0;
    UE4::Prop pr;
    if (!gameState(gs) || !ue_->find(gs, "Difficulty", pr)) return false;
    tag = tagAt(gs + pr.offset);
    locked = false;
    readBool(gs, "bDifficultyLocked", locked);
    return !tag.empty();
}

bool Mcd2::setDifficulty(const std::string& tag) {
    uint64_t gs = 0;
    UE4::Prop pr;
    if (!gameState(gs) || !ue_->find(gs, "Difficulty", pr)) return false;
    const uint32_t idx = findNameIndex(tag);
    if (idx == 0xFFFFFFFFu) return false;
    const uint32_t pair[2] = {idx, 0};                          // FName: comparison index + number
    return ue_->proc()->write(gs + pr.offset, pair, sizeof pair);
}

bool Mcd2::setDifficultyLocked(bool locked) {
    uint64_t gs = 0;
    return gameState(gs) && writeBool(gs, "bDifficultyLocked", locked);
}

// ------------------------------------------------------------------ nearby chests and mobs
bool Mcd2::nearby(double radius, bool chests, bool mobs, bool onlyClosed, std::vector<Near>& out, std::string& note) {
    out.clear();
    Player p;
    Vec3 me;
    if (!player(p) || !getLocation(me)) { note = "Player not found - load into a level first."; return false; }
    if (!scanned_) { note = "Press Refresh to scan the game for chests and enemies."; return false; }
    int seen = 0;
    for (auto& a : actors_) {
        if ((a.kind == 1 && !chests) || (a.kind == 2 && !mobs) || a.obj == p.pawn) continue;
        uint64_t root = 0, la = 0;
        if (!ue_->readPtr(a.obj, "RootComponent", root) || !floatAt(root, "RelativeLocation", la)) continue;
        double d[3];
        if (!ue_->proc()->read(la, d, sizeof d) || !std::isfinite(d[0]) || !std::isfinite(d[1]) || !std::isfinite(d[2])) continue;
        ++seen;
        const double dx = d[0] - me.x, dy = d[1] - me.y, dz = d[2] - me.z;
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > radius) continue;
        Near n;
        n.kind = a.kind == 1 ? "Chest" : "Enemy";
        std::string nm = a.name;
        const size_t c = nm.find("_C_");
        if (c != std::string::npos) nm = nm.substr(0, c);
        n.name = nm;
        n.pos = {d[0], d[1], d[2]};
        n.dist = dist;
        if (a.kind == 1) {
            bool opened = false, discovered = false;
            const bool haveOpen = readBool(a.obj, "bOpened", opened);
            readBool(a.obj, "bDiscovered", discovered);
            if (onlyClosed && haveOpen && opened) continue;
            n.state = haveOpen ? std::string(opened ? "Opened" : "Closed") + (discovered ? ", discovered" : "") : std::string("?");
        }
        out.push_back(n);
    }
    std::sort(out.begin(), out.end(), [](const Near& x, const Near& y) { return x.dist < y.dist; });
    if (out.size() > 400) out.resize(400);
    note = "Found " + std::to_string(actors_.size()) + " chests and enemies in the level, " + std::to_string(seen) +
           " with a readable position; " + std::to_string(out.size()) + " shown within " + std::to_string((int)radius) +
           " units (Chests / Enemies / Only unopened chests above can filter some out).";
    return true;
}

// ------------------------------------------------------------------ nearest blueprint (Camera & Display)
bool Mcd2::nearestEnemy(double radius, std::string& name, double& dist) {
    if (!ok()) return false;
    const uint64_t now = nowMs();
    if (blueprintScanAt_ == 0 || now - blueprintScanAt_ > 3000) {
        blueprintScanAt_ = now;
        std::string err;
        rescan(err);        // self-contained, the same periodic self-rescan Damage per second already does - no Refresh needed
    }
    std::vector<Near> out;
    std::string note;
    if (!nearby(radius, false, true, false, out, note) || out.empty()) return false;
    name = out.front().name;
    dist = out.front().dist;
    return true;
}

// ------------------------------------------------------------------ display mode overlay (Camera editor)
bool Mcd2::overlayStats(OverlayStats& s) {
    s = OverlayStats();
    Player p;
    if (!player(p)) return false;
    float v;
    if (getAttr(0, v)) { s.health = v; s.ok = true; }
    if (getAttr(1, v)) s.healthMax = v;
    for (int i = 0; i < (int)attrDefs().size(); ++i) {
        const std::string label = attrDefs()[i].label;
        if (label == "Base damage multiplier" && getAttr(i, v)) { s.dmgMult = v; s.ok = true; }
        else if (label == "Critical hit chance" && getAttr(i, v)) { s.critChance = v; s.ok = true; }
        else if (label == "Emeralds" && getAttr(i, v)) { s.emeralds = v; s.ok = true; }
        else if (label == "Spring stones" && getAttr(i, v)) { s.echoShards = v; s.ok = true; }      // Echo Shards, the game's own name for this currency
        else if (label == "Enchantment points" && getAttr(i, v)) { s.enchantPoints = v; s.ok = true; }
        else if (label == "Souls" && getAttr(i, v)) { s.souls = v; s.ok = true; }
    }
    std::vector<QuestRow> qs;
    std::string qnote;
    if (quests(qs, qnote)) {
        for (const auto& q : qs) {
            if (q.state != 2) continue;      // 2 = Active
            s.questName = q.name.empty() ? q.id : q.name;
            s.questState = questStateName(q.state);
            s.ok = true;
            break;
        }
    }
    return s.ok;
}

bool Mcd2::gameWindowRect(long& x, long& y, long& w, long& h) {
    if (!ok() || !ue_->proc()) return false;
    return ue_->proc()->mainWindowRect(x, y, w, h);
}

// ------------------------------------------------------------------ damage per second (Camera & Display)
void Mcd2::dpsReset() {
    dpsEnemies_.clear();
    dpsScanAt_ = 0;
}

double Mcd2::dpsPoll() {
    if (!ok()) return 0;
    const uint64_t now = nowMs();
    if (dpsScanAt_ == 0 || now - dpsScanAt_ > 3000) {
        dpsScanAt_ = now;
        std::string err;
        rescan(err);        // repopulates actors_ (kind 2 = mob), the same full object scan Nearby Viewer's own auto-refresh already does
        std::vector<DpsEnemy> fresh;
        for (const auto& a : actors_) {
            if (a.kind != 2 || fresh.size() >= 400) continue;
            if (!UE4::plausible(a.obj)) continue;
            uint64_t asc = 0, data = 0;
            int32_t num = 0;
            if (!ue_->readPtr(a.obj, "AbilitySystemComponent", asc) || !UE4::plausible(asc) || !ue_->readArray(asc, "SpawnedAttributes", data, num)) continue;
            for (int i = 0; i < num && i < 96; ++i) {
                const uint64_t so = ue_->rd64(data + (uint64_t)i * 8);
                if (!UE4::plausible(so)) continue;
                if (!startsWith(ue_->className(so), "ATR_Health")) continue;
                UE4::Prop pr;
                if (!ue_->find(so, "Health", pr)) break;
                DpsEnemy e;
                e.obj = a.obj;
                e.cls = ue_->rd64(a.obj + 0x10);
                e.addr = so + pr.offset + 0xC;        // CurrentValue (BaseValue is at +0x8)
                for (const auto& old : dpsEnemies_)    // keep the last reading for an enemy we already knew, so a rescan alone never looks like damage
                    if (old.obj == e.obj && old.addr == e.addr) { e.last = old.last; e.primed = old.primed; break; }
                fresh.push_back(e);
                break;
            }
        }
        dpsEnemies_ = std::move(fresh);
    }
    double dealt = 0;
    for (auto& e : dpsEnemies_) {
        if (!UE4::plausible(e.obj) || ue_->rd64(e.obj + 0x10) != e.cls) { e.primed = false; continue; }      // gone or reused
        float cur = 0;
        if (!ue_->rd(e.addr, cur) || !std::isfinite(cur) || cur < 0.f || cur > 1e8f) { e.primed = false; continue; }
        if (e.primed && cur < e.last - 1e-4f) dealt += (double)e.last - (double)cur;
        e.last = cur;
        e.primed = true;
    }
    return dealt;
}

// ------------------------------------------------------------------ deaths this session (Camera & Display)
void Mcd2::deathReset() {
    deathCount_ = 0;
    deathPrimed_ = false;
    deathLastHealth_ = 0;
}

int Mcd2::deathPoll() {
    if (!ok()) return deathCount_;
    // First choice: a real session-wide counter straight from the game's own GameState, the same kind Minecraft
    // Dungeons exposes as LivesLostThisSession - tried under a few plausible names in case Minecraft Dungeons II
    // kept an equivalent one under a different name. None of these are confirmed to exist here; if none match,
    // this silently falls through to counting Health hitting zero ourselves instead.
    uint64_t gs = 0;
    if (gameState(gs)) {
        static const char* const kNames[] = {"LivesLostThisSession", "LivesLost", "DeathsThisSession", "PlayerDeaths", "TimesDowned"};
        for (const char* name : kNames) {
            UE4::Prop pr;
            int32_t v = 0;
            if (ue_->find(gs, name, pr) && ue_->rd(gs + pr.offset, v) && v >= 0 && v < 100000) return v;
        }
    }
    float v;
    if (getAttr(0, v) && std::isfinite(v)) {
        if (deathPrimed_ && v <= 0.f && deathLastHealth_ > 0.f) ++deathCount_;
        deathLastHealth_ = v;
        deathPrimed_ = true;
    }
    return deathCount_;
}

// ------------------------------------------------------------------ deleting inventory entries
int Mcd2::deleteItems(const std::vector<DeleteTarget>& targets, std::string& err) {
    Player p;
    if (!player(p)) { err = "Player not found - load into a level first."; return -1; }
    if (!scanned_) { err = "Press Refresh to scan the game for your inventory."; return -1; }
    if (targets.empty()) { err = "Select an item first."; return -1; }
    const ObjRef* pick = nullptr;
    int bestCount = -1;
    UE4::Prop pr;
    for (auto& r : inv_) {
        if (!ue_->find(r.obj, "ReplicatedItems", pr)) continue;
        int32_t n = 0;
        ue_->rd(r.obj + pr.offset + 0x108 + 8, n);
        const bool mine = ownedBy(r.obj, p.pawn) || ownedBy(r.obj, p.controller) || ownedBy(r.obj, p.state);
        const int score = (mine ? 100000 : 0) + n;
        if (score > bestCount) { bestCount = score; pick = &r; }
    }
    if (!pick || !ue_->find(pick->obj, "ReplicatedItems", pr)) { err = "No inventory component found."; return -1; }
    const uint64_t cont = pick->obj + pr.offset;
    const uint64_t slots = ue_->rd64(cont + 0x108);
    int32_t slotNum = 0;
    ue_->rd(cont + 0x108 + 8, slotNum);
    if (!UE4::plausible(slots) || slotNum < 1 || slotNum > 512 || entrySize_ < 0x20) { err = "Inventory list is empty or unreadable."; return -1; }
    int removed = 0;
    for (int32_t s = 0; s < slotNum; ++s) {
        const uint64_t sl = slots + (uint64_t)s * slotSize_;
        const uint64_t ents = ue_->rd64(sl + 0x40);
        int32_t en = 0;
        ue_->rd(sl + 0x48, en);
        if (!UE4::plausible(ents) || en < 1 || en > 2048) continue;
        std::vector<char> drop((size_t)en, 0);
        int k = 0;
        for (const auto& t : targets) {
            if (t.addr < ents || t.addr >= ents + (uint64_t)en * entrySize_) continue;
            if ((t.addr - ents) % entrySize_ != 0) continue;
            const int32_t idx = (int32_t)((t.addr - ents) / entrySize_);
            if (drop[(size_t)idx] || tagAt(t.addr) != t.expect) continue;      // the list moved: never delete something else
            drop[(size_t)idx] = 1;
            ++k;
        }
        if (!k) continue;
        std::vector<uint8_t> block((size_t)en * entrySize_);
        if (!ue_->proc()->read(ents, block.data(), block.size())) { err = "Could not read the inventory."; return removed; }
        std::vector<uint8_t> out(block.size(), 0);
        size_t w = 0;
        for (int32_t i = 0; i < en; ++i) {
            if (drop[(size_t)i]) continue;
            std::memcpy(&out[w * entrySize_], &block[(size_t)i * entrySize_], entrySize_);
            ++w;
        }
        if (!ue_->proc()->write(ents, out.data(), out.size())) { err = "Could not write the inventory."; return removed; }
        const int32_t newNum = (int32_t)w;
        if (!ue_->proc()->write(sl + 0x48, &newNum, 4)) { err = "Could not update the item count."; return removed; }
        removed += k;
    }
    if (removed == 0) err = "Nothing was deleted. Press Refresh and select the items again.";
    return removed;
}

} // namespace dk
