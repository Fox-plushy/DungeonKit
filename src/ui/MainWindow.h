#include <QHash>
#include <QDateTime>
#include <QStringList>
#pragma once
#include <QElapsedTimer>
#include <QMainWindow>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include "../core/Process.h"
#include "../mods/Mcd2.h"
#include "../mods/Mods.h"
#include "Identity.h"
#include "Theme.h"

class QButtonGroup;
namespace dk { class ShopPricesCard; class ModMakerCard; class ProbeCard; struct ConsoleCmdGroup; }
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QPushButton;
class QLabel;
class QLineEdit;
class QPixmap;
class QPlainTextEdit;
class QSpinBox;
class QTreeWidget;
class QStackedWidget;
class QShowEvent;
class QTimer;
class QVBoxLayout;
class QWidget;

namespace dk {

class ItemsPage;
class ItemViewerPage;
class Mcd2Hub;
class TeleportsPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void setRole(Identity::Role role);   // reserved name unlocked with its password

protected:
    void showEvent(QShowEvent* e) override;

private slots:
    void pollGame();      // 1 s : attach / detach / status box
    void tickValues();    // 150 ms : live value refresh + freezes + hotkeys

private:
    // ---- build
    QWidget* buildSidebar();
    QWidget* buildHome();
    QWidget* buildMods();
    QWidget* buildAccount();
    QWidget* buildExperiments();
    void setBeta(bool on);
    bool betaOn_ = false;
    QWidget* buildCustomize();
    QWidget* buildNoClip();
    QWidget* buildPakDirectory();
    QWidget* buildWebsites();
    QWidget* buildBackup();
    QWidget* buildChangelog();
    QWidget* buildTools();
    QWidget* buildTest();
    QFrame* addSummonCard();
    void addCommandsCard(QVBoxLayout* v, const ConsoleCmdGroup* groups, size_t groupCount, bool mcd1);
    void setTarget(Process::Target t);
    bool mcd2Ready();
    void runTestCheck();
    QWidget* buildInformation();
    QFrame*  card(QVBoxLayout* into, const QString& title);
    QFrame*  cardStandalone(const QString& title);     // like card(), but not yet inserted into a layout (for wrapCollapsible)
    // Wraps an already-built card widget with a clickable header that shows/hides it, to save space on crowded
    // pages. The open/closed state is remembered (per key) between runs. Used on the Experiments page.
    QWidget* wrapCollapsible(QVBoxLayout* into, QWidget* content, const QString& key, bool defaultOpen);
    void addToggle(QVBoxLayout* into, Mod* mod);
    void addIntRow(QVBoxLayout* into, const QString& label, std::function<bool(int32_t&)> get, std::function<bool(int32_t)> set, Mod* owner, bool withFreeze = true);
    void addFloatRow(QVBoxLayout* into, const QString& label, std::function<bool(float&)> get, std::function<bool(float)> set, Mod* owner);

    // ---- helpers
    void applyTheme(bool persist = true);
    void applyLanguage();
    void refreshMods();
    void refreshBackupInfo();
    void renameSelectedMods(bool disable);
    // ---- sidebar layout (editable from Customize)
    struct NavCat { QString name; std::vector<int> pages; };
    static std::vector<NavCat> defaultNavLayout();
    void loadNavLayout();
    void saveNavLayout();
    void rebuildNav();
    void fillNavTree(int selCat, int selPos);
    void moveNav(int dir);
    void refreshNavIcons();
    void updateUserLabel();
    void updateTitle();
    void updateHomeStatus();
    void refreshFolders();
    void disableAll();
    void startDump(int mode);       // 0 names, 1 objects, 2 classes and fields
    void applyTitleBar();
    void setMessage(const QString& text);
    void setStatus(const QString& state, const QString& line1, const QString& line2);
    void refreshEnabledState();
    void disableAllMods();
    void toggleMod(Mod* mod, QCheckBox* box, bool on);
    QPixmap logoPixmap(int size) const;

    // ---- state
    Process proc_;
    std::unique_ptr<Mcd2> mcd2_;
    Mcd2Hub* mcd2Hub_ = nullptr;
    unsigned long mcd2Pid_ = 0;
    QElapsedTimer mcd2Tried_;
    Process procAlt_;                 // Dungeons 2 test: Dungeons.exe attached alongside Dungeons-WinGDK-Shipping.exe
    Process* engineProc();            // the attached process whose engine data can be read (Dungeons 2 test)
    // Minecraft Dungeons II's displayed version: read live from the game's own GeneralProjectSettings.ProjectVersion
    // (by name, like everything else in dk::UE4) instead of only the module-size table, so it stays correct on any
    // platform or future patch - Microsoft Store/Xbox, Steam, whatever - without needing a table entry for it.
    QString mcd2LiveVersion_;
    QString mcd2LiveVersionFor(Process& p);               // scans the live object table once per process; "" if not found yet
    unsigned long mcd2LiveVersionPid_ = 0;
    qint64 mcd2LiveVersionNextTry_ = 0;
    static QString mcd2PlatformLabel(const std::string& exeName);
    Process::Target target_ = Process::Target::Dungeons;
    QLabel* testInfo_ = nullptr;
    QLabel* testReport_ = nullptr;
    QDoubleSpinBox* testSpeed_ = nullptr;
    std::unique_ptr<GameProfile> game_;
    Theme theme_;

