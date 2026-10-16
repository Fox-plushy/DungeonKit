#pragma once
#include <QElapsedTimer>
#include <QSet>
#include <QString>
#include <QWidget>
#include "../mods/ItemViewer.h"

class QCheckBox;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace dk {

// Read-only tree of the stats of your equipped items. Credit: Hybrid.
class ItemViewerPage : public QWidget {
    Q_OBJECT
public:
    explicit ItemViewerPage(ItemViewerMod* mod, QWidget* parent = nullptr);
    QCheckBox* toggle() const { return toggle_; }
    void tick();                                   // called by a timer from the main window

signals:
    void disableAllRequested();

private:
    void rebuild();
    void addNode(QTreeWidgetItem* parent, const ViewNode& n, const QString& path, const QSet<QString>& open);
    void collectOpen(QTreeWidgetItem* item, const QString& path, QSet<QString>& out) const;

    ItemViewerMod* mod_;
    QCheckBox* toggle_ = nullptr;
    QCheckBox* auto_ = nullptr;
    QLabel* state_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QElapsedTimer last_;
    bool wasOn_ = false;
};

} // namespace dk
