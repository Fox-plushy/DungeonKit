#pragma once
#include <QWidget>
#include "../core/Process.h"
#include "../mods/Mods.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace dk {

// Mission / area teleports (the "Map Teleports" list from the original table).
class TeleportsPage : public QWidget {
    Q_OBJECT
public:
    // Lives inside the World page (Minecraft Dungeons), so it has no title of its own.
    TeleportsPage(Process& proc, PositionMod* mod, QWidget* parent = nullptr);
    QCheckBox* toggle() const { return toggle_; }
    void refresh();

signals:
    void message(const QString& text);
    void disableAllRequested();

private:
    void go(QTreeWidgetItem* item);
    void loadCustom();
    void saveCustom();
    QTreeWidgetItem* addCustomItem(const QString& name, double x, double y, double z);
    void applyFilter(const QString& text);

    Process& proc_;
    PositionMod* mod_;
    QCheckBox* toggle_ = nullptr;
    QLabel* state_ = nullptr;
    QLineEdit* search_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QTreeWidgetItem* mine_ = nullptr;      // the "My locations" group (saved between sessions)
    QLineEdit* nameEdit_ = nullptr;
};

} // namespace dk
