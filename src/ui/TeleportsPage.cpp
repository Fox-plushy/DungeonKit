#include "TeleportsPage.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QSettings>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "../data/MapTeleports.h"

namespace dk {

TeleportsPage::TeleportsPage(Process& proc, PositionMod* mod, QWidget* parent)
    : QWidget(parent), proc_(proc), mod_(mod) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);

    auto* title = new QLabel("Teleports");
    title->setObjectName("CardTitle");
    v->addWidget(title);

    auto* note = new QLabel(QString::fromUtf8(
        "Mission teleports can be tricky. Since the game can rotate and switch parts of levels around, positions may not be correct. "
        "They will still work, but not always."));
    note->setObjectName("Notice");
    note->setWordWrap(true);
    v->addWidget(note);

    auto* top = new QHBoxLayout;
    toggle_ = new QCheckBox(QString::fromStdString(mod_->name()));
    toggle_->setCursor(Qt::PointingHandCursor);
    state_ = new QLabel;
    state_->setObjectName("Hint");
    top->addWidget(toggle_);
    top->addWidget(state_, 1);
    v->addLayout(top);

    search_ = new QLineEdit;
    search_->setPlaceholderText("Search missions and locations...");
    search_->setClearButtonEnabled(true);
    v->addWidget(search_);

    tree_ = new QTreeWidget;
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(true);
    tree_->setUniformRowHeights(true);
    tree_->setMinimumHeight(340);
    mine_ = new QTreeWidgetItem(tree_, QStringList("My locations"));
    mine_->setData(0, Qt::UserRole, -1);
    size_t n = 0;
    const TeleportGroup* groups = teleportGroups(n);
    for (size_t g = 0; g < n; ++g) {
        auto* top = new QTreeWidgetItem(tree_, QStringList(QString::fromUtf8(groups[g].name)));
        top->setData(0, Qt::UserRole, -1);
        for (size_t s = 0; s < groups[g].count; ++s) {
            auto* ch = new QTreeWidgetItem(top, QStringList(QString::fromUtf8(groups[g].spots[s].name)));
            ch->setData(0, Qt::UserRole, (int)g);
            ch->setData(0, Qt::UserRole + 1, (int)s);
        }
    }
    v->addWidget(tree_, 1);
    loadCustom();
    mine_->setExpanded(true);

    auto* row = new QHBoxLayout;
    auto* hint = new QLabel("Double-click a location to teleport there.");
    hint->setObjectName("Hint");
    row->addWidget(hint, 1);
    v->addLayout(row);

    auto* mineHint = new QLabel("My locations: stand somewhere in the game, type a name and press Save my position. "
                                "Your locations are kept in the app and load again next time.");
    mineHint->setObjectName("Hint");
    mineHint->setWordWrap(true);
    v->addWidget(mineHint);
    auto* mineRow = new QHBoxLayout;
    nameEdit_ = new QLineEdit;
    nameEdit_->setPlaceholderText("Name for this location");
    nameEdit_->setMaxLength(60);
    auto* saveBtn = new QPushButton("Save my position");
    auto* renameBtn = new QPushButton("Rename");
    auto* delBtn = new QPushButton("Delete");
    for (auto* b : {saveBtn, renameBtn, delBtn}) b->setCursor(Qt::PointingHandCursor);
    mineRow->addWidget(nameEdit_, 1);
    mineRow->addWidget(saveBtn);
    mineRow->addWidget(renameBtn);
    mineRow->addWidget(delBtn);
    v->addLayout(mineRow);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        float xyz[3];
        if (!mod_->enabled() || !proc_.attached() || !mod_->get(proc_, xyz)) {
            emit message("Could not read your position yet. Turn on Position / Teleport and move around in the game so it can find you.");
            return;
        }
        QString name = nameEdit_->text().trimmed();
        if (name.isEmpty()) name = QString("Location %1").arg(mine_->childCount() + 1);
        auto* it = addCustomItem(name, xyz[0], xyz[1], xyz[2]);
        saveCustom();
        mine_->setExpanded(true);
        tree_->setCurrentItem(it);
        nameEdit_->clear();
        emit message(QString("Saved %1.").arg(name));
    });
    connect(renameBtn, &QPushButton::clicked, this, [this]() {
        auto* it = tree_->currentItem();
        if (!it || it->parent() != mine_) { emit message("Pick one of your own locations first."); return; }
        bool ok = false;
        const QString n = QInputDialog::getText(this, "Rename location", "Name", QLineEdit::Normal, it->text(0), &ok).trimmed();
        if (ok && !n.isEmpty()) { it->setText(0, n); saveCustom(); emit message("Renamed."); }
    });
    connect(delBtn, &QPushButton::clicked, this, [this]() {
        auto* it = tree_->currentItem();
        if (!it || it->parent() != mine_) { emit message("Pick one of your own locations first."); return; }
        const QString n = it->text(0);
        delete it;
        saveCustom();
        emit message(QString("Deleted %1.").arg(n));
    });

    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it, int) { this->go(it); });
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& t) { applyFilter(t); });
}

