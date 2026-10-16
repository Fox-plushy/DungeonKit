#include "ItemsPage.h"

#include <QAbstractItemView>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include "IdPicker.h"

namespace dk {

static bool busyWidget(QWidget* w) {
    QWidget* f = QApplication::focusWidget();
    return f && (f == w || w->isAncestorOf(f));
}

static void setComboData(QComboBox* box, int value) {
    if (busyWidget(box) || (box->view() && box->view()->isVisible())) return;
    box->blockSignals(true);
    int idx = box->findData(value);
    if (idx < 0) { box->addItem(QString("Unknown (%1)").arg(value), value); idx = box->count() - 1; }
    box->setCurrentIndex(idx);
    box->blockSignals(false);
}

ItemsPage::ItemsPage(Process& proc, ItemEditorMod* mod, QWidget* parent)
    : QWidget(parent), proc_(proc), mod_(mod) {
    lists_ = listsForVersion(29);
    build();
    setVersionNumber(29);
}

void ItemsPage::showLevels(int n) {
    shownLevels_ = std::max(3, std::min(n, ItemEditorMod::kGroups));
    for (int gi = 0; gi < ItemEditorMod::kGroups; ++gi) {
        const bool vis = gi < shownLevels_;
        if (levelLabel_[gi]) levelLabel_[gi]->setVisible(vis);
        for (int s = 0; s < ItemEditorMod::kSlots; ++s)
            if (levelCell_[gi][s]) levelCell_[gi][s]->setVisible(vis);
    }
    levelInfo_->setText(QString("Showing %1 of %2 levels.").arg(shownLevels_).arg(ItemEditorMod::kGroups));
    moreLevels_->setEnabled(shownLevels_ < ItemEditorMod::kGroups);
    fewerLevels_->setEnabled(shownLevels_ > 3);
}

void ItemsPage::fillTierBox(QComboBox* box) {
    IdList t = tierList();
    for (size_t i = 0; i < t.count; ++i) box->addItem(QString::fromUtf8(t.items[i].name), t.items[i].id);
}

static QFrame* makeCard(QVBoxLayout* into, const QString& title) {
    auto* f = new QFrame;
    f->setObjectName("Card");
    auto* v = new QVBoxLayout(f);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    auto* t = new QLabel(title);
    t->setObjectName("CardTitle");
    v->addWidget(t);
    into->addWidget(f);
    return f;
}

void ItemsPage::build() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);

    auto* title = new QLabel("Item Editor");
    title->setObjectName("PageTitle");
    outer->addWidget(title);

    auto* top = new QHBoxLayout;
    toggle_ = new QCheckBox(QString::fromStdString(mod_->name()));
    toggle_->setCursor(Qt::PointingHandCursor);
    state_ = new QLabel;
    state_->setObjectName("Hint");
    top->addWidget(toggle_);
    top->addWidget(state_, 1);
    auto* offAll = new QPushButton("Disable all mods");
    offAll->setCursor(Qt::PointingHandCursor);
    top->addWidget(offAll);
    connect(offAll, &QPushButton::clicked, this, [this]() { emit disableAllRequested(); });
    outer->addLayout(top);

    auto* modeRow = new QHBoxLayout;
    auto* modeLabel = new QLabel("Mode");
    modeRow->addWidget(modeLabel);
    modeBox_ = new QComboBox;
    modeBox_->addItem("Legacy Mode - highlight an item in-game");
    modeBox_->addItem("Modern Mode - pick any item from a list");
    modeRow->addWidget(modeBox_, 1);
    outer->addLayout(modeRow);
    connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const bool isModern = idx == 1;
        if (modernCard_) modernCard_->setVisible(isModern);
        if (isModern) refreshInventoryList();
        else mod_->clearModern();
    });

    // ---- Modern Mode item picker (hidden unless Modern Mode is selected). Lives outside the scroll body, on
    // `outer` directly, so it keeps working even while the body below is disabled waiting for an item to be linked -
    // picking an item here is exactly what links it.
    {
        auto* c = makeCard(outer, "Modern Mode - pick an item");
        modernCard_ = c;
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* hint = new QLabel("Lists everything in your inventory and storage chest by reading the game's own data directly, "
                                "the same way Minecraft Dungeons II's inventory editor does - no need to highlight anything in "
                                "the game. Pick an item below, then edit it with the fields underneath. Works on every platform, "
                                "Steam included.");
        hint->setObjectName("Hint");
        hint->setWordWrap(true);
        cl->addWidget(hint);
        auto* row = new QHBoxLayout;
        invBox_ = new QComboBox;
        invBox_->setMinimumWidth(260);
        refreshInv_ = new QPushButton("Refresh inventory");
        refreshInv_->setCursor(Qt::PointingHandCursor);
        row->addWidget(invBox_, 1);
        row->addWidget(refreshInv_);
        cl->addLayout(row);
        modernHint_ = new QLabel;
        modernHint_->setObjectName("Hint");
        modernHint_->setWordWrap(true);
        cl->addWidget(modernHint_);
        connect(refreshInv_, &QPushButton::clicked, this, [this]() { refreshInventoryList(); });
        connect(invBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
            if (idx < 0 || idx >= (int)inventory_.size()) return;
            if (!mod_->selectModern(proc_, inventory_[idx].itemStruct))
                emit message("That item is no longer available - press Refresh inventory.");
        });
        c->setVisible(false);          // Legacy Mode is on by default
    }

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    body_ = new QWidget;
    body_->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(body_);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);

    // ---- item
    {
        auto* c = makeCard(v, "Highlighted item");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* g = new QGridLayout;
        g->setColumnStretch(1, 1);
        int r = 0;
        auto label = [&](const QString& t) { auto* l = new QLabel(t); l->setMinimumWidth(120); g->addWidget(l, r, 0); };

        label("Item");
        itemId_ = new IdPicker;
        g->addWidget(itemId_, r++, 1);
        label("Power level");
        power_ = new QDoubleSpinBox;
        power_->setRange(-1.0e6, 1.0e6);
        power_->setDecimals(1);
        g->addWidget(power_, r++, 1);
        label("Rarity");
        rarity_ = new IdPicker;
        g->addWidget(rarity_, r++, 1);
        label("Gilded");
        gilded_ = new QCheckBox("Gilded");
        g->addWidget(gilded_, r++, 1);
        label("Gilded enchant");
        gildedEnchant_ = new IdPicker;
        g->addWidget(gildedEnchant_, r++, 1);
        label("Gilded level");
        gildedLevel_ = new IdPicker;                 // pick Level 1-3 from the list, or type any number and press Enter
        gildedLevel_->setList(tierList());
        g->addWidget(gildedLevel_, r++, 1);
        label("Cloned item");
        cloned_ = new QCheckBox("This item was cloned");
        cloned_->setToolTip("On a cloned character: tells you whether the highlighted item is a clone. Reads the game's own flag.");
        g->addWidget(cloned_, r++, 1);
        cl->addLayout(g);

        counts_ = new QLabel;
        counts_->setObjectName("Hint");
        cl->addWidget(counts_);

        auto* btns = new QHBoxLayout;
        auto* copy = new QPushButton("Copy item data");
        auto* paste = new QPushButton("Load item data");
        btns->addWidget(copy);
        btns->addWidget(paste);
        btns->addStretch(1);
        cl->addLayout(btns);

        connect(itemId_, &IdPicker::committed, this, [this](int val) {
            if (!mod_->setInt(proc_, ItemEditorMod::ItemId, val)) emit message("Could not write the item ID.");
        });
        connect(power_, &QDoubleSpinBox::editingFinished, this, [this]() {
            if (!mod_->setPower(proc_, (float)power_->value())) emit message("Could not write the power level.");
        });
        connect(rarity_, &IdPicker::committed, this, [this](int val) { mod_->setInt(proc_, ItemEditorMod::Rarity, val); });
        connect(gilded_, &QCheckBox::clicked, this, [this](bool on) { mod_->setInt(proc_, ItemEditorMod::Gilded, on ? 1 : 0); });
        connect(gildedEnchant_, &IdPicker::committed, this, [this](int val) { mod_->setInt(proc_, ItemEditorMod::GildedEnchant, val); });
        connect(gildedLevel_, &IdPicker::committed, this, [this](int val) {
            if (!mod_->setInt(proc_, ItemEditorMod::GildedLevel, val)) emit message("Could not write the gilded level.");
        });
        connect(cloned_, &QCheckBox::clicked, this, [this](bool on) {
            if (!mod_->setInt(proc_, ItemEditorMod::Cloned, on ? 1 : 0)) emit message("Could not write the cloned flag.");
        });
        connect(copy, &QPushButton::clicked, this, [this]() {
            emit message(mod_->copy(proc_) ? "Item copied, including its enchant levels (not its stats). Highlight another item and press Load item data."
                                           : "Nothing to copy - highlight an item first.");
        });
        connect(paste, &QPushButton::clicked, this, [this]() {
            if (!mod_->hasCopy()) { emit message("Copy an item first."); return; }
            int written = 0, wrong = 0;
            if (!mod_->paste(proc_, &written, &wrong)) { emit message("Highlight the item you want to overwrite first."); return; }
            emit message(wrong != 0 ? "Item data was written, but the game did not keep some of the values. Highlight the item again and try once more."
                                    : "Item data loaded and checked. Move off the item and back on it in the game to see the change.");
        });
    }

    // ---- enchantments
    {
        auto* c = makeCard(v, "Enchantments");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* hint = new QLabel("Each level has a Left, Middle and Right choice with an enchant and a tier. The game normally "
                                "uses 3 levels of 3 enchants, so only those are shown. Open more if you want to experiment. "
                                "Levels the item has no room for are greyed out. The enchant and tier boxes also accept any "
                                "number you type, for custom values.");
        hint->setObjectName("Hint");
        hint->setWordWrap(true);
        cl->addWidget(hint);
        auto* g = new QGridLayout;
        g->setColumnStretch(1, 1); g->setColumnStretch(2, 1); g->setColumnStretch(3, 1);
        for (int s = 0; s < ItemEditorMod::kSlots; ++s) {
            static const char* kCols[] = {"Left", "Middle", "Right"};
            auto* h = new QLabel(kCols[s]);
            h->setObjectName("Muted");
            g->addWidget(h, 0, s + 1);
        }
        for (int gi = 0; gi < ItemEditorMod::kGroups; ++gi) {
            auto* lvl = new QLabel(QString("Level %1").arg(gi + 1));
            levelLabel_[gi] = lvl;
            g->addWidget(lvl, gi + 1, 0);
            for (int s = 0; s < ItemEditorMod::kSlots; ++s) {
                auto* cell = new QWidget;
                auto* cv = new QVBoxLayout(cell);
                cv->setContentsMargins(0, 0, 0, 0);
                cv->setSpacing(3);
                auto* pick = new IdPicker;
                auto* tier = new IdPicker;            // pick a tier by name, or type any number
                tier->setList(tierList());
                cv->addWidget(pick);
                cv->addWidget(tier);
                g->addWidget(cell, gi + 1, s + 1);
                ench_[gi][s] = {pick, tier};
                levelCell_[gi][s] = cell;
                connect(pick, &IdPicker::committed, this, [this, gi, s](int val) {
                    if (!mod_->setEnchantId(proc_, gi, s, val)) emit message("This item has no room for that enchantment level.");
                });
                connect(tier, &IdPicker::committed, this, [this, gi, s](int val) {
                    if (!mod_->setEnchantTier(proc_, gi, s, val)) emit message("This item has no room for that enchantment level.");
                });
            }
        }
        cl->addLayout(g);

        auto* more = new QHBoxLayout;
        levelInfo_ = new QLabel;
        levelInfo_->setObjectName("Hint");
        moreLevels_ = new QPushButton("Show 3 more levels");
        fewerLevels_ = new QPushButton("Show fewer");
        more->addWidget(levelInfo_, 1);
        more->addWidget(fewerLevels_);
        more->addWidget(moreLevels_);
        cl->addLayout(more);
        connect(moreLevels_, &QPushButton::clicked, this, [this]() { showLevels(shownLevels_ + 3); });
        connect(fewerLevels_, &QPushButton::clicked, this, [this]() { showLevels(3); });
        showLevels(3);
    }

    // ---- presets
    {
        auto* c = makeCard(v, "Item presets");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* hint = new QLabel("Test feature - expected to not work reliably yet, though it sometimes does. Save or copy a highlighted "
                                "item, then apply it to a different highlighted item later (stats are not included). Applying a preset "
                                "onto a very different kind of item can crash the game, the same as Load item data can.");
        hint->setObjectName("Hint");
        hint->setWordWrap(true);
        cl->addWidget(hint);
        auto* r1 = new QHBoxLayout;
        presetName_ = new QLineEdit;
        presetName_->setPlaceholderText("Preset name (leave empty to use the item name)");
        auto* save = new QPushButton("Save item as preset");
        save->setCursor(Qt::PointingHandCursor);
        r1->addWidget(presetName_, 1);
        r1->addWidget(save);
        cl->addLayout(r1);
        auto* r2 = new QHBoxLayout;
        presetBox_ = new QComboBox;
        presetBox_->setMinimumWidth(240);
        auto* apply = new QPushButton("Apply preset");
        auto* del = new QPushButton("Delete preset");
        auto* code = new QPushButton("Copy preset code");
        auto* fromCode = new QPushButton("Add preset from code");
        for (auto* b : {apply, del, code, fromCode}) b->setCursor(Qt::PointingHandCursor);
        r2->addWidget(presetBox_, 1);
        r2->addWidget(apply);
        r2->addWidget(del);
        cl->addLayout(r2);
        auto* r3 = new QHBoxLayout;
        r3->addWidget(code);
        r3->addWidget(fromCode);
        r3->addStretch(1);
        cl->addLayout(r3);
        presetInfo_ = new QLabel;
        presetInfo_->setObjectName("Hint");
        presetInfo_->setWordWrap(true);
        cl->addWidget(presetInfo_);
        connect(save, &QPushButton::clicked, this, [this]() { saveCurrentAsPreset(); });
        connect(apply, &QPushButton::clicked, this, [this]() { applySelectedPreset(); });
        connect(del, &QPushButton::clicked, this, [this]() { deleteSelectedPreset(); });
        connect(code, &QPushButton::clicked, this, [this]() { copyPresetCode(); });
        connect(fromCode, &QPushButton::clicked, this, [this]() { addPresetFromCode(); });
        connect(presetBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { showPresetInfo(); });
        loadPresets();
        fillPresetBox();
    }

    // ---- stats
    {
        auto* c = makeCard(v, "Stats / properties");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        statInfo_ = new QLabel;
        statInfo_->setObjectName("Hint");
        statInfo_->setWordWrap(true);
        cl->addWidget(statInfo_);
        statsLayout_ = new QVBoxLayout;
        statsLayout_->setSpacing(4);
        cl->addLayout(statsLayout_);
        showAllStats_ = new QCheckBox("Show all stats");
        cl->addWidget(showAllStats_);
    }
    extraSlot_ = new QVBoxLayout;
    extraSlot_->setSpacing(12);
    v->addLayout(extraSlot_);
    v->addStretch(1);
    scroll->setWidget(body_);
    outer->addWidget(scroll, 1);
    built_ = true;
}

