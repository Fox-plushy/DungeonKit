#include <QGridLayout>
#include "Mcd2Pages.h"
#include "Overlay.h"
#include "../data/Mcd2Catalog.h"

#include "I18n.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QTimer>
#include <QClipboard>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <cstring>
#include <QCompleter>
#include <QPushButton>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace dk {

namespace {

QString nt(const std::string& s) { return I18n::tr(QString::fromStdString(s)); }
QString pretty(const QString& tag);       // defined with the inventory editor

bool containsCI(const std::string& s, const char* part) {
    std::string a = s, b = part;
    for (auto& c : a) c = (char)std::tolower((unsigned char)c);
    for (auto& c : b) c = (char)std::tolower((unsigned char)c);
    return a.find(b) != std::string::npos;
}

QWidget* makePage(const QString& title, const QString& notice, QVBoxLayout*& content) {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);
    auto* t = new QLabel(title);
    t->setObjectName("PageTitle");
    auto* top = new QHBoxLayout;
    top->addWidget(t);
    top->addStretch(1);
    auto* off = new QPushButton("Disable all mods");
    off->setObjectName("DisableAll");
    off->setCursor(Qt::PointingHandCursor);
    top->addWidget(off);
    outer->addLayout(top);
    if (!notice.isEmpty()) {
        auto* n = new QLabel(notice);
        n->setObjectName("Notice");
        n->setWordWrap(true);
        outer->addWidget(n);
    }
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    content = new QVBoxLayout(inner);
    content->setContentsMargins(0, 0, 8, 0);
    content->setSpacing(12);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

QVBoxLayout* makeCard(QVBoxLayout* into, const QString& title) {
    auto* f = new QFrame;
    f->setObjectName("Card");
    auto* v = new QVBoxLayout(f);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    auto* t = new QLabel(title);
    t->setObjectName("CardTitle");
    v->addWidget(t);
    into->addWidget(f);
    return v;
}

QLabel* hint(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("Hint");
    l->setWordWrap(true);
    return l;
}

const char* kExperimental =
    "For Minecraft Dungeons II. Choose Minecraft Dungeons II as the target game on Home, start the game and load into a level. "
    "Anything that changes a value is best used in a solo session: in an online session the server may undo it.";

} // namespace

Mcd2Hub::Mcd2Hub(Mcd2& m, std::function<bool()> ready, QObject* parent) : QObject(parent), m_(m), ready_(std::move(ready)) {
    slow_.start(); invTimer_.start(); worldTimer_.start(); nearTimer_.start(); nearRescanTimer_.start(); forceRarityTimer_.start();
    buildPlayer();
    buildMotion();
    buildInventory();
    buildWorld();
    buildCamera();
    buildNearby();
    buildEditor();
    for (QWidget* pg : {player_, motion_, inventory_, world_, camera_, nearby_, editor_})
        for (QPushButton* b : pg->findChildren<QPushButton*>("DisableAll"))
            connect(b, &QPushButton::clicked, this, [this]() { emit disableAllRequested(); });
}

void Mcd2Hub::addRow(QVBoxLayout* into, QWidget* page, const QString& label, std::function<bool(double&)> get,
                     std::function<bool(double)> set, double lo, double hi, int decimals, double step) {
    auto* w = new QWidget;
    w->setStyleSheet("background: transparent;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* l = new QLabel(label);
    l->setMinimumWidth(170);
    auto* spin = new QDoubleSpinBox;
    spin->setRange(lo, hi);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    spin->setMinimumWidth(130);
    auto* apply = new QPushButton("Set");
    auto* freeze = new QCheckBox("Freeze");
    h->addWidget(l);
    h->addWidget(spin, 1);
    h->addWidget(apply);
    h->addWidget(freeze);
    into->addWidget(w);
    rows_.push_back({spin, freeze, page, get, set});
    connect(apply, &QPushButton::clicked, this, [this, spin, set]() {
        if (!ready_() || !set(spin->value())) emit message("Could not write that value yet - load into a level and try again.");
        spin->clearFocus();
    });
}

// Overkill: the damage multipliers it raises (base, melee, ranged, artifact) and the value they are set to.
static const char* kOverLabels[4] = {"Base damage multiplier", "Melee damage multiplier", "Ranged damage multiplier", "Artifact damage multiplier"};
static const float kOverBig = 1000000.0f;
static int attrIndex(const char* label) {
    const auto& defs = Mcd2::attrDefs();
    for (int i = 0; i < (int)defs.size(); ++i) if (std::string(defs[i].label) == label) return i;
    return -1;
}

void Mcd2Hub::overkillOff() {
    if (!overBox_) return;
    const bool was = overBox_->isChecked();
    overBox_->blockSignals(true);
    overBox_->setChecked(false);
    overBox_->blockSignals(false);
    if (was && ready_())
        for (int k = 0; k < 4; ++k) {
            float cur = 0;
            const int d = attrIndex(kOverLabels[k]);
            if (overHave_[k] && d >= 0 && m_.getAttr(d, cur) && cur == kOverBig) m_.setAttr(d, overOrig_[k]);     // put the game's value back
        }
    for (bool& h : overHave_) h = false;
}

void Mcd2Hub::setWasd(bool on) {
    if (!wasdTimer_) {
        wasdTimer_ = new QTimer(this);
        wasdTimer_->setInterval(8);
        connect(wasdTimer_, &QTimer::timeout, this, [this]() { wasdStep(); });
    }
    wasd_ = on;
    if (on) wasdTimer_->start();
    else wasdTimer_->stop();
}

void Mcd2Hub::wasdStep() {
    if (!wasd_ || !ready_() || !m_.gameForeground()) return;
    auto key = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    double fx = 0, fy = 0;                                    // same axes as Minecraft Dungeons: W = +X, S = -X, A = -Y, D = +Y
    if (key('W')) fx += 1;
    if (key('S')) fx -= 1;
    if (key('A')) fy -= 1;
    if (key('D')) fy += 1;
    if (fx == 0 && fy == 0) return;
    if (noClipBox_ && noClipBox_->isChecked()) return;        // No Clip already uses these keys on its own
    const double len = std::sqrt(fx * fx + fy * fy);
    fx /= len; fy /= len;
    const double rad = wasdRot_ * 3.14159265358979 / 180.0, c = std::cos(rad), sn = std::sin(rad);
    m_.addMoveInput(fx * c - fy * sn, fx * sn + fy * c);      // the game turns this into normal walking (gravity and walls still apply)
}

void Mcd2Hub::shopTick() {
    if (!shopOn_) return;
    if (!shopOn_->isChecked()) { shopState_->setText(I18n::tr("Turn it on, then buy something.")); return; }
    if (!ready_()) { for (float& f : shopPrev_) f = -1; shopState_->setText(I18n::tr("Waiting for the game...")); return; }
    // emeralds, spring stones (the second currency) and enchantment points
    static const char* labels[3] = {"Emeralds", "Spring stones", "Enchantment points"};
    static int idx[3] = {-2, -2, -2};
    if (idx[0] == -2) {
        const auto& d = Mcd2::attrDefs();
        for (int k = 0; k < 3; ++k) {
            idx[k] = -1;
            for (int i = 0; i < (int)d.size(); ++i) if (QString(d[i].label) == labels[k]) { idx[k] = i; break; }
        }
    }
    QString txt;
    bool any = false;
    for (int k = 0; k < 3; ++k) {
        float cur = 0;
        if (idx[k] < 0 || !m_.getAttr(idx[k], cur)) { shopPrev_[k] = -1; continue; }
        any = true;
        const int now = (int)std::lround(cur);
        txt += (txt.isEmpty() ? "" : "    ") + I18n::tr(QString(k == 0 ? "Emeralds: " : k == 1 ? "Spring stones: " : "Enchantment points: ")) + QString::number(now);
        if (shopPrev_[k] >= 0 && now < (int)shopPrev_[k]) {
            const int spent = (int)shopPrev_[k] - now;
            const int refund = (int)std::lround(spent * (1.0 - shopPay_->value() / 100.0));
            if (refund > 0 && m_.setAttr(idx[k], (float)(now + refund))) {
                shopLast_->setText(I18n::tr("Last purchase: ") + QString::number(spent) +
                                   I18n::tr(k == 0 ? " emeralds, paid " : k == 1 ? " spring stones, paid " : " enchantment points, paid ") + QString::number(spent - refund));
                shopPrev_[k] = (float)(now + refund);
                continue;
            }
        }
        shopPrev_[k] = (float)now;
    }
    shopState_->setText(any ? txt : I18n::tr("Waiting for the game..."));
}

void Mcd2Hub::stopFreezes() {
    if (shopOn_) shopOn_->setChecked(false);
    if (wasd_) { setWasd(false); emit wasdChanged(false); }
    for (auto& r : rows_) r.freeze->setChecked(false);
    const bool wasFly = false, wasNoClip = noClipBox_ && noClipBox_->isChecked();
    for (QCheckBox* b : {dmEnabled_, dmHealth_, dmDamage_, dmDps_, dmSpeed_, dmQuest_, dmCoords_, dmDeaths_,
                         dmEmeralds_, dmShards_, dmEnchant_, dmSouls_, dmBlueprint_}) if (b) b->setChecked(false);
    if (godBox_) { godBox_->blockSignals(true); godBox_->setChecked(false); godBox_->blockSignals(false); }
    overkillOff();
    for (QCheckBox* b : {invOn_, edOn_, worldOn_, nearOn_}) if (b) b->setChecked(false);   // pages go back to off
    for (QCheckBox* b : {infArrowBox_, infEnchBox_, noCdBox_}) if (b) { b->blockSignals(true); b->setChecked(false); b->blockSignals(false); }
    if (noClipBox_) { noClipBox_->blockSignals(true); noClipBox_->setChecked(false); noClipBox_->blockSignals(false); }
    if (wasNoClip) m_.setCollision(true);                      // never leave the character without collision
    if (wasFly || wasNoClip) m_.setMoveMode(3);
    if (diffKeep_) diffKeep_->setChecked(false);
    if (ignBox_) ignBox_->setChecked(false);                   // puts the enemies' values back
}

void Mcd2Hub::wirePageSwitch(QCheckBox* on, const std::vector<QWidget*>& deps, const std::vector<QCheckBox*>& autos,
                             const std::function<void()>& refresh, const std::function<void()>& clear) {
    auto apply = [deps, autos](bool enabled) {
        for (QWidget* w : deps) if (w) w->setEnabled(enabled);
        for (QCheckBox* a : autos) if (a) { a->setEnabled(enabled); if (!enabled) a->setChecked(false); }
    };
    apply(false);
    connect(on, &QCheckBox::toggled, this, [this, on, apply, refresh, clear](bool checked) {
        if (!checked) { apply(false); clear(); return; }
        if (!ready_()) {
            emit message(I18n::tr("Minecraft Dungeons II is not connected."));
            on->blockSignals(true); on->setChecked(false); on->blockSignals(false);
            return;
        }
        if (!scanNow()) {
            on->blockSignals(true); on->setChecked(false); on->blockSignals(false);
            return;
        }
        apply(true);
        refresh();
    });
}

bool Mcd2Hub::scanNow() {
    std::string err;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = ready_() && m_.rescan(err);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) emit message(err.empty() ? I18n::tr("Minecraft Dungeons II is not ready.") : nt(err));
    return ok;
}

