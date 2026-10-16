#pragma once
// Minecraft Dungeons 2 (test build): everything here goes through the UE reflection walker, so property offsets
// are looked up by name at run time. Layouts of the few raw structs (inventory, tag containers) come from the
// game's own class dump.
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "../core/UE4.h"

namespace dk {

struct Vec3 { double x = 0, y = 0, z = 0; };

class Mcd2 {
public:
    explicit Mcd2(std::shared_ptr<UE4> ue) : ue_(std::move(ue)) {}
    bool ok() const { return ue_ && ue_->ready(); }
    void reset();

    struct Player { uint64_t eng = 0, world = 0, controller = 0, pawn = 0, state = 0, root = 0, camMgr = 0; };
    bool player(Player& p);
    std::string worldName();

    // ---- location (read; write is best effort)
    bool getLocation(Vec3& v, double* yaw = nullptr);
    bool setLocation(const Vec3& v);
    bool addMoveInput(double x, double y);                      // write the pawn's ControlInputVector (the game's own walk input)
    bool gameForeground() const;                                // the game window is the active window

    // ---- gameplay attributes (health, emeralds, xp ...)
    struct AttrDef { const char* label; const char* set; const char* prop; const char* group = nullptr; };   // group: which card the stat is shown in (nullptr = Player)
    static const std::vector<AttrDef>& attrDefs();
    bool getAttr(int def, float& cur);
    bool setAttr(int def, float v);
    std::string attrSummary();

    // ---- character movement component (speed, jump, gravity ...)
    struct MoveDef { const char* label; const char* prop; };
    static const std::vector<MoveDef>& moveDefs();
    bool getMove(int def, float& v);
    bool setMove(int def, float v);

    // ---- character movement mode (fly) and world time scale (photo mode)
    bool getMoveMode(int& mode);
    bool setMoveMode(int mode);                                 // 1 walking, 3 falling, 5 flying
    bool setCollision(bool on);                                 // Actor.bActorEnableCollision of the player pawn (no clip)
    bool getCollision(bool& on);
    bool getTimeScale(float& v);
    bool setTimeScale(float v);

    // ---- difficulty (lives on the game state)
    bool getDifficulty(std::string& tag, bool& locked);
    bool setDifficulty(const std::string& tag);                 // "SW.Difficulty.Low" / ".Normal" / ".High"
    bool setDifficultyLocked(bool locked);
    uint32_t findNameIndex(const std::string& name);            // 0xFFFFFFFF when the game has no such name

    // ---- camera
    enum CamKind { Fov = 0, DefaultFov, ArmLength, SeekArm, CamCount };
    bool getCam(int kind, float& v);
    bool setCam(int kind, float v);

    // ---- camera offset: the spring arm's own built-in SocketOffset, shifting the camera from its normal spot
    // while it still follows the player as usual. Same spring arm object as Camera distance (arm length) above.
    bool getCamOffset(Vec3& v);
    bool setCamOffset(const Vec3& v);

    // ---- live object scan (inventory component, exploration component, cameras ...)
    bool rescan(std::string& err);
    bool scanned() const { return scanned_; }

    struct EffRow { uint64_t addr = 0; std::string tag; float intensity = 0; int points = 0; int group = -1; };   // group -1: the item's own effects, 0.. : effects granted by item level N+1
    struct InvRow { std::string slot, item, rarity, equipped, effects; int level = 0, stack = 1, rerolls = 0; float power = 0, powerBase = 0, xp = 0; uint64_t addr = 0; std::vector<EffRow> fx; std::vector<std::string> props; };
    bool inventory(std::vector<InvRow>& rows, std::string& note);