// ---------------------------------------------------------------- presets
static QString encodePreset(const ItemEditorMod::Preset& s) {
    QStringList f;
    f << QString::number(s.id) << QString::number(s.rarity) << QString::number((double)s.power, 'g', 9)
      << QString::number(s.gilded) << QString::number(s.gildedEnchant) << QString::number(s.gildedLevel) << QString::number(s.levels);
    for (int g = 0; g < ItemEditorMod::kGroups; ++g)
        for (int sl = 0; sl < ItemEditorMod::kSlots; ++sl)
            f << QString::number(s.enchId[g][sl]) << QString::number(s.enchTier[g][sl]);
    return f.join(',');
}

static bool decodePreset(const QString& text, ItemEditorMod::Preset& s) {
    const QStringList f = text.split(',');
    const int need = 7 + ItemEditorMod::kGroups * ItemEditorMod::kSlots * 2;
    if (f.size() != need) return false;
    bool ok = true, all = true;
    auto I = [&](int i) { const int v = f[i].toInt(&ok); all = all && ok; return v; };
    s = ItemEditorMod::Preset();
    s.id = I(0); s.rarity = I(1);
    s.power = f[2].toFloat(&ok); all = all && ok;
    s.gilded = I(3) ? 1 : 0; s.gildedEnchant = I(4); s.gildedLevel = I(5); s.levels = I(6);
    int k = 7;
    for (int g = 0; g < ItemEditorMod::kGroups; ++g)
        for (int sl = 0; sl < ItemEditorMod::kSlots; ++sl) { s.enchId[g][sl] = I(k++); s.enchTier[g][sl] = I(k++); }
    if (!all) return false;
    if (s.levels < 0 || s.levels > ItemEditorMod::kGroups) s.levels = std::max(0, std::min(s.levels, ItemEditorMod::kGroups));
    return true;
}

