#pragma once
#include <QWidget>
#include <vector>
#include "../core/Process.h"
#include "../data/IdTables.h"
#include "../mods/Mods.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QVBoxLayout;
class QComboBox;
class QPushButton;
class QLineEdit;


namespace dk {

class IdPicker;

class ItemsPage : public QWidget {
    Q_OBJECT
public:
    ItemsPage(Process& proc, ItemEditorMod* mod, QWidget* parent = nullptr);

    void setVersionNumber(int number);   // reload the name lists (29 = 1.17.0.0)
    void refresh();                      // called by a timer; pulls values from the game
    QCheckBox* toggle() const { return toggle_; }
    void addExtra(QWidget* w);           // extra cards at the bottom of the page

signals:
    void message(const QString& text);
    void disableAllRequested();

private:
    struct EnchCell { IdPicker* id; IdPicker* tier; };

    void build();
    void ensureStatRows(int count);
    bool linked() const;
    void fillTierBox(QComboBox* box);
    void showLevels(int n);
    bool modern() const;
    void refreshInventoryList();
    QString nameForItemId(int32_t id) const;

    Process& proc_;
    ItemEditorMod* mod_;
    VersionLists lists_;
    uint64_t lastObject_ = 0;
    bool built_ = false;

    QVBoxLayout* extraSlot_ = nullptr;
    QCheckBox* toggle_ = nullptr;
    QLabel* state_ = nullptr;
    IdPicker* itemId_ = nullptr;
    QDoubleSpinBox* power_ = nullptr;
    IdPicker* rarity_ = nullptr;
    QCheckBox* gilded_ = nullptr;
    IdPicker* gildedEnchant_ = nullptr;
    IdPicker* gildedLevel_ = nullptr;
    QCheckBox* cloned_ = nullptr;
    int versionNumber_ = 29;
    // saved item presets (stored in the app settings, applied with the same code as Load item data)
    struct SavedPreset { QString name; ItemEditorMod::Preset p; };
    std::vector<SavedPreset> presets_;
    QComboBox* presetBox_ = nullptr;
    QLineEdit* presetName_ = nullptr;
    QLabel* presetInfo_ = nullptr;
    void loadPresets();
    void storePresets();
    void fillPresetBox(const QString& select = QString());
    void showPresetInfo();
    void saveCurrentAsPreset();
    void applySelectedPreset();
    void deleteSelectedPreset();
    void copyPresetCode();
    void addPresetFromCode();
    QLabel* statInfo_ = nullptr;
    QLabel* counts_ = nullptr;
    EnchCell ench_[ItemEditorMod::kGroups][ItemEditorMod::kSlots] = {};
    QWidget* levelLabel_[ItemEditorMod::kGroups] = {};
    QWidget* levelCell_[ItemEditorMod::kGroups][ItemEditorMod::kSlots] = {};
    int shownLevels_ = 3;
    QLabel* levelInfo_ = nullptr;
    QPushButton* moreLevels_ = nullptr;
    QPushButton* fewerLevels_ = nullptr;
    QVBoxLayout* statsLayout_ = nullptr;
    QCheckBox* showAllStats_ = nullptr;
    std::vector<IdPicker*> stats_;
    QWidget* body_ = nullptr;

    // ---- Legacy Mode / Modern Mode
    QComboBox* modeBox_ = nullptr;             // index 0 = Legacy, 1 = Modern
    QWidget* modernCard_ = nullptr;
    QComboBox* invBox_ = nullptr;
    QPushButton* refreshInv_ = nullptr;
    QLabel* modernHint_ = nullptr;
    std::vector<ItemEditorMod::InventoryEntry> inventory_;
};

} // namespace dk