// ---------------------------------------------------------------- player
void Mcd2Hub::buildPlayer() {
    QVBoxLayout* v = nullptr;
    player_ = makePage("Player",
        "Only works in OFFLINE - Choose Minecraft Dungeons II as the target game on Home, start the game and load into a level. "
        "In an online session the server may undo any changes.", v);

    {
        auto* c = makeCard(v, "Status");
        plState_ = new QLabel("Waiting for the game...");
        plState_->setWordWrap(true);
        c->addWidget(plState_);
        plSets_ = hint("");
        c->addWidget(plSets_);
    }
    {
        auto* c = makeCard(v, "Player");
        struct Opt { QCheckBox** box; const char* name; const char* desc; };
        const Opt opts[] = {
            {&infArrowBox_, "Infinite Arrows", "Arrows of every type are never used up."},
            {&infEnchBox_, "Infinite Enchant Points", "Enchantment points are never used up."},
            {&noCdBox_, "No Cooldown", "Potions, rolls and artifacts can be used again straight away, even without the souls."},
        };
        for (const Opt& o : opts) {
            QCheckBox* box = new QCheckBox(o.name);
            *o.box = box;
            box->setCursor(Qt::PointingHandCursor);
            auto* desc = new QLabel(o.desc);
            desc->setObjectName("Hint");
            desc->setWordWrap(true);
            desc->setContentsMargins(46, 0, 0, 4);
            c->addWidget(box);
            c->addWidget(desc);
            connect(box, &QCheckBox::toggled, this, [this, box](bool on) {
                enchFloor_ = potionFloor_ = rollFloor_ = -1;
                if (on && !ready_()) { box->blockSignals(true); box->setChecked(false); box->blockSignals(false); emit message("Minecraft Dungeons II is not connected."); }
            });
        }
        ignBox_ = new QCheckBox("Enemies Ignore You");
        ignBox_->setCursor(Qt::PointingHandCursor);
        c->addWidget(ignBox_);
        {
            auto* d = hint("Enemies stop noticing you: your aggro is set to zero and every loaded enemy's sight and alert range to zero. Experimental. Enemies that appear later need Re-scan enemies, and bosses may still react. Offline only.");
            d->setContentsMargins(46, 0, 0, 4);
            c->addWidget(d);
        }
        auto* rescanIgn = new QPushButton("Re-scan enemies");
        rescanIgn->setCursor(Qt::PointingHandCursor);
        auto* ignRow = new QHBoxLayout;
        ignRow->addSpacing(46);
        ignRow->addWidget(rescanIgn);
        ignRow->addStretch(1);
        c->addLayout(ignRow);
        connect(ignBox_, &QCheckBox::toggled, this, [this](bool on) {
            if (!on) { m_.enemyIgnoreOff(); return; }
            if (!ready_()) { ignBox_->blockSignals(true); ignBox_->setChecked(false); ignBox_->blockSignals(false); emit message("Minecraft Dungeons II is not connected."); return; }
            std::string note;
            if (!scanNow() || m_.enemyIgnoreOn(note) < 0) {
                ignBox_->blockSignals(true); ignBox_->setChecked(false); ignBox_->blockSignals(false);
                emit message(note.empty() ? QString("Could not find the enemies yet - load into a level.") : nt(note));
            }
        });
        connect(rescanIgn, &QPushButton::clicked, this, [this]() {
            if (!ignBox_->isChecked()) { emit message("Turn on Enemies Ignore You first."); return; }
            if (scanNow()) { m_.enemyIgnoreTick(); emit message("Enemies scanned."); }
        });
        c->addWidget(hint("Infinite Arrows and the rolling part of No Cooldown also work online."));
    }
    {
        auto* c = makeCard(v, "Stats");
        c->addWidget(hint("Health, emeralds, XP, level, souls and arrows. Set writes the value; Freeze keeps writing it. Stats only work in offline mode, not online."));
        const auto& defs = Mcd2::attrDefs();
        for (int i = 0; i < (int)defs.size(); ++i) {
            if (defs[i].group) continue;                                       // only the Player group (no group name)
            addRow(c, player_, defs[i].label,
                   [this, i](double& d) { float f; if (!m_.getAttr(i, f)) return false; d = f; return true; },
                   [this, i](double d) { return m_.setAttr(i, (float)d); }, -1.0e9, 1.0e9, 2, 1.0);
        }
    }
    {
        auto* c = makeCard(v, "Threat level and difficulty");
        c->addWidget(hint("The game recalculates the threat level, so use Freeze to keep a value. Set writes the difficulty and Keep re-applies it. Offline only."));
        const auto& defs = Mcd2::attrDefs();
        for (int i = 0; i < (int)defs.size(); ++i) {
            if (std::string(defs[i].set) != "ATR_ThreatLevel") continue;
            addRow(c, player_, defs[i].label,
                   [this, i](double& d) { float f; if (!m_.getAttr(i, f)) return false; d = f; return true; },
                   [this, i](double d) { return m_.setAttr(i, (float)d); }, -1.0e6, 1.0e6, 2, 1.0);
        }
        diffNow_ = new QLabel("Difficulty: -");
        c->addWidget(diffNow_);
        auto* row = new QHBoxLayout;
        diffBox_ = new QComboBox;
        diffBox_->addItem(I18n::tr("Low"), "SW.Difficulty.Low");
        diffBox_->addItem(I18n::tr("Normal"), "SW.Difficulty.Normal");
        diffBox_->addItem(I18n::tr("High"), "SW.Difficulty.High");
        diffBox_->setCurrentIndex(1);
        auto* set = new QPushButton("Set difficulty");
        auto* unlock = new QPushButton("Unlock difficulty");
        diffKeep_ = new QCheckBox("Keep");
        set->setCursor(Qt::PointingHandCursor);
        unlock->setCursor(Qt::PointingHandCursor);
        row->addWidget(diffBox_, 1);
        row->addWidget(set);
        row->addWidget(unlock);
        row->addWidget(diffKeep_);
        c->addLayout(row);
        connect(set, &QPushButton::clicked, this, [this]() {
            emit message(ready_() && m_.setDifficulty(diffBox_->currentData().toString().toStdString())
                             ? "Difficulty written." : "Could not write the difficulty (load into a level first).");
        });
        connect(unlock, &QPushButton::clicked, this, [this]() {
            emit message(ready_() && m_.setDifficultyLocked(false) ? "Difficulty unlocked." : "Could not unlock the difficulty.");
        });
    }
    {
        auto* c = makeCard(v, "Location");
        plPos_ = new QLabel("Position: -");
        plPos_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        c->addWidget(plPos_);
        auto* row = new QHBoxLayout;
        QDoubleSpinBox** sp[3] = {&tx_, &ty_, &tz_};
        const char* names[3] = {"X", "Y", "Z"};
        for (int i = 0; i < 3; ++i) {
            auto* l = new QLabel(names[i]);
            *sp[i] = new QDoubleSpinBox;
            (*sp[i])->setRange(-1.0e9, 1.0e9);
            (*sp[i])->setDecimals(2);
            (*sp[i])->setMinimumWidth(110);
            row->addWidget(l);
            row->addWidget(*sp[i], 1);
        }
        c->addLayout(row);
        auto* b = new QHBoxLayout;
        auto* use = new QPushButton("Use current");
        auto* go = new QPushButton("Teleport to these");
        auto* save = new QPushButton("Save position");
        auto* back = new QPushButton("Teleport to saved");
        auto* copy = new QPushButton("Copy coordinates");
        for (auto* x : {use, go, save, back, copy}) { x->setCursor(Qt::PointingHandCursor); b->addWidget(x); }
        b->addStretch(1);
        c->addLayout(b);
        c->addWidget(hint("Teleporting writes the character's position directly. It may not move you, or the game may pull you back, "
                          "especially online. If nothing happens, that is useful to know."));
        connect(use, &QPushButton::clicked, this, [this]() {
            Vec3 p;
            if (ready_() && m_.getLocation(p)) { tx_->setValue(p.x); ty_->setValue(p.y); tz_->setValue(p.z); }
            else emit message("Position not readable yet.");
        });
        connect(go, &QPushButton::clicked, this, [this]() {
            emit message(ready_() && m_.setLocation({tx_->value(), ty_->value(), tz_->value()}) ? "Position written." : "Could not write the position.");
        });
        connect(save, &QPushButton::clicked, this, [this]() {
            Vec3 p;
            if (ready_() && m_.getLocation(p)) { saved_ = p; haveSaved_ = true; emit message("Position saved."); }
            else emit message("Position not readable yet.");
        });
        connect(back, &QPushButton::clicked, this, [this]() {
            if (!haveSaved_) { emit message("No saved position yet."); return; }
            emit message(ready_() && m_.setLocation(saved_) ? "Position written." : "Could not write the position.");
        });
        connect(copy, &QPushButton::clicked, this, [this]() {
            Vec3 p;
            double yaw = 0;
            if (ready_() && m_.getLocation(p, &yaw)) {
                QGuiApplication::clipboard()->setText(QString("X=%1 Y=%2 Z=%3 Yaw=%4").arg(p.x, 0, 'f', 2).arg(p.y, 0, 'f', 2).arg(p.z, 0, 'f', 2).arg(yaw, 0, 'f', 1));
                emit message("Coordinates copied.");
            }
        });
    }
    v->addStretch(1);
}

void Mcd2Hub::buildMotion() {
    QVBoxLayout* v = nullptr;
    motion_ = makePage("No Clip & Stats", "Only works in OFFLINE - Load into a level first.", v);
    {
        auto* c = makeCard(v, "Movement");
        noClipBox_ = new QCheckBox("No Clip");
        noClipBox_->setCursor(Qt::PointingHandCursor);
        c->addWidget(noClipBox_);
        {
            auto* d = hint("Fly through walls. Hold Page Up to rise and Page Down to sink. Move out of any wall before turning it off. Offline only.");
            d->setContentsMargins(46, 0, 0, 4);
            c->addWidget(d);
        }
        auto* row = new QHBoxLayout;
        auto* l = new QLabel("Rise / sink speed");
        l->setMinimumWidth(170);
        flyStep_ = new QDoubleSpinBox;
        flyStep_->setRange(5, 1000);
        flyStep_->setValue(60);
        flyStep_->setSuffix(" per tick");
        auto* up = new QPushButton("Up 300");
        auto* down = new QPushButton("Down 300");
        row->addWidget(l);
        row->addWidget(flyStep_, 1);
        row->addWidget(up);
        row->addWidget(down);
        c->addLayout(row);
        connect(noClipBox_, &QCheckBox::toggled, this, [this](bool on) {
            if (!ready_()) { if (on) { noClipBox_->blockSignals(true); noClipBox_->setChecked(false); noClipBox_->blockSignals(false); emit message("Minecraft Dungeons II is not connected."); } return; }
            if (on) {
                if (!m_.setCollision(false)) {
                    noClipBox_->blockSignals(true); noClipBox_->setChecked(false); noClipBox_->blockSignals(false);
                    emit message("Could not turn off collision yet - load into a level.");
                    return;
                }
                m_.setMoveMode(5);
            } else {
                m_.setCollision(true);
                m_.setMoveMode(3);
            }
        });
        auto nudge = [this](double dz) {
            Vec3 p;
            if (ready_() && m_.getLocation(p)) { p.z += dz; m_.setLocation(p); }
        };
        connect(up, &QPushButton::clicked, this, [nudge]() { nudge(300); });
        connect(down, &QPushButton::clicked, this, [nudge]() { nudge(-300); });
        godBox_ = new QCheckBox("God Mode");
        godBox_->setCursor(Qt::PointingHandCursor);
        c->addWidget(godBox_);
        {   // descriptions sit under their switches, like the Minecraft Dungeons switches
            auto* d = hint("Keeps your health full. Offline only.");
            d->setContentsMargins(46, 0, 0, 4);
            c->addWidget(d);
        }
        connect(godBox_, &QCheckBox::toggled, this, [this](bool on) {
            if (on && !ready_()) { godBox_->blockSignals(true); godBox_->setChecked(false); godBox_->blockSignals(false); emit message("Minecraft Dungeons II is not connected."); }
        });
        overBox_ = new QCheckBox("Overkill");
        overBox_->setCursor(Qt::PointingHandCursor);
        c->addWidget(overBox_);
        {   // the description sits under the switch, like the Minecraft Dungeons switches
            auto* d = hint("Your attacks one-shot nearly everything. Raises your damage multipliers; turning it off puts them back. Offline only.");
            d->setContentsMargins(46, 0, 0, 4);
            c->addWidget(d);
        }
        connect(overBox_, &QCheckBox::toggled, this, [this](bool on) {
            if (!on) { overBox_->blockSignals(true); overBox_->setChecked(true); overBox_->blockSignals(false); overkillOff(); return; }     // restores, then unchecks
            if (!ready_()) { overBox_->blockSignals(true); overBox_->setChecked(false); overBox_->blockSignals(false); emit message("Minecraft Dungeons II is not connected."); return; }
            bool any = false;
            for (int k = 0; k < 4; ++k) {
                float cur = 0;
                const int d = attrIndex(kOverLabels[k]);
                overHave_[k] = d >= 0 && m_.getAttr(d, cur);
                if (overHave_[k]) { overOrig_[k] = cur; any = true; }
            }
            if (!any) {
                overBox_->blockSignals(true); overBox_->setChecked(false); overBox_->blockSignals(false);
                emit message("Could not find your damage values yet - load into a level.");
            }
        });
    }
    {
        auto* c = makeCard(v, "Walk, fly and jump values");
        c->addWidget(hint("Your character's movement values. Movement speed works offline only."));
        const auto& defs = Mcd2::moveDefs();
        for (int i = 0; i < (int)defs.size(); ++i)
            addRow(c, motion_, defs[i].label,
                   [this, i](double& d) { float f; if (!m_.getMove(i, f)) return false; d = f; return true; },
                   [this, i](double d) { return m_.setMove(i, (float)d); }, -1.0e7, 1.0e7, 2, 10.0);
    }
    {
        auto* hc = makeCard(v, "Player stats");
        hc->addWidget(hint("Edit your attack speed, damage, health, movement and more. Values reset when you change level. Tick Freeze on a row to hold it there. Most of these are multipliers (1 = normal)."));
        // Stats shown on this page, one card per group (Mcd2::attrDefs says which group each belongs to).
        static const char* groups[] = {"General combat", "Melee", "Ranged", "Artifacts", "Elemental damage", "Movement and rolling", "Defense", "Healing", "Companions and status"};
        static const char* groupHints[] = {
            "Applies to melee, ranged and artifact attacks. Set changes a value once; Freeze keeps it there. Many values are multipliers (1 is normal). Offline only.",
            "Melee weapons only: damage, attack speed, reach and critical hits.",
            "Bows and crossbows: damage, attack speed, projectile speed, ammo recharge, multishot, ricochet and critical hits.",
            "Artifact damage, cooldowns and TNT.",
            "Extra damage by element.",
            "Walking speed, rolling, jumping, gravity and reach.",
            "Damage resistances, armor, shield, poise and thorns.",
            "Healing, life leech and health potions.",
            "Companion bonuses, how long effects last, and souls."};
        const auto& defs = Mcd2::attrDefs();
        for (int g = 0; g < 9; ++g) {
            QVBoxLayout* c = nullptr;
            for (int i = 0; i < (int)defs.size(); ++i) {
                if (!defs[i].group || std::string(defs[i].group) != groups[g]) continue;
                if (!c) { c = makeCard(v, groups[g]); c->addWidget(hint(groupHints[g])); }
                addRow(c, motion_, defs[i].label,
                       [this, i](double& d) { float f; if (!m_.getAttr(i, f)) return false; d = f; return true; },
                       [this, i](double d) { return m_.setAttr(i, (float)d); }, -1.0e9, 1.0e9, 3, 0.1);
            }
        }
    }
    v->addStretch(1);
}

// ---------------------------------------------------------------- inventory
void Mcd2Hub::buildInventory() {
    QVBoxLayout* v = nullptr;
    inventory_ = makePage("Inventory Viewer", "Read-only list of what your character holds. It changes nothing in the game. "
                          "Press Refresh the first time and again after you change level (it scans the game, which can take a few seconds).", v);
    auto* bar = new QHBoxLayout;
    invOn_ = new QCheckBox("Turn on");
    auto* refresh = new QPushButton("Refresh");
    auto* copy = new QPushButton("Copy as text");
    invAuto_ = new QCheckBox("Update automatically");
    refresh->setCursor(Qt::PointingHandCursor);
    copy->setCursor(Qt::PointingHandCursor);
    bar->addWidget(invOn_);
    bar->addWidget(refresh);
    bar->addWidget(copy);
    bar->addWidget(invAuto_);
    bar->addStretch(1);
    v->addLayout(bar);
    invNote_ = hint("Nothing read yet.");
    v->addWidget(invNote_);
    invTree_ = new QTreeWidget;
    invTree_->setColumnCount(7);
    invTree_->setHeaderLabels({"Slot", "Item", "Rarity", "Level", "Stack", "Equipped in", "Enchantments"});
    invTree_->setRootIsDecorated(false);
    invTree_->setAlternatingRowColors(true);
    invTree_->setMinimumHeight(420);
    invTree_->header()->setSectionResizeMode(QHeaderView::Interactive);
    invTree_->setColumnWidth(0, 150);
    invTree_->setColumnWidth(1, 260);
    invTree_->setColumnWidth(2, 130);
    invTree_->setColumnWidth(3, 50);
    invTree_->setColumnWidth(4, 50);
    invTree_->setColumnWidth(5, 150);
    v->addWidget(invTree_, 1);
    connect(refresh, &QPushButton::clicked, this, [this]() { if (invOn_->isChecked() && scanNow()) refreshInventory(); });
    wirePageSwitch(invOn_, {refresh, copy}, {invAuto_}, [this]{ refreshInventory(); },
                   [this]{ invTree_->clear(); invNote_->setText(I18n::tr("Turn this page on to read the game.")); });
    invNote_->setText(I18n::tr("Turn this page on to read the game."));
    connect(copy, &QPushButton::clicked, this, [this]() {
        QStringList lines;
        for (int i = 0; i < invTree_->topLevelItemCount(); ++i) {
            QStringList cols;
            for (int c = 0; c < invTree_->columnCount(); ++c) cols << invTree_->topLevelItem(i)->text(c);
            lines << cols.join("\t");
        }
        QGuiApplication::clipboard()->setText(lines.join("\n"));
        emit message("Inventory copied.");
    });
}

void Mcd2Hub::refreshInventory() {
    if (!invOn_ || !invOn_->isChecked()) return;
    std::vector<Mcd2::InvRow> rows;
    std::string note;
    const bool ok = ready_() && m_.inventory(rows, note);
    invNote_->setText(nt(note.empty() ? std::string("Not ready.") : note));
    if (!ok) return;
    invTree_->setUpdatesEnabled(false);
    invTree_->clear();
    for (auto& r : rows) {
        auto* it = new QTreeWidgetItem(invTree_);
        it->setText(0, QString::fromStdString(r.slot));
        it->setText(1, QString::fromStdString(r.item));
        it->setText(2, QString::fromStdString(r.rarity));
        it->setText(3, QString::number(r.level));
        it->setText(4, QString::number(r.stack));
        it->setText(5, QString::fromStdString(r.equipped));
        it->setText(6, QString::fromStdString(r.effects));
    }
    invTree_->setUpdatesEnabled(true);
}