void ItemsPage::loadPresets() {
    presets_.clear();
    QSettings st;
    const QStringList names = st.value("items/presetNames").toStringList();
    const QStringList data = st.value("items/presetData").toStringList();
    for (int i = 0; i < names.size() && i < data.size(); ++i) {
        SavedPreset sp;
        sp.name = names[i];
        if (decodePreset(data[i], sp.p)) presets_.push_back(sp);
    }
}

void ItemsPage::storePresets() {
    QStringList names, data;
    for (const auto& sp : presets_) { names << sp.name; data << encodePreset(sp.p); }
    QSettings st;
    st.setValue("items/presetNames", names);
    st.setValue("items/presetData", data);
    st.sync();
}

void ItemsPage::fillPresetBox(const QString& select) {
    presetBox_->blockSignals(true);
    presetBox_->clear();
    for (const auto& sp : presets_) presetBox_->addItem(sp.name);
    int idx = select.isEmpty() ? -1 : presetBox_->findText(select);
    if (idx < 0 && presetBox_->count() > 0) idx = 0;
    presetBox_->setCurrentIndex(idx);
    presetBox_->blockSignals(false);
    showPresetInfo();
}

void ItemsPage::showPresetInfo() {
    const int i = presetBox_ ? presetBox_->currentIndex() : -1;
    QString t;
    if (i < 0 || i >= (int)presets_.size()) t = "No presets saved yet.";
    else {
        const auto& p = presets_[i].p;
        int ench = 0;
        for (int g = 0; g < p.levels && g < ItemEditorMod::kGroups; ++g)
            for (int sl = 0; sl < ItemEditorMod::kSlots; ++sl) if (p.enchId[g][sl] != 0) ++ench;
        t = QString("Item %1, power %2, rarity %3, %4 enchants over %5 levels%6.")
                .arg(p.id).arg((double)p.power, 0, 'f', 0).arg(p.rarity).arg(ench).arg(p.levels)
                .arg(p.gilded ? ", gilded" : "");
    }
    if (presetInfo_->property("st").toString() != t) { presetInfo_->setProperty("st", t); presetInfo_->setText(t); }
}

