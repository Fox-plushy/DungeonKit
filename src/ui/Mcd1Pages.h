#pragma once
#include <QElapsedTimer>
#include <QStringList>
#include <QThread>
#include <atomic>
#include <vector>
// Minecraft Dungeons pages that work through the game's object tables (UE reflection), so they do not depend on
// a code pattern and behave the same on Steam, the Xbox App and the Minecraft Launcher:
//   Camera Editor (field of view, camera distance, presets) and World (mission, tower and merchant read-outs).
#include <QWidget>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>
#include "../core/UE4.h"
#include "../mods/PlayerTools.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QTreeWidget;
class QVBoxLayout;

namespace dk {

class Mcd1CameraPage : public QWidget {
    Q_OBJECT
public:
    Mcd1CameraPage(std::shared_ptr<UE4> ue, std::function<Process*()> proc, QWidget* parent = nullptr);
    void restore();                          // put the camera back to how the game had it and stop holding values
signals:
    void message(const QString& text);
    void disableAllRequested();
private:
    enum Kind { Fov = 0, DefaultFov, Arm, KindCount };
    struct Row { QDoubleSpinBox* spin = nullptr; QCheckBox* freeze = nullptr; };
    bool readVal(Process* p, int kind, float& out);
    bool writeVal(Process* p, int kind, float v);
    bool controller(uint64_t& pc);
    void tick();
    void applyPreset(const QString& name, float fovScale, float armScale);
    void captureOriginals(Process* p);
    void setWorldSpeed(double value, bool hold);

    QDoubleSpinBox* speedSpin_ = nullptr;
    QCheckBox* speedFreeze_ = nullptr;

    std::shared_ptr<UE4> ue_;
    std::function<Process*()> proc_;
    TimeControl time_;
    Row rows_[KindCount];
    bool have_[KindCount] = {false, false, false};
    float orig_[KindCount] = {0, 0, 0};
    unsigned long pid_ = 0;
    QCheckBox* keep_ = nullptr;
    QLabel* note_ = nullptr;
    QTimer timer_;
};

class Mcd1WorldPage : public QWidget {
    Q_OBJECT
public:
    Mcd1WorldPage(std::shared_ptr<UE4> ue, std::function<Process*()> proc, QWidget* parent = nullptr);
    ~Mcd1WorldPage() override;
    void addTeleports(QWidget* w);           // the Teleports section sits at the top of this page
signals:
    void message(const QString& text);
    void disableAllRequested();
private:
    struct Line { QLabel* value = nullptr; };
    QLabel* addLine(QVBoxLayout* into, const QString& label);
    void tick();
    uint64_t findGameBP(uint64_t world);
    void scanRules(Process* p, std::vector<uint64_t>& found);
    void startRulesScan();
    bool readFText(uint64_t addr, std::string& out);
    void collectRules(uint64_t obj, QStringList& lines, int& total, int& unread, int& rows);
    void updateRules();
    std::vector<uint64_t> rulesCands_, scanFound_;
    QThread* scanThread_ = nullptr;
    CsLock scanMu_;
    std::atomic<bool> scanBusy_{false}, scanDone_{false};
    QElapsedTimer rulesClock_;
    QLabel* rulesText_ = nullptr;
    QLabel* rulesNote_ = nullptr;
    uint64_t gameBP_ = 0, gbWorld_ = 0;
    bool prop(uint64_t obj, const char* name, uint32_t& off);
    bool readFString(uint64_t addr, std::string& out);
    bool readMapList(uint64_t mapAddr, std::vector<std::string>& names, int& count, bool wantNames);
    std::string levelName(unsigned v) const;

    std::shared_ptr<UE4> ue_;
    std::function<Process*()> proc_;
    QLabel* state_ = nullptr;
    QVBoxLayout* teleportSlot_ = nullptr;
    // live mission
    QLabel *lives_, *livesLost_, *revive_, *spawned_, *killed_, *bosses_, *parts_, *power_;
    // current mission
    QLabel *mLevel_, *mDiff_, *mThreat_, *mApoc_, *mSeed_, *mLost_, *mParts_, *mRaid_, *mAffectors_, *mFlags_, *mMap_;
    // tower
    QLabel *tState_, *tId_, *tFloors_, *tSeed_, *tPlayers_, *tRewards_, *tManager_;
    QTreeWidget* floors_ = nullptr;
    // merchants + saves
    QLabel *merchCount_, *saveCount_;
    QTreeWidget* merchants_ = nullptr;
    QTimer timer_;
    unsigned long lastKey_ = 0;
};

} // namespace dk