// ---------------------------------------------------------------- world
void Mcd2Hub::buildWorld() {
    QVBoxLayout* v = nullptr;
    world_ = makePage("World", "Only works in OFFLINE - The map you are on, what your character has discovered, and your saved waypoints. "
                      "Press Refresh the first time and again after you change level. Teleporting to waypoints or other places does not work online.", v);
    auto* bar = new QHBoxLayout;
    worldOn_ = new QCheckBox("Turn on");
    auto* refresh = new QPushButton("Refresh");
    worldAuto_ = new QCheckBox("Update automatically");
    refresh->setCursor(Qt::PointingHandCursor);
    bar->addWidget(worldOn_);
    bar->addWidget(refresh);
    bar->addWidget(worldAuto_);
    bar->addStretch(1);
    v->addLayout(bar);
    worldMap_ = new QLabel("Map: -");
    v->addWidget(worldMap_);
    worldNote_ = hint("Nothing read yet.");
    v->addWidget(worldNote_);
    {
        auto* c = makeCard(v, "Item rarity drops");
        c->addWidget(hint("Bonus: raises the chance that items drop at a higher rarity, and how often things drop. Set writes it once, Freeze keeps it. "
                          "Chances: press Read chances to see how likely each rarity is, then change a rarity's weight. A higher weight means more of that rarity. Offline only."));
        for (const char* lab : {"Rarity bonus chance", "Drop chance increase", "Drop duplication chance", "Looting multiplier"}) {
            const int i = attrIndex(lab);
            if (i < 0) continue;
            addRow(c, world_, lab,
                   [this, i](double& d) { float f; if (!m_.getAttr(i, f)) return false; d = f; return true; },
                   [this, i](double d) { return m_.setAttr(i, (float)d); }, -1.0e6, 1.0e6, 2, 0.1);
        }
        auto* bar2 = new QHBoxLayout;
        auto* read = new QPushButton("Read chances");
        auto* setw = new QPushButton("Set weight");
        auto* back = new QPushButton("Restore original");
        rarSpin_ = new QDoubleSpinBox;
        rarSpin_->setRange(0, 1000000);
        rarSpin_->setDecimals(0);
        rarSpin_->setSingleStep(10);
        for (auto* b : {read, setw, back}) b->setCursor(Qt::PointingHandCursor);
        bar2->addWidget(read);
        bar2->addWidget(new QLabel("Weight"));
        bar2->addWidget(rarSpin_);
        bar2->addWidget(setw);
        bar2->addWidget(back);
        bar2->addStretch(1);
        c->addLayout(bar2);
        rarNote_ = hint("Press Read chances.");
        c->addWidget(rarNote_);
        rarTree_ = new QTreeWidget;
        rarTree_->setHeaderLabels({"Rarity", "Weight", "Chance", "Table"});
        rarTree_->setRootIsDecorated(false);
        rarTree_->setAlternatingRowColors(true);
        rarTree_->setMinimumHeight(170);
        rarTree_->header()->setSectionResizeMode(QHeaderView::Interactive);
        c->addWidget(rarTree_);
        connect(read, &QPushButton::clicked, this, [this]() { readRarity(); });
        connect(rarTree_, &QTreeWidget::currentItemChanged, this, [this]() {
            const int r = rarTree_->indexOfTopLevelItem(rarTree_->currentItem());
            if (r >= 0 && r < (int)rarRows_.size()) rarSpin_->setValue(rarRows_[r].weight);
        });
        connect(setw, &QPushButton::clicked, this, [this]() {
            const int r = rarTree_->indexOfTopLevelItem(rarTree_->currentItem());
            if (r < 0 || r >= (int)rarRows_.size()) { emit message("Select a rarity first."); return; }
            std::string err;
            if (!ready_() || !m_.setRarityWeight(rarRows_[r], (int)rarSpin_->value(), err)) { emit message(nt(err.empty() ? std::string("Not ready.") : err)); return; }
            readRarity();
            emit message("Weight written.");
        });
        connect(back, &QPushButton::clicked, this, [this]() {
            std::string err;
            int n = 0;
            for (auto& r : rarRows_) {
                auto it = rarOrig_.find(r.addr);
                if (it != rarOrig_.end() && ready_() && m_.setRarityWeight(r, it->second, err)) ++n;
            }
            readRarity();
            emit message(n ? "Original weights restored." : "Nothing to restore.");
        });
        auto* bar3 = new QHBoxLayout;
        auto* forceLabel = new QLabel("Force");
        forceRarityBox_ = new QComboBox;
        for (const char* r : {"Common", "Rare", "Special", "Unique"}) forceRarityBox_->addItem(I18n::tr(r), QString(r));
        auto* forceBtn = new QPushButton("Attempt force");
        forceRarityKeep_ = new QCheckBox("Keep forcing");
        forceBtn->setCursor(Qt::PointingHandCursor);
        bar3->addWidget(forceLabel);
        bar3->addWidget(forceRarityBox_);
        bar3->addWidget(forceBtn);
        bar3->addWidget(forceRarityKeep_);
        bar3->addStretch(1);
        c->addLayout(bar3);
        c->addWidget(hint("Experimental, and not a guarantee: sets the chosen rarity's weight very high and every other rarity's weight to 0, across every rarity "
                          "table found, so that rarity is more likely to drop - a harder push than the Bonus values above, but the game may still decide drops some "
                          "other way we can't see, so this can help a bit without pushing it anywhere near 100%. Keep forcing re-reads and reapplies it every few "
                          "seconds in case the game resets the table, such as on a level change."));
        connect(forceBtn, &QPushButton::clicked, this, [this]() {
            const std::string want = forceRarityBox_->currentData().toString().toStdString();
            std::string err;
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const bool ok = applyForceRarity(want, err);
            QApplication::restoreOverrideCursor();
            if (!ok) { emit message(nt(err.empty() ? std::string("Not ready.") : err)); return; }
            readRarity();
            emit message("Attempted - no guarantee it sticks.");
        });
    }
    {
        auto* c = makeCard(v, "Waypoints");
        c->addWidget(hint("Save your position, then teleport back to it later. Double-click a name to rename it. Teleporting may not work online."));
        auto* row = new QHBoxLayout;
        wpName_ = new QLineEdit;
        wpName_->setPlaceholderText("Name for the new waypoint");
        auto* add = new QPushButton("Add current position");
        row->addWidget(wpName_, 1);
        row->addWidget(add);
        c->addLayout(row);
        wpTree_ = new QTreeWidget;
        wpTree_->setHeaderLabels({"Name", "Map", "X", "Y", "Z"});
        wpTree_->setRootIsDecorated(false);
        wpTree_->setMinimumHeight(180);
        wpTree_->setColumnWidth(0, 200);
        wpTree_->setColumnWidth(1, 140);
        c->addWidget(wpTree_);
        auto* b = new QHBoxLayout;
        auto* go = new QPushButton("Teleport to selected");
        auto* del = new QPushButton("Delete selected");
        auto* copy = new QPushButton("Copy list");
        for (auto* x : {go, del, copy}) { x->setCursor(Qt::PointingHandCursor); b->addWidget(x); }
        add->setCursor(Qt::PointingHandCursor);
        b->addStretch(1);
        c->addLayout(b);
        loadWaypoints();
        connect(add, &QPushButton::clicked, this, [this]() {
            Vec3 p;
            if (!ready_() || !m_.getLocation(p)) { emit message("Position not readable yet."); return; }
            QString name = wpName_->text().trimmed();
            if (name.isEmpty()) name = QString("Waypoint %1").arg(wpTree_->topLevelItemCount() + 1);
            addWaypointRow(name, QString::fromStdString(m_.worldName()), p.x, p.y, p.z);
            wpName_->clear();
            saveWaypoints();
            emit message("Waypoint added.");
        });
        connect(go, &QPushButton::clicked, this, [this]() {
            auto* it = wpTree_->currentItem();
            if (!it) { emit message("Select a waypoint first."); return; }
            const Vec3 p{it->text(2).toDouble(), it->text(3).toDouble(), it->text(4).toDouble()};
            emit message(ready_() && m_.setLocation(p) ? "Position written." : "Could not write the position.");
        });
        connect(del, &QPushButton::clicked, this, [this]() {
            delete wpTree_->currentItem();
            saveWaypoints();
        });
        connect(copy, &QPushButton::clicked, this, [this]() {
            QStringList lines;
            for (int i = 0; i < wpTree_->topLevelItemCount(); ++i) {
                auto* it = wpTree_->topLevelItem(i);
                lines << QString("%1\t%2\t%3\t%4\t%5").arg(it->text(0), it->text(1), it->text(2), it->text(3), it->text(4));
            }
            QGuiApplication::clipboard()->setText(lines.join("\n"));
            emit message("Waypoints copied.");
        });
        connect(wpTree_, &QTreeWidget::itemChanged, this, [this]() { if (!wpLoading_) saveWaypoints(); });
    }
    {
        auto* c = makeCard(v, "Missions");
        c->addWidget(hint("Lists the missions the game has loaded. Unlock locked missions makes every Locked or Unavailable mission Available. Scan again after you change level. Offline only."));
        auto* b = new QHBoxLayout;
        auto* scan = new QPushButton("Scan missions");
        auto* unlock = new QPushButton("Unlock locked missions");
        auto* copy = new QPushButton("Copy list");
        for (auto* x : {scan, unlock, copy}) { x->setCursor(Qt::PointingHandCursor); b->addWidget(x); }
        questFilter_ = new QLineEdit;
        questFilter_->setPlaceholderText("Filter (for example boss)");
        b->addWidget(questFilter_, 1);
        c->addLayout(b);
        questNote_ = hint("Nothing scanned yet.");
        c->addWidget(questNote_);
        questTree_ = new QTreeWidget;
        questTree_->setHeaderLabels({"Mission", "State", "ID"});
        questTree_->setRootIsDecorated(false);
        questTree_->setAlternatingRowColors(true);
        questTree_->setMinimumHeight(260);
        questTree_->setColumnWidth(0, 300);
        questTree_->setColumnWidth(1, 100);
        questTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        c->addWidget(questTree_);
        auto* b2 = new QHBoxLayout;
        auto* stateBox = new QComboBox;
        stateBox->addItem("Completed", 4);
        stateBox->addItem("Available", 3);
        stateBox->addItem("Active", 2);
        stateBox->addItem("Locked", 1);
        auto* setSel = new QPushButton("Set selected missions to");
        auto* doneAll = new QPushButton("Complete all active and available");
        for (auto* x : {setSel, doneAll}) x->setCursor(Qt::PointingHandCursor);
        b2->addWidget(setSel);
        b2->addWidget(stateBox);
        b2->addWidget(doneAll);
        b2->addStretch(1);
        c->addLayout(b2);
        c->addWidget(hint("Click a mission, or hold Ctrl / Shift to pick several, choose a state and press Set selected missions to. This changes the state the game holds in memory. "
                          "The game may not give rewards for it, and may change it back when you change level. Use it offline and keep a backup of your save."));
        connect(setSel, &QPushButton::clicked, this, [this, stateBox]() {
            if (!ready_()) { emit message("Not ready."); return; }
            const auto sel = questTree_->selectedItems();
            if (sel.isEmpty()) { emit message("Select a mission first."); return; }
            const int state = stateBox->currentData().toInt();
            int n = 0;
            for (auto* it : sel) {
                const quint64 obj = it->data(0, Qt::UserRole).toULongLong();
                if (m_.setQuestState(obj, it->text(2).toStdString(), state)) ++n;
            }
            emit message(n ? QString("%1 mission(s) changed.").arg(n) : QString("Could not change the missions. Scan again and try once more."));
            refreshQuests();
        });
        connect(doneAll, &QPushButton::clicked, this, [this]() {
            if (!ready_()) { emit message("Not ready."); return; }
            std::string note;
            std::vector<Mcd2::QuestRow> rows;
            if (!m_.quests(rows, note)) { emit message(nt(note)); return; }
            int n = 0;
            for (auto& r : rows)
                if ((r.state == 2 || r.state == 3) && m_.setQuestState(r.obj, r.id, 4)) ++n;
            emit message(n ? QString("%1 mission(s) set to Completed.").arg(n) : QString("No active or available missions found."));
            refreshQuests();
        });
        connect(scan, &QPushButton::clicked, this, [this]() { if (scanNow()) refreshQuests(); });
        connect(questFilter_, &QLineEdit::textChanged, this, [this]() { refreshQuests(); });
        connect(unlock, &QPushButton::clicked, this, [this]() {
            if (!ready_()) { emit message("Not ready."); return; }
            std::string note;
            const int n = m_.unlockQuests(note);
            if (n < 0) { emit message(nt(note)); return; }
            emit message(n ? QString("%1 mission(s) set to Available.").arg(n) : QString("No locked missions found."));
            refreshQuests();
        });
        connect(copy, &QPushButton::clicked, this, [this]() {
            QStringList lines;
            for (int i = 0; i < questTree_->topLevelItemCount(); ++i) {
                auto* it = questTree_->topLevelItem(i);
                lines << QString("%1\t%2\t%3").arg(it->text(0), it->text(1), it->text(2));
            }
            QGuiApplication::clipboard()->setText(lines.join("\n"));
            emit message("List copied.");
        });
    }
    worldTree_ = new QTreeWidget;
    worldTree_->setHeaderLabels({"Discovered", "Name"});
    worldTree_->setMinimumHeight(420);
    worldTree_->setColumnWidth(0, 260);
    v->addWidget(worldTree_, 1);
    connect(refresh, &QPushButton::clicked, this, [this]() { if (worldOn_->isChecked() && scanNow()) refreshWorld(); });
    wirePageSwitch(worldOn_, {refresh}, {worldAuto_}, [this]{ refreshWorld(); },
                   [this]{ worldTree_->clear(); worldMap_->setText(I18n::tr("Map: -")); worldNote_->setText(I18n::tr("Turn this page on to read the game.")); });
    worldNote_->setText(I18n::tr("Turn this page on to read the game."));
}

void Mcd2Hub::refreshQuests() {
    std::string note;
    std::vector<Mcd2::QuestRow> rows;
    const bool ok = ready_() && m_.quests(rows, note);
    if (!ok) { questNote_->setText(nt(note.empty() ? std::string("Not ready.") : note)); return; }
    questRows_ = rows;
    const QString f = questFilter_->text().trimmed();
    questTree_->setUpdatesEnabled(false);
    questTree_->clear();
    int shown = 0, locked = 0;
    for (auto& r : rows) {
        if (r.state == 1 || r.state == 6) ++locked;
        const QString name = QString::fromStdString(r.name), id = QString::fromStdString(r.id);
        if (!f.isEmpty() && !name.contains(f, Qt::CaseInsensitive) && !id.contains(f, Qt::CaseInsensitive)) continue;
        auto* qi = new QTreeWidgetItem(questTree_, QStringList() << name << I18n::tr(Mcd2::questStateName(r.state)) << id);
        qi->setData(0, Qt::UserRole, (qulonglong)r.obj);
        ++shown;
    }
    questTree_->setUpdatesEnabled(true);
    questNote_->setText(nt(note) + QString("   ") + I18n::tr(QString("Locked or unavailable: %1.").arg(locked)) + QString("   ") + I18n::tr(QString("Missions shown: %1.").arg(shown)));
}

