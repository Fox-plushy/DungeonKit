#pragma once
// Minecraft Dungeons II > Tools: Hide version text / Hide join ID. Best effort: finds the game's on-screen text widget in memory and
// collapses it and/or rewrites its text. The game may not redraw a widget until it refreshes it, so this may not always show.
#include <QFrame>
#include <QMutex>
#include <QTimer>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "../core/UE4.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QThread;

namespace dk {

class Process;

class WidgetHideCard : public QFrame {
    Q_OBJECT
public:
    struct Spec {
        QString title, hint, settingsKey, widgetClass, defaultText;
        std::vector<std::string> textProps;       // properties of the widget that hold its text blocks
    };
    WidgetHideCard(const Spec& spec, std::function<Process*()> proc, QWidget* parent = nullptr, bool embedded = false);
    void setAutoUpdate(bool on) { auto_ = on; if (on) pending_ = 0; }
    void applyNow() { pending_ = 14; }

private:
    struct SavedText { uint64_t data = 0; std::wstring original, written; };
    struct SavedVis { uint8_t original = 0, written = 0; };
    void tick();
    void saveSettings();
    void rescan(Process& p);
    bool readFString(Process& p, uint64_t addr, uint64_t& data, int32_t& num, int32_t& max, std::wstring& text);
    uint64_t findTextString(Process& p, uint64_t textBlock);     // address of the FString behind a text block's Text, or 0
    void setText(Process& p, uint64_t addr, const std::wstring& want);
    void setVisible(Process& p, uint64_t obj, bool hide);
    void restoreAll(Process* p);

    Spec spec_;
    std::function<Process*()> proc_;
    UE4 ue_;
    QTimer timer_;
    QComboBox* mode_ = nullptr;
    QLineEdit* custom_ = nullptr;
    QLabel* status_ = nullptr;
    std::map<uint64_t, SavedText> savedText_;
    std::map<uint64_t, SavedVis> savedVis_;
    std::vector<uint64_t> widgets_;
    QMutex scanLock_;
    std::vector<uint64_t> scanResult_;
    bool scanDone_ = false, scanning_ = false;
    qint64 nextScan_ = 0, nextInit_ = 0;
    bool auto_ = true;
    int pending_ = 0;                          // ticks left to apply when automatic updating is off
    unsigned long pid_ = 0, ueFor_ = 0;
};

} // namespace dk
