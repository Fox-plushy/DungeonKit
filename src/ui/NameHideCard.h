#pragma once
// Minecraft Dungeons > Tools > Hide my name: streamer-friendly names. Overwrites the name text of the player(s) in the game's memory so it
// does not show on your screen. Only changes what your own game shows; other players still see the real name.
//
// The normal path (World -> GameState -> PlayerArray, or PlayerController -> PlayerState) only exists once you've loaded into a level or
// the camp. On the main menu there is no World yet, so that path finds nothing. To cover the main menu too, this also runs a background
// scan of every live object in the game (the same technique WidgetHideCard uses for on-screen widgets) looking for any object whose class
// name contains "PlayerState" and applying the same name fields to it, regardless of whether a level is loaded.
#include <QFrame>
#include <QMutex>
#include <QTimer>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "../core/UE4.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QThread;

namespace dk {

class Process;

class NameHideCard : public QFrame {
    Q_OBJECT
public:
    NameHideCard(std::function<Process*()> proc, const QString& settingsKey, QWidget* parent = nullptr, bool embedded = false);
    void setAutoUpdate(bool on) { auto_ = on; if (on) pending_ = 0; }
    void applyNow() { pending_ = 6; }
signals:
    void message(const QString& text);

private:
    struct Saved { uint64_t data = 0; std::wstring original, written; };
    void tick();
    void saveSettings();
    void updateEnabled();
    bool readString(Process& p, uint64_t addr, uint64_t& data, int32_t& num, int32_t& max, std::wstring& text);
    int applyString(Process& p, uint64_t addr, const std::wstring& want, bool& cut);   // 1 changed, 0 already fine, -1 failed
    void restoreString(Process& p, uint64_t addr, const Saved& s);
    void restoreAll(Process* p);
    bool localPlayerState(uint64_t& ps);
    void allPlayerStates(std::vector<uint64_t>& out);
    void rescanAllPlayerStates(Process& p);    // background full-object-table scan, for when there's no World/GameState yet (main menu)

    std::function<Process*()> proc_;
    UE4 ue_;
    QTimer timer_;
    QComboBox* mode_ = nullptr;
    QLineEdit* custom_ = nullptr;
    QCheckBox* others_ = nullptr;
    QLabel* status_ = nullptr;
    std::map<uint64_t, Saved> saved_;          // address of a name string -> what it was before
    QString key_;
    bool auto_ = true;
    int pending_ = 0;                          // ticks left to apply once when automatic updating is off
    unsigned long pid_ = 0, ueFor_ = 0;
    qint64 nextInitTry_ = 0;

    // background scan for PlayerState-like objects when there is no World/GameState to walk (e.g. the main menu)
    std::vector<uint64_t> scannedStates_;
    QMutex scanLock_;
    std::vector<uint64_t> scanResult_;
    bool scanDone_ = false, scanning_ = false;
    qint64 nextScan_ = 0;
};

} // namespace dk