void ItemsPage::saveCurrentAsPreset() {
    ItemEditorMod::Preset s;
    if (!mod_->enabled()) { emit message("Switch the Item Editor on first."); return; }
    if (!mod_->readPreset(proc_, s)) { emit message("Highlight an item in the game first, then press Save item as preset."); return; }
    QString name = presetName_->text().trimmed();
    if (name.isEmpty()) name = itemId_->currentText().trimmed();
    if (name.isEmpty()) name = QString("Item %1").arg(s.id);
    name.replace(QRegularExpression("[\\r\\n\\t]"), " ");
    if (name.size() > 60) name = name.left(60);
    bool replaced = false;
    for (auto& sp : presets_) if (sp.name == name) { sp.p = s; replaced = true; break; }
    if (!replaced) presets_.push_back({name, s});
    storePresets();
    fillPresetBox(name);
    presetName_->clear();
    emit message(replaced ? "Preset updated: " + name : "Preset saved: " + name);
}

void ItemsPage::applySelectedPreset() {
    const int i = presetBox_->currentIndex();
    if (i < 0 || i >= (int)presets_.size()) { emit message("Pick a preset first, or save one."); return; }
    if (!mod_->enabled()) { emit message("Switch the Item Editor on first."); return; }
    const ItemEditorMod::Preset s = presets_[i].p;
    int written = 0;
    if (!mod_->applyPreset(proc_, s, written)) { emit message("Highlight the item you want to overwrite first."); return; }
    if (mod_->mismatches(proc_, s, written) != 0) {
        emit message("Item data was written, but the game did not keep some of the values. Highlight the item again and try once more.");
        return;
    }
    emit message("Preset applied and checked. Move off the item and back on it in the game to see the change.");
}

