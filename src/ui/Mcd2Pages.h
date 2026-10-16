#pragma once
// Minecraft Dungeons 2 test pages: Player (location, stats, movement), Inventory, World and Camera.
#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QString>
#include <deque>
#include <functional>
#include <utility>
#include <vector>
#include "../mods/Mcd2.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QSlider;
class QTreeWidget;
class QWidget;
class QTimer;
class QVBoxLayout;

namespace dk {

class StatOverlay;

class Mcd2Hub : public QObject {
    Q_OBJECT
public:
    // ready() must make sure the game's engine data is loaded and return true when Dungeons 2 is usable.
    Mcd2Hub(Mcd2& m, std::function<bool()> ready, QObject* parent = nullptr);
    QWidget* playerPage() const { return player_; }
    QWidget* inventoryPage() const { return inventory_; }
    QWidget* motionPage() const { return motion_; }
    QWidget* worldPage() const { return world_; }
    QWidget* cameraPage() const { return camera_; }
    QWidget* nearbyPage() const { return nearby_; }
    QWidget* editorPage() const { return editor_; }
    void tick();                       // called every ~150 ms by the main window
    void stopFreezes();                // game closed or target changed
    // WASD movement (experimental, from Account > Experimental): glides the character while W A S D are held.
    void setWasd(bool on);
    void setWasdRotation(double degrees) { wasdRot_ = degrees; }
    bool wasd() const { return wasd_; }

signals:
    void message(const QString& text);
    void wasdChanged(bool on);                 // switched off by Disable all or when the game closes
    void disableAllRequested();

private:
    struct Row {
        QDoubleSpinBox* spin; QCheckBox* freeze; QWidget* page;
        std::function<bool(double&)> get; std::function<bool(double)> set;
    };
    void buildPlayer();
    void buildMotion();
    void buildInventory();
    void buildWorld();
    void buildCamera();
    void buildDisplayMode(QVBoxLayout* v);     // Camera editor > Display mode: an on-screen overlay over the game window
    void overlayTick();
    void buildNearby();
    void buildEditor();
    void shopTick();
    void readRarity();
    QTreeWidget* rarTree_ = nullptr;
    QLabel* rarNote_ = nullptr;
    QDoubleSpinBox* rarSpin_ = nullptr;
    std::vector<Mcd2::RarityRow> rarRows_;
    std::map<uint64_t, int> rarOrig_;
    // Force [rarity] only: zeroes every other rarity's weight and maxes the chosen one, across every rarity table found.
    // Keep forcing re-reads and reapplies it every few seconds (unlike Set weight above, which is a one-time write),
    // since the table itself has no Freeze-style mechanism and can be reset by a level change.
    QComboBox* forceRarityBox_ = nullptr;
    QCheckBox* forceRarityKeep_ = nullptr;
    QElapsedTimer forceRarityTimer_;
    bool applyForceRarity(const std::string& want, std::string& err);
    QCheckBox* shopOn_ = nullptr;
    QDoubleSpinBox* shopPay_ = nullptr;
    QLabel *shopState_ = nullptr, *shopLast_ = nullptr;
    float shopPrev_[3] = {-1, -1, -1};
    void refreshEditor();
    void wasdStep();
    QTimer* wasdTimer_ = nullptr;
    bool wasd_ = false;
    double wasdRot_ = 0;
    // On/off switch in front of a page's Refresh bar. Off: controls disabled, lists cleared, nothing is read.
    void wirePageSwitch(QCheckBox* on, const std::vector<QWidget*>& deps, const std::vector<QCheckBox*>& autos,
                        const std::function<void()>& refresh, const std::function<void()>& clear);
    QCheckBox *invOn_ = nullptr, *edOn_ = nullptr, *worldOn_ = nullptr, *nearOn_ = nullptr;
    void refreshNearby();
    void saveWaypoints();
    void loadWaypoints();
    void addWaypointRow(const QString& name, const QString& map, double x, double y, double z);
    void addRow(QVBoxLayout* into, QWidget* page, const QString& label, std::function<bool(double&)> get,
                std::function<bool(double)> set, double lo, double hi, int decimals, double step);
    bool scanNow();                    // (re)scan the game's objects with a wait cursor
    void refreshInventory();
    void refreshWorld();
    void applyPreset(const QString& name, float fov, float dfov, float arm, float seek);

    Mcd2& m_;
    std::function<bool()> ready_;
    QWidget *player_ = nullptr, *inventory_ = nullptr, *world_ = nullptr, *camera_ = nullptr, *nearby_ = nullptr, *editor_ = nullptr, *motion_ = nullptr;
    std::vector<Row> rows_;
    QElapsedTimer slow_, invTimer_, worldTimer_;

