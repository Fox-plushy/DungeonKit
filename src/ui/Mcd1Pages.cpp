#include "Mcd1Pages.h"
#include "I18n.h"
#include "LevelNames.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QApplication>
#include <QGuiApplication>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>

namespace dk {

namespace {

QVBoxLayout* makePage(QWidget* host, const QString& title, const QString& notice, QVBoxLayout*& content) {
    auto* outer = new QVBoxLayout(host);
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
    return outer;
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

bool readI32(UE4& ue, uint64_t obj, const char* name, int32_t& v) {
    UE4::Prop pr;
    return ue.find(obj, name, pr) && ue.rd(obj + pr.offset, v);
}
bool readF32(UE4& ue, uint64_t obj, const char* name, float& v) {
    UE4::Prop pr;
    return ue.find(obj, name, pr) && ue.rd(obj + pr.offset, v);
}
bool readBool(UE4& ue, uint64_t obj, const char* name, bool& v) {
    UE4::Prop pr;
    uint8_t b = 0;
    if (!ue.find(obj, name, pr) || !pr.isBool || !ue.rd(obj + pr.offset + pr.byteOffset, b)) return false;
    v = (b & pr.mask) != 0;
    return true;
}

void addConnectDisable(QWidget* page, QObject* receiver, const std::function<void()>& fn) {
    for (QPushButton* b : page->findChildren<QPushButton*>("DisableAll"))
        QObject::connect(b, &QPushButton::clicked, receiver, [fn]() { fn(); });
}

} // namespace

// ====================================================================== Camera
Mcd1CameraPage::Mcd1CameraPage(std::shared_ptr<UE4> ue, std::function<Process*()> proc, QWidget* parent)
    : QWidget(parent), ue_(std::move(ue)), proc_(std::move(proc)), time_(ue_) {
    QVBoxLayout* v = nullptr;
    makePage(this, "Camera Editor",
             "Camera tweaks that only change what you see on your own screen. Load into a level first. The game may reset a value when the camera changes "
             "(entering a building, a cutscene), so use Freeze to keep it.", v);
    addConnectDisable(this, this, [this]() { emit disableAllRequested(); });

    auto* bar = new QHBoxLayout;
    auto* reset = new QPushButton("Reset to original");
    reset->setCursor(Qt::PointingHandCursor);
    bar->addWidget(reset);
    bar->addStretch(1);
    v->addLayout(bar);
    note_ = hint("Waiting for the game...");
    v->addWidget(note_);

    {
        auto* pc = makeCard(v, "Camera presets");
        pc->addWidget(hint("One click sets a camera look. Presets scale your normal camera field of view, so they work whatever the game's own values are. "
                           "They do not change the camera distance. Use Reset to original to go back."));
        keep_ = new QCheckBox("Keep the preset applied (freeze)");
        keep_->setChecked(true);
        pc->addWidget(keep_);
        struct Preset { const char* label; float fov, arm; };
        static const Preset presets[] = {
            {"FOV Far", 1.25f, 1.4f},
            {"FOV Super", 1.6f, 1.0f},
            {"Quake Pro", 1.35f, 1.0f},
            {"Tactical", 0.9f, 1.8f},
        };
        auto* grid = new QGridLayout;
        int n = 0;
        for (const Preset& pr : presets) {
            auto* b = new QPushButton(pr.label);
            b->setCursor(Qt::PointingHandCursor);
            grid->addWidget(b, n / 4, n % 4);
            ++n;
            const float fs = pr.fov, as = pr.arm;
            const QString name = pr.label;
            connect(b, &QPushButton::clicked, this, [this, name, fs, as]() { applyPreset(name, fs, as); });
        }
        pc->addLayout(grid);
    }
    {
        auto* pm = makeCard(v, "Photo mode");
        pm->addWidget(hint("Slow the world down or freeze it to line up a shot, then use the presets or the camera values below for the framing. "
                           "DungeonKit cannot hide the game's on-screen interface. Press Normal speed when you are done; a frozen world stays frozen. "
                           "If Timer or Pause Game is also switched on in Tools, they fight over the same value."));
        auto* w = new QWidget;
        w->setStyleSheet("background: transparent;");
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        auto* l = new QLabel("World speed");
        l->setMinimumWidth(190);
        speedSpin_ = new QDoubleSpinBox;
        speedSpin_->setRange(0.0001, 20.0);
        speedSpin_->setDecimals(4);
        speedSpin_->setSingleStep(0.1);
        speedSpin_->setValue(1.0);
        speedSpin_->setMinimumWidth(130);
        auto* apply = new QPushButton("Set");
        speedFreeze_ = new QCheckBox("Freeze");
        h->addWidget(l);
        h->addWidget(speedSpin_, 1);
        h->addWidget(apply);
        h->addWidget(speedFreeze_);
        pm->addWidget(w);
        auto* row = new QHBoxLayout;
        auto* freezeBtn = new QPushButton("Freeze time");
        auto* slowBtn = new QPushButton("Slow motion");
        auto* normalBtn = new QPushButton("Normal speed");
        for (auto* x : {freezeBtn, slowBtn, normalBtn}) { x->setCursor(Qt::PointingHandCursor); row->addWidget(x); }
        row->addStretch(1);
        pm->addLayout(row);
        connect(apply, &QPushButton::clicked, this, [this]() { setWorldSpeed(speedSpin_->value(), speedFreeze_->isChecked()); speedSpin_->clearFocus(); });
        connect(freezeBtn, &QPushButton::clicked, this, [this]() { setWorldSpeed(0.0001, true); });
        connect(slowBtn, &QPushButton::clicked, this, [this]() { setWorldSpeed(0.2, true); });
        connect(normalBtn, &QPushButton::clicked, this, [this]() { setWorldSpeed(1.0, false); });
    }
    {
        auto* c = makeCard(v, "Camera");
        struct Def { const char* label; double lo, hi, step; };
        static const Def defs[KindCount] = {
            {"Field of view", 5, 170, 1},
            {"Default field of view", 5, 170, 1},
            {"Camera distance (arm length)", 0, 30000, 50},
        };
        for (int k = 0; k < KindCount; ++k) {
            auto* w = new QWidget;
            w->setStyleSheet("background: transparent;");
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(0, 0, 0, 0);
            auto* l = new QLabel(defs[k].label);
            l->setMinimumWidth(190);
            auto* spin = new QDoubleSpinBox;
            spin->setRange(defs[k].lo, defs[k].hi);
            spin->setDecimals(1);
            spin->setSingleStep(defs[k].step);
            spin->setMinimumWidth(130);
            auto* apply = new QPushButton("Set");
            auto* freeze = new QCheckBox("Freeze");
            h->addWidget(l);
            h->addWidget(spin, 1);
            h->addWidget(apply);
            h->addWidget(freeze);
            c->addWidget(w);
            rows_[k] = {spin, freeze};
            connect(apply, &QPushButton::clicked, this, [this, k]() {
                Process* p = proc_();
                std::string err;
                if (!p || !ue_->init(*p, err)) { emit message("Minecraft Dungeons is not connected."); return; }
                captureOriginals(p);
                if (!writeVal(p, k, (float)rows_[k].spin->value())) emit message("Could not write that value yet - load into a level and try again.");
                rows_[k].spin->clearFocus();
            });
        }
    }
    v->addStretch(1);

    connect(reset, &QPushButton::clicked, this, [this]() { restore(); emit message("Camera values restored."); });
    connect(&timer_, &QTimer::timeout, this, [this]() { tick(); });
    timer_.start(150);
}

bool Mcd1CameraPage::controller(uint64_t& pc) {
    uint64_t eng = 0, gi = 0, players = 0, lp = 0;
    int32_t num = 0;
    if (!ue_->engine(eng)) return false;
    if (!ue_->readPtr(eng, "GameInstance", gi)) return false;
    if (!ue_->readArray(gi, "LocalPlayers", players, num) || num <= 0) return false;
    lp = ue_->rd64(players);
    if (!UE4::plausible(lp)) return false;
    return ue_->readPtr(lp, "PlayerController", pc);
}

bool Mcd1CameraPage::readVal(Process*, int kind, float& out) {
    uint64_t pc = 0;
    if (kind == Arm) {
        uint64_t ch = 0, arm = 0;
        if (!ue_->character(ch) || !ue_->readPtr(ch, "CameraSpringArm", arm)) return false;
        return readF32(*ue_, arm, "TargetArmLength", out) && out > 0.0f;
    }
    if (!controller(pc)) return false;
    if (kind == Fov) return (readF32(*ue_, pc, "mCurrentCameraFOV", out) || readF32(*ue_, pc, "mTargetCameraFOV", out)) && out > 1.0f && out < 179.0f;
    uint64_t cm = 0;
    return ue_->readPtr(pc, "PlayerCameraManager", cm) && readF32(*ue_, cm, "DefaultFOV", out) && out > 1.0f && out < 179.0f;
}

bool Mcd1CameraPage::writeVal(Process* p, int kind, float v) {
    auto put = [&](uint64_t obj, const char* name) {
        UE4::Prop pr;
        return ue_->find(obj, name, pr) && p->write(obj + pr.offset, &v, sizeof v);
    };
    uint64_t pc = 0;
    if (kind == Arm) {
        uint64_t ch = 0, arm = 0;
        return ue_->character(ch) && ue_->readPtr(ch, "CameraSpringArm", arm) && put(arm, "TargetArmLength");
    }
    if (!controller(pc)) return false;
    if (kind == Fov) {
        const bool a = put(pc, "mTargetCameraFOV");
        const bool b = put(pc, "mCurrentCameraFOV");
        return a || b;
    }
    uint64_t cm = 0;
    return ue_->readPtr(pc, "PlayerCameraManager", cm) && put(cm, "DefaultFOV");
}

void Mcd1CameraPage::captureOriginals(Process* p) {
    for (int k = 0; k < KindCount; ++k) {
        if (have_[k] || rows_[k].freeze->isChecked()) continue;
        float f;
        if (readVal(p, k, f)) { have_[k] = true; orig_[k] = f; }
    }
}

void Mcd1CameraPage::tick() {
    Process* p = proc_();
    std::string err;
    if (!p || !ue_->init(*p, err)) {
        if (pid_) { pid_ = 0; for (bool& h : have_) h = false; for (auto& r : rows_) r.freeze->setChecked(false); speedFreeze_->setChecked(false); }
        note_->setText(I18n::tr("Waiting for the game..."));
        return;
    }
    if (p->pid() != pid_) { pid_ = p->pid(); for (bool& h : have_) h = false; for (auto& r : rows_) r.freeze->setChecked(false); speedFreeze_->setChecked(false); }
    int found = 0;
    for (int k = 0; k < KindCount; ++k) {
        float f = 0;
        const bool ok = readVal(p, k, f);
        if (ok) ++found;
        if (rows_[k].freeze->isChecked()) {
            writeVal(p, k, (float)rows_[k].spin->value());
        } else if (ok) {
            if (!have_[k]) { have_[k] = true; orig_[k] = f; }
            if (isVisible() && !rows_[k].spin->hasFocus()) {
                rows_[k].spin->blockSignals(true);
                rows_[k].spin->setValue(f);
                rows_[k].spin->blockSignals(false);
            }
        }
    }
    if (speedFreeze_->isChecked()) {
        float cur = 0;
        const float want = (float)speedSpin_->value();
        if (!time_.readRaw(cur) || std::fabs(cur - want) > 1e-6f) time_.writeRaw(want);
    } else {
        float cur = 0;
        if (isVisible() && !speedSpin_->hasFocus() && time_.readRaw(cur)) {
            speedSpin_->blockSignals(true);
            speedSpin_->setValue(cur);
            speedSpin_->blockSignals(false);
        }
    }
    note_->setText(found == KindCount ? I18n::tr("Camera found.") : found ? I18n::tr("Camera partly found - load into a level.") : I18n::tr("Camera not found - load into a level."));
}

void Mcd1CameraPage::applyPreset(const QString& name, float fovScale, float armScale) {
    Process* p = proc_();
    std::string err;
    if (!p || !ue_->init(*p, err)) { emit message("Minecraft Dungeons is not connected."); return; }
    captureOriginals(p);
    if (!have_[Fov]) { emit message("Camera not found - load into a level first."); return; }
    const float scales[KindCount] = {fovScale, fovScale, armScale};
    int ok = 0;
    for (int k = 0; k < KindCount; ++k) {
        if (k == Arm || !have_[k]) continue;                    // presets never touch the camera distance
        float val = orig_[k] * scales[k];
        if (k != Arm) val = std::min(150.0f, std::max(10.0f, val));
        const bool untouched = std::fabs(scales[k] - 1.0f) < 1e-4f;
        if (untouched) val = orig_[k];
        rows_[k].freeze->setChecked(false);
        if (writeVal(p, k, val)) ++ok;
        rows_[k].spin->blockSignals(true);
        rows_[k].spin->setValue(val);
        rows_[k].spin->blockSignals(false);
        if (!untouched && keep_->isChecked()) rows_[k].freeze->setChecked(true);
    }
    emit message(ok ? QString("%1 applied.").arg(name) : QString("Camera not found - load into a level first."));
}

void Mcd1CameraPage::setWorldSpeed(double value, bool hold) {
    Process* p = proc_();
    std::string err;
    if (!p || !ue_->init(*p, err)) { emit message("Minecraft Dungeons is not connected."); return; }
    speedSpin_->blockSignals(true);
    speedSpin_->setValue(value);
    speedSpin_->blockSignals(false);
    speedFreeze_->setChecked(hold);
    if (!time_.writeRaw((float)value)) emit message("Could not reach the world's time settings - load into a level.");
}

void Mcd1CameraPage::restore() {
    for (auto& r : rows_) r.freeze->setChecked(false);
    speedFreeze_->setChecked(false);
    Process* p = proc_();
    std::string err;
    if (!p || !ue_->init(*p, err)) return;
    for (int k = 0; k < KindCount; ++k) if (have_[k]) writeVal(p, k, orig_[k]);
}

// ====================================================================== World
Mcd1WorldPage::~Mcd1WorldPage() {
    if (scanThread_) { scanThread_->wait(); delete scanThread_; scanThread_ = nullptr; }
}

Mcd1WorldPage::Mcd1WorldPage(std::shared_ptr<UE4> ue, std::function<Process*()> proc, QWidget* parent)
    : QWidget(parent), ue_(std::move(ue)), proc_(std::move(proc)) {
    QVBoxLayout* v = nullptr;
    makePage(this, "World",
             "", v);
    addConnectDisable(this, this, [this]() { emit disableAllRequested(); });
    state_ = hint("Waiting for the game...");
    v->addWidget(state_);
    teleportSlot_ = new QVBoxLayout;
    v->addLayout(teleportSlot_);
    {
        auto* c = makeCard(v, "Live mission");
        lives_ = addLine(c, "Lives left");
        livesLost_ = addLine(c, "Lives lost this session");
        revive_ = addLine(c, "Revive enabled");
        spawned_ = addLine(c, "Enemies spawned");
        killed_ = addLine(c, "Enemies defeated");
        bosses_ = addLine(c, "Bosses defeated");
        parts_ = addLine(c, "Parts discovered");
        power_ = addLine(c, "Average team power");
    }
    {
        auto* c = makeCard(v, "Current mission");
        mLevel_ = addLine(c, "Level");
        mMap_ = addLine(c, "Map name");
        mDiff_ = addLine(c, "Difficulty");
        mThreat_ = addLine(c, "Threat level");
        mApoc_ = addLine(c, "Apocalypse+ level");
        mSeed_ = addLine(c, "Level seed");
        mLost_ = addLine(c, "Lives lost");
        mParts_ = addLine(c, "Parts discovered");
        mRaid_ = addLine(c, "Raid difficulty");
        mAffectors_ = addLine(c, "Mid-game affectors");
        mFlags_ = addLine(c, "Flags");
        rulesNote_ = hint("");
        c->addWidget(rulesNote_);
        rulesText_ = new QLabel("-");
        rulesText_->setWordWrap(true);
        rulesText_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        c->addWidget(rulesText_);
    }
    {
        auto* c = makeCard(v, "Tower");
        tState_ = addLine(c, "Status");
        tId_ = addLine(c, "Tower id");
        tFloors_ = addLine(c, "Floors");
        tSeed_ = addLine(c, "Tower seed");
        tPlayers_ = addLine(c, "Players");
        tRewards_ = addLine(c, "Final rewards");
        tManager_ = addLine(c, "Current rewards (tower manager)");
        floors_ = new QTreeWidget;
        floors_->setHeaderLabels({"Floor", "Tile", "Type"});
        floors_->setRootIsDecorated(false);
        floors_->setAlternatingRowColors(true);
        floors_->setMinimumHeight(160);
        floors_->setColumnWidth(0, 70);
        floors_->setColumnWidth(1, 300);
        c->addWidget(floors_);
    }
    {
        auto* c = makeCard(v, "Merchants and save states");
        c->addWidget(hint("Read from the game's own tables, best effort. The list shows the merchant types the game has unlocked."));
        merchCount_ = addLine(c, "Unlocked merchant types");
        saveCount_ = addLine(c, "Save states");
        merchants_ = new QTreeWidget;
        merchants_->setHeaderLabels({"Merchant type"});
        merchants_->setRootIsDecorated(false);
        merchants_->setAlternatingRowColors(true);
        merchants_->setMinimumHeight(160);
        c->addWidget(merchants_);
    }
    v->addStretch(1);
    connect(&timer_, &QTimer::timeout, this, [this]() { if (isVisible()) tick(); });
    timer_.start(700);
}

QLabel* Mcd1WorldPage::addLine(QVBoxLayout* into, const QString& label) {
    auto* w = new QWidget;
    w->setStyleSheet("background: transparent;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* l = new QLabel(label);
    l->setMinimumWidth(230);
    auto* val = new QLabel("-");
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);
    h->addWidget(l);
    h->addWidget(val, 1);
    into->addWidget(w);
    return val;
}

std::string Mcd1WorldPage::levelName(unsigned v) const {
    if (v == 0 || v >= (unsigned)kLevelNameCount) return v ? "Unknown (" + std::to_string(v) + ")" : "-";
    std::string s = kLevelNames[v];
    if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    return s;
}

bool Mcd1WorldPage::prop(uint64_t obj, const char* name, uint32_t& off) {
    UE4::Prop pr;
    if (!ue_->find(obj, name, pr)) return false;
    off = pr.offset;
    return true;
}

bool Mcd1WorldPage::readFString(uint64_t addr, std::string& out) {
    out.clear();
    Process* p = proc_();
    if (!p) return false;
    uint64_t data = 0;
    int32_t num = 0;
    if (!p->read(addr, &data, 8) || !p->read(addr + 8, &num, 4)) return false;
    if (num <= 1 || num > 300 || !UE4::plausible(data)) return num == 0;
    std::vector<uint16_t> w((size_t)num);
    if (!p->read(data, w.data(), (size_t)num * 2)) return false;
    for (int i = 0; i < num && w[(size_t)i]; ++i) out += w[(size_t)i] < 128 ? (char)w[(size_t)i] : '?';
    return true;
}

// TMap / TSet header (TSparseArray): data pointer, element count, allocation bit array, free-list counters.
bool Mcd1WorldPage::readMapList(uint64_t m, std::vector<std::string>& names, int& count, bool wantNames) {
    Process* p = proc_();
    names.clear();
    count = 0;
    if (!p) return false;
    uint64_t data = 0, secondary = 0;
    int32_t num = 0, numBits = 0, maxBits = 0, numFree = 0;
    if (!p->read(m, &data, 8) || !p->read(m + 8, &num, 4) || num < 0 || num > 100000) return false;
    p->read(m + 0x20, &secondary, 8);
    p->read(m + 0x28, &numBits, 4);
    p->read(m + 0x2C, &maxBits, 4);
    p->read(m + 0x34, &numFree, 4);
    if (num == 0) return true;
    if (!UE4::plausible(data)) return false;
    count = (numFree >= 0 && numFree <= num) ? num - numFree : num;
    if (!wantNames || num > 400) return true;
    const uint64_t flags = (maxBits > 128 && UE4::plausible(secondary)) ? secondary : m + 0x10;
    std::vector<int> used;
    for (int i = 0; i < num; ++i) {
        uint32_t word = 0;
        if (!p->read(flags + (uint64_t)(i / 32) * 4, &word, 4)) break;
        if (word & (1u << (i % 32))) used.push_back(i);
    }
    static const int strides[] = {16, 24, 32, 40, 48, 56, 64};
    for (int stride : strides) {
        bool good = !used.empty();
        std::vector<std::string> tmp;
        for (size_t u = 0; u < used.size() && good; ++u) {
            uint64_t o = 0;
            if (!p->read(data + (uint64_t)used[u] * (uint64_t)stride, &o, 8) || !UE4::plausible(o)) { good = false; break; }
            const std::string n = ue_->objectName(o);
            if (n.empty() || n.size() > 120) { good = false; break; }
            tmp.push_back(n);
        }
        if (good) { names = tmp; return true; }
    }
    return true;
}

void Mcd1WorldPage::addTeleports(QWidget* w) {
    if (teleportSlot_ && w) teleportSlot_->addWidget(w);
}

// ---- mission screen rules list (the "Recommended power / mob damage / mob health ..." bullets)
// The widget UMG_ThreatLevelRules_C keeps the lines in "rulesTexts" (an array of FText) and the line widgets in "Rows".
// The game has several copies of this widget (templates inside the asset packages); only the live one, which sits under
// /Engine/Transient, is filled in. Finding it means walking the whole object table, so that happens on a background thread.
static bool underTransient(UE4& ue, uint64_t o) {
    for (int depth = 0; depth < 24 && UE4::plausible(o); ++depth) {
        if (ue.objectName(o).find("Transient") != std::string::npos) return true;
        o = ue.rd64(o + 0x20);                                  // UObject::Outer
    }
    return false;
}

void Mcd1WorldPage::scanRules(Process* p, std::vector<uint64_t>& found) {
    std::string err;
    UE4::ObjArray oa;
    if (!p || !ue_->objectArray(oa, err)) return;
    std::map<uint64_t, bool> isTarget;
    std::vector<uint8_t> items((size_t)oa.perChunk * 0x18);
    std::vector<uint64_t> live, any;
    int32_t index = 0;
    for (int32_t ch = 0; ch < oa.numChunks && index < oa.num; ++ch) {
        const uint64_t chunk = ue_->rd64(oa.chunks + (uint64_t)ch * 8);
        if (!UE4::plausible(chunk)) { index += oa.perChunk; continue; }
        const int32_t inChunk = std::min(oa.perChunk, oa.num - index);
        if (!p->read(chunk, items.data(), (size_t)inChunk * 0x18)) { index += inChunk; continue; }
        for (int32_t i = 0; i < inChunk; ++i, ++index) {
            uint64_t o;
            std::memcpy(&o, &items[(size_t)i * 0x18], 8);
            if (!UE4::plausible(o)) continue;
            uint64_t cls = 0;
            if (!p->read(o + 0x10, &cls, 8) || !UE4::plausible(cls)) continue;
            auto it = isTarget.find(cls);
            if (it == isTarget.end()) it = isTarget.emplace(cls, ue_->objectName(cls) == "UMG_ThreatLevelRules_C").first;
            if (!it->second) continue;
            const std::string nm = ue_->objectName(o);
            if (nm.compare(0, 9, "Default__") == 0 || nm.compare(0, 5, "SKEL_") == 0 || nm.compare(0, 6, "REINST") == 0) continue;
            any.push_back(o);
            if (underTransient(*ue_, o)) live.push_back(o);
        }
    }
    found = live.empty() ? any : live;
}

void Mcd1WorldPage::startRulesScan() {
    if (scanBusy_.exchange(true)) return;
    if (scanThread_) { scanThread_->wait(); delete scanThread_; scanThread_ = nullptr; }
    Process* p = proc_();
    if (!p) { scanBusy_ = false; return; }
    scanThread_ = QThread::create([this, p]() {
        std::vector<uint64_t> found;
        scanRules(p, found);
        { CsGuard g(scanMu_); scanFound_ = found; }
        scanDone_ = true;
        scanBusy_ = false;
    });
    scanThread_->start();
}

// FText = { pointer to the text data, shared-reference counter, flags }. The text data holds the displayed FString
// somewhere in its first bytes (as a pointer to it, or inline); try each slot and keep the first that decodes to readable text.
bool Mcd1WorldPage::readFText(uint64_t addr, std::string& out) {
    out.clear();
    Process* p = proc_();
    if (!p) return false;
    uint64_t td = 0;
    if (!p->read(addr, &td, 8) || !UE4::plausible(td)) return false;
    auto good = [](const std::string& s) {
        if (s.size() < 2) return false;
        for (unsigned char c : s) if (c < 32 && c != '\n') return false;
        return true;
    };
    for (uint32_t off = 0x08; off <= 0x48; off += 8) {
        uint64_t sp = 0;
        std::string s;
        if (p->read(td + off, &sp, 8) && UE4::plausible(sp) && readFString(sp, s) && good(s)) { out = s; return true; }
        s.clear();
        if (readFString(td + off, s) && good(s)) { out = s; return true; }
    }
    return false;
}

void Mcd1WorldPage::collectRules(uint64_t obj, QStringList& lines, int& total, int& unread, int& rows) {
    lines.clear();
    total = unread = rows = 0;
    Process* p = proc_();
    if (!p) return;
    uint64_t data = 0;
    int32_t num = 0;
    if (ue_->readArray(obj, "rulesTexts", data, num) && num > 0 && num <= 64) {
        total = num;
        for (int i = 0; i < num; ++i) {
            std::string t;
            if (readFText(data + (uint64_t)i * 0x18, t)) lines << QString::fromStdString(t);
            else ++unread;
        }
    }
    if (!lines.isEmpty()) return;
    // second source: the line widgets themselves, each has a "Rule" text
    uint64_t rd = 0;
    int32_t rn = 0;
    if (!ue_->readArray(obj, "Rows", rd, rn) || rn <= 0 || rn > 64) return;
    rows = rn;
    total = std::max(total, (int)rn);
    unread = 0;
    for (int i = 0; i < rn; ++i) {
        const uint64_t row = ue_->rd64(rd + (uint64_t)i * 8);
        UE4::Prop rp;
        std::string t;
        if (UE4::plausible(row) && ue_->find(row, "Rule", rp) && readFText(row + rp.offset, t)) lines << QString::fromStdString(t);
        else ++unread;
    }
}

void Mcd1WorldPage::updateRules() {
    if (scanDone_.exchange(false)) {
        CsGuard g(scanMu_);
        rulesCands_ = scanFound_;
    }
    QStringList best;
    int bestTotal = -1, bestUnread = 0, bestRows = 0;
    uint64_t bestObj = 0;
    std::vector<uint64_t> alive;
    for (uint64_t o : rulesCands_) {
        if (!UE4::plausible(o) || ue_->objectName(ue_->rd64(o + 0x10)) != "UMG_ThreatLevelRules_C") continue;   // widget was destroyed
        alive.push_back(o);
        QStringList l;
        int total, unread, rows;
        collectRules(o, l, total, unread, rows);
        if (l.size() > best.size() || (best.isEmpty() && total > bestTotal)) { best = l; bestTotal = total; bestUnread = unread; bestRows = rows; bestObj = o; }
    }
    rulesCands_ = alive;
    if (alive.empty()) {
        rulesText_->setText("-");
        rulesNote_->setText(I18n::tr("Shown automatically when you open a mission on the map."));
        if (isVisible() && !scanBusy_ && (!rulesClock_.isValid() || rulesClock_.elapsed() > 4000)) { rulesClock_.restart(); startRulesScan(); }
        return;
    }
    UE4::Prop gp;
    int32_t gear = 0;
    if (bestObj && ue_->find(bestObj, "DisplayGearPower", gp) && ue_->rd(bestObj + gp.offset, gear) && gear > 0 && gear < 1000000)
        best.prepend(I18n::tr("Gear and artifact power: ") + QString::number(gear));
    const int readable = best.size() - (gear > 0 && gear < 1000000 ? 1 : 0);
    if (bestTotal <= 0) rulesNote_->setText(I18n::tr("The list is empty. Pick a mission on the map first."));
    else if (readable <= 0) rulesNote_->setText(I18n::tr("Found the mission list but could not read its text.") +
                                                QString(" (%1 / %2 / %3)").arg(bestTotal).arg(bestRows).arg((int)alive.size()));
    else if (bestUnread > 0) rulesNote_->setText(I18n::tr("Some lines could not be read."));
    else rulesNote_->setText(QString());
    rulesText_->setText(best.isEmpty() ? QString("-") : best.join("\n"));
}

// The mission, difficulty and tower data live on the "GameBP" actor (an Info actor, not the game mode itself).
// Find it through the player character (BP_PlayerCharacter has a GameBP pointer), otherwise by scanning the level's actors.
uint64_t Mcd1WorldPage::findGameBP(uint64_t world) {
    auto isGb = [this](uint64_t o) {
        if (!UE4::plausible(o)) return false;
        const std::string c = ue_->className(o);
        return c.find("GameBP") != std::string::npos || c.find("LobbyBP") != std::string::npos;
    };
    if (gbWorld_ == world && gameBP_ && isGb(gameBP_)) return gameBP_;
    gbWorld_ = world;
    gameBP_ = 0;
    uint64_t ch = 0, o = 0;
    if (ue_->character(ch) && ue_->readPtr(ch, "GameBP", o) && isGb(o)) return gameBP_ = o;
    uint64_t lvl = 0, data = 0;
    int32_t num = 0;
    if (ue_->readPtr(world, "PersistentLevel", lvl) && ue_->readArray(lvl, "Actors", data, num) && num > 0 && num < 200000) {
        for (int32_t i = 0; i < num; ++i) {
            const uint64_t a = ue_->rd64(data + (uint64_t)i * 8);
            if (isGb(a)) return gameBP_ = a;
        }
    }
    return 0;
}

void Mcd1WorldPage::tick() {
    Process* p = proc_();
    std::string err;
    auto dash = [this]() {
        for (QLabel* l : {lives_, livesLost_, revive_, spawned_, killed_, bosses_, parts_, power_, mLevel_, mMap_, mDiff_, mThreat_, mApoc_, mSeed_, mLost_,
                          mParts_, mRaid_, mAffectors_, mFlags_, tState_, tId_, tFloors_, tSeed_, tPlayers_, tRewards_, tManager_, merchCount_, saveCount_})
            l->setText("-");
        floors_->clear();
        merchants_->clear();
    };
    if (!p || !ue_->init(*p, err)) { state_->setText(I18n::tr("Waiting for the game...")); if (lastKey_) { lastKey_ = 0; gameBP_ = gbWorld_ = 0; dash(); } return; }
    lastKey_ = p->pid();
    updateRules();                                    // mission screen multipliers, shown automatically while that screen exists

    uint64_t eng = 0, gi = 0, vp = 0, world = 0, gs = 0;
    if (!ue_->engine(eng) || !ue_->readPtr(eng, "GameInstance", gi)) { state_->setText(I18n::tr("Game objects not readable yet.")); dash(); return; }
    ue_->readPtr(eng, "GameViewport", vp);
    if (!vp || !ue_->readPtr(vp, "World", world)) { state_->setText(I18n::tr("Load into a level first (not a menu).")); dash(); return; }
    ue_->readPtr(world, "GameState", gs);
    const uint64_t gm = findGameBP(world);            // (named gm below: it holds the mission data)
    state_->setText(gm ? I18n::tr("Level found.") : I18n::tr("Level found, but the mission data is not available yet. Move around in the game and wait a moment."));

    auto num = [](int64_t v) { return QString::number(v); };
    // ---- live mission (game state)
    {
        int32_t i;
        float f;
        bool b;
        lives_->setText(gs && readI32(*ue_, gs, "Lives", i) ? num(i) : "-");
        livesLost_->setText(gs && readI32(*ue_, gs, "LivesLostThisSession", i) ? num(i) : "-");
        revive_->setText(gs && readBool(*ue_, gs, "ReviveEnabled", b) ? (b ? I18n::tr("Yes") : I18n::tr("No")) : "-");
        spawned_->setText(gs && readI32(*ue_, gs, "HostileMobsSpawned", i) ? num(i) : "-");
        killed_->setText(gs && readI32(*ue_, gs, "HostileMobsKilled", i) ? num(i) : "-");
        bosses_->setText(gs && readI32(*ue_, gs, "HostileBossMobsKilled", i) ? num(i) : "-");
        parts_->setText(gs && readI32(*ue_, gs, "partsDiscovered", i) ? num(i) : "-");
        power_->setText(gs && readF32(*ue_, gs, "AverageTeamPower", f) ? QString::number(f, 'f', 1) : "-");
    }
    // ---- current mission (level settings held by the game mode)
    uint32_t off = 0;
    if (gm && prop(gm, "mReplicatedLevelSettings_ONLY_FOR_INITIAL_REPLICATION", off)) {
        const uint64_t ls = gm + off;
        uint8_t mission = 0, diff = 0, threat = 0, f1 = 0, f2 = 0, f3 = 0;
        int32_t struggle = 0, seed = 0, lost = 0, parts = 0;
        ue_->rd(ls, mission); ue_->rd(ls + 1, diff); ue_->rd(ls + 2, threat); ue_->rd(ls + 4, struggle);
        ue_->rd(ls + 8, seed); ue_->rd(ls + 0x158, lost); ue_->rd(ls + 0x15C, parts);
        ue_->rd(ls + 0x170, f1); ue_->rd(ls + 0x171, f2); ue_->rd(ls + 0x172, f3);
        static const char* const diffNames[] = {"-", "Default", "Adventure", "Apocalypse"};
        mLevel_->setText(QString::fromStdString(levelName(mission)));
        std::string map;
        mMap_->setText(readFString(ls + 0x1A0, map) && !map.empty() ? QString::fromStdString(map) : "-");
        mDiff_->setText(diff < 4 ? I18n::tr(diffNames[diff]) : num(diff));
        mThreat_->setText(num(threat));
        mApoc_->setText(num(struggle));
        mSeed_->setText(num(seed));
        mLost_->setText(num(lost));
        mParts_->setText(num(parts));
        QStringList flags;
        if (f1) flags << I18n::tr("completed before");
        if (f2) flags << I18n::tr("single player");
        if (f3) flags << I18n::tr("random mission");
        mFlags_->setText(flags.isEmpty() ? "-" : flags.join(", "));
    } else {
        for (QLabel* l : {mLevel_, mMap_, mDiff_, mThreat_, mApoc_, mSeed_, mLost_, mParts_, mFlags_}) l->setText("-");
    }
    if (gm && prop(gm, "mReplicatedEmergentDifficulty", off)) {
        int32_t raid = 0, aff = 0;
        ue_->rd(gm + off, raid); ue_->rd(gm + off + 4, aff);
        mRaid_->setText(num(raid));
        mAffectors_->setText(num(aff));
    } else { mRaid_->setText("-"); mAffectors_->setText("-"); }

    // ---- tower
    floors_->setUpdatesEnabled(false);
    floors_->clear();
    if (gm && prop(gm, "mReplicatedTowerMissionState", off)) {
        const uint64_t tm = gm + off;
        std::string id;
        readFString(tm + 0x90, id);
        uint8_t fin = 0;
        int32_t seed = 0, nPlayers = 0, nFloors = 0, nRewards = 0;
        uint64_t fdata = 0;
        ue_->rd(tm + 0xC8, fin); ue_->rd(tm + 0x78, seed); ue_->rd(tm + 8, nPlayers);
        ue_->rd(tm + 0x80, fdata); ue_->rd(tm + 0x88, nFloors); ue_->rd(tm + 0xC0, nRewards);
        const bool inTower = !id.empty() || (nFloors > 0 && nFloors < 100);
        tState_->setText(!inTower ? I18n::tr("Not in a tower") : fin ? I18n::tr("Finished") : I18n::tr("In progress"));
        tId_->setText(id.empty() ? "-" : QString::fromStdString(id));
        tFloors_->setText(inTower && nFloors >= 0 && nFloors < 100 ? num(nFloors) : "-");
        tSeed_->setText(inTower ? num(seed) : "-");
        tPlayers_->setText(inTower && nPlayers >= 0 && nPlayers < 16 ? num(nPlayers) : "-");
        tRewards_->setText(inTower && nRewards >= 0 && nRewards < 1000 ? num(nRewards) : "-");
        if (inTower && nFloors > 0 && nFloors < 100 && UE4::plausible(fdata)) {
            static const char* const types[] = {"No type", "Combat", "Merchant", "Boss", "Empty"};
            for (int i = 0; i < nFloors; ++i) {
                const uint64_t e = fdata + (uint64_t)i * 0x38;
                std::string tile;
                uint8_t t = 0;
                readFString(e, tile);
                ue_->rd(e + 0x10, t);
                new QTreeWidgetItem(floors_, QStringList() << num(i + 1) << QString::fromStdString(tile) << (t < 5 ? I18n::tr(types[t]) : num(t)));
            }
        }
    } else {
        for (QLabel* l : {tState_, tId_, tFloors_, tSeed_, tPlayers_, tRewards_}) l->setText("-");
    }
    floors_->setUpdatesEnabled(true);
    {
        uint64_t tmgr = 0;
        int32_t n = 0;
        if (ue_->readPtr(gi, "TowerManager", tmgr) && prop(tmgr, "currentRewards", off) && ue_->rd(tmgr + off + 8, n) && n >= 0 && n < 1000) tManager_->setText(num(n));
        else tManager_->setText("-");
    }

    // ---- merchants and save states (game instance)
    {
        std::vector<std::string> names;
        int count = 0;
        if (prop(gi, "mUnlockedMerchantClasses", off) && readMapList(gi + off, names, count, true)) {
            merchCount_->setText(num(count));
            if ((int)merchants_->topLevelItemCount() != (int)names.size()) {
                merchants_->clear();
                for (const std::string& n : names) new QTreeWidgetItem(merchants_, QStringList(QString::fromStdString(n)));
            }
        } else { merchCount_->setText("-"); merchants_->clear(); }
        int sc = 0;
        std::vector<std::string> none;
        saveCount_->setText(prop(gi, "mSaveStates", off) && readMapList(gi + off, none, sc, false) ? num(sc) : "-");
    }
}

} // namespace dk