void ItemsPage::deleteSelectedPreset() {
    const int i = presetBox_->currentIndex();
    if (i < 0 || i >= (int)presets_.size()) { emit message("Pick a preset first."); return; }
    const QString name = presets_[i].name;
    presets_.erase(presets_.begin() + i);
    storePresets();
    fillPresetBox();
    emit message("Preset deleted: " + name);
}

void ItemsPage::copyPresetCode() {
    // Reads the currently highlighted item directly, the same as Copy item data, so sharing a code never needs a local save first.
    ItemEditorMod::Preset s;
    if (!mod_->readPreset(proc_, s)) { emit message("Highlight an item in the game first, then press Copy preset code."); return; }
    const QString name = presetName_->text().trimmed().left(60);
    const QByteArray raw = (name + "\n" + encodePreset(s)).toUtf8();
    QApplication::clipboard()->setText("DKITEM2:" + QString::fromLatin1(raw.toBase64()));
    emit message("Preset code copied, including its enchant levels (not its stats). Send it to someone; they press Add preset from code.");
}

void ItemsPage::addPresetFromCode() {
    const QString clip = QApplication::clipboard()->text().trimmed();
    bool ok = false;
    QString t = QInputDialog::getMultiLineText(this, "Add preset from code",
        "Paste the preset code someone sent you (it starts with DKITEM2:):",
        clip.startsWith("DKITEM2:") ? clip : QString(), &ok).trimmed();
    if (!ok) return;
    if (!t.startsWith("DKITEM2:")) { emit message("That doesn't look like a preset code (it should start with DKITEM2:)."); return; }
    t = t.mid(8).trimmed();
    if (t.size() > 20000) { emit message("That code is too long to be a preset."); return; }
    const QString raw = QString::fromUtf8(QByteArray::fromBase64(t.toLatin1()));
    const int nl = raw.indexOf('\n');
    ItemEditorMod::Preset s;
    if (nl <= 0 || !decodePreset(raw.mid(nl + 1), s)) { emit message("That preset code is damaged or not a DungeonKit preset."); return; }
    QString name = raw.left(nl).trimmed().left(60);
    if (name.isEmpty()) name = QString("Item %1").arg(s.id);
    bool replaced = false;
    for (auto& sp : presets_) if (sp.name == name) { sp.p = s; replaced = true; break; }
    if (!replaced) presets_.push_back({name, s});
    storePresets();
    fillPresetBox(name);
    emit message(replaced ? "Preset updated from code: " + name : "Preset added from code: " + name);
}