void TeleportsPage::go(QTreeWidgetItem* item) {
    if (!item) { emit message("Pick a location first."); return; }
    bool ok = false;
    int g = item->data(0, Qt::UserRole).toInt(&ok);
    if (ok && g == -2) {                                                      // one of my own locations
        float xyz[3] = {(float)item->data(0, Qt::UserRole + 2).toDouble(), (float)item->data(0, Qt::UserRole + 3).toDouble(),
                        (float)item->data(0, Qt::UserRole + 4).toDouble()};
        if (mod_->set(proc_, xyz)) emit message(QString("Teleported to %1.").arg(item->text(0)));
        else emit message("Could not teleport yet. Turn on Position / Teleport and move around in the game so it can find you.");
        return;
    }
    if (!ok || g < 0) { item->setExpanded(!item->isExpanded()); return; }   // a mission header: just fold/unfold
    int s = item->data(0, Qt::UserRole + 1).toInt();
    size_t n = 0;
    const TeleportGroup* groups = teleportGroups(n);
    const TeleportSpot& spot = groups[g].spots[s];
    float xyz[3] = {spot.x, spot.y, spot.z};
    if (mod_->set(proc_, xyz))
        emit message(QString("Teleported to %1 - %2.").arg(QString::fromUtf8(groups[g].name), QString::fromUtf8(spot.name)));
    else
        emit message("Could not teleport yet. Turn on Position / Teleport and move around in the game so it can find you.");
}

void TeleportsPage::applyFilter(const QString& text) {
    const QString t = text.trimmed();
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* top = tree_->topLevelItem(i);
        const bool groupHit = t.isEmpty() || top->text(0).contains(t, Qt::CaseInsensitive);
        int visible = 0;
        for (int j = 0; j < top->childCount(); ++j) {
            QTreeWidgetItem* ch = top->child(j);
            const bool hit = groupHit || ch->text(0).contains(t, Qt::CaseInsensitive);
            ch->setHidden(!hit);
            if (hit) ++visible;
        }
        top->setHidden(visible == 0);
        top->setExpanded((!t.isEmpty() && visible > 0) || (t.isEmpty() && top == mine_));
    }
}

QTreeWidgetItem* TeleportsPage::addCustomItem(const QString& name, double x, double y, double z) {
    auto* it = new QTreeWidgetItem(mine_, QStringList(name));
    it->setData(0, Qt::UserRole, -2);
    it->setData(0, Qt::UserRole + 2, x);
    it->setData(0, Qt::UserRole + 3, y);
    it->setData(0, Qt::UserRole + 4, z);
    it->setToolTip(0, QString("X %1   Y %2   Z %3").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1).arg(z, 0, 'f', 1));
    return it;
}

// Saved as one line per location: name<TAB>x<TAB>y<TAB>z, under teleports/custom.
void TeleportsPage::loadCustom() {
    for (const QString& line : QSettings().value("teleports/custom").toStringList()) {
        const QStringList f = line.split('\t');
        bool a = false, b = false, c = false;
        if (f.size() != 4) continue;
        const double x = f[1].toDouble(&a), y = f[2].toDouble(&b), z = f[3].toDouble(&c);
        if (a && b && c && !f[0].isEmpty()) addCustomItem(f[0], x, y, z);
    }
}

void TeleportsPage::saveCustom() {
    QStringList out;
    for (int i = 0; i < mine_->childCount(); ++i) {
        auto* it = mine_->child(i);
        QString name = it->text(0);
        name.replace('\t', ' ').replace('\n', ' ');
        out << QString("%1\t%2\t%3\t%4").arg(name).arg(it->data(0, Qt::UserRole + 2).toDouble(), 0, 'f', 3)
                                           .arg(it->data(0, Qt::UserRole + 3).toDouble(), 0, 'f', 3).arg(it->data(0, Qt::UserRole + 4).toDouble(), 0, 'f', 3);
    }
    QSettings().setValue("teleports/custom", out);
}

void TeleportsPage::refresh() {
    if (!isVisible()) return;
    const bool on = mod_->enabled() && proc_.attached() && proc_.alive();
    state_->setText(!on ? "Off. Switch it on, then move around in the game."
                        : mod_->object(proc_) ? "Linked - ready to teleport."
                                              : "On - move around in the game to link it.");
}

} // namespace dk
