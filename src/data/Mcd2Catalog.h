#pragma once
// Reference data for Minecraft Dungeons II, pulled from community data dumps (metabot.gg): what every item
// and effect tag the game uses is actually called, what strength each enchant/effect tier rolls at, what
// Talisman levels look like, and the game's own effect-slot and enchantment-point limits. None of this is
// read from the game itself - it's a static reference table used so the Inventory Editor can show real
// names instead of raw "SW.Item.X" tags and keep edits inside values the game can actually produce.
#include <array>
#include <map>
#include <string>
#include <vector>

namespace dk {

struct Mcd2Tier { std::string roman; float strength = 0; };                 // "I" / "II" / "III"

struct Mcd2ItemInfo { std::string id, name, kind, slot, setName; };          // id: base "SW.Item.X" or its "..._Unique" variant

struct Mcd2EffectInfo { std::string tag, name, category; std::vector<Mcd2Tier> tiers; };   // tag: "SW.Effect.X" or "SW.Enchantment.X"

struct Mcd2TalismanLevel { std::string roman, effect; float intensity = 0; };
struct Mcd2TalismanInfo { std::string id; std::vector<Mcd2TalismanLevel> levels; };

class Mcd2Catalog {
public:
    static const std::vector<Mcd2ItemInfo>& items();
    static const std::vector<Mcd2EffectInfo>& effects();
    static const std::vector<Mcd2TalismanInfo>& talismans();
    // rarity ("Common"/"Rare"/"Special"/"Unique") -> enchantment points spent at tier I/II/III
    static const std::map<std::string, std::array<int, 3>>& enchantPointsByRarity();
    // cumulative XP needed to reach Talisman level 2 and level 3 (2 entries)
    static const std::vector<int>& talismanXpThresholds();
    // the game's own cap on how many effect slots (built-in + rerolls + enchant) one item can have
    static const int kMaxEffectSlots;

    // ---- lookups (linear scan over the small tables above; builds an index on first use) ----
    static const Mcd2ItemInfo* findItem(const std::string& id);
    static const Mcd2EffectInfo* findEffect(const std::string& tag);
    static const Mcd2TalismanInfo* findTalisman(const std::string& id);
    // Display name for any item/effect tag the catalog knows about, else empty.
    static std::string itemName(const std::string& id);
    static std::string effectName(const std::string& tag);
};

} // namespace dk