    // ---- inventory editing. `expect` is the item name the row had when it was listed; nothing is written if the
    // entry at that address no longer holds that item (the list moved), so a stale row can never hit another item.
    struct ItemEdit { int level = -1, stack = -1, rerolls = -1; double power = -1, xp = -1; std::string rarity, item; };
    bool editItem(uint64_t addr, const std::string& expect, const ItemEdit& e, std::string& err);
    // effAddr is the address of one effect row from inventory(); it is checked against the item's current effect arrays before anything is written.
    // tag empty = keep the effect; intensity NaN = keep the strength; points < 0 = keep the points.
    bool editEffect(uint64_t entryAddr, const std::string& expect, uint64_t effAddr, const std::string& tag, float intensity, int points, std::string& err);
    // Removes entries from the inventory list: each target is (entry address, item name it had when listed). Entries whose item no
    // longer matches are skipped. The following entries are shifted down and the slot's count is reduced. Returns how many were removed.
    struct DeleteTarget { uint64_t addr = 0; std::string expect; };
    int deleteItems(const std::vector<DeleteTarget>& targets, std::string& err);
    std::vector<std::string> namesWithPrefix(const std::string& prefix, size_t cap = 4000);

    // ---- item rarity: the game's rarity table (DataTable whose row struct is RarityTableRow). Each row has a RarityWeight; the chance of
    // a rarity is its weight divided by the total of all weights. Weights are only changed in memory.
    struct RarityRow { uint64_t addr = 0 /* the weight itself */, tagAddr = 0; std::string table, name, tag; int weight = 0; };
    bool rarityRows(std::vector<RarityRow>& out, std::string& note);
    bool setRarityWeight(const RarityRow& r, int weight, std::string& err);

    // ---- enemies ignore you (experimental). Sets the player's aggro multiplier and every loaded enemy's sight and aggro radius to 0,
    // keeps the old values, and puts them back when switched off. Needs a scan (rescan) so the enemies are known.
    int enemyIgnoreOn(std::string& note);                       // returns how many values were changed, -1 on failure
    void enemyIgnoreTick();                                     // re-applies (the game can reset values, new enemies can appear after a scan)
    void enemyIgnoreOff();
    bool enemyIgnoreActive() const { return ignOn_; }

    // ---- region the player is in (the Sift is a region inside the Overworld map). Empty when it cannot be read.
    std::string regionName();

    struct WorldInfo { std::string map; std::vector<std::string> stations, gimmicks, cutscenes; int doors = -1; };
    bool world(WorldInfo& w, std::string& note);

    // ---- missions (quests): the game's LiveQuest objects. State: 1 Locked, 2 Active, 3 Available, 4 Completed, 5 Failed, 6 Unavailable
    struct QuestRow { uint64_t obj = 0; std::string id, name; int state = 0; };
    bool quests(std::vector<QuestRow>& out, std::string& note);
    // Writes a mission's State. `expectId` is the ID it had when listed; nothing is written if the object no longer has that ID.
    bool setQuestState(uint64_t obj, const std::string& expectId, int state);
    int unlockQuests(std::string& note);                        // Locked / Unavailable -> Available; returns how many were changed
    static const char* questStateName(int s);

    struct Near { std::string kind, name, state; Vec3 pos; double dist = 0; };
    bool nearby(double radius, bool chests, bool mobs, bool onlyClosed, std::vector<Near>& out, std::string& note);

    // ---- overlay (Camera > Display mode): a small HUD drawn over the game window showing live stats. This just
    // reads values that already exist elsewhere in this app (health, damage-related attributes, walk speed, the
    // active quest, emeralds/echo shards/enchantment points/souls); it changes nothing. `gameWindowRect` locates
    // the game's own window so the overlay can track it.
    struct OverlayStats { bool ok = false; float health = 0, healthMax = 0; float dmgMult = 0, critChance = 0; std::string questName, questState; float emeralds = 0, echoShards = 0, enchantPoints = 0, souls = 0; };
    bool overlayStats(OverlayStats& s);
    bool gameWindowRect(long& x, long& y, long& w, long& h);

