#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../core/Hook.h"

namespace dk {

class NoClipMod;
class GodModeMod;
class StatsMod;
class NoTargetMod;
class OverkillMod;
class WasdMod;
class TimerMod;
class ItemViewerMod;
class PauseMod;
class UE4;

// ---------------------------------------------------------------- base
class Mod {
public:
    virtual ~Mod() = default;
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;

    bool enabled() const { return on_; }
    bool enable(const Process& p, std::string& err);
    void disable(const Process& p);         // safe to call after the game exited (just drops state)

    // A short, honest note on how this mod is actually wired into the game once it is on - not the exact
    // source, but the real mechanism (a byte patch, a code hook, or a direct read/write through the game's
    // own reflection tables) plus whatever real address this run found. Empty if there is nothing to show yet.
    virtual std::string technique() const { return {}; }

protected:
    virtual bool onEnable(const Process& p, std::string& err) = 0;
    virtual void onDisable(const Process& p) = 0;

    // Scan the main module for `pattern`; first hit wins (same as aobscanmodule).
    static bool findOne(const Process& p, const char* pattern, uint64_t& addr, std::string& err);

private:
    bool on_ = false;
};

// ---------------------------------------------------------------- simple toggles
// Replaces N bytes at the pattern match with NOPs (Infinite Arrows / Infinite Lives).
class NopMod : public Mod {
public:
    NopMod(std::string id, std::string name, std::string desc, const char* pattern, size_t len)
        : id_(std::move(id)), name_(std::move(name)), desc_(std::move(desc)), pat_(pattern), len_(len) {}
    std::string id() const override { return id_; }
    std::string name() const override { return name_; }
    std::string description() const override { return desc_; }
    std::string technique() const override { return patch_.describe(); }
protected:
    bool onEnable(const Process& p, std::string& err) override;
    void onDisable(const Process& p) override { patch_.remove(p); }
private:
    std::string id_, name_, desc_; const char* pat_; size_t len_;
    Patch patch_;
};

class InfiniteEnchantMod : public Mod {
public:
    std::string id() const override { return "inf_enchant"; }
    std::string name() const override { return "Infinite Enchant Points"; }
    std::string description() const override { return "Enchantment points are never used up. Picks the 1.17 hook, or the 1.12+ hooks on older builds."; }
    std::string technique() const override;
protected:
    bool onEnable(const Process& p, std::string& err) override;
    void onDisable(const Process& p) override { for (auto& h : hooks_) h.remove(p); hooks_.clear(); }
private:
    std::vector<Hook> hooks_;
};

class SlotCooldownMod;
class NoCooldownMod : public Mod {
public:
    explicit NoCooldownMod(std::shared_ptr<UE4> ue);
    ~NoCooldownMod() override;
    std::string note() const;
    std::string id() const override { return "no_cooldown"; }
    std::string name() const override { return "No Cooldown"; }
    std::string description() const override { return "Artifact cooldowns never start."; }
    std::string technique() const override;
protected:
    bool onEnable(const Process& p, std::string& err) override;
    void onDisable(const Process& p) override;
private:
    Hook hook_;
    std::unique_ptr<SlotCooldownMod> fallback_;
};

// ---------------------------------------------------------------- pointer-capture mods
class CaptureMod : public Mod {
public:
    // Object pointer captured by the hook (0 until the game executes the hooked code). Virtual so a subclass
    // (ItemEditorMod's Modern Mode) can redirect it to an address found through the reflection tables instead.
    virtual uint64_t object(const Process& p) const;
    std::string technique() const override {
        return hook_.describe() + " The reserved slot it fills in is the object this mod then reads and writes - "
               "for example the player's own stats object, or whichever item was last highlighted.";
    }
protected:
    void onDisable(const Process& p) override { hook_.remove(p); }
    Hook hook_;
};

class CurrencyMod : public CaptureMod {
public:
    enum Kind { Emerald, Gold, EnderEyes };
    std::string id() const override { return "currency"; }
    std::string name() const override { return "Currency Editor"; }
    std::string description() const override { return "Read/write emeralds, gold and Ender Eyes. Open the inventory or pick up an emerald to link it."; }
    bool get(const Process& p, Kind k, int32_t& v) const;
    bool set(const Process& p, Kind k, int32_t v) const;
protected:
    bool onEnable(const Process& p, std::string& err) override;
};

class XpMod : public CaptureMod {
public:
    std::string id() const override { return "xp"; }
    std::string name() const override { return "XP Multiplier"; }
    std::string description() const override { return "Multiplies all XP gained. Also lets you read/write total XP."; }
    bool setMultiplier(const Process& p, int64_t m);
    bool total(const Process& p, int32_t& v) const;
    bool setTotal(const Process& p, int32_t v) const;
    int64_t multiplier() const { return mult_; }
protected:
    bool onEnable(const Process& p, std::string& err) override;
private:
    int64_t mult_ = 1;
};

class ThreatMod : public CaptureMod {
public:
    std::string id() const override { return "threat"; }
    std::string name() const override { return "Threat Level"; }
    std::string description() const override { return "Read/write the current threat level. Starts a mission to link it."; }
    bool get(const Process& p, int32_t& v) const;
    bool set(const Process& p, int32_t v) const;
protected:
    bool onEnable(const Process& p, std::string& err) override;
};

class PositionMod : public CaptureMod {
public:
    std::string id() const override { return "position"; }
    std::string name() const override { return "Position / Teleport"; }
    std::string description() const override { return "Live X/Y/Z. Save position: F5   Teleport to saved: F6"; }
    bool get(const Process& p, float xyz[3]) const;
    bool set(const Process& p, const float xyz[3]) const;
    bool save(const Process& p);
    bool teleportSaved(const Process& p) const;
    bool hasSaved() const { return hasSaved_; }
protected:
    bool onEnable(const Process& p, std::string& err) override;
private:
    float saved_[3] = {0, 0, 0};
    bool hasSaved_ = false;
};

class ScaleMod : public CaptureMod {
public:
    std::string id() const override { return "scale"; }
    std::string name() const override { return "Model Scale"; }
    std::string description() const override { return "Read/write your model size. Move or change your character to link it."; }
    bool get(const Process& p, float& w, float& s) const;
    bool set(const Process& p, float w, float s) const;
protected:
    bool onEnable(const Process& p, std::string& err) override;
};

class ItemEditorMod : public CaptureMod {
public:
    // `ue` is optional: it only powers Modern Mode (see below). Pass the game's shared UE4 reflection helper so
    // the mode works even on builds (Steam) where Legacy Mode's pattern-scan hook can't find anything to hook.
    explicit ItemEditorMod(std::shared_ptr<UE4> ue = nullptr) : ue_(std::move(ue)) {}
    enum Field { ItemId, Rarity, Gilded, GildedEnchant, GildedLevel, TotalEnchants, MaxEnchants, TotalStats, MaxStats, Cloned };
    static constexpr int kGroups = 15;      // enchantment levels
    static constexpr int kSlots = 3;        // left / middle / right
    static constexpr int kMaxStats = 4096;
    std::string id() const override { return "items"; }
    std::string name() const override { return "Item Editor"; }
    std::string description() const override { return "Edit an item: ID, power, rarity, gilded, enchantments and stats. Legacy Mode: turn on, then highlight an item in your inventory. Modern Mode: turn on, then pick an item from your full inventory list - no need to highlight it in game."; }
    std::string technique() const override;
    bool getInt(const Process& p, Field f, int32_t& v) const;
    bool setInt(const Process& p, Field f, int32_t v) const;
    bool getPower(const Process& p, float& v) const;
    bool setPower(const Process& p, float v) const;
    int enchantCapacity(const Process& p) const;
    bool getEnchant(const Process& p, int group, int slot, int32_t& id, int32_t& tier) const;
    bool setEnchantId(const Process& p, int group, int slot, int32_t id) const;
    bool setEnchantTier(const Process& p, int group, int slot, int32_t tier) const;
    int statCapacity(const Process& p) const;
    bool statInfo(const Process& p, uint64_t& arrayPtr, int32_t& used, int32_t& max) const;
    bool getStat(const Process& p, int i, uint16_t& v) const;
    bool getStats(const Process& p, std::vector<uint16_t>& out) const;
    bool setStat(const Process& p, int i, uint16_t v) const;
    // A preset is everything about an item except its stats / armor properties: ID, power, rarity, gilded (+ enchant, level) and the
    // enchantments with their tiers. It can be saved to a file and applied to another highlighted item.
    struct Preset {
        int32_t id = 0, rarity = 0, gilded = 0, gildedEnchant = 0, gildedLevel = 0;
        float power = 0;
        int levels = 0;                                    // enchant levels the source item had
        int32_t enchId[kGroups][kSlots] = {};
        int32_t enchTier[kGroups][kSlots] = {};
    };
    bool readPreset(const Process& p, Preset& out) const;
    bool applyPreset(const Process& p, const Preset& in, int& levelsWritten) const;     // never writes past the target's own enchant room
    // Reads the item back and counts the values that do not match `want` (only the enchant levels that were written are compared).
    int mismatches(const Process& p, const Preset& want, int levelsWritten) const;
    bool copy(const Process& p);                       // remembers a preset in memory (everything but the stats)
    bool paste(const Process& p, int* levelsWritten = nullptr, int* wrong = nullptr) const;
    bool hasCopy() const { return snapValid_; }