void Mcd2Hub::addWaypointRow(const QString& name, const QString& map, double x, double y, double z) {
    wpLoading_ = true;
    auto* it = new QTreeWidgetItem(wpTree_, QStringList() << name << map << QString::number(x, 'f', 2) << QString::number(y, 'f', 2) << QString::number(z, 'f', 2));
    it->setFlags(it->flags() | Qt::ItemIsEditable);
    wpLoading_ = false;
}

void Mcd2Hub::saveWaypoints() {
    QStringList out;
    for (int i = 0; i < wpTree_->topLevelItemCount(); ++i) {
        auto* it = wpTree_->topLevelItem(i);
        QString name = it->text(0);
        name.replace('\t', ' ').replace('\n', ' ');
        out << QString("%1\t%2\t%3\t%4\t%5").arg(name, it->text(1), it->text(2), it->text(3), it->text(4));
    }
    QSettings().setValue("waypoints/d2", out);
}

void Mcd2Hub::loadWaypoints() {
    for (const QString& line : QSettings().value("waypoints/d2").toStringList()) {
        const QStringList f = line.split('\t');
        if (f.size() == 5) addWaypointRow(f[0], f[1], f[2].toDouble(), f[3].toDouble(), f[4].toDouble());
    }
}

void Mcd2Hub::readRarity() {
    std::string note;
    rarRows_.clear();
    rarTree_->clear();
    if (!ready_()) { rarNote_->setText(I18n::tr("Not ready.")); return; }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = m_.rarityRows(rarRows_, note);
    QApplication::restoreOverrideCursor();
    if (!ok) { rarNote_->setText(nt(note.empty() ? std::string("Not ready.") : note)); return; }
    double total = 0;
    for (auto& r : rarRows_) total += r.weight;
    for (auto& r : rarRows_) {
        if (!rarOrig_.count(r.addr)) rarOrig_[r.addr] = r.weight;       // the first value seen is the game's own
        const double pct = total > 0 ? 100.0 * r.weight / total : 0.0;
        const QString nm = QString::fromStdString(r.tag.empty() ? r.name : r.tag);
        auto* it = new QTreeWidgetItem(rarTree_, QStringList() << pretty(nm) << QString::number(r.weight) << QString::number(pct, 'f', 1) + " %" << QString::fromStdString(r.table));
        it->setToolTip(0, nm);
    }
    for (int c = 0; c < 4; ++c) rarTree_->resizeColumnToContents(c);
    rarNote_->setText(I18n::tr("Chance = weight divided by the total of all weights. The first values read are kept so you can restore them."));
}

bool Mcd2Hub::applyForceRarity(const std::string& want, std::string& err) {
    std::vector<Mcd2::RarityRow> rows;
    std::string note;
    if (!ready_() || !m_.rarityRows(rows, note)) { err = note.empty() ? "Not ready." : note; return false; }
    int failed = 0;
    for (auto& r : rows) {
        if (!rarOrig_.count(r.addr)) rarOrig_[r.addr] = r.weight;      // remember the game's own value before forcing it, same as Read chances
        const bool match = containsCI(r.tag, want.c_str()) || containsCI(r.name, want.c_str());
        const int target = match ? 1000000 : 0;
        if (r.weight == target) continue;
        std::string e;
        if (!m_.setRarityWeight(r, target, e)) ++failed;
    }
    if (failed == (int)rows.size()) { err = "Could not write the rarity weights."; return false; }
    err.clear();
    return true;
}

void Mcd2Hub::refreshWorld() {
    if (!worldOn_ || !worldOn_->isChecked()) return;
    Mcd2::WorldInfo w;
    std::string note;
    const bool ok = ready_() && m_.world(w, note);
    QString mapText = w.map.empty() ? QString("-") : QString::fromStdString(w.map);
    if (ok && !w.map.empty()) {
        const QString region = QString::fromStdString(m_.regionName());           // the Sift and the Camp are regions of the Overworld map
        if (!region.isEmpty() && region != mapText) mapText += " (" + region + ")";
    }
    worldMap_->setText(I18n::tr(QString("Map: %1").arg(mapText)));
    worldNote_->setText(nt(note.empty() ? std::string("Not ready.") : note));
    if (!ok) return;
    worldTree_->clear();
    auto group = [&](const QString& title, const std::vector<std::string>& names) {
        auto* top = new QTreeWidgetItem(worldTree_, QStringList() << QString("%1 (%2)").arg(title).arg(names.size()));
        for (auto& n : names) new QTreeWidgetItem(top, QStringList() << "" << QString::fromStdString(n));
        top->setExpanded(names.size() <= 40);
    };
    group("Minecart stations", w.stations);
    group("Activated gimmicks", w.gimmicks);
    group("Played cutscenes", w.cutscenes);
    new QTreeWidgetItem(worldTree_, QStringList() << QString("Dungeon doors discovered: %1").arg(w.doors < 0 ? QString("-") : QString::number(w.doors)));
}

// ---------------------------------------------------------------- camera
void Mcd2Hub::buildCamera() {
    QVBoxLayout* v = nullptr;
    camera_ = makePage("Camera & Display", "Camera tweaks that only change what you see on your own screen. The game may reset a value when the camera changes "
                       "(entering a building, a cutscene), so use Freeze to keep it. Press Find camera first.", v);
    auto* bar = new QHBoxLayout;
    auto* find = new QPushButton("Find camera");
    auto* reset = new QPushButton("Reset to original");
    find->setCursor(Qt::PointingHandCursor);
    reset->setCursor(Qt::PointingHandCursor);
    bar->addWidget(find);
    bar->addWidget(reset);
    bar->addStretch(1);
    v->addLayout(bar);
    camNote_ = hint("Not scanned yet.");
    v->addWidget(camNote_);
    {
        auto* pc = makeCard(v, "Camera presets");
        pc->addWidget(hint("One click sets a camera look. Presets only change the field of view and never the camera distance. "
                           "Use Reset to original to go back."));
        camKeep_ = new QCheckBox("Keep the preset applied (freeze)");
        camKeep_->setChecked(true);
        pc->addWidget(camKeep_);
        struct Preset { const char* label; float fov, dfov, arm, seek; };
        const float keep = std::numeric_limits<float>::quiet_NaN();   // NaN = leave that value alone
        const Preset presets[] = {
            {"FOV Far", 65.0f, 90.0f, 3700.0f, 3700.0f},
            {"FOV Super", 80.0f, 120.0f, keep, keep},
            {"Quake Pro", 95.0f, 105.0f, keep, keep},
            {"Tactical", 55.0f, 70.0f, 5500.0f, 5500.0f},
        };
        auto* grid = new QGridLayout;
        int n = 0;
        for (const Preset& pr : presets) {
            auto* b = new QPushButton(pr.label);
            b->setCursor(Qt::PointingHandCursor);
            grid->addWidget(b, n / 4, n % 4);
            ++n;
            const float a = pr.fov, d = pr.dfov, l = pr.arm, sk = pr.seek;
            const QString name = pr.label;
            connect(b, &QPushButton::clicked, this, [this, a, d, l, sk, name]() { applyPreset(name, a, d, l, sk); });
        }
        pc->addLayout(grid);
    }
    {
        auto* pm = makeCard(v, "Photo mode");
        pm->addWidget(hint("Slow the world down or freeze it to line up a shot, then use the presets or the camera values below for the framing. "
                           "DungeonKit cannot hide the game's on-screen interface. Press Normal speed when you are done; a frozen world stays frozen. "
                           "If Timer or Pause Game is also switched on in Tools, they fight over the same value."));
        timeRow_ = (int)rows_.size();
        addRow(pm, camera_, "World speed", [this](double& x) { float f; if (!m_.getTimeScale(f)) return false; x = f; return true; },
               [this](double x) { return m_.setTimeScale((float)x); }, 0.0001, 20.0, 4, 0.1);
        auto* row = new QHBoxLayout;
        auto* freezeBtn = new QPushButton("Freeze time");
        auto* slowBtn = new QPushButton("Slow motion");
        auto* normalBtn = new QPushButton("Normal speed");
        for (auto* x : {freezeBtn, slowBtn, normalBtn}) { x->setCursor(Qt::PointingHandCursor); row->addWidget(x); }
        row->addStretch(1);
        pm->addLayout(row);
        auto setTime = [this](double value, bool keep) {
            if (!ready_()) { emit message("Minecraft Dungeons II is not connected."); return; }
            auto& r = rows_[(size_t)timeRow_];
            r.spin->blockSignals(true);
            r.spin->setValue(value);
            r.spin->blockSignals(false);
            r.freeze->setChecked(keep);
            if (!m_.setTimeScale((float)value)) emit message("Could not reach the world's time settings - load into a level.");
        };
        connect(freezeBtn, &QPushButton::clicked, this, [setTime]() { setTime(0.0001, true); });
        connect(slowBtn, &QPushButton::clicked, this, [setTime]() { setTime(0.2, true); });
        connect(normalBtn, &QPushButton::clicked, this, [setTime]() { setTime(1.0, false); });
    }
    auto* c = makeCard(v, "Camera");
    struct Def { const char* label; int kind; double lo, hi, step; };
    const Def defs[] = {
        {"Field of view", Mcd2::Fov, 5, 170, 1},
        {"Default field of view", Mcd2::DefaultFov, 5, 170, 1},
        {"Camera distance (arm length)", Mcd2::ArmLength, 0, 20000, 50},
        {"Camera distance target (seek)", Mcd2::SeekArm, 0, 20000, 50},
    };
    for (const Def& d : defs) {
        const int k = d.kind;
        camRow_[k] = (int)rows_.size();
        addRow(c, camera_, d.label,
               [this, k](double& x) {
                   float f;
                   if (!m_.getCam(k, f)) return false;
                   if (!camHave_[k]) { camHave_[k] = true; camOrig_[k] = f; }
                   x = f;
                   return true;
               },
               [this, k](double x) { return m_.setCam(k, (float)x); }, d.lo, d.hi, 1, d.step);
    }
    connect(find, &QPushButton::clicked, this, [this]() {
        if (!scanNow()) return;
        float a = 0;
        const bool fov = m_.getCam(Mcd2::Fov, a), arm = m_.getCam(Mcd2::ArmLength, a);
        camNote_->setText(QString("Camera component: %1, spring arm: %2.").arg(QString(fov ? "found" : "not found"), QString(arm ? "found" : "not found")));
    });
    connect(reset, &QPushButton::clicked, this, [this]() {
        for (auto& r : rows_) if (r.page == camera_) r.freeze->setChecked(false);
        for (int k = 0; k < Mcd2::CamCount; ++k) if (camHave_[k]) m_.setCam(k, camOrig_[k]);
        emit message("Camera values restored.");
    });
    {
        auto* co = makeCard(v, "Camera offset");
        co->addWidget(hint("Shifts the camera sideways, up/down, or forward/back from its normal spot. Freeze keeps a value applied."));
        struct OffDef { const char* label; int axis; };
        const OffDef offs[] = {{"Camera offset X", 0}, {"Camera offset Y", 1}, {"Camera offset Z", 2}};
        for (const OffDef& d : offs) {
            const int ax = d.axis;
            addRow(co, camera_, d.label,
                   [this, ax](double& x) {
                       Vec3 v;
                       if (!m_.getCamOffset(v)) return false;
                       x = ax == 0 ? v.x : ax == 1 ? v.y : v.z;
                       return true;
                   },
                   [this, ax](double x) {
                       Vec3 v;
                       m_.getCamOffset(v);     // best effort: keep whatever the other two axes currently are
                       if (ax == 0) v.x = x; else if (ax == 1) v.y = x; else v.z = x;
                       return m_.setCamOffset(v);
                   },
                   -5000, 5000, 1, 10);
        }
    }
    buildDisplayMode(v);
    v->addStretch(1);
}