    QStackedWidget* pages_ = nullptr;
    QWidget* navHolder_ = nullptr;
    std::vector<NavCat> navCats_;
    QTreeWidget* navTree_ = nullptr;
    QButtonGroup* navGroup_ = nullptr;
    QLabel* message_ = nullptr;
    QLabel* welcome_ = nullptr;
    QLabel* statusDot_ = nullptr;
    QLabel* statusTitle_ = nullptr;
    QLabel* statusLine1_ = nullptr;
    QLabel* statusLine2_ = nullptr;
    ItemsPage* itemsPage_ = nullptr;
    ItemViewerPage* viewerPage_ = nullptr;
    TeleportsPage* teleportsPage_ = nullptr;
    int listVersion_ = -1;
    QLabel* sidebarLogo_ = nullptr;
    QLabel* playerMsg_ = nullptr;
    QLabel* toolsMsg_ = nullptr;
    QLabel* dumpStatus_ = nullptr;
    QLabel* testDumpStatus_ = nullptr;
    QPushButton* testDumpBtns_[3] = {nullptr, nullptr, nullptr};
    QPushButton* dumpNamesBtn_ = nullptr;
    QPushButton* dumpObjectsBtn_ = nullptr;
    QPushButton* dumpClassesBtn_ = nullptr;
    class Mcd1CameraPage* mcd1Cam_ = nullptr;
    class Mcd1WorldPage* mcd1World_ = nullptr;
    ShopPricesCard* shopCard_ = nullptr;
    ModMakerCard* modMaker_ = nullptr;
    QPushButton* expDumpBtns_[3] = {nullptr, nullptr, nullptr};
    QLabel* expDumpStatus_ = nullptr;
    std::atomic<int> dumpCount_{0};
    bool dumpRunning_ = false;
    QLabel* homeDot_ = nullptr;
    QLabel* homeState_ = nullptr;
    QLabel* homeStateText_ = nullptr;
    QLabel* homeGame_ = nullptr;
    QLabel* paksLabel_ = nullptr;
    QLabel* paksCount_ = nullptr;
    QLabel* paksMsg_ = nullptr;
    QLabel* paksRunning_ = nullptr;
    QLineEdit* paksEdit_ = nullptr;
    QTreeWidget* modTree_ = nullptr;
    QLabel* modsSummary_ = nullptr;
    QLabel* backupInfo_ = nullptr;
    QLabel* backupResult_ = nullptr;
    QString lastBackupText_, backupLog_;
    QCheckBox* backupMods_ = nullptr;
    std::shared_ptr<class DiscordPresence> discord_;
    QTimer* discordTimer_ = nullptr;
    QLabel* discordStatus_ = nullptr;
    qint64 discordStart_ = 0;
    QString discordLast_;
    void updatePresence(bool force);
    void syncBackupMods();
    QPlainTextEdit* backupDebug_ = nullptr;
    void refreshBackupDebug();
    void logEvent(const QString& text);
    void watchToggles();
    QStringList events_;
    QHash<QWidget*, bool> toggleState_;
    unsigned long lastPid_ = 0;
    QDateTime openedAt_;
    QTimer* i18nTimer_ = nullptr;
    QString paksDir_;
    unsigned long foldersPid_ = 0xFFFFFFFFul;
    QLabel* playerInfo_ = nullptr;
    QLabel* playerDiag_ = nullptr;
    QString lastTitle_;
    QLabel* sidebarUser_ = nullptr;
    QLineEdit* userEdit_ = nullptr;
    QComboBox* gameCombo_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    QTimer* tickTimer_ = nullptr;

    struct ToggleRef { Mod* mod; QCheckBox* box; };
    std::vector<ToggleRef> toggles_;

    struct Row {
        Mod* owner;
        QSpinBox* ispin = nullptr;
        QDoubleSpinBox* fspin = nullptr;
        QCheckBox* freeze = nullptr;
        std::function<bool(int32_t&)> iget;  std::function<bool(int32_t)> iset;
        std::function<bool(float&)>   fget;  std::function<bool(float)>   fset;
        QWidget* holder = nullptr;
    };
    std::vector<Row> rows_;

    bool keyF5_ = false, keyF6_ = false, keyPlus_ = false, keyMinus_ = false, keyPause_ = false;
    QString username_;
    Identity::Role role_ = Identity::Role::None;
    std::vector<QPushButton*> navButtons_;
};

} // namespace dk