void ItemsPage::addExtra(QWidget* w) {
    if (extraSlot_ && w) extraSlot_->addWidget(w);
}

void ItemsPage::setVersionNumber(int number) {
    versionNumber_ = number;
    lists_ = listsForVersion(number);
    itemId_->setList(lists_.items);
    rarity_->setList(rarityList());
    gildedEnchant_->setList(lists_.enchants);
    for (auto& row : ench_) for (auto& cell : row) cell.id->setList(lists_.enchants);
    for (auto* s : stats_) s->setList(lists_.armor);
}

bool ItemsPage::linked() const { return mod_->enabled() && mod_->object(proc_) != 0; }

bool ItemsPage::modern() const { return modeBox_ && modeBox_->currentIndex() == 1; }

QString ItemsPage::nameForItemId(int32_t id) const {
    for (size_t i = 0; i < lists_.items.count; ++i)
        if (lists_.items.items[i].id == id) return QString::fromUtf8(lists_.items.items[i].name);
    return QString("Item %1").arg(id);
}

void ItemsPage::refreshInventoryList() {
    if (!invBox_) return;
    std::string err;
    const bool ok = mod_->listInventory(proc_, inventory_, err);
    invBox_->blockSignals(true);
    invBox_->clear();
    if (ok)
        for (const auto& e : inventory_)
            invBox_->addItem(QString("%1Slot %2 - %3").arg(e.storage ? "[Storage] " : "").arg(e.slotIndex).arg(nameForItemId(e.itemId)));
    invBox_->blockSignals(false);
    if (ok && !inventory_.empty()) {
        if (modernHint_) modernHint_->setText(QString("%1 item(s) found.").arg((int)inventory_.size()));
        invBox_->setCurrentIndex(0);              // fires currentIndexChanged -> links the first item
    } else {
        mod_->clearModern();
        if (modernHint_) modernHint_->setText(ok ? "No items found." : QString::fromStdString(err));
    }
}