// ---------------------------------------------------------------- display mode (on-screen overlay)
void Mcd2Hub::buildDisplayMode(QVBoxLayout* v) {
    auto* c = makeCard(v, "Display mode");
    c->addWidget(hint("An on-screen panel of live stats, drawn right over the game window - pick what shows, where it sits, and how big. "
                       "The panel groups what's on into Combat, Position, Currency and Status, each a line apart, so it stays readable "
                       "with a lot switched on."));
    dmEnabled_ = new QCheckBox("Enable Display mode");
    c->addWidget(dmEnabled_);
    c->addWidget(new QLabel("<b>Combat</b>"));
    auto* row1 = new QHBoxLayout;
    dmHealth_ = new QCheckBox("Health");
    dmDamage_ = new QCheckBox("Damage (multiplier + crit chance)");
    dmDps_ = new QCheckBox("Damage per second");
    dmSpeed_ = new QCheckBox("Walk speed");
    row1->addWidget(dmHealth_);
    row1->addWidget(dmDamage_);
    row1->addWidget(dmDps_);
    row1->addWidget(dmSpeed_);
    row1->addStretch(1);
    c->addLayout(row1);
    c->addWidget(new QLabel("<b>Position</b>"));
    auto* row2 = new QHBoxLayout;
    dmCoords_ = new QCheckBox("Coordinates (X, Y, Z)");
    dmBlueprint_ = new QCheckBox("Nearest blueprint");
    row2->addWidget(dmCoords_);
    row2->addWidget(dmBlueprint_);
    row2->addWidget(new QLabel("within"));
    dmBlueprintRadius_ = new QDoubleSpinBox;
    dmBlueprintRadius_->setRange(100, 50000);
    dmBlueprintRadius_->setValue(1500);
    dmBlueprintRadius_->setDecimals(0);
    dmBlueprintRadius_->setSingleStep(100);
    row2->addWidget(dmBlueprintRadius_);
    row2->addStretch(1);
    c->addLayout(row2);
    c->addWidget(new QLabel("<b>Currency</b>"));
    auto* row3c = new QHBoxLayout;
    dmEmeralds_ = new QCheckBox("Emeralds");
    dmShards_ = new QCheckBox("Echo Shards");
    dmEnchant_ = new QCheckBox("Enchantment points");
    dmSouls_ = new QCheckBox("Souls");
    row3c->addWidget(dmEmeralds_);
    row3c->addWidget(dmShards_);
    row3c->addWidget(dmEnchant_);
    row3c->addWidget(dmSouls_);
    row3c->addStretch(1);
    c->addLayout(row3c);
    c->addWidget(new QLabel("<b>Status</b>"));
    auto* row2b = new QHBoxLayout;
    dmQuest_ = new QCheckBox("Active quest");
    dmDeaths_ = new QCheckBox("Deaths");
    row2b->addWidget(dmQuest_);
    row2b->addWidget(dmDeaths_);
    row2b->addStretch(1);
    c->addLayout(row2b);
    dmNote_ = hint("Damage per second and Nearest blueprint track themselves - no other switch needed. Deaths counts from "
                   "the moment you turn it on, not the whole play session, and may count a co-op knockdown the same as a real death.");
    c->addWidget(dmNote_);
    auto* row3 = new QHBoxLayout;
    row3->addWidget(new QLabel("Corner"));
    dmCorner_ = new QComboBox;
    for (const char* o : {"Top-left", "Top-right", "Bottom-left", "Bottom-right"}) dmCorner_->addItem(I18n::tr(o));
    row3->addWidget(dmCorner_);
    row3->addWidget(new QLabel("Size"));
    dmSize_ = new QSlider(Qt::Horizontal);
    dmSize_->setRange(50, 300);     // percent
    dmSize_->setMaximumWidth(150);
    row3->addWidget(dmSize_);
    row3->addStretch(1);
    c->addLayout(row3);
    auto* row4 = new QHBoxLayout;
    row4->addWidget(new QLabel("Nudge left/right"));
    dmOffX_ = new QSpinBox;
    dmOffX_->setRange(-2000, 2000);
    row4->addWidget(dmOffX_);
    row4->addWidget(new QLabel("up/down"));
    dmOffY_ = new QSpinBox;
    dmOffY_->setRange(-2000, 2000);
    row4->addWidget(dmOffY_);
    row4->addStretch(1);
    c->addLayout(row4);
    c->addWidget(hint("If the panel tracks the game window but sits a little off from where you'd like it, use Nudge to drag it into place - positive moves it right/down, negative left/up. Settings here are remembered."));

    QSettings set;
    dmEnabled_->setChecked(set.value("displaymode/enabled", false).toBool());
    dmCorner_->setCurrentIndex(set.value("displaymode/corner", 0).toInt());
    dmSize_->setValue(set.value("displaymode/size", 100).toInt());
    dmOffX_->setValue(set.value("displaymode/offx", 0).toInt());
    dmOffY_->setValue(set.value("displaymode/offy", 0).toInt());
    dmBlueprintRadius_->setValue(set.value("displaymode/blueprintradius", 1500).toDouble());
    connect(dmBlueprintRadius_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double r) { QSettings().setValue("displaymode/blueprintradius", r); });

    overlay_ = new StatOverlay(nullptr);
    overlay_->setScale(dmSize_->value() / 100.0);
    connect(dmSize_, &QSlider::valueChanged, this, [this](int pct) { overlay_->setScale(pct / 100.0); QSettings().setValue("displaymode/size", pct); });
    connect(dmCorner_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int i) { QSettings().setValue("displaymode/corner", i); });
    connect(dmOffX_, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int x) { QSettings().setValue("displaymode/offx", x); });
    connect(dmOffY_, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int y) { QSettings().setValue("displaymode/offy", y); });

    // The master switch: turning it off forces every stat checkbox off and greys them out (plus the corner/size/
    // nudge controls, since they'd do nothing with nothing shown); turning it back on just re-enables them, letting
    // you pick whichever stats you want - it doesn't restore the previous selection on its own.
    const auto applyEnabled = [this](bool on) {
        for (QCheckBox* b : {dmHealth_, dmDamage_, dmDps_, dmSpeed_, dmQuest_, dmCoords_, dmDeaths_,
                             dmEmeralds_, dmShards_, dmEnchant_, dmSouls_, dmBlueprint_}) {
            if (!b) continue;
            b->setEnabled(on);
            if (!on) b->setChecked(false);
        }
        for (QWidget* w : {(QWidget*)dmCorner_, (QWidget*)dmSize_, (QWidget*)dmOffX_, (QWidget*)dmOffY_, (QWidget*)dmBlueprintRadius_}) if (w) w->setEnabled(on);
    };
    applyEnabled(dmEnabled_->isChecked());
    connect(dmEnabled_, &QCheckBox::toggled, this, [this, applyEnabled](bool on) {
        applyEnabled(on);
        QSettings().setValue("displaymode/enabled", on);
        if (!on) { dmHavePos_ = false; m_.dpsReset(); dmDpsHits_.clear(); m_.deathReset(); if (overlay_) overlay_->setLinesHtml(QString()); }
    });

    for (QCheckBox* b : {dmHealth_, dmDamage_, dmSpeed_, dmQuest_, dmCoords_, dmEmeralds_, dmShards_, dmEnchant_, dmSouls_, dmBlueprint_}) {
        connect(b, &QCheckBox::toggled, this, [this](bool) {
            dmHavePos_ = false;     // restart the walk-speed timer cleanly whenever the set of visible lines changes
        });
    }
    connect(dmDps_, &QCheckBox::toggled, this, [this](bool on) {
        dmHavePos_ = false;
        if (!on) { m_.dpsReset(); dmDpsHits_.clear(); }     // drop the enemy list and rolling window so it starts clean next time
    });
    connect(dmDeaths_, &QCheckBox::toggled, this, [this](bool on) {
        dmHavePos_ = false;
        if (!on) m_.deathReset();       // so turning it back on later starts the count fresh rather than picking up where it left off
    });
}

void Mcd2Hub::overlayTick() {
    if (!overlay_) return;
    const bool anyOn = dmEnabled_ && dmEnabled_->isChecked() &&
                       ((dmHealth_ && dmHealth_->isChecked()) || (dmDamage_ && dmDamage_->isChecked()) ||
                        (dmDps_ && dmDps_->isChecked()) || (dmSpeed_ && dmSpeed_->isChecked()) ||
                        (dmQuest_ && dmQuest_->isChecked()) || (dmCoords_ && dmCoords_->isChecked()) || (dmDeaths_ && dmDeaths_->isChecked()) ||
                        (dmEmeralds_ && dmEmeralds_->isChecked()) || (dmShards_ && dmShards_->isChecked()) ||
                        (dmEnchant_ && dmEnchant_->isChecked()) || (dmSouls_ && dmSouls_->isChecked()) || (dmBlueprint_ && dmBlueprint_->isChecked()));
    if (!anyOn || !ready_()) {
        overlay_->setLinesHtml(QString());
        dmHavePos_ = false;
        return;
    }
    // Grouped into Combat / Position / Currency / Status, each its own block, with a blank line between whichever
    // blocks have anything on - so a lot switched on at once still reads as sections instead of one long list.
    QStringList combat, position, currency, status;
    Mcd2::OverlayStats s;
    const bool haveStats = m_.overlayStats(s);
    Vec3 pos;
    const bool havePos = m_.getLocation(pos);
    if (dmHealth_->isChecked()) {
        combat << (haveStats ? QString("<span style=\"color:#5fd16a\">Health: %1 / %2</span>").arg((int)s.health).arg((int)s.healthMax)
                             : QString("<span style=\"color:#5fd16a\">Health: -</span>"));
    }
    if (dmDamage_->isChecked()) {
        combat << (haveStats ? QString("<span style=\"color:#ff9f43\">Damage x%1, crit %2%</span>").arg(s.dmgMult, 0, 'f', 2).arg(s.critChance * 100.0, 0, 'f', 0)
                             : QString("<span style=\"color:#ff9f43\">Damage: -</span>"));
    }
    if (dmDps_->isChecked()) {
        const double dealt = m_.dpsPoll();     // damage dealt to enemies since the last call, almost always 0
        const qint64 nowDps = QDateTime::currentMSecsSinceEpoch();
        if (dealt > 0) dmDpsHits_.push_back({nowDps, dealt});
        while (!dmDpsHits_.empty() && nowDps - dmDpsHits_.front().first > 5000) dmDpsHits_.pop_front();
        double sum = 0;
        for (const auto& h : dmDpsHits_) sum += h.second;
        combat << QString("<span style=\"color:#ff6b6b\">DPS: %1</span>").arg(sum / 5.0, 0, 'f', 0);
    }
    if (dmSpeed_->isChecked()) {
        if (havePos) {
            const qint64 now = dmSpeedTimer_.isValid() ? dmSpeedTimer_.elapsed() : 0;
            if (dmHavePos_ && now > 0) {
                const double dt = now / 1000.0;
                if (dt > 0.01) {
                    const double dx = pos.x - dmLastPos_.x, dy = pos.y - dmLastPos_.y, dz = pos.z - dmLastPos_.z;
                    const double inst = std::sqrt(dx * dx + dy * dy + dz * dz) / dt;
                    dmSpeedSmoothed_ = dmSpeedSmoothed_ * 0.7 + inst * 0.3;
                }
            }
            dmSpeedTimer_.restart();
            combat << QString("<span style=\"color:#4fc3f7\">Walk speed: %1 units/s</span>").arg(dmSpeedSmoothed_, 0, 'f', 0);
        } else {
            combat << QString("<span style=\"color:#4fc3f7\">Walk speed: -</span>");
        }
    }
    if (dmCoords_->isChecked()) {
        position << (havePos ? QString("<span style=\"color:#a78bfa\">X: %1  Y: %2  Z: %3</span>").arg(pos.x, 0, 'f', 0).arg(pos.y, 0, 'f', 0).arg(pos.z, 0, 'f', 0)
                              : QString("<span style=\"color:#a78bfa\">X/Y/Z: -</span>"));
    }
    if (dmBlueprint_->isChecked()) {
        std::string name; double dist = 0;
        position << (m_.nearestEnemy(dmBlueprintRadius_ ? dmBlueprintRadius_->value() : 1500, name, dist)
                         ? QString("<span style=\"color:#ff79c6\">Nearest: %1 (%2)</span>").arg(QString::fromStdString(name).toHtmlEscaped()).arg((int)dist)
                         : QString("<span style=\"color:#ff79c6\">Nearest: none close by</span>"));
    }
    if (dmEmeralds_->isChecked()) {
        currency << (haveStats ? QString("<span style=\"color:#16c172\">Emeralds: %1</span>").arg((int)s.emeralds)
                               : QString("<span style=\"color:#16c172\">Emeralds: -</span>"));
    }
    if (dmShards_->isChecked()) {
        currency << (haveStats ? QString("<span style=\"color:#00cec9\">Echo Shards: %1</span>").arg((int)s.echoShards)
                               : QString("<span style=\"color:#00cec9\">Echo Shards: -</span>"));
    }
    if (dmEnchant_->isChecked()) {
        currency << (haveStats ? QString("<span style=\"color:#cc66ff\">Enchant points: %1</span>").arg((int)s.enchantPoints)
                               : QString("<span style=\"color:#cc66ff\">Enchant points: -</span>"));
    }
    if (dmSouls_->isChecked()) {
        currency << (haveStats ? QString("<span style=\"color:#ecf0f1\">Souls: %1</span>").arg((int)s.souls)
                               : QString("<span style=\"color:#ecf0f1\">Souls: -</span>"));
    }
    if (dmQuest_->isChecked()) {
        status << (haveStats && !s.questName.empty()
                      ? QString("<span style=\"color:#ffe066\">Quest: %1</span>").arg(QString::fromStdString(s.questName).toHtmlEscaped())
                      : QString("<span style=\"color:#ffe066\">Quest: none active</span>"));
    }
    if (dmDeaths_->isChecked()) {
        status << QString("<span style=\"color:#9e9e9e\">Deaths: %1</span>").arg(m_.deathPoll());
    }
    dmHavePos_ = havePos;
    if (havePos) dmLastPos_ = pos;
    QStringList blocks;
    for (const QStringList& g : {combat, position, currency, status}) if (!g.isEmpty()) blocks << g.join("<br>");
    overlay_->setLinesHtml(blocks.join("<br><br>"));
    long x, y, w, h;
    if (m_.gameWindowRect(x, y, w, h))
        overlay_->trackWindow(x, y, w, h, dmCorner_ ? dmCorner_->currentIndex() : 0,
                              dmOffX_ ? dmOffX_->value() : 0, dmOffY_ ? dmOffY_->value() : 0);
}

void Mcd2Hub::applyPreset(const QString& name, float fov, float dfov, float arm, float seek) {
    if (!ready_()) { emit message("Minecraft Dungeons II is not connected."); return; }
    if (!scanNow()) return;                                    // always look again: a new level has new camera objects
    for (int k = 0; k < Mcd2::CamCount; ++k) {                 // remember the original values before the first change
        float f;
        if (!camHave_[k] && m_.getCam(k, f)) { camHave_[k] = true; camOrig_[k] = f; }
    }
    (void)arm; (void)seek;                                       // presets never touch the camera distance
    const float nan = std::numeric_limits<float>::quiet_NaN();
    float vals[Mcd2::CamCount] = {fov, dfov, nan, nan};
    vals[Mcd2::Fov] = fov; vals[Mcd2::DefaultFov] = dfov;
    vals[Mcd2::ArmLength] = nan; vals[Mcd2::SeekArm] = nan;
    int ok = 0;
    for (int k = 0; k < Mcd2::CamCount; ++k) {
        if (std::isnan(vals[k])) continue;
        if (!m_.setCam(k, vals[k])) continue;
        ++ok;
        auto& r = rows_[(size_t)camRow_[k]];
        r.spin->blockSignals(true);
        r.spin->setValue(vals[k]);
        r.spin->blockSignals(false);
        r.freeze->setChecked(camKeep_->isChecked());
    }
    emit message(ok ? QString("%1 applied.").arg(name) : QString("Camera not found - press Find camera first."));
}