    // ---- Modern Mode: lists and edits any item straight from the game's own reflection tables (character ->
    // ItemStashComponent -> InventorySlots / StorageChestSlots -> item), the same kind of name-based lookup the
    // Minecraft Dungeons II side uses. No byte pattern or hook involved, so unlike Legacy Mode it works the same
    // way on every build, Steam included, and items can be picked from a list instead of highlighted in-game.
    struct InventoryEntry {
        uint64_t itemStruct = 0;   // feed straight into selectModern()
        int32_t itemId = 0;
        int slotIndex = 0;
        bool storage = false;      // false = main inventory slot, true = storage chest slot
    };
    bool modernAvailable() const { return (bool)ue_; }
    bool legacyLinked() const { return hook_.active(); }       // Legacy Mode's hook actually installed on this build
    bool listInventory(const Process& p, std::vector<InventoryEntry>& out, std::string& err) const;
    bool selectModern(const Process& p, uint64_t itemStruct);  // picks one entry from listInventory() as the edit target
    void clearModern() { modernTarget_ = 0; }
    bool modernSelected() const { return modernTarget_ != 0; }
    uint64_t object(const Process& p) const override;          // Modern target wins when selected, else the Legacy hook capture
protected:
    bool onEnable(const Process& p, std::string& err) override;
private:
    bool enchantAddr(const Process& p, int group, int slot, uint64_t& addr) const;
    Preset snap_;
    bool snapValid_ = false;
    std::shared_ptr<UE4> ue_;
    uint64_t modernTarget_ = 0;
};

// Apocalypse level: found through the table's saved pointer chains (static pointers inside the game's modules).
class ApocalypseMod : public Mod {
public:
    explicit ApocalypseMod(std::shared_ptr<UE4> ue) : ue_(std::move(ue)) {}
    std::string id() const override { return "apocalypse"; }
    std::string name() const override { return "Apocalypse Level"; }
    std::string description() const override { return "View and set your Apocalypse level. Uses a saved pointer path if one matches, otherwise the open mission map."; }
    bool get(const Process& p, int32_t& v) const;
    bool set(const Process& p, int32_t v) const;
    bool mapMode() const { return slider_ != 0; }
    std::string technique() const override {
        if (mapMode())
            return "Live widget: no code is patched. DungeonKit found the mission map's own threat slider through "
                   "the game's reflection tables (the same name-based property lookup the Minecraft Dungeons II "
                   "side uses) and reads/writes its selectedStruggleLevel property directly.";
        return "Pointer chain: DungeonKit tries a short list of previously recorded chains of static pointers "
               "inside the game's own modules, each one dereferenced in turn (module base -> offset -> offset ...). "
               "Chain #" + std::to_string(path_ + 1) + " matched this build and lands on the 4-byte Apocalypse level "
               "value with no code touched at all.";
    }
protected:
    bool onEnable(const Process& p, std::string& err) override;
    void onDisable(const Process&) override { path_ = -1; slider_ = 0; }
private:
    bool resolve(const Process& p, int index, uint64_t& addr) const;
    bool sliderValid() const;
    bool findSlider(const Process& p, std::string& err);
    std::shared_ptr<UE4> ue_;
    int path_ = -1;
    uint64_t slider_ = 0;                  // live UMG_MissionThreatSlider_C widget (mission map)
    uint32_t selOff_ = 0, hiOff_ = 0;
};

// ---------------------------------------------------------------- game profile
struct Mods {
    std::vector<Mod*> all;                 // everything, for bulk disable
    NopMod* arrows = nullptr;
    NopMod* lives = nullptr;
    InfiniteEnchantMod* enchant = nullptr;
    NoCooldownMod* cooldown = nullptr;
    CurrencyMod* currency = nullptr;
    XpMod* xp = nullptr;
    ThreatMod* threat = nullptr;
    PositionMod* position = nullptr;
    ScaleMod* scale = nullptr;
    ApocalypseMod* apocalypse = nullptr;
    ItemViewerMod* viewer = nullptr;
    ItemEditorMod* items = nullptr;
    NoClipMod* noclip = nullptr;
    GodModeMod* god = nullptr;
    StatsMod* stats = nullptr;
    NoTargetMod* notarget = nullptr;
    OverkillMod* overkill = nullptr;
    WasdMod* wasd = nullptr;
    TimerMod* timer = nullptr;
    PauseMod* pause = nullptr;
    std::shared_ptr<UE4> ue4;
};

struct GameProfile {
    std::string name;
    std::string subtitle;
    std::vector<std::unique_ptr<Mod>> owned;
    Mods mods;
};

std::unique_ptr<GameProfile> makeMinecraftDungeons();

} // namespace dk