void ItemsPage::ensureStatRows(int count) {
    int made = 0;
    while ((int)stats_.size() < count && made++ < 24) {          // build in small batches so big items don't freeze the window
        int i = (int)stats_.size();
        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto* l = new QLabel(QString("Stat %1").arg(i + 1));
        l->setMinimumWidth(70);
        auto* pick = new IdPicker;
        pick->setList(lists_.armor);
        h->addWidget(l);
        h->addWidget(pick, 1);
        statsLayout_->addWidget(row);
        stats_.push_back(pick);
        connect(pick, &IdPicker::committed, this, [this, i](int val) {
            if (!mod_->setStat(proc_, i, (uint16_t)val)) emit message("Could not write that stat.");
        });
    }
}

void ItemsPage::refresh() {
    if (!isVisible()) return;
    const bool on = mod_->enabled() && proc_.attached() && proc_.alive();
    const bool isModern = modern();
    if (on && isModern && invBox_ && invBox_->count() == 0) refreshInventoryList();   // pick up a list without needing a manual press
    const bool obj = on && mod_->object(proc_) != 0;
    state_->setText(!on ? QString("Off. Switch it on, then %1.").arg(isModern ? "pick an item from the list below" : "highlight an item in your inventory or the gear screen")
                        : !obj ? QString(isModern ? "On - pick an item from the list below to link it." : "On - highlight an item in the game to link it.")
                               : QString(isModern ? "Linked to the picked item." : "Linked to the highlighted item."));
    body_->setEnabled(obj);
    if (!obj) return;

    int32_t iv;
    float fv;
    if (!busyWidget(itemId_) && mod_->getInt(proc_, ItemEditorMod::ItemId, iv)) itemId_->setValueQuiet(iv);
    if (!busyWidget(power_) && mod_->getPower(proc_, fv)) { power_->blockSignals(true); power_->setValue(fv); power_->blockSignals(false); }
    if (!rarity_->busy() && mod_->getInt(proc_, ItemEditorMod::Rarity, iv)) rarity_->setValueQuiet(iv);
    if (mod_->getInt(proc_, ItemEditorMod::Gilded, iv) && !busyWidget(gilded_)) {
        gilded_->blockSignals(true); gilded_->setChecked(iv != 0); gilded_->blockSignals(false);
    }
    if (!gildedEnchant_->busy() && mod_->getInt(proc_, ItemEditorMod::GildedEnchant, iv)) gildedEnchant_->setValueQuiet(iv);
    if (!gildedLevel_->busy() && mod_->getInt(proc_, ItemEditorMod::GildedLevel, iv)) gildedLevel_->setValueQuiet(iv);
    if (!busyWidget(cloned_)) {
        const bool have = mod_->getInt(proc_, ItemEditorMod::Cloned, iv) && (iv == 0 || iv == 1);      // anything else is not a flag: show nothing
        cloned_->setEnabled(have);
        cloned_->blockSignals(true); cloned_->setChecked(have && iv == 1); cloned_->blockSignals(false);
        const int st = !have ? 2 : (int)iv;
        if (cloned_->property("st").toInt() != st + 1) {                   // only when it changes, so the language switch is not fought
            cloned_->setProperty("st", st + 1);
            cloned_->setText(st == 2 ? "Cloned: unknown" : st == 1 ? "Yes, this item was cloned" : "No, this item was not cloned");
        }
    }

    const int cap = mod_->enchantCapacity(proc_);
    int32_t te = 0, me = 0, ts = 0, ms = 0;
    mod_->getInt(proc_, ItemEditorMod::TotalEnchants, te);
    mod_->getInt(proc_, ItemEditorMod::MaxEnchants, me);
    mod_->getInt(proc_, ItemEditorMod::TotalStats, ts);
    mod_->getInt(proc_, ItemEditorMod::MaxStats, ms);
    counts_->setText(QString("Enchant levels: %1 used / %2 max      Stats: %3 used / %4 max").arg(te).arg(me).arg(ts).arg(ms));

    for (int g = 0; g < ItemEditorMod::kGroups; ++g)
        for (int s = 0; s < ItemEditorMod::kSlots; ++s) {
            EnchCell& cell = ench_[g][s];
            const bool ok = g < cap;
            cell.id->setEnabled(ok);
            cell.tier->setEnabled(ok);
            int32_t id = 0, tier = 0;
            if (ok && mod_->getEnchant(proc_, g, s, id, tier)) {
                if (!cell.id->busy()) cell.id->setValueQuiet(id);
                if (!cell.tier->busy()) cell.tier->setValueQuiet(tier);
            }
        }

    uint64_t sptr = 0;
    int32_t sused = 0, smax = 0;
    mod_->statInfo(proc_, sptr, sused, smax);
    const int scap = mod_->statCapacity(proc_);
    statInfo_->setText(scap > 0
        ? QString("%1 of %2 stat slots in use.").arg(sused).arg(smax)
        : QString("This item has no stat slots (array 0x%1, used %2, max %3). Armor pieces normally have them; try one.")
              .arg(sptr, 0, 16).arg(sused).arg(smax));
    const int want = showAllStats_->isChecked() ? scap : std::min(scap, 8);
    ensureStatRows(want);
    std::vector<uint16_t> values;
    if (want > 0) mod_->getStats(proc_, values);
    for (size_t i = 0; i < stats_.size(); ++i) {
        const bool show = (int)i < want;
        stats_[i]->parentWidget()->setVisible(show);
        if (show && i < values.size() && !stats_[i]->busy()) stats_[i]->setValueQuiet(values[i]);
    }
}

} // namespace dk
