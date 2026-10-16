#include "ItemViewerPage.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace dk {

ItemViewerPage::ItemViewerPage(ItemViewerMod* mod, QWidget* parent) : QWidget(parent), mod_(mod) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(28, 24, 28, 20);
    v->setSpacing(10);

    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel("Item Stat Viewer");
    title->setObjectName("PageTitle");
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    auto* offAll = new QPushButton("Disable all mods");
    offAll->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(offAll);
    connect(offAll, &QPushButton::clicked, this, [this]() { emit disableAllRequested(); });
    v->addLayout(titleRow);

    auto* credit = new QLabel(QString::fromUtf8("Credit: Hybrid"));
    credit->setObjectName("Muted");
    v->addWidget(credit);

    auto* note = new QLabel(
        "View only. Everything on this page is read from the game and nothing here can be changed. "
        "Turn it on inside a level, then press Refresh whenever you swap gear.");
    note->setObjectName("Notice");
    note->setWordWrap(true);
    v->addWidget(note);

    auto* top = new QHBoxLayout;
    toggle_ = new QCheckBox(QString::fromStdString(mod_->name()));
    toggle_->setCursor(Qt::PointingHandCursor);
    state_ = new QLabel;
    state_->setObjectName("Hint");
    auto* refresh = new QPushButton("Refresh");
    auto* expand = new QPushButton("Expand all");
    auto* collapse = new QPushButton("Collapse all");
    auto_ = new QCheckBox("Auto refresh");
    top->addWidget(toggle_);
    top->addWidget(state_, 1);
    top->addWidget(auto_);
    top->addWidget(expand);
    top->addWidget(collapse);
    top->addWidget(refresh);
    v->addLayout(top);

    tree_ = new QTreeWidget;
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({"Name", "Value"});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->setColumnWidth(0, 330);
    tree_->setUniformRowHeights(true);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setAlternatingRowColors(false);
    v->addWidget(tree_, 1);

    connect(refresh, &QPushButton::clicked, this, [this]() { rebuild(); });
    connect(expand, &QPushButton::clicked, this, [this]() { tree_->expandAll(); });
    connect(collapse, &QPushButton::clicked, this, [this]() { tree_->collapseAll(); });
    last_.start();
}

void ItemViewerPage::collectOpen(QTreeWidgetItem* item, const QString& path, QSet<QString>& out) const {
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem* c = item->child(i);
        const QString p = path + "/" + c->text(0);
        if (c->isExpanded()) out.insert(p);
        collectOpen(c, p, out);
    }
}

void ItemViewerPage::addNode(QTreeWidgetItem* parent, const ViewNode& n, const QString& path, const QSet<QString>& open) {
    auto* it = new QTreeWidgetItem(parent, QStringList() << QString::fromStdString(n.label) << QString::fromStdString(n.value));
    const QString p = path + "/" + it->text(0);
    for (const auto& k : n.kids) addNode(it, k, p, open);
    if (open.contains(p)) it->setExpanded(true);
}

void ItemViewerPage::rebuild() {
    last_.restart();
    if (!mod_->enabled()) { state_->setText("Off. Switch it on first."); return; }
    std::vector<ViewNode> nodes;
    std::string err;
    if (!mod_->snapshot(nodes, err)) { state_->setText(QString::fromStdString(err)); return; }

    QSet<QString> open;
    const bool first = tree_->topLevelItemCount() == 0;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* t = tree_->topLevelItem(i);
        if (t->isExpanded()) open.insert("/" + t->text(0));
        collectOpen(t, "/" + t->text(0), open);
    }
    const int scroll = tree_->verticalScrollBar()->value();
    tree_->setUpdatesEnabled(false);
    tree_->clear();
    for (const auto& n : nodes) {
        auto* top = new QTreeWidgetItem(tree_, QStringList() << QString::fromStdString(n.label) << QString::fromStdString(n.value));
        const QString p = "/" + top->text(0);
        for (const auto& k : n.kids) addNode(top, k, p, open);
        if (first || open.contains(p)) top->setExpanded(true);
    }
    tree_->verticalScrollBar()->setValue(scroll);
    tree_->setUpdatesEnabled(true);
    state_->setText("Linked.");
}

void ItemViewerPage::tick() {
    const bool on = mod_->enabled();
    if (on && !wasOn_) rebuild();                               // first look right after switching it on
    if (!on && wasOn_) { tree_->clear(); state_->setText("Off."); }
    wasOn_ = on;
    if (on && isVisible() && auto_->isChecked() && last_.elapsed() > 1500) rebuild();
    if (!on && state_->text().isEmpty()) state_->setText("Off.");
}

} // namespace dk