    // player page
    QLabel *plState_ = nullptr, *plPos_ = nullptr, *plSets_ = nullptr;
    QDoubleSpinBox *tx_ = nullptr, *ty_ = nullptr, *tz_ = nullptr;
    Vec3 saved_;
    bool haveSaved_ = false;
    QCheckBox *diffKeep_ = nullptr, *noClipBox_ = nullptr, *godBox_ = nullptr, *overBox_ = nullptr;
    float overOrig_[4] = {1, 1, 1, 1};
    bool overHave_[4] = {false, false, false, false};
    void overkillOff();
    QDoubleSpinBox* flyStep_ = nullptr;
    QCheckBox* ignBox_ = nullptr;
    int ignTicks_ = 0;
    QCheckBox *infArrowBox_ = nullptr, *infEnchBox_ = nullptr, *noCdBox_ = nullptr;
    float enchFloor_ = -1, potionFloor_ = -1, rollFloor_ = -1;
    QComboBox* diffBox_ = nullptr;
    QLabel* diffNow_ = nullptr;
    // Camera > Display mode: an on-screen overlay over the game window with a few live stats, each its own color,
    // independently toggled, with an adjustable size and a choice of corner. Nothing here is written to the game.
    StatOverlay* overlay_ = nullptr;
    QCheckBox* dmEnabled_ = nullptr;        // master switch: off forces every stat below off too and greys them out
    QCheckBox *dmHealth_ = nullptr, *dmDamage_ = nullptr, *dmDps_ = nullptr, *dmSpeed_ = nullptr, *dmQuest_ = nullptr, *dmCoords_ = nullptr, *dmDeaths_ = nullptr;
    QCheckBox *dmEmeralds_ = nullptr, *dmShards_ = nullptr, *dmEnchant_ = nullptr, *dmSouls_ = nullptr, *dmBlueprint_ = nullptr;
    QDoubleSpinBox* dmBlueprintRadius_ = nullptr;      // how close an enemy needs to be before Nearest blueprint shows it
    QComboBox* dmCorner_ = nullptr;
    QSlider* dmSize_ = nullptr;
    QSpinBox *dmOffX_ = nullptr, *dmOffY_ = nullptr;        // nudges the panel left/right and up/down from its corner
    QLabel* dmNote_ = nullptr;
    Vec3 dmLastPos_;
    QElapsedTimer dmSpeedTimer_;
    bool dmHavePos_ = false;
    double dmSpeedSmoothed_ = 0;
    std::deque<std::pair<qint64, double>> dmDpsHits_;      // self-contained rolling 5s damage-per-second window, fed by Mcd2::dpsPoll()
    // nearby page
    QTreeWidget* nearTree_ = nullptr;
    QLabel* nearNote_ = nullptr;
    QDoubleSpinBox* nearRadius_ = nullptr;
    QCheckBox *nearChests_ = nullptr, *nearMobs_ = nullptr, *nearClosed_ = nullptr, *nearAuto_ = nullptr;
    QElapsedTimer nearTimer_;
    QElapsedTimer nearRescanTimer_;   // Update automatically: how often it re-scans the game for newly loaded chests/enemies (not just re-reading the ones already known)
    // waypoints (World Viewer)
    QTreeWidget* wpTree_ = nullptr;
    QLineEdit* wpName_ = nullptr;
    bool wpLoading_ = false;
    // inventory editor
    QTreeWidget* edTree_ = nullptr;
    QLabel* edNote_ = nullptr;
    QComboBox *edItem_ = nullptr, *edRarity_ = nullptr;
    QSpinBox *edLevel_ = nullptr, *edStack_ = nullptr, *edRerolls_ = nullptr;
    QDoubleSpinBox *edPower_ = nullptr, *edXp_ = nullptr;
    QLabel *edProps_ = nullptr;
    QComboBox* edJump_ = nullptr;
    QComboBox* edRarityFilter_ = nullptr;   // "All rarities" or one specific rarity - narrows the list before you pick what to edit
    QCheckBox *edEquipped_ = nullptr, *edAll_ = nullptr;
    QWidget *edLevelRow_ = nullptr, *edXpRow_ = nullptr;        // Level / Item XP rows: only shown for Talismans
    bool edLoading_ = false;                                    // true while fields are being filled from a selected row (suppresses instant-apply)
    // Selected item > Enchants: one row per enchant actually found on the item (tag starting "SW.Enchantment."),
    // each with its own Enchant/Tier pick and Points field, wired straight to Mcd2::editEffect. Rebuilt whenever
    // the selected item changes; shows a note instead when the item has no enchant. Editing only - it can't add
    // an enchant slot that isn't already there.
    QVBoxLayout* edEnchantLayout_ = nullptr;
    QLabel* edEnchantNote_ = nullptr;
    struct EnchantRow { QWidget* row = nullptr; QComboBox *tag = nullptr, *tier = nullptr; QSpinBox* points = nullptr;
                         uint64_t itemAddr = 0; std::string itemName; uint64_t effAddr = 0; int group = -1; };
    std::vector<EnchantRow> edEnchantRows_;
    void rebuildEnchants(const Mcd2::InvRow& row);
    QElapsedTimer edTimer_;
    std::vector<Mcd2::InvRow> edRows_;
    QComboBox* edCategoryBox_ = nullptr;     // Delete category: every slot (Helmet, Melee, ...) seen in the full inventory, not just what's currently shown
    std::vector<Mcd2::InvRow> edAllRows_;    // the full, unfiltered read from the last Refresh - Delete category works off this, ignoring the Rarity filter/toggles above
    // inventory page
    QTreeWidget* invTree_ = nullptr;
    QLabel* invNote_ = nullptr;
    QCheckBox* invAuto_ = nullptr;
    // missions (World)
    QTreeWidget* questTree_ = nullptr;
    QLabel* questNote_ = nullptr;
    QLineEdit* questFilter_ = nullptr;
    std::vector<Mcd2::QuestRow> questRows_;
    void refreshQuests();
    // world page
    QTreeWidget* worldTree_ = nullptr;
    QLabel *worldNote_ = nullptr, *worldMap_ = nullptr;
    QCheckBox* worldAuto_ = nullptr;
    // camera page
    QLabel* camNote_ = nullptr;
    QCheckBox* camKeep_ = nullptr;
    int camRow_[Mcd2::CamCount] = {0, 0, 0, 0};
    int timeRow_ = -1;
    float camOrig_[Mcd2::CamCount] = {0, 0, 0, 0};
    bool camHave_[Mcd2::CamCount] = {false, false, false, false};
    std::vector<QCheckBox*> camFreezes_;
};

} // namespace dk