// ---------------------------------------------------------------- nearby
void Mcd2Hub::buildNearby() {
    QVBoxLayout* v = nullptr;
    nearby_ = makePage("Nearby Viewer", "Only works in OFFLINE - Chests and enemies close to your character, nearest first. Press Refresh the first time and again after you change level "
                       "(it scans the game, which can take a few seconds). Double-click a row (or use Teleport to selected) to teleport there.", v);
    auto* bar = new QHBoxLayout;
    nearOn_ = new QCheckBox("Turn on");
    auto* refresh = new QPushButton("Refresh");
    nearChests_ = new QCheckBox("Chests");
    nearMobs_ = new QCheckBox("Enemies");
    nearClosed_ = new QCheckBox("Only unopened chests");
    nearAuto_ = new QCheckBox("Update automatically");
    nearChests_->setChecked(true);
    nearMobs_->setChecked(true);
    nearClosed_->setChecked(true);
    auto* rl = new QLabel("Radius");
    nearRadius_ = new QDoubleSpinBox;
    nearRadius_->setRange(100, 5000000);
    nearRadius_->setValue(6000);
    nearRadius_->setDecimals(0);
    nearRadius_->setSingleStep(500);
    refresh->setCursor(Qt::PointingHandCursor);
    bar->addWidget(nearOn_);
    bar->addWidget(refresh);
    bar->addWidget(rl);
    bar->addWidget(nearRadius_);
    bar->addWidget(nearChests_);
    bar->addWidget(nearMobs_);
    bar->addWidget(nearClosed_);
    bar->addWidget(nearAuto_);
    bar->addStretch(1);
    v->addLayout(bar);
    {   // radius presets
        auto* pr = new QHBoxLayout;
        auto* pl = new QLabel("Radius presets");
        pr->addWidget(pl);
        struct Preset { const char* label; double radius; };
        for (const Preset& p : {Preset{"Normal", 6000}, Preset{"Far", 750000}, Preset{"Super far", 3000000}}) {
            auto* btn = new QPushButton(p.label);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setToolTip(QString("%1").arg(p.radius, 0, 'f', 0));
            pr->addWidget(btn);
            const double rad = p.radius;
            connect(btn, &QPushButton::clicked, this, [this, rad]() {
                nearRadius_->setValue(rad);
                if (ready_()) refreshNearby();
            });
        }
        pr->addStretch(1);
        v->addLayout(pr);
    }
    nearNote_ = hint("Nothing read yet.");
    v->addWidget(nearNote_);
    nearTree_ = new QTreeWidget;
    nearTree_->setHeaderLabels({"Type", "Name", "State", "Distance", "X", "Y", "Z"});
    nearTree_->setRootIsDecorated(false);
    nearTree_->setAlternatingRowColors(true);
    nearTree_->setMinimumHeight(420);
    nearTree_->setColumnWidth(0, 70);
    nearTree_->setColumnWidth(1, 260);
    nearTree_->setColumnWidth(2, 150);
    nearTree_->setColumnWidth(3, 80);
    v->addWidget(nearTree_, 1);
    auto* b = new QHBoxLayout;
    auto* go = new QPushButton("Teleport to selected");
    auto* save = new QPushButton("Save as waypoint");
    auto* copy = new QPushButton("Copy list");
    for (auto* x : {go, save, copy}) { x->setCursor(Qt::PointingHandCursor); b->addWidget(x); }
    b->addStretch(1);
    v->addLayout(b);
    connect(refresh, &QPushButton::clicked, this, [this]() { if (nearOn_->isChecked() && scanNow()) refreshNearby(); });
    wirePageSwitch(nearOn_, {refresh, nearRadius_, nearChests_, nearMobs_, nearClosed_}, {nearAuto_}, [this]{ refreshNearby(); },
                   [this]{ nearTree_->clear(); nearNote_->setText(I18n::tr("Turn this page on to read the game.")); });
    nearNote_->setText(I18n::tr("Turn this page on to read the game."));
    connect(go, &QPushButton::clicked, this, [this]() {
        auto* it = nearTree_->currentItem();
        if (!it) { emit message("Select a row first."); return; }
        const Vec3 p{it->text(4).toDouble(), it->text(5).toDouble(), it->text(6).toDouble() + 120.0};     // land just above it
        emit message(ready_() && m_.setLocation(p) ? "Position written." : "Could not write the position.");
    });
    connect(nearTree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it, int) {
        if (!it) return;
        const Vec3 p{it->text(4).toDouble(), it->text(5).toDouble(), it->text(6).toDouble() + 120.0};
        emit message(ready_() && m_.setLocation(p) ? "Position written." : "Could not write the position.");
    });
    connect(save, &QPushButton::clicked, this, [this]() {
        auto* it = nearTree_->currentItem();
        if (!it) { emit message("Select a row first."); return; }
        addWaypointRow(QString("%1 (%2)").arg(it->text(1), it->text(0)), QString::fromStdString(m_.worldName()),
                       it->text(4).toDouble(), it->text(5).toDouble(), it->text(6).toDouble());
        saveWaypoints();
        emit message("Saved as a waypoint (see World).");
    });
    connect(copy, &QPushButton::clicked, this, [this]() {
        QStringList lines;
        for (int i = 0; i < nearTree_->topLevelItemCount(); ++i) {
            QStringList cols;
            for (int c = 0; c < nearTree_->columnCount(); ++c) cols << nearTree_->topLevelItem(i)->text(c);
            lines << cols.join("\t");
        }
        QGuiApplication::clipboard()->setText(lines.join("\n"));
        emit message("List copied.");
    });
}

void Mcd2Hub::refreshNearby() {
    if (!nearOn_ || !nearOn_->isChecked()) return;
    std::vector<Mcd2::Near> list;
    std::string note;
    const bool ok = ready_() && m_.nearby(nearRadius_->value(), nearChests_->isChecked(), nearMobs_->isChecked(), nearClosed_->isChecked(), list, note);
    nearNote_->setText(nt(note.empty() ? std::string("Not ready.") : note));
    if (!ok) return;
    const QString keepName = nearTree_->currentItem() ? nearTree_->currentItem()->text(1) + nearTree_->currentItem()->text(4) : QString();
    nearTree_->setUpdatesEnabled(false);
    nearTree_->clear();
    QTreeWidgetItem* reselect = nullptr;
    for (auto& n : list) {
        auto* it = new QTreeWidgetItem(nearTree_);
        it->setText(0, I18n::tr(QString::fromStdString(n.kind)));
        it->setText(1, QString::fromStdString(n.name));
        it->setText(2, I18n::tr(QString::fromStdString(n.state)));
        it->setText(3, QString::number(n.dist, 'f', 0));
        it->setText(4, QString::number(n.pos.x, 'f', 1));
        it->setText(5, QString::number(n.pos.y, 'f', 1));
        it->setText(6, QString::number(n.pos.z, 'f', 1));
        if (!keepName.isEmpty() && it->text(1) + it->text(4) == keepName) reselect = it;
    }
    if (reselect) nearTree_->setCurrentItem(reselect);
    nearTree_->setUpdatesEnabled(true);
}

// ---------------------------------------------------------------- timer
void Mcd2Hub::tick() {
    bool anyFreeze = (infArrowBox_ && infArrowBox_->isChecked()) || (infEnchBox_ && infEnchBox_->isChecked()) || (noCdBox_ && noCdBox_->isChecked()) || (noClipBox_ && noClipBox_->isChecked()) || (godBox_ && godBox_->isChecked()) || (overBox_ && overBox_->isChecked()) || (diffKeep_ && diffKeep_->isChecked()) || (forceRarityKeep_ && forceRarityKeep_->isChecked()) ||
                     (dmEnabled_ && dmEnabled_->isChecked() && ((dmHealth_ && dmHealth_->isChecked()) || (dmDamage_ && dmDamage_->isChecked()) || (dmDps_ && dmDps_->isChecked()) || (dmSpeed_ && dmSpeed_->isChecked()) || (dmQuest_ && dmQuest_->isChecked()) || (dmCoords_ && dmCoords_->isChecked()) || (dmDeaths_ && dmDeaths_->isChecked()) ||
                                                                 (dmEmeralds_ && dmEmeralds_->isChecked()) || (dmShards_ && dmShards_->isChecked()) || (dmEnchant_ && dmEnchant_->isChecked()) || (dmSouls_ && dmSouls_->isChecked()) || (dmBlueprint_ && dmBlueprint_->isChecked())));
    for (auto& r : rows_) if (r.freeze->isChecked()) { anyFreeze = true; break; }
    const bool slowDue = slow_.elapsed() >= 300;
    const bool anyVisible = (player_ && player_->isVisible()) || (motion_ && motion_->isVisible()) || (inventory_ && inventory_->isVisible()) ||
                            (world_ && world_->isVisible()) || (camera_ && camera_->isVisible()) || (nearby_ && nearby_->isVisible());
    if (!anyFreeze && !anyVisible) return;
    if (!slowDue && !anyFreeze) return;
    if (slowDue) slow_.restart();
    if (!ready_()) {
        if (plState_ && player_->isVisible()) plState_->setText("Minecraft Dungeons II is not connected. Pick it on Home and start the game.");
        overlayTick();          // hides the overlay (it checks ready_() itself) instead of leaving stale numbers on screen
        return;
    }
    // no clip and the difficulty lock-in also run while their page is hidden
    overlayTick();
    if (noClipBox_ && noClipBox_->isChecked()) {
        {
            bool coll = true;
            if (m_.getCollision(coll) && coll) m_.setCollision(false);       // the game can switch collision back on
        }
        int mode = 0;
        if (m_.getMoveMode(mode) && mode != 5) m_.setMoveMode(5);
        const bool upKey = (GetAsyncKeyState(VK_PRIOR) & 0x8000) != 0, downKey = (GetAsyncKeyState(VK_NEXT) & 0x8000) != 0;
        if (upKey != downKey) {
            Vec3 p;
            if (m_.getLocation(p)) { p.z += (upKey ? 1 : -1) * flyStep_->value(); m_.setLocation(p); }
        }
    }
    if (overBox_ && overBox_->isChecked()) {
        for (int k = 0; k < 4; ++k) {
            const int d = attrIndex(kOverLabels[k]);
            float cur = 0;
            if (d < 0 || !m_.getAttr(d, cur)) continue;
            if (cur != kOverBig) { overOrig_[k] = cur; overHave_[k] = true; m_.setAttr(d, kOverBig); }   // the game resets these on level changes
        }
    }
    if (godBox_ && godBox_->isChecked()) {
        static int hp = -1, hpMax = -1;
        if (hp < 0) {
            const auto& defs = Mcd2::attrDefs();
            for (int i = 0; i < (int)defs.size(); ++i) {
                if (std::string(defs[i].label) == "Health") hp = i;
                else if (std::string(defs[i].label) == "Max health") hpMax = i;
            }
        }
        float mx = 0;
        if (hp >= 0 && hpMax >= 0 && m_.getAttr(hpMax, mx) && mx > 0) m_.setAttr(hp, mx);
    }
    if ((infArrowBox_ && infArrowBox_->isChecked()) || (infEnchBox_ && infEnchBox_->isChecked()) || (noCdBox_ && noCdBox_->isChecked())) {
        auto idx = [](const char* label) {
            const auto& defs = Mcd2::attrDefs();
            for (int i = 0; i < (int)defs.size(); ++i) if (std::string(defs[i].label) == label) return i;
            return -1;
        };
        static const int iArrow = idx("Arrows (ammo)"), iArrowMax = idx("Max arrows"), iEnch = idx("Enchantment points"),
                         iPotion = idx("Health potion charges"), iRoll = idx("Roll charges"), iSoul = idx("Souls"), iSoulMax = idx("Max souls"),
                         iPotionCd = idx("Health potion cooldown"), iPotionCdMult = idx("Health potion cooldown multiplier"),
                         iArtifactCdMult = idx("Artifact cooldown multiplier");
        float cur = 0, mx = 0;
        if (infArrowBox_ && infArrowBox_->isChecked() && m_.getAttr(iArrow, cur) && m_.getAttr(iArrowMax, mx) && mx > 0 && cur < mx) m_.setAttr(iArrow, mx);
        // a value that falls is put back to the highest it has been since the switch was turned on
        auto keep = [this](int def, float& floorV) {
            float c;
            if (def < 0 || !m_.getAttr(def, c)) return;
            if (floorV < 0 || c > floorV) floorV = c;
            else if (c < floorV) m_.setAttr(def, floorV);
        };
        if (infEnchBox_ && infEnchBox_->isChecked()) keep(iEnch, enchFloor_);
        if (noCdBox_ && noCdBox_->isChecked()) {
            keep(iPotion, potionFloor_);
            keep(iRoll, rollFloor_);
            if (m_.getAttr(iSoulMax, mx) && mx > 0 && m_.getAttr(iSoul, cur) && cur < mx) m_.setAttr(iSoul, mx);
            // Holding charges/souls at their floor isn't enough by itself: Potions and Artifacts also have
            // their own cooldown-duration attributes (separate from charge count) gating reuse. Force those
            // to zero too so the cooldown is always already elapsed. Roll isn't touched here since it's
            // reported to work fine off charges alone, and its "modifier" attribute's sign isn't known to be
            // safe to zero blindly.
            auto zero = [this](int def) {
                float c;
                if (def >= 0 && m_.getAttr(def, c) && c != 0.0f) m_.setAttr(def, 0.0f);
            };
            zero(iPotionCd);
            zero(iPotionCdMult);
            zero(iArtifactCdMult);
        }
    }
    if (diffKeep_ && diffKeep_->isChecked()) {
        std::string tag;
        bool locked;
        const std::string want = diffBox_->currentData().toString().toStdString();
        if (m_.getDifficulty(tag, locked) && tag != want) m_.setDifficulty(want);
    }
    if (forceRarityKeep_ && forceRarityKeep_->isChecked() && forceRarityTimer_.elapsed() > 4000) {
        forceRarityTimer_.restart();
        std::string e;
        applyForceRarity(forceRarityBox_->currentData().toString().toStdString(), e);     // silently reapplied; the button already reports errors
    }
    if (ignBox_ && ignBox_->isChecked() && slowDue && ++ignTicks_ % 3 == 0) m_.enemyIgnoreTick();     // about once a second
    // freezes run even when their page is hidden
    for (auto& r : rows_) {
        if (r.freeze->isChecked()) { r.set(r.spin->value()); continue; }
        if (!r.page->isVisible() || r.spin->hasFocus() || !slowDue) continue;
        double d;
        if (r.get(d)) { r.spin->blockSignals(true); r.spin->setValue(d); r.spin->blockSignals(false); }
    }
    if (!slowDue) return;
    if (player_->isVisible()) {
        Vec3 p;
        double yaw = 0;
        Mcd2::Player pl;
        if (m_.player(pl)) {
            plState_->setText("Connected. Player found.");
            plPos_->setText(m_.getLocation(p, &yaw) ? QString("Position:  X %1   Y %2   Z %3   (facing %4 deg)").arg(p.x, 0, 'f', 1).arg(p.y, 0, 'f', 1).arg(p.z, 0, 'f', 1).arg(yaw, 0, 'f', 0)
                                                    : QString("Position: not readable"));
            plSets_->setText(QString::fromStdString(m_.attrSummary()));
        } else {
            plState_->setText("Engine found, but no player yet. Load into a level.");
        }
    }
    if (player_->isVisible() && diffNow_) {
        std::string tag;
        bool locked = false;
        diffNow_->setText(m_.getDifficulty(tag, locked) ? QString("Difficulty: %1%2").arg(QString::fromStdString(tag), locked ? "  (locked)" : "")
                                                        : QString("Difficulty: not readable yet"));
    }
    if (nearAuto_ && nearAuto_->isChecked() && nearby_->isVisible()) {
        // Every 6s, re-scan the game itself (same work as pressing Refresh) so newly loaded chests/enemies appear
        // on their own, not just updated positions/state for the ones already known. Quietly skipped if it fails
        // (e.g. between levels) - the next tick tries again.
        if (nearRescanTimer_.elapsed() > 6000) { nearRescanTimer_.restart(); std::string e; if (ready_()) m_.rescan(e); }
        if (nearTimer_.elapsed() > 1500) { nearTimer_.restart(); refreshNearby(); }
    }
    if (invAuto_ && invAuto_->isChecked() && inventory_->isVisible() && invTimer_.elapsed() > 2000) { invTimer_.restart(); refreshInventory(); }
    if (worldAuto_ && worldAuto_->isChecked() && world_->isVisible() && worldTimer_.elapsed() > 3000) { worldTimer_.restart(); refreshWorld(); }
    // Inventory Editor: always kept current (no on/off switch for it) so new pickups and inventory changes show up on their own.
    if (edOn_ && edOn_->isChecked() && editor_->isVisible() && edTimer_.elapsed() > 2000) { edTimer_.restart(); refreshEditor(); }
}

} // namespace dk

