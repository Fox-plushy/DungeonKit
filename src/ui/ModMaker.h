#pragma once
// Experiments > Mod Maker: browse the live game's objects and properties (the same data as the class dump) and turn any
// number or on/off value into your own mod: set it once, or hold it. Mods are saved and can be shared as a short code.
#include <QFrame>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "../core/UE4.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace dk {

class Process;

class ModMakerCard : public QFrame {
    Q_OBJECT
public:
    ModMakerCard(std::shared_ptr<UE4> ue, std::function<Process*()> proc, QWidget* parent = nullptr);
    ~ModMakerCard() override;
    void stopAll();                          // switch every custom mod off
signals:
    void message(const QString& text);

private:
    struct Spec { QString root; QStringList path; QString type; };       // path: property names from the root; the last one is the value
    struct Custom { QString name; Spec spec; double value = 0; bool on = false; };
    struct Hit { uint64_t addr = 0; std::string cls, name; bool transient = false; };
    struct Target { uint64_t addr = 0; UE4::Prop prop; };

    bool ready(Process* p);
    uint64_t rootObject(Process* p, const QString& root);
    uint64_t followPath(Process* p, const Spec& s, int steps);            // object reached after `steps` pointer steps
    bool resolve(Process* p, const Spec& s, Target& t);
    bool readValue(Process* p, const Target& t, const QString& type, double& v);
    bool writeValue(Process* p, const Target& t, const QString& type, double v);
    static bool numericType(const QString& type);

    void refreshTree();
    void updateValues();
    void selectField(QTreeWidgetItem* it);
    void descend(QTreeWidgetItem* it);
    Spec currentSpec(const QString& field, const QString& type) const;
    QString describe(const Spec& s) const;
    void startScan(const QString& needle, bool exact, bool forUi);
    void scanWorker(Process* p, const std::string& needle, bool exact, std::vector<Hit>& out);
    void pollScan();
    void applyAll();
    void saveMods();
    void loadMods();
    void refreshList();
    QString toCode(const Custom& c) const;
    bool fromCode(const QString& json, int& added);

    std::shared_ptr<UE4> ue_;
    std::function<Process*()> proc_;
    QComboBox* root_ = nullptr;
    QLineEdit* classEdit_ = nullptr;
    QLabel* crumb_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* selLabel_ = nullptr;
    QDoubleSpinBox* value_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QListWidget* list_ = nullptr;
    QLabel* status_ = nullptr;
    QStringList path_;                       // pointer properties followed from the root while browsing
    Spec selected_;
    bool haveSel_ = false;
    std::vector<Custom> mods_;
    std::map<QString, uint64_t> classCache_;
    QTimer valueTimer_, applyTimer_, scanTimer_;
    QThread* scanThread_ = nullptr;
    CsLock scanMu_;
    std::vector<Hit> scanHits_;
    std::string scanNeedle_;
    bool scanExact_ = false, scanForUi_ = false;
    std::atomic<bool> scanBusy_{false}, scanDone_{false};
    std::map<QString, qint64> scanAsked_;
    bool building_ = false;
};

} // namespace dk
