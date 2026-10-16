// Auto-generated from community data dumps (metabot.gg): item names, effect/enchant tier
// strength tables, talisman level tables, and the game's own effect-slot/enchant-point limits.
// Used by the Minecraft Dungeons II Inventory Editor to show real names instead of raw tags
// and to keep edits inside values the game itself can actually produce.
//
// NOTE ON SHAPE: the raw data below is kept as plain-old-data (const char*, float, int - no
// std::string/std::vector construction at file scope). A single function containing hundreds
// of inline std::string/std::vector constructions can blow up MinGW/GCC compile-time memory
// (each needs exception-safety unwind bookkeeping) and in practice got the compiler silently
// OOM-killed on a real build. Building the std::vector<...> results once, in a small runtime
// loop over POD tables, avoids that entirely.
#include "Mcd2Catalog.h"
#include <algorithm>
#include <cstring>

namespace dk {

namespace {
struct RawTier { const char* roman; float strength; };
struct RawItem { const char* id; const char* name; const char* kind; const char* slot; const char* setName; };
struct RawEffect { const char* tag; const char* name; const char* category; const RawTier* tiers; int tierCount; };
struct RawTalismanLevel { const char* roman; const char* effect; float intensity; };
struct RawTalisman { const char* id; const RawTalismanLevel* levels; int levelCount; };
} // namespace

const int Mcd2Catalog::kMaxEffectSlots = 4;

static const RawItem kRawItems[] = {
    {"SW.Item.Artifact.BlizzardStaff", "Winter Staff", "Artifact", "Frost", ""},
    {"SW.Item.Artifact.CarapaceOcarina", "Echo Ocarina", "Artifact", "", ""},
    {"SW.Item.Artifact.ConductiveBracelet", "Electric Bangle", "Artifact", "Lightning", ""},
    {"SW.Item.Artifact.CorruptedBeacon", "Corrupted Beacon", "Artifact", "Soul", ""},
    {"SW.Item.Artifact.CorruptedSeeds", "Corrupted Seeds", "Artifact", "Poison", ""},
    {"SW.Item.Artifact.CreeperCandle", "Creeper Candle", "Artifact", "", ""},
    {"SW.Item.Artifact.DeathcapMushroom", "Death Cap Mushroom", "Artifact", "", ""},
    {"SW.Item.Artifact.FightersFife", "Fighter's Flute", "Artifact", "", ""},
    {"SW.Item.Artifact.FireBracelet", "Blaze Bangle", "Artifact", "Fire", ""},
    {"SW.Item.Artifact.FireworkQuiver", "Firework Arrow", "Artifact", "", ""},
    {"SW.Item.Artifact.FlameQuiver", "Flaming Quiver", "Artifact", "Fire", ""},
    {"SW.Item.Artifact.FlameSceptre", "Cinder Scepter", "Artifact", "Fire", ""},
    {"SW.Item.Artifact.FrostBracelet", "Blizzard Bangle", "Artifact", "Frost", ""},
    {"SW.Item.Artifact.FrostQuiver", "Freezing Quiver", "Artifact", "Frost", ""},
    {"SW.Item.Artifact.Grindstone", "Grindstone", "Artifact", "", ""},
    {"SW.Item.Artifact.HasteMushroom", "Tempo Truffle", "Artifact", "", ""},
    {"SW.Item.Artifact.Honeypot", "Honey Dipper", "Artifact", "", ""},
    {"SW.Item.Artifact.IronHideLute", "Turtle Master's Mandolin", "Artifact", "", ""},
    {"SW.Item.Artifact.LightningQuiver", "Conductive Quiver", "Artifact", "Lightning", ""},
    {"SW.Item.Artifact.LightningRod", "Lightning Rod", "Artifact", "Lightning", ""},
    {"SW.Item.Artifact.MaimingMushroom", "Fighting Fungus", "Artifact", "", ""},
    {"SW.Item.Artifact.PicnicBlanket", "Picnic Basket", "Artifact", "", ""},
    {"SW.Item.Artifact.PoisonBracelet", "Blight Bangle", "Artifact", "Poison", ""},
    {"SW.Item.Artifact.PoisonQuiver", "Venomous Quiver", "Artifact", "Poison", ""},
    {"SW.Item.Artifact.Powershaker", "Powershaker", "Artifact", "", ""},
    {"SW.Item.Artifact.RallyingHorn", "Humbling Horn", "Artifact", "", ""},
    {"SW.Item.Artifact.RedstoneMines", "Redstone Mine Launcher", "Artifact", "", ""},
    {"SW.Item.Artifact.Satchel.Conductive", "Pouch of Thunder", "Artifact", "Lightning", ""},
    {"SW.Item.Artifact.Satchel.Fire", "Pouch of Ember", "Artifact", "Fire", ""},
    {"SW.Item.Artifact.Satchel.Freezing", "Pouch of Frost", "Artifact", "Frost", ""},
    {"SW.Item.Artifact.Satchel.Poison", "Pouch of Poison", "Artifact", "Poison", ""},
    {"SW.Item.Artifact.SmokeBomb", "Ender Fog", "Artifact", "", ""},
    {"SW.Item.Artifact.SoulHarvester", "Soul Harvester", "Artifact", "Soul", ""},
    {"SW.Item.Artifact.TotemOfCasting", "Totem of Casting", "Artifact", "", ""},
    {"SW.Item.Artifact.TotemOfRegeneration", "Totem of Regeneration", "Artifact", "", ""},
    {"SW.Item.Artifact.TotemOfShielding", "Totem of Shielding", "Artifact", "", ""},
    {"SW.Item.Artifact.WarBanner", "Battle Banner", "Artifact", "", ""},
    {"SW.Item.Artifact.WardingChimes", "Warding Chimes", "Artifact", "", ""},
    {"SW.Item.Artifact.WarriorsDrums", "Warrior Drums", "Artifact", "", ""},
    {"SW.Item.Artifact.WitchesBrew", "Witch Brew", "Artifact", "", ""},
    {"SW.Item.Axe", "Axe", "Melee", "", ""},
    {"SW.Item.Axe_Unique1", "Hunter's Hatchet", "Melee", "", ""},
    {"SW.Item.Battlestaff", "Battlestaff", "Melee", "", ""},
    {"SW.Item.Battlestaff_Unique1", "Elemental Staff", "Melee", "", ""},
    {"SW.Item.Bow", "Bow", "Ranged", "", ""},
    {"SW.Item.Bow_Unique1", "Ranger's Promise", "Ranged", "", ""},
    {"SW.Item.CaveCrawlerBoots", "Sculk Digger Boots", "Armor", "Boots", "Sculk Digger"},
    {"SW.Item.CaveCrawlerBoots_Unique", "Twisted Warden Sneakers", "Armor", "Boots", "Sculk Digger"},
    {"SW.Item.CaveCrawlerChest", "Sculk Digger Robe", "Armor", "Chestplate", "Sculk Digger"},
    {"SW.Item.CaveCrawlerChest_Unique", "Twisted Warden Vest", "Armor", "Chestplate", "Sculk Digger"},
    {"SW.Item.CaveCrawlerHelmet", "Sculk Digger Hood", "Armor", "Helmet", "Sculk Digger"},
    {"SW.Item.CaveCrawlerHelmet_Unique", "Twisted Warden Blindfold", "Armor", "Helmet", "Sculk Digger"},
    {"SW.Item.CaveCrawlerLeggings", "Sculk Digger Leggings", "Armor", "Leggings", "Sculk Digger"},
    {"SW.Item.CaveCrawlerLeggings_Unique", "Twisted Warden Tights", "Armor", "Leggings", "Sculk Digger"},
    {"SW.Item.Claws", "Wolf Claws", "Melee", "", ""},
    {"SW.Item.Claws_Unique1", "Sculker Claws", "Melee", "", ""},
    {"SW.Item.Claymore", "Claymore", "Melee", "", ""},
    {"SW.Item.Claymore_Unique1", "Slaymore", "Melee", "", ""},
    {"SW.Item.Cleaver", "Meat Cleaver", "Melee", "", ""},
    {"SW.Item.Cleaver_Unique1", "Cacaphonous Cleaver", "Melee", "", ""},
    {"SW.Item.Crossbow", "Crossbow", "Ranged", "", ""},
    {"SW.Item.Crossbow_Unique1", "The Shackler", "Ranged", "", ""},
    {"SW.Item.CurvedGreatsword", "Cookiecutter", "Melee", "", ""},
    {"SW.Item.CurvedGreatsword_Unique1", "Lullaby Blade", "Melee", "", ""},
    {"SW.Item.CurvedLongsword", "Riftslasher", "Melee", "", ""},
    {"SW.Item.CurvedLongsword_Unique1", "Pride of the Plains", "Melee", "", ""},
    {"SW.Item.Dagger", "Twilight Dagger", "Melee", "", ""},
    {"SW.Item.Dagger_Unique1", "Sculker's Bane", "Melee", "", ""},
    {"SW.Item.Daggers", "Double Daggers", "Melee", "", ""},
    {"SW.Item.Daggers_Unique1", "Venomous Fangs", "Melee", "", ""},
    {"SW.Item.DiscipleBoots", "Disciple Wraps", "Armor", "Boots", "Disciple"},
    {"SW.Item.DiscipleBoots_Unique", "Sage Wraps", "Armor", "Boots", "Disciple"},
    {"SW.Item.DiscipleChest", "Disciple Robe", "Armor", "Chestplate", "Disciple"},
    {"SW.Item.DiscipleChest_Unique", "Sage Tunic", "Armor", "Chestplate", "Disciple"},
    {"SW.Item.DiscipleHelmet", "Disciple mask", "Armor", "Helmet", "Disciple"},
    {"SW.Item.DiscipleHelmet_Unique", "Sage Headband", "Armor", "Helmet", "Disciple"},
    {"SW.Item.DiscipleLeggings", "Disciple Leggings", "Armor", "Leggings", "Disciple"},
    {"SW.Item.DiscipleLeggings_Unique", "Sage Belt", "Armor", "Leggings", "Disciple"},
    {"SW.Item.DualCrossbow", "Dual Crossbows", "Ranged", "", ""},
    {"SW.Item.DualCrossbow_Unique1", "Double Crossers", "Ranged", "", ""},
    {"SW.Item.EnchantmentBook.Arcane", "Artifact Amplifier", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Blowback", "Blowback", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Borealis", "Borealis", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.BurstBowstring", "Burst Bowstring", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.ChainReaction", "Chain Reaction", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Channeling", "Channeling", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.CriticalQuiver", "Critical Quiver", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Dynamo", "Dynamo", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.ExpandedQuiver", "Ender Quiver", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.FireAspect", "Fire Aspect", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.FrostCrescent", "Frost Crescent", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.GravityPulse", "Gravity Pulse", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.GuardingStrike", "Guarding Strike", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.HealthSynergy", "Health Synergy", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.LingeringPower", "Lingering Power", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.MultiRoll", "Somersault", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Piercing", "Piercing", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.PoisonFog", "Poison Fog", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.PotionBarrier", "Barrier Brew", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.PotionSharing", "Buddy Brew", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Radiance", "Healing Smite", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Ricochet", "Ricochet", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.ShadowStrike", "Shadow Strike", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Shockwave", "Shockwave", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.SoulAspect", "Soul Aspect", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.SoulInfusedPotion", "Ancient Alchemy", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.SpringLoaded", "Springload", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Swirling", "Swirling", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.TempoTheft", "Tempo Theft", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Thundering", "Thundering", "Enchantment Book", "", ""},
    {"SW.Item.EnchantmentBook.Unstoppable", "Cow Stampede", "Enchantment Book", "", ""},
    {"SW.Item.EvocationBoots", "Sorcerer Boots", "Armor", "Boots", "Sorcerer"},
    {"SW.Item.EvocationBoots_Unique", "Alchemist Loafers", "Armor", "Boots", "Sorcerer"},
    {"SW.Item.EvocationChest", "Sorcerer Robe", "Armor", "Chestplate", "Sorcerer"},
    {"SW.Item.EvocationChest_Unique", "Alchemist Overcoat", "Armor", "Chestplate", "Sorcerer"},
    {"SW.Item.EvocationHelmet", "Sorcerer Hat", "Armor", "Helmet", "Sorcerer"},
    {"SW.Item.EvocationHelmet_Unique", "Alchemist Top Hat", "Armor", "Helmet", "Sorcerer"},
    {"SW.Item.EvocationLeggings", "Sorcerer Leggings", "Armor", "Leggings", "Sorcerer"},
    {"SW.Item.EvocationLeggings_Unique", "Alchemist Trousers", "Armor", "Leggings", "Sorcerer"},
    {"SW.Item.FrostRimeBoots", "Steel Wool Boots", "Armor", "Boots", "Steel Wool"},
    {"SW.Item.FrostRimeBoots_Unique", "Rimefrost Plodders", "Armor", "Boots", "Steel Wool"},
    {"SW.Item.FrostRimeChest", "Steel Wool Sweater", "Armor", "Chestplate", "Steel Wool"},
    {"SW.Item.FrostRimeChest_Unique", "Rimefrost Armor", "Armor", "Chestplate", "Steel Wool"},
    {"SW.Item.FrostRimeHelmet", "Steel Wool Cap", "Armor", "Helmet", "Steel Wool"},
    {"SW.Item.FrostRimeHelmet_Unique", "Rimefrost Icecap", "Armor", "Helmet", "Steel Wool"},
    {"SW.Item.FrostRimeLeggings", "Steel Wool Leggings", "Armor", "Leggings", "Steel Wool"},
    {"SW.Item.FrostRimeLeggings_Unique", "Rimefrost Longjohns", "Armor", "Leggings", "Steel Wool"},
    {"SW.Item.Gauntlet", "Gauntlets", "Melee", "", ""},
    {"SW.Item.Gauntlet_Unique1", "Prime Enchanter's Gauntlets", "Melee", "", ""},
    {"SW.Item.GiantClub", "Clobberer", "Melee", "", ""},
    {"SW.Item.GiantClub_Unique1", "Redstone Wrecker", "Melee", "", ""},
    {"SW.Item.GiantMallet", "Mob Mallet", "Melee", "", ""},
    {"SW.Item.GiantMallet_Unique1", "Monster Masher", "Melee", "", ""},
    {"SW.Item.Glaive", "Glaive", "Melee", "", ""},
    {"SW.Item.Glaive_Unique1", "Golden Glaive", "Melee", "", ""},
    {"SW.Item.GreatAxe", "Greataxe", "Melee", "", ""},
    {"SW.Item.GreatAxe_Unique1", "Awesomeaxe", "Melee", "", ""},
    {"SW.Item.Greatbow", "Greatbow", "Ranged", "", ""},
    {"SW.Item.Greatbow_Unique1", "Humbler Heartstring", "Ranged", "", ""},
    {"SW.Item.Greatsword", "Greatsword", "Melee", "", ""},
    {"SW.Item.Greatsword_Unique1", "The Darkshard", "Melee", "", ""},
    {"SW.Item.Hammer", "Battle Hammer", "Melee", "", ""},
    {"SW.Item.Hammer_Unique1", "Emerald Hammer", "Melee", "", ""},
    {"SW.Item.HeavyCrossbow", "Heavy Crossbow", "Ranged", "", ""},
    {"SW.Item.HeavyCrossbow_Unique1", "The Close Ranger", "Ranged", "", ""},
    {"SW.Item.HewnBarkBoots", "Treehugger Clogs", "Armor", "Boots", "Treehugger"},
    {"SW.Item.HewnBarkBoots_Unique", "Woodsprite Root Boots", "Armor", "Boots", "Treehugger"},
    {"SW.Item.HewnBarkChest", "Treehugger Chestplate", "Armor", "Chestplate", "Treehugger"},
    {"SW.Item.HewnBarkChest_Unique", "Woodsprite Barkpiece", "Armor", "Chestplate", "Treehugger"},
    {"SW.Item.HewnBarkHelmet", "Treehugger Mask", "Armor", "Helmet", "Treehugger"},
    {"SW.Item.HewnBarkHelmet_Unique", "Woodsprite Crown", "Armor", "Helmet", "Treehugger"},
    {"SW.Item.HewnBarkLeggings", "Treehugger Leggings", "Armor", "Leggings", "Treehugger"},
    {"SW.Item.HewnBarkLeggings_Unique", "Woodsprite Trunks", "Armor", "Leggings", "Treehugger"},
    {"SW.Item.HoneyBoots", "Beekeeper Boots", "Armor", "Boots", "Beekeeper"},
    {"SW.Item.HoneyBoots_Unique", "Hivemind Stingers", "Armor", "Boots", "Beekeeper"},
    {"SW.Item.HoneyChest", "Beekeeper Suit", "Armor", "Chestplate", "Beekeeper"},
    {"SW.Item.HoneyChest_Unique", "Hivemind Thorax", "Armor", "Chestplate", "Beekeeper"},
    {"SW.Item.HoneyHelmet", "Beekeeper Veil", "Armor", "Helmet", "Beekeeper"},
    {"SW.Item.HoneyHelmet_Unique", "Hivemind Hardhat", "Armor", "Helmet", "Beekeeper"},
    {"SW.Item.HoneyLeggings", "Beekeeper Leggings", "Armor", "Leggings", "Beekeeper"},
    {"SW.Item.HoneyLeggings_Unique", "Hivemind Trousers", "Armor", "Leggings", "Beekeeper"},
    {"SW.Item.Longbow", "Longbow", "Ranged", "", ""},
    {"SW.Item.Longbow_Unique1", "Creaking's Reach", "Ranged", "", ""},
    {"SW.Item.Mace", "Mace", "Melee", "", ""},
    {"SW.Item.Mace_Unique1", "Carapace Mace", "Melee", "", ""},
    {"SW.Item.MoonSword", "Tidal Sickle", "Melee", "", ""},
    {"SW.Item.MoonSword_Unique1", "Lunar Sickle", "Melee", "", ""},
    {"SW.Item.MushroomBoots", "Mushroom Boots", "Armor", "Boots", "Mushroom"},
    {"SW.Item.MushroomBoots_Unique", "Fly Agaric Galoshes", "Armor", "Boots", "Mushroom"},
    {"SW.Item.MushroomChest", "Mushroom Robe", "Armor", "Chestplate", "Mushroom"},
    {"SW.Item.MushroomChest_Unique", "Fly Agaric Coat", "Armor", "Chestplate", "Mushroom"},
    {"SW.Item.MushroomHelmet", "Mushroom Cap", "Armor", "Helmet", "Mushroom"},
    {"SW.Item.MushroomHelmet_Unique", "Fly Agaric Cap", "Armor", "Helmet", "Mushroom"},
    {"SW.Item.MushroomLeggings", "Mushroom Leggings", "Armor", "Leggings", "Mushroom"},
    {"SW.Item.MushroomLeggings_Unique", "Fly Agaric Trousers", "Armor", "Leggings", "Mushroom"},
    {"SW.Item.MysticBoots", "Mystic Boots", "Armor", "Boots", "Mystic"},
    {"SW.Item.MysticBoots_Unique", "Oracle Sandals", "Armor", "Boots", "Mystic"},
    {"SW.Item.MysticChest", "Mystic Robe", "Armor", "Chestplate", "Mystic"},
    {"SW.Item.MysticChest_Unique", "Oracle Mantle", "Armor", "Chestplate", "Mystic"},
    {"SW.Item.MysticHelmet", "Mystic Circlet", "Armor", "Helmet", "Mystic"},
    {"SW.Item.MysticHelmet_Unique", "Oracle Crown", "Armor", "Helmet", "Mystic"},
    {"SW.Item.MysticLeggings", "Mystic Leggings", "Armor", "Leggings", "Mystic"},
    {"SW.Item.MysticLeggings_Unique", "Oracle Tights", "Armor", "Leggings", "Mystic"},
    {"SW.Item.PhantomBoots", "Phantom Boots", "Armor", "Boots", "Phantom"},
    {"SW.Item.PhantomBoots_Unique", "Dreamruler Slippers", "Armor", "Boots", "Phantom"},
    {"SW.Item.PhantomChest", "Phantom Chestplate", "Armor", "Chestplate", "Phantom"},
    {"SW.Item.PhantomChest_Unique", "Dreamruler Cover", "Armor", "Chestplate", "Phantom"},
    {"SW.Item.PhantomHelmet", "Phantom Helmet", "Armor", "Helmet", "Phantom"},
    {"SW.Item.PhantomHelmet_Unique", "Dreamruler Crown", "Armor", "Helmet", "Phantom"},
    {"SW.Item.PhantomLeggings", "Phantom Pants", "Armor", "Leggings", "Phantom"},
    {"SW.Item.PhantomLeggings_Unique", "Dreamruler Pyjamas", "Armor", "Leggings", "Phantom"},
    {"SW.Item.Pickaxe", "Pickaxe", "Melee", "", ""},
    {"SW.Item.Pickaxe_Unique1", "The Prospector's Pick", "Melee", "", ""},
    {"SW.Item.Pike", "Pike", "Melee", "", ""},
    {"SW.Item.Pike_Unique1", "Stormcaller", "Melee", "", ""},
    {"SW.Item.Powerbow", "Powerbow", "Ranged", "", ""},
    {"SW.Item.Powerbow_Unique1", "Paragon", "Ranged", "", ""},
    {"SW.Item.RapidCrossbow", "Rapid Crossbow", "Ranged", "", ""},
    {"SW.Item.RapidCrossbow_Unique1", "The Cross Pollinator", "Ranged", "", ""},
    {"SW.Item.Rapier", "Rapier", "Melee", "", ""},
    {"SW.Item.Rapier_Unique1", "The Thousand Cuts", "Melee", "", ""},
    {"SW.Item.RealmreacherBoots", "Ranger Boots", "Armor", "Boots", "Ranger"},
    {"SW.Item.RealmreacherBoots_Unique", "Sharpshooter Spurs", "Armor", "Boots", "Ranger"},
    {"SW.Item.RealmreacherChest", "Ranger Jacket", "Armor", "Chestplate", "Ranger"},
    {"SW.Item.RealmreacherChest_Unique", "Sharpshooter Duster", "Armor", "Chestplate", "Ranger"},
    {"SW.Item.RealmreacherHelmet", "Ranger Cap", "Armor", "Helmet", "Ranger"},
    {"SW.Item.RealmreacherHelmet_Unique", "Sharpshooter Fedora", "Armor", "Helmet", "Ranger"},
    {"SW.Item.RealmreacherLeggings", "Ranger Leggings", "Armor", "Leggings", "Ranger"},
    {"SW.Item.RealmreacherLeggings_Unique", "Sharpshooter Chaps", "Armor", "Leggings", "Ranger"},
    {"SW.Item.RedstoneBoots", "Redstone Boots", "Armor", "Boots", "Redstone"},
    {"SW.Item.RedstoneBoots_Unique", "Monstrosity Stompers", "Armor", "Boots", "Redstone"},
    {"SW.Item.RedstoneChest", "Redstone Chestplate", "Armor", "Chestplate", "Redstone"},
    {"SW.Item.RedstoneChest_Unique", "Monstrosity Armor", "Armor", "Chestplate", "Redstone"},
    {"SW.Item.RedstoneHelmet", "Redstone Helmet", "Armor", "Helmet", "Redstone"},
    {"SW.Item.RedstoneHelmet_Unique", "Monstrosity Horns", "Armor", "Helmet", "Redstone"},
    {"SW.Item.RedstoneLeggings", "Redstone Shinguards", "Armor", "Leggings", "Redstone"},
    {"SW.Item.RedstoneLeggings_Unique", "Monstrosity Greaves", "Armor", "Leggings", "Redstone"},
    {"SW.Item.Sabre", "Sabre", "Melee", "", ""},
    {"SW.Item.Sabre_Unique1", "Rascal's Razor", "Melee", "", ""},
    {"SW.Item.ScampBoots", "Scamp Sneakers", "Armor", "Boots", "Scamp"},
    {"SW.Item.ScampBoots_Unique", "Scoundrel Sneakers", "Armor", "Boots", "Scamp"},
    {"SW.Item.ScampChest", "Scamp Jacket", "Armor", "Chestplate", "Scamp"},
    {"SW.Item.ScampChest_Unique", "Scoundrel Blazer", "Armor", "Chestplate", "Scamp"},
    {"SW.Item.ScampHelmet", "Scamp Hood", "Armor", "Helmet", "Scamp"},
    {"SW.Item.ScampHelmet_Unique", "Scoundrel Cowl", "Armor", "Helmet", "Scamp"},
    {"SW.Item.ScampLeggings", "Scamp Leggings", "Armor", "Leggings", "Scamp"},
    {"SW.Item.ScampLeggings_Unique", "Scoundrel Chaps", "Armor", "Leggings", "Scamp"},
    {"SW.Item.ScatterCrossbow", "Scatter Crossbow", "Ranged", "", ""},
    {"SW.Item.ScatterCrossbow_Unique1", "Harp Crossbow", "Ranged", "", ""},
    {"SW.Item.ScavengerBoots", "Sculker Boots", "Armor", "Boots", "Sculker"},
    {"SW.Item.ScavengerBoots_Unique", "Monarch Talons", "Armor", "Boots", "Sculker"},
    {"SW.Item.ScavengerChest", "Sculker Robe", "Armor", "Chestplate", "Sculker"},
    {"SW.Item.ScavengerChest_Unique", "Monarch Mantle", "Armor", "Chestplate", "Sculker"},
    {"SW.Item.ScavengerHelmet", "Sculker Mask", "Armor", "Helmet", "Sculker"},
    {"SW.Item.ScavengerHelmet_Unique", "Monarch Crown", "Armor", "Helmet", "Sculker"},
    {"SW.Item.ScavengerLeggings", "Sculker Slacks", "Armor", "Leggings", "Sculker"},
    {"SW.Item.ScavengerLeggings_Unique", "Monarch Sash", "Armor", "Leggings", "Sculker"},
    {"SW.Item.Scythe", "Scythe", "Melee", "", ""},
    {"SW.Item.Scythe_Unique1", "Soul Reaper", "Melee", "", ""},
    {"SW.Item.ShortSpear", "Short Spear", "Melee", "", ""},
    {"SW.Item.ShortSpear_Unique1", "Spectral Spear", "Melee", "", ""},
    {"SW.Item.Shortbow", "Shortbow", "Ranged", "", ""},
    {"SW.Item.Shortbow_Unique1", "Shooting Star", "Ranged", "", ""},
    {"SW.Item.Shovel", "Shovel", "Melee", "", ""},
    {"SW.Item.Shovel_Unique1", "Diamond Shovel", "Melee", "", ""},
    {"SW.Item.Sickles", "Double Sickles", "Melee", "", ""},
    {"SW.Item.Sickles_Unique1", "Sift Sickles", "Melee", "", ""},
    {"SW.Item.StalwartBoots", "Protector Boots", "Armor", "Boots", "Protector"},
    {"SW.Item.StalwartBoots_Unique", "Humbler Tarsi", "Armor", "Boots", "Protector"},
    {"SW.Item.StalwartChest", "Protector Chestplate", "Armor", "Chestplate", "Protector"},
    {"SW.Item.StalwartChest_Unique", "Humbler Carapace", "Armor", "Chestplate", "Protector"},
    {"SW.Item.StalwartHelmet", "Protector Helmet", "Armor", "Helmet", "Protector"},
    {"SW.Item.StalwartHelmet_Unique", "Humbler Antenna", "Armor", "Helmet", "Protector"},
    {"SW.Item.StalwartLeggings", "Protector Shinguards", "Armor", "Leggings", "Protector"},
    {"SW.Item.StalwartLeggings_Unique", "Humbler Greaves", "Armor", "Leggings", "Protector"},
    {"SW.Item.StraightSword", "Longsword", "Melee", "", ""},
    {"SW.Item.StraightSword_Unique1", "Frostbiter", "Melee", "", ""},
    {"SW.Item.Sword", "Sword", "Melee", "", ""},
    {"SW.Item.Sword_Unique1", "The Burning Blade", "Melee", "", ""},
    {"SW.Item.Talisman.AmmoCapacity", "Twig of Dark Oak", "Talisman", "", ""},
    {"SW.Item.Talisman.ArtifactCooldown", "Essence of Efficiency", "Talisman", "", ""},
    {"SW.Item.Talisman.Brawling", "Fist of Iron", "Talisman", "", ""},
    {"SW.Item.Talisman.DoubleDrop", "Lucky Clover", "Talisman", "", ""},
    {"SW.Item.Talisman.DropChance", "Looter's Charm", "Talisman", "", ""},
    {"SW.Item.Talisman.EmeraldIncrease", "Emerald of Good Fortune", "Talisman", "", ""},
    {"SW.Item.Talisman.ExperienceIncrease", "The Eye of Experience", "Talisman", "", ""},
    {"SW.Item.Talisman.FiringEmeralds", "Thrifty Pendant", "Talisman", "", ""},
    {"SW.Item.Talisman.Healing", "Healing Heart", "Talisman", "", ""},
    {"SW.Item.Talisman.HealthBoost", "Sigil of Beeswax", "Talisman", "", ""},
    {"SW.Item.Talisman.HealthyStrike", "Tendrils of the Sprout", "Talisman", "", ""},
    {"SW.Item.Talisman.IronGolem", "Golem Kit", "Talisman", "", ""},
    {"SW.Item.Talisman.Plummeting", "Ocelot's Paw", "Talisman", "", ""},
    {"SW.Item.Talisman.PotionCooldown", "Glowstone Flask", "Talisman", "", ""},
    {"SW.Item.Talisman.Prickle", "Prickle's Mark", "Talisman", "", ""},
    {"SW.Item.Talisman.RangedBuff", "Amethyst Lens", "Talisman", "", ""},
    {"SW.Item.Talisman.RollingCooldown", "Armadillo Amulet", "Talisman", "", ""},
    {"SW.Item.Talisman.SoulCapacity", "Soul Chip", "Talisman", "", ""},
    {"SW.Item.Talisman.SoulGather", "Twisted Tooth", "Talisman", "", ""},
    {"SW.Item.Talisman.SpeedIncrement", "Medallion of Momentum", "Talisman", "", ""},
    {"SW.Item.Talisman.StatusBuff", "Sculk Badge", "Talisman", "", ""},
    {"SW.Item.Talisman.Wobble", "Wobblestone", "Talisman", "", ""},
    {"SW.Item.Talisman.Wolf", "Tasty Bone", "Talisman", "", ""},
    {"SW.Item.TimewornBoots", "Realmreacher Boots", "Armor", "Boots", "Realmreacher"},
    {"SW.Item.TimewornBoots_Unique", "Soul Corruptor Sandals", "Armor", "Boots", "Realmreacher"},
    {"SW.Item.TimewornChest", "Realmreacher Robe", "Armor", "Chestplate", "Realmreacher"},
    {"SW.Item.TimewornChest_Unique", "Soul Corruptor Duster", "Armor", "Chestplate", "Realmreacher"},
    {"SW.Item.TimewornHelmet", "Realmreacher Hat", "Armor", "Helmet", "Realmreacher"},
    {"SW.Item.TimewornHelmet_Unique", "Soul Corruptor Mask", "Armor", "Helmet", "Realmreacher"},
    {"SW.Item.TimewornLeggings", "Realmreacher Leggings", "Armor", "Leggings", "Realmreacher"},
    {"SW.Item.TimewornLeggings_Unique", "Soul Corruptor Sash", "Armor", "Leggings", "Realmreacher"},
    {"SW.Item.Trickbow", "Trickbow", "Ranged", "", ""},
    {"SW.Item.Trickbow_Unique1", "Phantom Wing", "Ranged", "", ""},
    {"SW.Item.UndauntedBoots", "Bounty Hunter Boots", "Armor", "Boots", "Bounty Hunter"},
    {"SW.Item.UndauntedBoots_Unique", "Dauntless Sabatons", "Armor", "Boots", "Bounty Hunter"},
    {"SW.Item.UndauntedChest", "Bounty Hunter Chestplate", "Armor", "Chestplate", "Bounty Hunter"},
    {"SW.Item.UndauntedChest_Unique", "Dauntless Pauldrons", "Armor", "Chestplate", "Bounty Hunter"},
    {"SW.Item.UndauntedHelmet", "Bounty Hunter Helmet", "Armor", "Helmet", "Bounty Hunter"},
    {"SW.Item.UndauntedHelmet_Unique", "Dauntless Horns", "Armor", "Helmet", "Bounty Hunter"},
    {"SW.Item.UndauntedLeggings", "Bounty Hunter Shinguards", "Armor", "Leggings", "Bounty Hunter"},
    {"SW.Item.UndauntedLeggings_Unique", "Dauntless Greaves", "Armor", "Leggings", "Bounty Hunter"},
    {"SW.Item.VoyagerBoots", "Nomad Boots", "Armor", "Boots", "Nomad"},
    {"SW.Item.VoyagerBoots_Unique", "Rover Sabatons", "Armor", "Boots", "Nomad"},
    {"SW.Item.VoyagerChest", "Nomad Chestplate", "Armor", "Chestplate", "Nomad"},
    {"SW.Item.VoyagerChest_Unique", "Rover Pauldrons", "Armor", "Chestplate", "Nomad"},
    {"SW.Item.VoyagerHelmet", "Nomad Hood", "Armor", "Helmet", "Nomad"},
    {"SW.Item.VoyagerHelmet_Unique", "Rover Visor", "Armor", "Helmet", "Nomad"},
    {"SW.Item.VoyagerLeggings", "Nomad Leggings", "Armor", "Leggings", "Nomad"},
    {"SW.Item.VoyagerLeggings_Unique", "Rover Gaiters", "Armor", "Leggings", "Nomad"},
    {"SW.Item.WarHammer", "War Hammer", "Melee", "", ""},
    {"SW.Item.WarHammer_Unique1", "Heartbreaker", "Melee", "", ""},
    {"SW.Item.WellspringBoots", "Sifter Boots", "Armor", "Boots", "Sifter"},
    {"SW.Item.WellspringBoots_Unique", "Mad Sifter Paws", "Armor", "Boots", "Sifter"},
    {"SW.Item.WellspringChest", "Sifter Robe", "Armor", "Chestplate", "Sifter"},
    {"SW.Item.WellspringChest_Unique", "Mad Sifter Vest", "Armor", "Chestplate", "Sifter"},
    {"SW.Item.WellspringHelmet", "Sifter Hood", "Armor", "Helmet", "Sifter"},
    {"SW.Item.WellspringHelmet_Unique", "Mad Sifter Mask", "Armor", "Helmet", "Sifter"},
    {"SW.Item.WellspringLeggings", "Sifter Leggings", "Armor", "Leggings", "Sifter"},
    {"SW.Item.WellspringLeggings_Unique", "Mad Sifter Slacks", "Armor", "Leggings", "Sifter"},
    {"SW.Item.WolfclutchBoots", "Wolfpack Boots", "Armor", "Boots", "Wolfpack"},
    {"SW.Item.WolfclutchBoots_Unique", "Packleader Paws", "Armor", "Boots", "Wolfpack"},
    {"SW.Item.WolfclutchChest", "Wolfpack Jacket", "Armor", "Chestplate", "Wolfpack"},
    {"SW.Item.WolfclutchChest_Unique", "Packleader Hide", "Armor", "Chestplate", "Wolfpack"},
    {"SW.Item.WolfclutchHelmet", "Wolfpack Hood", "Armor", "Helmet", "Wolfpack"},
    {"SW.Item.WolfclutchHelmet_Unique", "Packleader Muzzle", "Armor", "Helmet", "Wolfpack"},
    {"SW.Item.WolfclutchLeggings", "Wolfpack Leggings", "Armor", "Leggings", "Wolfpack"},
    {"SW.Item.WolfclutchLeggings_Unique", "Packleader Fur Chaps", "Armor", "Leggings", "Wolfpack"},
    {"sw.Item.Talisman.Llama", "Wonderful Wheat", "Talisman", "", ""},
};
static const int kRawItemCount = sizeof(kRawItems) / sizeof(kRawItems[0]);

static const RawTier kEffTiers0[] = {{"I", 0.2f}, {"II", 0.25f}, {"III", 0.3f}};
static const RawTier kEffTiers1[] = {{"I", 0.2f}, {"II", 0.25f}, {"III", 0.3f}};
static const RawTier kEffTiers2[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.25f}};
static const RawTier kEffTiers3[] = {{"I", 0.2f}, {"II", 0.25f}, {"III", 0.3f}};
static const RawTier kEffTiers4[] = {{"I", 0.1f}, {"II", 0.2f}, {"III", 0.3f}};
static const RawTier kEffTiers5[] = {{"I", -0.1f}, {"II", -0.15f}, {"III", -0.2f}};
static const RawTier kEffTiers6[] = {{"I", 0.1f}, {"II", 0.2f}, {"III", 0.3f}};
static const RawTier kEffTiers7[] = {{"I", 0.05f}, {"II", 0.1f}, {"III", 0.15f}};
static const RawTier kEffTiers8[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers9[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers10[] = {{"I", 0.2f}, {"II", 0.25f}, {"III", 0.3f}};
static const RawTier kEffTiers11[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers12[] = {{"I", -0.1f}, {"II", -0.15f}, {"III", -0.2f}};
static const RawTier kEffTiers13[] = {{"I", 0.05f}, {"II", 0.1f}, {"III", 0.15f}};
static const RawTier kEffTiers14[] = {{"I", 0.2f}, {"II", 0.3f}, {"III", 0.4f}};
static const RawTier kEffTiers15[] = {{"I", 0.15f}, {"II", 0.25f}, {"III", 0.35f}};
static const RawTier kEffTiers16[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers17[] = {{"I", -0.05f}, {"II", -0.1f}, {"III", -0.15f}};
static const RawTier kEffTiers18[] = {{"I", -0.05f}, {"II", -0.1f}, {"III", -0.15f}};
static const RawTier kEffTiers19[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers20[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers21[] = {{"I", 0.25f}, {"II", 0.35f}, {"III", 0.5f}};
static const RawTier kEffTiers22[] = {{"I", 0.15f}, {"II", 0.2f}, {"III", 0.3f}};
static const RawTier kEffTiers23[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers24[] = {{"I", 0.2f}, {"II", 0.4f}, {"III", 0.6f}};
static const RawTier kEffTiers25[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers26[] = {{"I", 0.15f}, {"II", 0.2f}, {"III", 0.25f}};
static const RawTier kEffTiers27[] = {{"I", 0.15f}, {"II", 0.2f}, {"III", 0.25f}};
static const RawTier kEffTiers28[] = {{"I", 0.2f}, {"II", 0.3f}, {"III", 0.4f}};
static const RawTier kEffTiers29[] = {{"I", 0.25f}, {"II", 0.35f}, {"III", 0.5f}};
static const RawTier kEffTiers30[] = {{"I", 0.01f}, {"II", 0.02f}, {"III", 0.03f}};
static const RawTier kEffTiers31[] = {{"I", 0.25f}, {"II", 0.5f}, {"III", 0.75f}};
static const RawTier kEffTiers32[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers33[] = {{"I", -0.05f}, {"II", -0.1f}, {"III", -0.2f}};
static const RawTier kEffTiers34[] = {{"I", 0.1f}, {"II", 0.2f}, {"III", 0.3f}};
static const RawTier kEffTiers35[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers36[] = {{"I", -0.1f}, {"II", -0.15f}, {"III", -0.2f}};
static const RawTier kEffTiers37[] = {{"I", -0.1f}, {"II", -0.15f}, {"III", -0.2f}};
static const RawTier kEffTiers38[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers39[] = {{"I", -0.15f}, {"II", -0.2f}, {"III", -0.25f}};
static const RawTier kEffTiers40[] = {{"I", 0.2f}, {"II", 0.3f}, {"III", 0.4f}};
static const RawTier kEffTiers41[] = {{"I", 0.05f}, {"II", 0.075f}, {"III", 0.1f}};
static const RawTier kEffTiers42[] = {{"I", -0.2f}, {"II", -0.25f}, {"III", -0.3f}};
static const RawTier kEffTiers43[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.25f}};
static const RawTier kEffTiers44[] = {{"I", 0.4f}, {"II", 0.6f}, {"III", 0.8f}};
static const RawTier kEffTiers45[] = {{"I", 0.2f}, {"II", 0.25f}, {"III", 0.35f}};
static const RawTier kEffTiers46[] = {{"I", 0.1f}, {"II", 0.2f}, {"III", 0.3f}};
static const RawTier kEffTiers47[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers48[] = {{"I", 0.25f}, {"II", 0.5f}, {"III", 0.75f}};
static const RawTier kEffTiers49[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers50[] = {{"I", 0.25f}, {"II", 0.35f}, {"III", 0.45f}};
static const RawTier kEffTiers51[] = {{"I", 1.25f}, {"II", 1.3f}, {"III", 1.35f}};
static const RawTier kEffTiers52[] = {{"I", 0.05f}, {"II", 0.1f}, {"III", 0.15f}};
static const RawTier kEffTiers53[] = {{"I", 0.3f}, {"II", 0.35f}, {"III", 0.4f}};
static const RawTier kEffTiers54[] = {{"I", 0.2f}, {"II", 0.3f}, {"III", 0.4f}};
static const RawTier kEffTiers55[] = {{"I", 0.15f}, {"II", 0.2f}, {"III", 0.3f}};
static const RawTier kEffTiers56[] = {{"I", 0.5f}, {"II", 0.65f}, {"III", 0.8f}};
static const RawTier kEffTiers57[] = {{"I", 0.2f}, {"II", 0.35f}, {"III", 0.5f}};
static const RawTier kEffTiers58[] = {{"I", 0.1f}, {"II", 0.15f}, {"III", 0.2f}};
static const RawTier kEffTiers59[] = {{"I", 0.15f}, {"II", 0.3f}, {"III", 0.5f}};
static const RawTier kEffTiers60[] = {{"III", 9.0f}};
static const RawTier kEffTiers61[] = {{"III", 0.5f}};
static const RawTier kEffTiers62[] = {{"III", 0.287547f}};
static const RawTier kEffTiers63[] = {{"III", 5.0f}};
static const RawTier kEffTiers64[] = {{"III", 3.0f}};
static const RawTier kEffTiers65[] = {{"II", 3.0f}, {"III", 4.0f}};
static const RawTier kEffTiers66[] = {{"III", 1.0f}};
static const RawTier kEffTiers67[] = {{"III", 0.0f}};
static const RawTier kEffTiers68[] = {{"I", 0.15f}, {"III", 0.35f}};
static const RawTier kEffTiers69[] = {{"III", 0.244871f}};
static const RawTier kEffTiers70[] = {{"II", 2.0f}};
static const RawTier kEffTiers71[] = {{"I", 1.0f}, {"III", 5.0f}};
static const RawTier kEffTiers72[] = {{"III", 6.0f}};
static const RawTier kEffTiers73[] = {{"I", 0.3f}, {"III", 0.5f}};
static const RawTier kEffTiers74[] = {{"III", 3.0f}};
static const RawTier kEffTiers75[] = {{"I", 0.5f}, {"II", 0.6f}, {"III", 0.85f}};
static const RawTier kEffTiers76[] = {{"III", 6.0f}};
static const RawTier kEffTiers77[] = {{"III", 1.0f}};
static const RawTier kEffTiers78[] = {{"III", 0.264703f}};
static const RawTier kEffTiers79[] = {{"III", 0.3f}};
static const RawEffect kRawEffects[] = {
    {"SW.Effect.ArrowBurst", "Bowyer", "Utility", kEffTiers0, 3},
    {"SW.Effect.BeastBoss", "Pack Leader", "Attack", kEffTiers1, 3},
    {"SW.Effect.Chains", "Shackler", "Attack", kEffTiers2, 3},
    {"SW.Effect.Committed", "Bounty Hunter", "Attack", kEffTiers3, 3},
    {"SW.Effect.Constitution", "Recovery", "Defense", kEffTiers4, 3},
    {"SW.Effect.Cooldown", "Cooldown", "Utility", kEffTiers5, 3},
    {"SW.Effect.CriticalEdge", "Critical Edge", "Attack", kEffTiers6, 3},
    {"SW.Effect.CriticalHit", "Critical Hit", "Attack", kEffTiers7, 3},
    {"SW.Effect.Deflect", "Deflection", "Defense", kEffTiers8, 3},
    {"SW.Effect.Desperation", "Persistence", "Attack", kEffTiers9, 3},
    {"SW.Effect.Duelist", "Duelist", "Attack", kEffTiers10, 3},
    {"SW.Effect.EagleEye", "Aim", "Attack", kEffTiers11, 3},
    {"SW.Effect.ElementalProtection", "Elemental Protection", "Defense", kEffTiers12, 3},
    {"SW.Effect.EmeraldsIncrease", "Prospector", "Utility", kEffTiers13, 3},
    {"SW.Effect.Expand", "Totem Radius", "Utility", kEffTiers14, 3},
    {"SW.Effect.Finesse", "Finesse", "Attack", kEffTiers15, 3},
    {"SW.Effect.FireFocus", "Pyromancer", "Attack", kEffTiers16, 3},
    {"SW.Effect.FriendsForever", "Veterinarian", "Utility", kEffTiers17, 3},
    {"SW.Effect.Friendship", "Shepherd", "Defense", kEffTiers18, 3},
    {"SW.Effect.FrostFocus", "Cryomancer", "Attack", kEffTiers19, 3},
    {"SW.Effect.Gambler", "Assassin", "Utility", kEffTiers20, 3},
    {"SW.Effect.HealingFocus", "Healer", "Defense", kEffTiers21, 3},
    {"SW.Effect.Knockback", "Knockback", "Attack", kEffTiers22, 3},
    {"SW.Effect.LightningFocus", "Electromancer", "Attack", kEffTiers23, 3},
    {"SW.Effect.Looting", "Looter", "Utility", kEffTiers24, 3},
    {"SW.Effect.Lucky", "Luck", "Utility", kEffTiers25, 3},
    {"SW.Effect.MasterMarksman", "Marksman", "Attack", kEffTiers26, 3},
    {"SW.Effect.MasterStrike", "Strength", "Attack", kEffTiers27, 3},
    {"SW.Effect.MultiShot", "Ranger", "Attack", kEffTiers28, 3},
    {"SW.Effect.Opportunist", "Bully", "Attack", kEffTiers29, 3},
    {"SW.Effect.Opulence", "Raider", "Utility", kEffTiers30, 3},
    {"SW.Effect.PointBlank", "Point Blank", "Attack", kEffTiers31, 3},
    {"SW.Effect.PoisonFocus", "Venomancer", "Attack", kEffTiers32, 3},
    {"SW.Effect.PotionCooldown", "Potion Maker", "Utility", kEffTiers33, 3},
    {"SW.Effect.Power", "Impact", "Attack", kEffTiers34, 3},
    {"SW.Effect.Precision", "Precision", "Attack", kEffTiers35, 3},
    {"SW.Effect.ProjectileProtection", "Projectile Protection", "Defense", kEffTiers36, 3},
    {"SW.Effect.Protection", "Protection", "Defense", kEffTiers37, 3},
    {"SW.Effect.RapidStrike", "Swiftness", "Attack", kEffTiers38, 3},
    {"SW.Effect.Reconstruction", "Fletcher", "Utility", kEffTiers39, 3},
    {"SW.Effect.Reeling", "Momentum", "Attack", kEffTiers40, 3},
    {"SW.Effect.Regeneration", "Regeneration", "Defense", kEffTiers41, 3},
    {"SW.Effect.Resilience", "Resilience", "Defense", kEffTiers42, 3},
    {"SW.Effect.RollCooldown", "Acrobat", "Utility", kEffTiers43, 3},
    {"SW.Effect.Saboteur", "Tainted", "Utility", kEffTiers44, 3},
    {"SW.Effect.ShadowWalk", "Stealth", "Utility", kEffTiers45, 3},
    {"SW.Effect.Sharpness", "Sharpness", "Attack", kEffTiers46, 3},
    {"SW.Effect.Sidestep", "Evasion", "Utility", kEffTiers47, 3},
    {"SW.Effect.Sniper", "Sniper", "Attack", kEffTiers48, 3},
    {"SW.Effect.SoulFocus", "Soulmancer", "Attack", kEffTiers49, 3},
    {"SW.Effect.SoulGatherMultiply", "Reaper", "Utility", kEffTiers50, 3},
    {"SW.Effect.SoulMax", "Spiritual", "Utility", kEffTiers51, 3},
    {"SW.Effect.SpeedBoost", "Speed", "Utility", kEffTiers52, 3},
    {"SW.Effect.Supercharge", "Sharpshooter", "Attack", kEffTiers53, 3},
    {"SW.Effect.SweepingEdge", "Brawler", "Attack", kEffTiers54, 3},
    {"SW.Effect.SwiftSneak", "Prowler", "Utility", kEffTiers55, 3},
    {"SW.Effect.Thorns", "Prickly", "Attack", kEffTiers56, 3},
    {"SW.Effect.Vanguard", "Vanguard", "Attack", kEffTiers57, 3},
    {"SW.Effect.Vestige", "Sorcerer", "Attack", kEffTiers58, 3},
    {"SW.Effect.Vivify", "Ally", "Utility", kEffTiers59, 3},
    {"SW.Enchantment.Arcane", "Artifact Amplifier", "Enchantment", kEffTiers60, 1},
    {"SW.Enchantment.Blowback", "Blowback", "Enchantment", kEffTiers61, 1},
    {"SW.Enchantment.Borealis", "Borealis", "Enchantment", kEffTiers62, 1},
    {"SW.Enchantment.ChainReaction", "Chain Reaction", "Enchantment", kEffTiers63, 1},
    {"SW.Enchantment.Channeling", "Channeling", "Enchantment", kEffTiers64, 1},
    {"SW.Enchantment.ExpandedQuiver", "Ender Quiver", "Enchantment", kEffTiers65, 2},
    {"SW.Enchantment.FireAspect", "Fire Aspect", "Enchantment", kEffTiers66, 1},
    {"SW.Enchantment.GravityPulse", "Gravity Pulse", "Enchantment", kEffTiers67, 1},
    {"SW.Enchantment.HealthSynergy", "Health Synergy", "Enchantment", kEffTiers68, 2},
    {"SW.Enchantment.LingeringPower", "Lingering Power", "Enchantment", kEffTiers69, 1},
    {"SW.Enchantment.MultiRoll", "Somersault", "Enchantment", kEffTiers70, 1},
    {"SW.Enchantment.Piercing", "Piercing", "Enchantment", kEffTiers71, 2},
    {"SW.Enchantment.PotionBarrier", "Barrier Brew", "Enchantment", kEffTiers72, 1},
    {"SW.Enchantment.Radiance", "Healing Smite", "Enchantment", kEffTiers73, 2},
    {"SW.Enchantment.SoulFireAspect", "Soul Fire Aspect", "Enchantment", kEffTiers74, 1},
    {"SW.Enchantment.SoulInfusedPotion", "Ancient Alchemy", "Enchantment", kEffTiers75, 3},
    {"SW.Enchantment.SpringLoaded", "Springload", "Enchantment", kEffTiers76, 1},
    {"SW.Enchantment.Swirling", "Swirling", "Enchantment", kEffTiers77, 1},
    {"SW.Enchantment.Thundering", "Thundering", "Enchantment", kEffTiers78, 1},
    {"SW.Enchantment.Unstoppable", "Cow Stampede", "Enchantment", kEffTiers79, 1},
};
static const int kRawEffectCount = sizeof(kRawEffects) / sizeof(kRawEffects[0]);

static const RawTalismanLevel kTalLevels0[] = {{"I", "SW.Effect.AmmoCapacity", 1.2f}, {"II", "SW.Effect.AmmoCapacity", 1.4f}, {"III", "SW.Effect.AmmoCapacity", 1.6f}};
static const RawTalismanLevel kTalLevels1[] = {{"I", "SW.Effect.Cooldown", -0.04f}, {"II", "SW.Effect.Cooldown", -0.08f}, {"III", "SW.Effect.Cooldown", -0.14f}};
static const RawTalismanLevel kTalLevels2[] = {{"I", "SW.Effect.Sharpness", 0.1f}, {"II", "SW.Effect.Sharpness", 0.2f}, {"III", "SW.Effect.Sharpness", 0.35f}};
static const RawTalismanLevel kTalLevels3[] = {{"I", "SW.Effect.Looting", 0.02f}, {"II", "SW.Effect.Looting", 0.04f}, {"III", "SW.Effect.Looting", 0.07f}};
static const RawTalismanLevel kTalLevels4[] = {{"I", "SW.Effect.DropChance", 0.01f}, {"II", "SW.Effect.DropChance", 0.02f}, {"III", "SW.Effect.DropChance", 0.04f}};
static const RawTalismanLevel kTalLevels5[] = {{"I", "SW.Effect.EmeraldsIncrease", 0.1f}, {"II", "SW.Effect.EmeraldsIncrease", 0.2f}, {"III", "SW.Effect.EmeraldsIncrease", 0.25f}};
static const RawTalismanLevel kTalLevels6[] = {{"I", "SW.Effect.ExperienceIncrease", 1.04f}, {"II", "SW.Effect.ExperienceIncrease", 1.06f}, {"III", "SW.Effect.ExperienceIncrease", 1.1f}};
static const RawTalismanLevel kTalLevels7[] = {{"I", "SW.Effect.FiringEmerald", 1.0f}, {"II", "SW.Effect.FiringEmerald", 1.75f}, {"III", "SW.Effect.FiringEmerald", 2.0f}};
static const RawTalismanLevel kTalLevels8[] = {{"I", "SW.Effect.Regeneration", 0.01f}, {"II", "SW.Effect.Regeneration", 0.03f}, {"III", "SW.Effect.Regeneration", 0.05f}};
static const RawTalismanLevel kTalLevels9[] = {{"I", "SW.Effect.HealthBoost", 1.2f}, {"II", "SW.Effect.HealthBoost", 1.25f}, {"III", "SW.Effect.HealthBoost", 1.35f}};
static const RawTalismanLevel kTalLevels10[] = {{"I", "SW.Effect.HealthyStrike", 1.0f}, {"II", "SW.Effect.HealthyStrike", 2.0f}, {"III", "SW.Effect.HealthyStrike", 3.0f}};
static const RawTalismanLevel kTalLevels11[] = {{"I", "", 0.0f}, {"II", "", 0.0f}, {"III", "", 0.0f}};
static const RawTalismanLevel kTalLevels12[] = {{"I", "SW.Effect.Plummeting", 1.1f}, {"II", "SW.Effect.Plummeting", 1.2f}, {"III", "SW.Effect.Plummeting", 1.35f}};
static const RawTalismanLevel kTalLevels13[] = {{"I", "SW.Effect.PotionCooldown", -0.1f}, {"II", "SW.Effect.PotionCooldown", -0.15f}, {"III", "SW.Effect.PotionCooldown", -0.25f}};
static const RawTalismanLevel kTalLevels14[] = {{"I", "", 0.0f}, {"II", "", 0.0f}, {"III", "", 0.0f}};
static const RawTalismanLevel kTalLevels15[] = {{"I", "SW.Effect.Power", 0.1f}, {"II", "SW.Effect.Power", 0.15f}, {"III", "SW.Effect.Power", 0.25f}};
static const RawTalismanLevel kTalLevels16[] = {{"I", "SW.Effect.RollCooldown", 0.2f}, {"II", "SW.Effect.RollCooldown", 0.25f}, {"III", "SW.Effect.RollCooldown", 0.45f}};
static const RawTalismanLevel kTalLevels17[] = {{"I", "SW.Effect.SoulMax", 1.15f}, {"II", "SW.Effect.SoulMax", 1.2f}, {"III", "SW.Effect.SoulMax", 1.35f}};
static const RawTalismanLevel kTalLevels18[] = {{"I", "SW.Effect.SoulGather", 0.4f}, {"II", "SW.Effect.SoulGather", 0.8f}, {"III", "SW.Effect.SoulGather", 1.4f}};
static const RawTalismanLevel kTalLevels19[] = {{"I", "SW.Effect.SpeedIncrement", 1.0f}, {"II", "SW.Effect.SpeedIncrement", 2.0f}, {"III", "SW.Effect.SpeedIncrement", 3.0f}};
static const RawTalismanLevel kTalLevels20[] = {{"I", "SW.Effect.StatusBuff", 1.1f}, {"II", "SW.Effect.StatusBuff", 1.15f}, {"III", "SW.Effect.StatusBuff", 1.25f}};
static const RawTalismanLevel kTalLevels21[] = {{"I", "", 0.0f}, {"II", "", 0.0f}, {"III", "", 0.0f}};
static const RawTalismanLevel kTalLevels22[] = {{"I", "", 0.0f}, {"II", "", 0.0f}, {"III", "", 0.0f}};
static const RawTalismanLevel kTalLevels23[] = {{"I", "", 0.0f}, {"II", "", 0.0f}, {"III", "", 0.0f}};
static const RawTalisman kRawTalismans[] = {
    {"SW.Item.Talisman.AmmoCapacity", kTalLevels0, 3},
    {"SW.Item.Talisman.ArtifactCooldown", kTalLevels1, 3},
    {"SW.Item.Talisman.Brawling", kTalLevels2, 3},
    {"SW.Item.Talisman.DoubleDrop", kTalLevels3, 3},
    {"SW.Item.Talisman.DropChance", kTalLevels4, 3},
    {"SW.Item.Talisman.EmeraldIncrease", kTalLevels5, 3},
    {"SW.Item.Talisman.ExperienceIncrease", kTalLevels6, 3},
    {"SW.Item.Talisman.FiringEmeralds", kTalLevels7, 3},
    {"SW.Item.Talisman.Healing", kTalLevels8, 3},
    {"SW.Item.Talisman.HealthBoost", kTalLevels9, 3},
    {"SW.Item.Talisman.HealthyStrike", kTalLevels10, 3},
    {"SW.Item.Talisman.IronGolem", kTalLevels11, 3},
    {"SW.Item.Talisman.Plummeting", kTalLevels12, 3},
    {"SW.Item.Talisman.PotionCooldown", kTalLevels13, 3},
    {"SW.Item.Talisman.Prickle", kTalLevels14, 3},
    {"SW.Item.Talisman.RangedBuff", kTalLevels15, 3},
    {"SW.Item.Talisman.RollingCooldown", kTalLevels16, 3},
    {"SW.Item.Talisman.SoulCapacity", kTalLevels17, 3},
    {"SW.Item.Talisman.SoulGather", kTalLevels18, 3},
    {"SW.Item.Talisman.SpeedIncrement", kTalLevels19, 3},
    {"SW.Item.Talisman.StatusBuff", kTalLevels20, 3},
    {"SW.Item.Talisman.Wobble", kTalLevels21, 3},
    {"SW.Item.Talisman.Wolf", kTalLevels22, 3},
    {"sw.Item.Talisman.Llama", kTalLevels23, 3},
};
static const int kRawTalismanCount = sizeof(kRawTalismans) / sizeof(kRawTalismans[0]);

const std::vector<Mcd2ItemInfo>& Mcd2Catalog::items() {
    static const std::vector<Mcd2ItemInfo> v = [] {
        std::vector<Mcd2ItemInfo> out;
        out.reserve(kRawItemCount);
        for (int i = 0; i < kRawItemCount; ++i) {
            const RawItem& r = kRawItems[i];
            out.push_back({r.id, r.name, r.kind, r.slot, r.setName});
        }
        return out;
    }();
    return v;
}

const std::vector<Mcd2EffectInfo>& Mcd2Catalog::effects() {
    static const std::vector<Mcd2EffectInfo> v = [] {
        std::vector<Mcd2EffectInfo> out;
        out.reserve(kRawEffectCount);
        for (int i = 0; i < kRawEffectCount; ++i) {
            const RawEffect& r = kRawEffects[i];
            std::vector<Mcd2Tier> tiers;
            tiers.reserve(r.tierCount);
            for (int j = 0; j < r.tierCount; ++j) tiers.push_back({r.tiers[j].roman, r.tiers[j].strength});
            out.push_back({r.tag, r.name, r.category, tiers});
        }
        return out;
    }();
    return v;
}

const std::vector<Mcd2TalismanInfo>& Mcd2Catalog::talismans() {
    static const std::vector<Mcd2TalismanInfo> v = [] {
        std::vector<Mcd2TalismanInfo> out;
        out.reserve(kRawTalismanCount);
        for (int i = 0; i < kRawTalismanCount; ++i) {
            const RawTalisman& r = kRawTalismans[i];
            std::vector<Mcd2TalismanLevel> levels;
            levels.reserve(r.levelCount);
            for (int j = 0; j < r.levelCount; ++j) levels.push_back({r.levels[j].roman, r.levels[j].effect, r.levels[j].intensity});
            out.push_back({r.id, levels});
        }
        return out;
    }();
    return v;
}

const std::map<std::string, std::array<int, 3>>& Mcd2Catalog::enchantPointsByRarity() {
    static const std::map<std::string, std::array<int, 3>> v = {
        {"Common", {1, 3, 6}},
        {"Rare", {1, 4, 9}},
        {"Special", {2, 6, 12}},
        {"Unique", {3, 8, 15}},
    };
    return v;
}

const std::vector<int>& Mcd2Catalog::talismanXpThresholds() {
    static const std::vector<int> v = {18480, 73920};
    return v;
}

const Mcd2ItemInfo* Mcd2Catalog::findItem(const std::string& id) {
    if (id.empty()) return nullptr;
    static const std::map<std::string, const Mcd2ItemInfo*> idx = [] {
        std::map<std::string, const Mcd2ItemInfo*> m;
        for (const auto& it : items()) m[it.id] = &it;
        return m;
    }();
    const auto f = idx.find(id);
    return f == idx.end() ? nullptr : f->second;
}

const Mcd2EffectInfo* Mcd2Catalog::findEffect(const std::string& tag) {
    if (tag.empty()) return nullptr;
    static const std::map<std::string, const Mcd2EffectInfo*> idx = [] {
        std::map<std::string, const Mcd2EffectInfo*> m;
        for (const auto& e : effects()) m[e.tag] = &e;
        return m;
    }();
    const auto f = idx.find(tag);
    return f == idx.end() ? nullptr : f->second;
}

const Mcd2TalismanInfo* Mcd2Catalog::findTalisman(const std::string& id) {
    if (id.empty()) return nullptr;
    static const std::map<std::string, const Mcd2TalismanInfo*> idx = [] {
        std::map<std::string, const Mcd2TalismanInfo*> m;
        for (const auto& t : talismans()) m[t.id] = &t;
        return m;
    }();
    const auto f = idx.find(id);
    return f == idx.end() ? nullptr : f->second;
}

std::string Mcd2Catalog::itemName(const std::string& id) {
    const auto* it = findItem(id);
    return it ? it->name : std::string();
}

std::string Mcd2Catalog::effectName(const std::string& tag) {
    const auto* e = findEffect(tag);
    return e ? e->name : std::string();
}

} // namespace dk