namespace dk {

// ---------------------------------------------------------------- inventory editor
namespace {

// "SW.Item.Club" -> "Club", "SW.Enchantment.Arcane.II" -> "Arcane II". Looks up the game's real display name
// in the community data catalog (Mcd2Catalog) first - for items and effects/enchants that's a proper name
// like "Humbler Heartstring" or "Prospector" instead of a guessed-at tag, which is what the item/effect
// editors below actually show. Falls back to the old tag-cleanup for anything the catalog doesn't know.
QString pretty(const QString& tag) {
    static const QRegularExpression tierRe("^(.*)\\.(I|II|III|IV|V)$");
    if (tag.startsWith("SW.Item.")) {
        const std::string nm = Mcd2Catalog::itemName(tag.toStdString());
        if (!nm.empty()) return QString::fromStdString(nm);
    } else if (tag.startsWith("SW.Effect.") || tag.startsWith("SW.Enchantment.")) {
        QString base = tag; QString tier;
        const auto m = tierRe.match(tag);
        if (m.hasMatch()) { base = m.captured(1); tier = m.captured(2); }
        const std::string nm = Mcd2Catalog::effectName(base.toStdString());
        if (!nm.empty()) return tier.isEmpty() ? QString::fromStdString(nm) : QString::fromStdString(nm) + " " + tier;
    }
    static const char* prefixes[] = {"SW.Item.", "SW.Rarity.", "SW.Enchantment.", "SW.ItemSlot.Equipment.", "SW.ItemSlot.", "SW."};
    QString t = tag;
    for (const char* p : prefixes) if (t.startsWith(QLatin1String(p))) { t = t.mid((int)strlen(p)); break; }
    t.replace('.', ' ').replace('_', ' ');
    return t;
}

// Things that are not gear: audio, cosmetics, consumables, quest items, currencies, crafting materials ...
bool isGear(const QString& tag) {
    static const char* skipGroup[] = {"Audio", "Cosmetic", "Consumable", "QuestItem", "Property", "Currency", "Effect", "AttackVariant",
                                      "Attributes", "ExceptFromDefaultLootTables", "TalismanTemplate", "TestTalisman"};
    static const char* skipFlat[] = {"Carrot", "Coal", "CorruptedSeeds", "DeathcapMushroom", "Iron", "Leather", "Potato", "SpiderString", "Stick",
                                     "Wheat", "WheatSeeds", "Wood", "Wool", "WitchesBrew", "BucketOfAxolotl", "Fists"};
    if (!tag.startsWith("SW.Item.")) return false;
    const QString rest = tag.mid(8);
    const QString first = rest.section('.', 0, 0);
    for (const char* g : skipGroup) if (first == QLatin1String(g)) return false;
    if (!rest.contains('.')) for (const char* f : skipFlat) if (rest == QLatin1String(f)) return false;
    return true;
}

// Talismans are the only items whose Level and Item XP fields actually do anything in game (everything else
// levels up through Power instead), so those two fields are hidden until a Talisman is selected. Based on the
// naming convention the game's own tags already use elsewhere ("TalismanTemplate" and "TestTalisman" are
// explicitly excluded as non-gear above, implying plain "Talisman" is the real, live item group).
bool isTalisman(const QString& tag) {
    return tag.startsWith("SW.Item.Talisman.");
}

} // namespace

void Mcd2Hub::buildEditor() {
    QVBoxLayout* v = nullptr;
    editor_ = makePage("Inventory Editor", "Offline only. Press Refresh, double-click an item below to select it, then edit it under Selected item - changes apply right away.", v);
    auto* bar = new QHBoxLayout;
    edOn_ = new QCheckBox("Turn on");
    auto* refresh = new QPushButton("Refresh");
    refresh->setCursor(Qt::PointingHandCursor);
    bar->addWidget(edOn_);
    bar->addWidget(refresh);
    bar->addStretch(1);
    v->addLayout(bar);
    auto* filterBar = new QHBoxLayout;
    edEquipped_ = new QCheckBox("Equipped only");
    edAll_ = new QCheckBox("Show everything");
    auto* filterLabel = new QLabel("Rarity");
    edRarityFilter_ = new QComboBox;
    edRarityFilter_->addItem(I18n::tr("All rarities"), QString());
    for (const char* r : {"Common", "Rare", "Special", "Unique"}) edRarityFilter_->addItem(I18n::tr(r), QString("SW.Rarity.") + r);
    filterBar->addWidget(edEquipped_);
    filterBar->addWidget(edAll_);
    filterBar->addWidget(filterLabel);
    filterBar->addWidget(edRarityFilter_);
    filterBar->addStretch(1);
    v->addLayout(filterBar);
    auto* jump = new QHBoxLayout;
    auto* jl = new QLabel("Jump to equipped");
    jl->setMinimumWidth(110);
    edJump_ = new QComboBox;
    jump->addWidget(jl);
    jump->addWidget(edJump_, 1);
    v->addLayout(jump);
    edNote_ = hint("Nothing read yet.");
    v->addWidget(edNote_);
    edTree_ = new QTreeWidget;
    edTree_->setColumnCount(8);
    edTree_->setHeaderLabels({"Slot", "Item", "Rarity", "Power", "Level", "Stack", "Equipped in", "Effects"});
    edTree_->setRootIsDecorated(false);
    edTree_->setAlternatingRowColors(true);
    edTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    edTree_->setMinimumHeight(320);
    edTree_->header()->setSectionResizeMode(QHeaderView::Interactive);
    edTree_->setColumnWidth(0, 130);
    edTree_->setColumnWidth(1, 200);
    edTree_->setColumnWidth(2, 80);
    edTree_->setColumnWidth(3, 70);
    edTree_->setColumnWidth(4, 55);
    edTree_->setColumnWidth(5, 50);
    edTree_->setColumnWidth(6, 130);
    v->addWidget(edTree_);

    QPushButton *selAll, *del, *delCat;
    {
        auto* dc = makeCard(v, "Delete items");
        dc->addWidget(hint("Equipped items are always kept."));
        auto* row1 = new QHBoxLayout;
        selAll = new QPushButton("Select all shown");
        del = new QPushButton("Delete selected");
        selAll->setCursor(Qt::PointingHandCursor);
        del->setCursor(Qt::PointingHandCursor);
        row1->addWidget(selAll);
        row1->addWidget(del);
        row1->addStretch(1);
        dc->addLayout(row1);
        auto* row2 = new QHBoxLayout;
        auto* catLabel = new QLabel("Category");
        edCategoryBox_ = new QComboBox;
        delCat = new QPushButton("Delete entire category");
        delCat->setCursor(Qt::PointingHandCursor);
        row2->addWidget(catLabel);
        row2->addWidget(edCategoryBox_, 1);
        row2->addWidget(delCat);
        dc->addLayout(row2);
    }

    auto* c = makeCard(v, "Selected item");
    c->addWidget(hint("Change any field below and it applies right away. Level and Item XP only show up for Talismans (Level maxes out at 3)."));
    auto line = [&](QVBoxLayout* into, const QString& label, QWidget* w) -> QWidget* {
        auto* row = new QWidget;
        row->setStyleSheet("background: transparent;");
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto* l = new QLabel(label);
        l->setMinimumWidth(110);
        h->addWidget(l);
        h->addWidget(w, 1);
        into->addWidget(row);
        return row;
    };
    edItem_ = new QComboBox;
    edItem_->setEditable(true);
    edItem_->setInsertPolicy(QComboBox::NoInsert);
    edItem_->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    edItem_->completer()->setFilterMode(Qt::MatchContains);
    edRarity_ = new QComboBox;
    for (const char* r : {"Common", "Rare", "Special", "Unique"}) edRarity_->addItem(I18n::tr(r), QString("SW.Rarity.") + r);
    edPower_ = new QDoubleSpinBox; edPower_->setRange(0, 99999); edPower_->setDecimals(0); edPower_->setSingleStep(1);
    edLevel_ = new QSpinBox; edLevel_->setRange(0, 3);   // Talismans only go up to level 3; typing higher just snaps back down to it
    edStack_ = new QSpinBox; edStack_->setRange(0, 99999);
    edXp_ = new QDoubleSpinBox; edXp_->setRange(0, 99999999); edXp_->setDecimals(1); edXp_->setSingleStep(10);
    {   // Community data puts Talisman level-up thresholds at these two cumulative XP totals (XP keeps
        // counting up past them rather than stopping, so this isn't a hard cap - just a reference point).
        const auto& xp = dk::Mcd2Catalog::talismanXpThresholds();
        if (xp.size() >= 2)
            edXp_->setToolTip(I18n::tr(QString("Reaches level 2 around %1 XP, level 3 around %2 XP.").arg(xp[0]).arg(xp[1])));
    }
    edRerolls_ = new QSpinBox; edRerolls_->setRange(0, 255);
    edProps_ = new QLabel("-");
    edProps_->setWordWrap(true);
    edProps_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    line(c, "Item", edItem_);
    line(c, "Rarity", edRarity_);
    line(c, "Power", edPower_);
    edLevelRow_ = line(c, "Level", edLevel_);
    line(c, "Stack", edStack_);
    edXpRow_ = line(c, "Item XP", edXp_);
    line(c, "Effect rerolls used", edRerolls_);
    line(c, "Properties", edProps_);
    {
        auto* encHead = new QLabel("<b>Enchants</b>");
        c->addWidget(encHead);
        c->addWidget(hint("Edits an enchant already on the item - which one it is, its tier (I/II/III), and its enchantment-point cost. Can't add an enchant slot the item doesn't already have."));
        edEnchantNote_ = new QLabel("-");
        edEnchantNote_->setWordWrap(true);
        c->addWidget(edEnchantNote_);
        edEnchantLayout_ = new QVBoxLayout;
        c->addLayout(edEnchantLayout_);
    }
    edLevelRow_->setVisible(false);
    edXpRow_->setVisible(false);

    {
        auto* c = makeCard(v, "Merchant prices");
        c->addWidget(hint("Shop prices cannot be changed directly. When you buy something, the part you do not want to pay is given back in emeralds, spring stones and enchantment points. You still need the full price to buy."));
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        shopPay_ = new QDoubleSpinBox;
        shopPay_->setRange(0, 100);
        shopPay_->setDecimals(0);
        shopPay_->setSingleStep(5);
        shopPay_->setSuffix(" %");
        shopPay_->setValue(0);
        struct Preset { const char* label; int pct; };
        static const Preset presets[] = {{"Free", 0}, {"25% of normal", 25}, {"50% of normal", 50}, {"75% of normal", 75}, {"Normal", 100}};
        for (const Preset& pr : presets) {
            auto* b = new QPushButton(pr.label);
            b->setCursor(Qt::PointingHandCursor);
            const int pct = pr.pct;
            connect(b, &QPushButton::clicked, this, [this, pct]() { shopPay_->setValue(pct); });
            row->addWidget(b);
        }
        row->addStretch(1);
        c->addLayout(row);
        auto* r2 = new QHBoxLayout;
        r2->setSpacing(6);
        r2->addWidget(new QLabel("I pay"));
        r2->addWidget(shopPay_);
        shopOn_ = new QCheckBox("On");
        r2->addWidget(shopOn_);
        r2->addStretch(1);
        c->addLayout(r2);
        shopState_ = hint("-");
        c->addWidget(shopState_);
        shopLast_ = new QLabel;
        c->addWidget(shopLast_);
        connect(shopOn_, &QCheckBox::toggled, this, [this](bool) { for (float& f : shopPrev_) f = -1; });
        auto* t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this]() { shopTick(); });
        t->start(60);
    }
    v->addStretch(1);

    // a typed or picked name -> the full tag the game uses
    auto resolve = [this](QComboBox* box, const QString& prefix) -> std::string {
        const QString txt = box->currentText().trimmed();
        const int i = box->findText(txt, Qt::MatchFixedString);
        if (i >= 0) return box->itemData(i).toString().toStdString();
        if (txt.startsWith("SW.")) return txt.toStdString();
        return (prefix + txt).toStdString();
    };
    auto selectedRow = [this]() -> int {
        auto* it = edTree_->currentItem();
        return it ? edTree_->indexOfTopLevelItem(it) : -1;
    };
    // Builds and writes whatever item fields differ from the selected row; no-op if nothing actually changed.
    auto applyItemNow = [this, selectedRow, resolve]() {
        if (edLoading_) return;
        const int r = selectedRow();
        if (r < 0 || r >= (int)edRows_.size()) return;
        if (!ready_()) return;
        const auto& row = edRows_[r];
        const bool tali = isTalisman(QString::fromStdString(row.item));
        Mcd2::ItemEdit ed;
        // Level and Item XP are hidden (and left alone) for anything that isn't a Talisman - they're the only
        // items those two fields actually do anything for, and Level is capped at 3 either way.
        if (tali && edLevel_->value() != row.level) ed.level = edLevel_->value();
        if (edStack_->value() != row.stack) ed.stack = edStack_->value();
        if (std::fabs(edPower_->value() - row.power) >= 0.5) ed.power = edPower_->value();
        if (tali && std::fabs(edXp_->value() - row.xp) >= 0.05) ed.xp = edXp_->value();
        if (edRerolls_->value() != row.rerolls) ed.rerolls = edRerolls_->value();
        const std::string rar = edRarity_->currentData().toString().toStdString();
        if (rar != row.rarity) ed.rarity = rar;
        const std::string item = resolve(edItem_, "SW.Item.");
        if (item != row.item) ed.item = item;
        if (ed.level < 0 && ed.stack < 0 && ed.power < 0 && ed.xp < 0 && ed.rerolls < 0 && ed.rarity.empty() && ed.item.empty()) return;   // nothing actually changed
        std::string err;
        if (m_.editItem(row.addr, row.item, ed, err)) refreshEditor();
        else emit message(nt(err));
    };
    connect(refresh, &QPushButton::clicked, this, [this]() { if (edOn_->isChecked() && scanNow()) refreshEditor(); });
    wirePageSwitch(edOn_, {refresh, edEquipped_, edAll_, edRarityFilter_, edCategoryBox_}, {}, [this]{ refreshEditor(); },
                   [this]{ edTree_->clear(); edRows_.clear(); edAllRows_.clear(); edCategoryBox_->clear(); edNote_->setText(I18n::tr("Turn this page on to read the game."));
                           if (edLevelRow_) edLevelRow_->setVisible(false); if (edXpRow_) edXpRow_->setVisible(false);
                           Mcd2::InvRow empty; rebuildEnchants(empty); });
    edNote_->setText(I18n::tr("Turn this page on to read the game."));
    connect(edEquipped_, &QCheckBox::toggled, this, [this]() { refreshEditor(); });
    connect(edAll_, &QCheckBox::toggled, this, [this]() { refreshEditor(); });
    connect(edRarityFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshEditor(); });
    connect(edJump_, QOverload<int>::of(&QComboBox::activated), this, [this](int i) {
        const quint64 a = edJump_->itemData(i).toULongLong();
        for (int k = 0; k < edTree_->topLevelItemCount(); ++k)
            if (edRows_[k].addr == a) { edTree_->setCurrentItem(edTree_->topLevelItem(k)); edTree_->scrollToItem(edTree_->topLevelItem(k)); break; }
    });
    connect(edTree_, &QTreeWidget::currentItemChanged, this, [this, selectedRow]() {
        const int r = selectedRow();
        if (r < 0 || r >= (int)edRows_.size()) {
            if (edLevelRow_) edLevelRow_->setVisible(false);
            if (edXpRow_) edXpRow_->setVisible(false);
            Mcd2::InvRow empty;
            rebuildEnchants(empty);
            return;
        }
        const auto& row = edRows_[r];
        edLoading_ = true;
        edItem_->setEditText(pretty(QString::fromStdString(row.item)));
        const int ri = edRarity_->findData(QString::fromStdString(row.rarity));
        if (ri >= 0) edRarity_->setCurrentIndex(ri);
        edPower_->setValue(row.power);
        edLevel_->setValue(row.level);
        edStack_->setValue(row.stack);
        edXp_->setValue(row.xp);
        edRerolls_->setValue(row.rerolls);
        const bool tali = isTalisman(QString::fromStdString(row.item));
        if (edLevelRow_) edLevelRow_->setVisible(tali);
        if (edXpRow_) edXpRow_->setVisible(tali);
        {
            QStringList pr;
            for (const auto& t : row.props) pr << pretty(QString::fromStdString(t));
            edProps_->setText(pr.isEmpty() ? QString("-") : pr.join(", "));
        }
        rebuildEnchants(row);
        edLoading_ = false;
    });
    connect(edTree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it, int) {
        edTree_->setCurrentItem(it);
        if (auto* sa = editor_->findChild<QScrollArea*>()) sa->ensureWidgetVisible(edItem_->parentWidget() ? edItem_->parentWidget()->parentWidget() : edItem_, 0, 40);
        emit message("Loaded into Selected item below the list. Changes there apply instantly.");
    });
    connect(edRarity_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [applyItemNow](int) { applyItemNow(); });
    connect(edPower_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [applyItemNow](double) { applyItemNow(); });
    connect(edLevel_, QOverload<int>::of(&QSpinBox::valueChanged), this, [applyItemNow](int) { applyItemNow(); });
    connect(edStack_, QOverload<int>::of(&QSpinBox::valueChanged), this, [applyItemNow](int) { applyItemNow(); });
    connect(edXp_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [applyItemNow](double) { applyItemNow(); });
    connect(edRerolls_, QOverload<int>::of(&QSpinBox::valueChanged), this, [applyItemNow](int) { applyItemNow(); });
    connect(edItem_, QOverload<int>::of(&QComboBox::activated), this, [applyItemNow](int) { applyItemNow(); });
    connect(edItem_->lineEdit(), &QLineEdit::editingFinished, this, [applyItemNow]() { applyItemNow(); });
    connect(selAll, &QPushButton::clicked, this, [this]() { edTree_->selectAll(); });
    connect(del, &QPushButton::clicked, this, [this]() {
        if (!ready_()) { emit message("Not ready."); return; }
        std::vector<Mcd2::DeleteTarget> targets;
        QStringList names;
        int skipped = 0;
        for (QTreeWidgetItem* it : edTree_->selectedItems()) {
            const int i = edTree_->indexOfTopLevelItem(it);
            if (i < 0 || i >= (int)edRows_.size()) continue;
            const auto& row = edRows_[i];
            if (!row.equipped.empty() && row.equipped != "None") { ++skipped; continue; }
            targets.push_back({row.addr, row.item});
            if (names.size() < 8) names << pretty(QString::fromStdString(row.item));
        }
        if (targets.empty()) { emit message(skipped ? "Equipped items are skipped. Take them off in the game first." : "Select an item first."); return; }
        QString list = names.join(", ");
        if ((int)targets.size() > names.size()) list += QString(" ...");
        const auto a = QMessageBox::question(QApplication::activeWindow(), "DungeonKit",
            I18n::tr(QString("Delete %1 item(s) from your inventory?").arg(targets.size())) + "\n\n" + list + "\n\n" +
            I18n::tr("This cannot be undone. Use it offline and keep a backup of your save."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (a != QMessageBox::Yes) return;
        std::string err;
        const int n = m_.deleteItems(targets, err);
        if (n > 0) {
            emit message(I18n::tr(QString("Deleted %1 item(s).").arg(n)) + (skipped ? QString(" ") + I18n::tr(QString("%1 equipped item(s) were skipped.").arg(skipped)) : QString()));
            refreshEditor();
        } else {
            emit message(nt(err));
        }
    });
    connect(delCat, &QPushButton::clicked, this, [this]() {
        if (!ready_()) { emit message("Not ready."); return; }
        const std::string cat = edCategoryBox_->currentData().toString().toStdString();
        if (cat.empty()) { emit message("Refresh first, then pick a category."); return; }
        std::vector<Mcd2::DeleteTarget> targets;
        QStringList names;
        int skipped = 0;
        for (const auto& row : edAllRows_) {
            if (row.slot != cat) continue;
            if (!row.equipped.empty() && row.equipped != "None") { ++skipped; continue; }
            targets.push_back({row.addr, row.item});
            if (names.size() < 8) names << pretty(QString::fromStdString(row.item));
        }
        if (targets.empty()) { emit message(skipped ? "Everything in that category is equipped - nothing to delete." : "Nothing found in that category."); return; }
        QString list = names.join(", ");
        if ((int)targets.size() > names.size()) list += QString(" ...");
        const auto a = QMessageBox::question(QApplication::activeWindow(), "DungeonKit",
            I18n::tr(QString("Delete %1 item(s) from %2?").arg(targets.size()).arg(edCategoryBox_->currentText())) + "\n\n" + list + "\n\n" +
            I18n::tr("This cannot be undone. Use it offline and keep a backup of your save."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (a != QMessageBox::Yes) return;
        std::string err;
        const int n = m_.deleteItems(targets, err);
        if (n > 0) {
            emit message(I18n::tr(QString("Deleted %1 item(s).").arg(n)) + (skipped ? QString(" ") + I18n::tr(QString("%1 equipped item(s) were kept.").arg(skipped)) : QString()));
            refreshEditor();
        } else {
            emit message(nt(err));
        }
    });
}

// Rebuilds the Enchants rows under Selected item for whichever item is now picked. `row.fx` already holds every
// effect-array entry on the item (read in Mcd2::inventory, both the item's own group and each level's group) -
// this just keeps the ones whose tag starts "SW.Enchantment." (the same prefix the data catalog uses to mark an
// entry as an enchant rather than a built-in/gilded effect), one editable row per entry found.
void Mcd2Hub::rebuildEnchants(const Mcd2::InvRow& row) {
    edEnchantRows_.clear();
    if (edEnchantLayout_) {
        while (QLayoutItem* it = edEnchantLayout_->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
    }
    if (row.addr == 0) { if (edEnchantNote_) edEnchantNote_->setText("-"); return; }
    std::vector<const Mcd2::EffRow*> found;
    for (const auto& f : row.fx) if (QString::fromStdString(f.tag).startsWith("SW.Enchantment.")) found.push_back(&f);
    if (!edEnchantNote_ || !edEnchantLayout_) return;
    if (found.empty()) { edEnchantNote_->setText(I18n::tr("No enchant on this item.")); return; }
    edEnchantNote_->setText(I18n::tr(found.size() == 1 ? QString("1 enchant found.") : QString("%1 enchants found.").arg(found.size())));

    static const QRegularExpression tierRe("^(.*)\\.(I|II|III|IV|V)$");
    for (const auto* f : found) {
        const QString fullTag = QString::fromStdString(f->tag);
        QString baseTag = fullTag, curTier;
        const auto tm = tierRe.match(fullTag);
        if (tm.hasMatch()) { baseTag = tm.captured(1); curTier = tm.captured(2); }

        auto* rowW = new QWidget;
        rowW->setStyleSheet("background: transparent;");
        auto* h = new QHBoxLayout(rowW);
        h->setContentsMargins(0, 0, 0, 0);
        if (f->group >= 0) h->addWidget(new QLabel(QString("L%1").arg(f->group + 1)));
        auto* tagBox = new QComboBox;
        auto* tierBox = new QComboBox;
        auto* pointsBox = new QSpinBox;
        pointsBox->setRange(0, 99);
        pointsBox->setValue(f->points);
        int curTagIdx = -1;
        for (const auto& e : Mcd2Catalog::effects()) {
            if (e.category != "Enchantment") continue;
            tagBox->addItem(QString::fromStdString(e.name), QString::fromStdString(e.tag));
            if (e.tag == baseTag.toStdString()) curTagIdx = tagBox->count() - 1;
        }
        if (curTagIdx < 0 && tagBox->count() > 0) curTagIdx = 0;    // tag the catalog doesn't know: default to the first rather than leave it blank
        if (curTagIdx >= 0) tagBox->setCurrentIndex(curTagIdx);
        auto fillTiers = [tierBox](const std::string& baseTagStd, const QString& keepTier) {
            tierBox->blockSignals(true);
            tierBox->clear();
            const auto* info = Mcd2Catalog::findEffect(baseTagStd);
            int keepIdx = -1;
            if (info) for (size_t i = 0; i < info->tiers.size(); ++i) {
                tierBox->addItem(QString::fromStdString(info->tiers[i].roman));
                if (QString::fromStdString(info->tiers[i].roman) == keepTier) keepIdx = (int)i;
            }
            tierBox->setCurrentIndex(keepIdx >= 0 ? keepIdx : (tierBox->count() > 0 ? 0 : -1));
            tierBox->blockSignals(false);
        };
        fillTiers(curTagIdx >= 0 ? tagBox->itemData(curTagIdx).toString().toStdString() : baseTag.toStdString(), curTier);

        h->addWidget(new QLabel(I18n::tr("Enchant")));
        h->addWidget(tagBox, 1);
        h->addWidget(new QLabel(I18n::tr("Tier")));
        h->addWidget(tierBox);
        h->addWidget(new QLabel(I18n::tr("Points")));
        h->addWidget(pointsBox);
        edEnchantLayout_->addWidget(rowW);

        EnchantRow er;
        er.row = rowW; er.tag = tagBox; er.tier = tierBox; er.points = pointsBox;
        er.itemAddr = row.addr; er.itemName = row.item; er.effAddr = f->addr; er.group = f->group;
        edEnchantRows_.push_back(er);
        const size_t idx = edEnchantRows_.size() - 1;

        auto apply = [this, idx]() {
            if (edLoading_ || idx >= edEnchantRows_.size()) return;
            auto& r2 = edEnchantRows_[idx];
            const std::string base = r2.tag->currentData().toString().toStdString();
            const QString tier = r2.tier->currentText();
            const std::string fullTagStd = tier.isEmpty() ? base : base + "." + tier.toStdString();
            float intensity = std::numeric_limits<float>::quiet_NaN();
            const auto* info = Mcd2Catalog::findEffect(base);
            if (info) for (const auto& t : info->tiers) if (QString::fromStdString(t.roman) == tier) { intensity = t.strength; break; }
            std::string err;
            if (!m_.editEffect(r2.itemAddr, r2.itemName, r2.effAddr, fullTagStd, intensity, r2.points->value(), err)) { emit message(nt(err)); return; }
            refreshEditor();
        };
        connect(tagBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, idx, fillTiers](int i) {
            if (idx >= edEnchantRows_.size()) return;
            fillTiers(edEnchantRows_[idx].tag->itemData(i).toString().toStdString(), QString());
        });
        connect(tagBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [apply](int) { apply(); });
        connect(tierBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [apply](int) { apply(); });
        connect(pointsBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [apply](int) { apply(); });
    }
}

void Mcd2Hub::refreshEditor() {
    if (!edOn_ || !edOn_->isChecked()) return;
    std::string note;
    std::vector<Mcd2::InvRow> all;
    const bool ok = ready_() && m_.inventory(all, note);
    if (!ok) { edNote_->setText(nt(note.empty() ? std::string("Not ready.") : note)); return; }
    const quint64 keepAddr = (edTree_->currentItem() && edTree_->indexOfTopLevelItem(edTree_->currentItem()) < (int)edRows_.size() &&
                              edTree_->indexOfTopLevelItem(edTree_->currentItem()) >= 0)
                                 ? edRows_[edTree_->indexOfTopLevelItem(edTree_->currentItem())].addr : 0;
    if (edItem_->count() == 0) {
        auto fill = [this](QComboBox* box, const std::vector<std::string>& names) {
            QSet<QString> seen;
            for (const auto& n : names) {
                const QString full = QString::fromStdString(n);
                QString shown = pretty(full);
                if (seen.contains(shown)) shown = full;                  // two tags with the same short name: keep the long one
                seen.insert(shown);
                box->addItem(shown, full);
            }
        };
        std::vector<std::string> items;
        for (const auto& n : m_.namesWithPrefix("SW.Item."))
            if (isGear(QString::fromStdString(n))) items.push_back(n);
        fill(edItem_, items);
    }
    edAllRows_ = all;
    {
        const QString keepCat = edCategoryBox_->currentData().toString();
        edCategoryBox_->blockSignals(true);
        edCategoryBox_->clear();
        QSet<QString> seenCats;
        for (const auto& r : all) {
            if (!isGear(QString::fromStdString(r.item))) continue;
            const QString full = QString::fromStdString(r.slot);
            if (full.isEmpty() || seenCats.contains(full)) continue;
            seenCats.insert(full);
            edCategoryBox_->addItem(pretty(full), full);
        }
        const int ci = keepCat.isEmpty() ? -1 : edCategoryBox_->findData(keepCat);
        edCategoryBox_->setCurrentIndex(ci >= 0 ? ci : (edCategoryBox_->count() > 0 ? 0 : -1));
        edCategoryBox_->blockSignals(false);
    }
    edRows_.clear();
    const bool showAll = edAll_->isChecked(), onlyEq = edEquipped_->isChecked();
    const std::string rarityFilter = edRarityFilter_->currentData().toString().toStdString();
    int hidden = 0, hiddenByRarity = 0;
    for (auto& r : all) {
        if (!showAll && !isGear(QString::fromStdString(r.item))) { ++hidden; continue; }
        if (onlyEq && r.equipped.empty()) continue;
        if (!rarityFilter.empty() && r.rarity != rarityFilter) { ++hiddenByRarity; continue; }
        edRows_.push_back(r);
    }
    edTree_->blockSignals(true);
    edTree_->clear();
    edJump_->blockSignals(true);
    edJump_->clear();
    int keepIdx = -1;
    for (size_t k = 0; k < edRows_.size(); ++k) {
        const auto& r = edRows_[k];
        auto* it = new QTreeWidgetItem(edTree_);
        QStringList fx;
        for (const auto& f : r.fx) fx << QString("%1%2 x%3 (%4)").arg(f.group >= 0 ? QString("L%1: ").arg(f.group + 1) : QString()).arg(pretty(QString::fromStdString(f.tag))).arg(f.intensity, 0, 'g', 4).arg(f.points);
        const QString eq = QString::fromStdString(r.equipped);
        it->setText(0, pretty(QString::fromStdString(r.slot)));
        it->setText(1, pretty(QString::fromStdString(r.item)));
        it->setText(2, pretty(QString::fromStdString(r.rarity)));
        it->setText(3, QString::number((double)r.power, 'f', 0));
        it->setText(4, QString::number(r.level));
        it->setText(5, QString::number(r.stack));
        it->setText(6, eq.isEmpty() || eq == "None" ? QString() : pretty(eq));
        it->setText(7, fx.join("; "));
        if (!it->text(6).isEmpty()) edJump_->addItem(it->text(6) + ":  " + it->text(1), (qulonglong)r.addr);
        if (keepAddr && r.addr == keepAddr) keepIdx = (int)k;
    }
    edJump_->setCurrentIndex(-1);
    edJump_->blockSignals(false);
    edTree_->blockSignals(false);
    edNote_->setText(nt(note) + QString("   ") + I18n::tr(QString("Showing %1 of %2 entries.").arg(edRows_.size()).arg(all.size()))
                         + (hidden ? QString(" ") + I18n::tr(QString("(%1 non-gear hidden)").arg(hidden)) : QString())
                         + (hiddenByRarity ? QString(" ") + I18n::tr(QString("(%1 hidden by the rarity filter)").arg(hiddenByRarity)) : QString()));
    if (keepIdx >= 0) edTree_->setCurrentItem(edTree_->topLevelItem(keepIdx));
}

} // namespace dk