    // ---- nearest blueprint (Camera & Display): the class name of the closest enemy within range, the same
    // "blueprint name" shown by Nearby Viewer's own Name column, reusing the same actor scan.
    bool nearestEnemy(double radius, std::string& name, double& dist);

    // ---- damage per second (Camera & Display's own reading - entirely self-contained, no Tools > Experiments
    // card involved). It rescans for nearby enemies' Health attribute every few seconds (the same object scan
    // Nearby Viewer's own auto-refresh already does) and simply re-reads each one's current Health every call in
    // between. A drop since the last read is counted as damage dealt; a rise (healing) or the enemy disappearing
    // is ignored. Call dpsPoll() every tick while the Damage per second stat is on - it returns the damage dealt
    // since the last call (almost always 0). Call dpsReset() once when it's switched off, so a stale enemy list
    // from the last level isn't reused the next time it's switched back on.
    double dpsPoll();
    void dpsReset();

    // ---- deaths this session (Camera & Display). Tries the same kind of GameState counter Minecraft Dungeons
    // exposes (LivesLostThisSession) under a few plausible names, in case Minecraft Dungeons II kept an
    // equivalent one; none of these are confirmed to exist in this game. If none match, falls back to counting
    // every time your own Health crosses from above zero to zero or below, counted only from the moment the
    // Deaths stat is turned on (deathReset() clears it). If the game has a co-op "knocked down, then revived"
    // state that also zeroes Health, the fallback counts each knockdown, not just a full game-over - there is no
    // way to tell those apart from outside without a confirmed counter for it.
    int deathPoll();
    void deathReset();

private:
    struct ObjRef { uint64_t obj = 0, outer = 0; std::string cls, name; };
    bool ownedBy(uint64_t obj, uint64_t owner);
    std::string tagAt(uint64_t addr);                           // FGameplayTag -> text
    bool resolveSets(const Player& p);
    bool floatAt(uint64_t obj, const char* prop, uint64_t& addr);
    bool tagList(uint64_t arrAddr, std::vector<std::string>& out);
    bool readBool(uint64_t obj, const char* prop, bool& v);
    bool writeBool(uint64_t obj, const char* prop, bool v);
    bool worldSettings(uint64_t& ws);
    bool gameState(uint64_t& gs);
    struct ActorRef { uint64_t obj = 0; int kind = 0; std::string name; };       // kind 1 chest, 2 mob
    std::vector<uint64_t> worldPosAddrs(uint64_t root, const Vec3& rel);

    std::shared_ptr<UE4> ue_;
    bool scanned_ = false;
    std::vector<ObjRef> inv_, expl_, cams_, arms_, quests_;
    std::vector<ActorRef> actors_;
    std::map<std::string, uint32_t> nameIdx_;
    uint32_t slotSize_ = 0, entrySize_ = 0;
    std::vector<std::pair<std::string, uint64_t>> sets_;
    uint64_t setsAt_ = 0;
    uint64_t camScanAt_ = 0;
    struct IgnoreSave { uint64_t obj = 0, cls = 0, addr = 0; float base = 0, cur = 0; };
    std::vector<IgnoreSave> ign_;
    bool ignOn_ = false;
    int applyIgnore();
    uint64_t mapsSub_ = 0, mapsWorld_ = 0;
    bool camOwned(uint64_t obj, const Player& p);
    std::vector<uint64_t> camTargets(int kind, const Player& p);
    uint64_t setsPawn_ = 0;
    std::map<uint64_t, std::vector<uint32_t>> posOffs_;         // root component -> offsets of the world-space copy
    struct DpsEnemy { uint64_t obj = 0, cls = 0, addr = 0; float last = 0; bool primed = false; };
    std::vector<DpsEnemy> dpsEnemies_;
    uint64_t dpsScanAt_ = 0;
    uint64_t blueprintScanAt_ = 0;
    int deathCount_ = 0;
    bool deathPrimed_ = false;
    float deathLastHealth_ = 0;
};

} // namespace dk
