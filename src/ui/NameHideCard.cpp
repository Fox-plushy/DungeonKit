#include "NameHideCard.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMutexLocker>
#include <QSettings>
#include <QThread>
#include <QVBoxLayout>
#include <set>
#include <vector>
#include "../core/Process.h"

namespace dk {

namespace {
// As many names of the "what is this player called" field as we know of, across both games and across menu/in-level objects. Harmless
// to list ones that don't exist on a given class - UE4::find() just won't find them there, so every entry is tried on every object.
const char* const kNameFields[] = {"PlayerName", "PlayerNamePrivate", "PlayerNamePublic", "mDisplayName", "mSecondaryDisplayName",
                                    "DisplayName", "mUserName", "UserName", "mPlayerName", "PlayerDisplayName"};
}

NameHideCard::NameHideCard(std::function<Process*()> proc, const QString& settingsKey, QWidget* parent, bool embedded) : QFrame(parent), proc_(std::move(proc)), key_(settingsKey) {
    if (!embedded) setObjectName("Card");
    auto* v = new QVBoxLayout(this);
    if (embedded) v->setContentsMargins(0, 4, 0, 4); else v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    // Minecraft Dungeons II's version of this (embedded in the Streamer mode card) is still beta - it's known to fail on the main menu.
    // Minecraft Dungeons' own name hider doesn't have that problem, so only the MCD2 instance gets the beta label.
    auto* title = new QLabel(embedded ? "Hide my name (BETA)" : "Hide my name");
    if (embedded) title->setStyleSheet("font-weight: 600;"); else title->setObjectName("CardTitle");
    v->addWidget(title);
    auto* hint = new QLabel(embedded ? "Hides or changes your name on your own screen, for streaming. Other players still see your real name. "
                            "Menus that already show your name may only update after a level loads. Still in beta - it does not "
                            "reliably work yet (known to fail on the Minecraft Dungeons II main menu in particular)."
                            : "Hides or changes your name on your own screen, for streaming. Other players still see your real name. "
                            "Menus that already show your name may only update after a level loads.");
    hint->setObjectName("Hint");
    hint->setWordWrap(true);
    v->addWidget(hint);

    auto* row = new QHBoxLayout;
    auto* ml = new QLabel("Name");
    mode_ = new QComboBox;
    mode_->addItem("Show my real name", 0);
    mode_->addItem("Hide it completely", 1);
    mode_->addItem("Default (Player)", 2);
    mode_->addItem("Custom", 3);
    custom_ = new QLineEdit;
    custom_->setMaxLength(24);
    custom_->setPlaceholderText("Custom name");
    row->addWidget(ml);
    row->addWidget(mode_);
    row->addWidget(custom_, 1);
    v->addLayout(row);
    others_ = new QCheckBox("Also hide everyone else's names (lobby list)");
    v->addWidget(others_);
    status_ = new QLabel;
    status_->setObjectName("Hint");
    status_->setWordWrap(true);
    v->addWidget(status_);

    QSettings s;
    mode_->setCurrentIndex(qBound(0, s.value(key_ + "/mode", 0).toInt(), 3));
    custom_->setText(s.value(key_ + "/custom", "").toString());
    others_->setChecked(s.value(key_ + "/others", false).toBool());
    updateEnabled();

    connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { updateEnabled(); saveSettings(); pending_ = 6; });
    connect(custom_, &QLineEdit::textChanged, this, [this]() { saveSettings(); pending_ = 6; });
    connect(others_, &QCheckBox::toggled, this, [this]() { saveSettings(); pending_ = 6; });
    connect(&timer_, &QTimer::timeout, this, [this]() { tick(); });
    timer_.start(500);
}

void NameHideCard::saveSettings() {
    QSettings s;
    s.setValue(key_ + "/mode", mode_->currentIndex());
    s.setValue(key_ + "/custom", custom_->text());
    s.setValue(key_ + "/others", others_->isChecked());
}

void NameHideCard::updateEnabled() { custom_->setEnabled(mode_->currentIndex() == 3); }

bool NameHideCard::readString(Process& p, uint64_t addr, uint64_t& data, int32_t& num, int32_t& max, std::wstring& text) {
    data = ue_.rd64(addr);
    num = 0;
    max = 0;
    ue_.rd(addr + 8, num);
    ue_.rd(addr + 12, max);
    if (!UE4::plausible(data) || num < 0 || num > 256 || max < num || max > 4096) return false;
    text.clear();
    if (num > 1) {
        std::vector<uint16_t> u((size_t)num, 0);                    // UTF-16 units, read explicitly
        if (!p.read(data, u.data(), (size_t)num * 2)) return false;
        for (int i = 0; i < num && u[(size_t)i]; ++i) text.push_back((wchar_t)u[(size_t)i]);
    }
    return true;
}

int NameHideCard::applyString(Process& p, uint64_t addr, const std::wstring& want, bool& cut) {
    uint64_t data = 0;
    int32_t num = 0, max = 0;
    std::wstring cur;
    if (!readString(p, addr, data, num, max, cur)) return -1;
    if (cur == want) return 0;
    auto it = saved_.find(addr);
    const bool ours = it != saved_.end() && it->second.data == data && it->second.written == cur;
    if (!ours) { Saved s; s.data = data; s.original = cur; saved_[addr] = s; it = saved_.find(addr); }
    const int capacity = max - 1;
    if (capacity < (int)want.size() && !want.empty()) cut = true;
    if (capacity < 0) return -1;
    std::wstring text = want.substr(0, (size_t)capacity);
    if (text == cur) { it->second.written = text; return 0; }
    std::vector<uint16_t> u(text.size() + 1, 0);
    for (size_t i = 0; i < text.size(); ++i) u[i] = (uint16_t)text[i];
    if (!p.write(data, u.data(), u.size() * 2)) return -1;
    const int32_t newNum = (int32_t)text.size() + 1;
    if (!p.write(addr + 8, &newNum, sizeof newNum)) return -1;
    it->second.written = text;
    return 1;
}

void NameHideCard::restoreString(Process& p, uint64_t addr, const Saved& s) {
    uint64_t data = 0;
    int32_t num = 0, max = 0;
    std::wstring cur;
    if (!readString(p, addr, data, num, max, cur)) return;
    if (data != s.data || cur != s.written) return;                          // the game has changed it since, leave it alone
    if ((int)s.original.size() + 1 > max) return;
    std::vector<uint16_t> u(s.original.size() + 1, 0);
    for (size_t i = 0; i < s.original.size(); ++i) u[i] = (uint16_t)s.original[i];
    p.write(data, u.data(), u.size() * 2);
    const int32_t newNum = s.original.empty() ? num : (int32_t)s.original.size() + 1;
    p.write(addr + 8, &newNum, sizeof newNum);
}

void NameHideCard::restoreAll(Process* p) {
    if (p) for (const auto& kv : saved_) restoreString(*p, kv.first, kv.second);
    saved_.clear();
}

bool NameHideCard::localPlayerState(uint64_t& ps) {
    uint64_t eng = 0, gi = 0, players = 0, lp = 0, pc = 0;
    int32_t num = 0;
    if (!ue_.engine(eng) || !ue_.readPtr(eng, "GameInstance", gi)) return false;
    if (!ue_.readArray(gi, "LocalPlayers", players, num) || num <= 0) return false;
    lp = ue_.rd64(players);
    if (!UE4::plausible(lp) || !ue_.readPtr(lp, "PlayerController", pc) || !UE4::plausible(pc)) return false;
    return ue_.readPtr(pc, "PlayerState", ps) && UE4::plausible(ps);
}

void NameHideCard::allPlayerStates(std::vector<uint64_t>& out) {
    uint64_t eng = 0, vp = 0, world = 0, gs = 0, data = 0;
    int32_t num = 0;
    if (!ue_.engine(eng) || !ue_.readPtr(eng, "GameViewport", vp) || !ue_.readPtr(vp, "World", world) || !UE4::plausible(world)) return;
    if (!ue_.readPtr(world, "GameState", gs) || !UE4::plausible(gs)) return;
    if (!ue_.readArray(gs, "PlayerArray", data, num) || !UE4::plausible(data) || num <= 0 || num > 16) return;
    for (int i = 0; i < num; ++i) {
        const uint64_t ps = ue_.rd64(data + (uint64_t)i * 8);
        if (UE4::plausible(ps)) out.push_back(ps);
    }
}

// World -> GameState -> PlayerArray only exists once you're in a level or the camp, so on the main menu localPlayerState()/allPlayerStates()
// find nothing. This instead walks the game's full live-object table (same approach WidgetHideCard uses for on-screen widgets) and picks
// out anything whose class name contains "PlayerState", wherever it lives - including the one the main menu keeps around for your profile.
// It runs on a background thread since scanning the whole object table every tick would stall the UI.
void NameHideCard::rescanAllPlayerStates(Process&) {
    {
        QMutexLocker l(&scanLock_);
        if (scanDone_) { scannedStates_ = scanResult_; scanDone_ = false; scanning_ = false; }
        if (scanning_) return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < nextScan_) return;
    nextScan_ = now + 5000;
    UE4::ObjArray arr;
    std::string err;
    if (!ue_.objectArray(arr, err)) return;
    {
        QMutexLocker l(&scanLock_);
        scanning_ = true;
    }
    QThread* th = QThread::create([this, arr]() {
        std::vector<uint64_t> found;
        const int lim = arr.num < 2000000 ? arr.num : 2000000;
        for (int i = 0; i < lim; ++i) {
            const uint64_t o = ue_.objectAt(arr, i);
            if (!UE4::plausible(o)) continue;
            if (ue_.className(o).find("PlayerState") == std::string::npos) continue;
            if (ue_.objectName(o).rfind("Default__", 0) == 0) continue;
            found.push_back(o);
        }
        QMutexLocker l(&scanLock_);
        scanResult_ = found;
        scanDone_ = true;
    });
    connect(th, &QThread::finished, th, &QObject::deleteLater);
    th->start();
}

void NameHideCard::tick() {
    if (!auto_) { if (pending_ <= 0) return; --pending_; }
    Process* p = proc_ ? proc_() : nullptr;
    const int mode = mode_->currentData().toInt();
    if (!p || !p->attached() || !p->alive()) {
        saved_.clear();                                    // the game is gone, nothing to put back
        scannedStates_.clear();
        pid_ = 0;
        status_->setText(mode == 0 ? QString() : tr("Waiting for Minecraft Dungeons. Pick the game on Home and load into the game."));
        return;
    }
    if (pid_ != p->pid()) { saved_.clear(); scannedStates_.clear(); pid_ = p->pid(); }
    if (mode == 0 && saved_.empty()) { status_->setText(QString()); return; }
    if (!ue_.ready() || ueFor_ != p->pid()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now < nextInitTry_) return;
        std::string err;
        if (!ue_.init(*p, err)) { nextInitTry_ = now + 3000; status_->setText(tr("The game is not ready yet.")); return; }
        ueFor_ = p->pid();
    }

    // which strings should hold what
    std::map<uint64_t, std::wstring> want;
    uint64_t local = 0;
    const bool haveLocal = localPlayerState(local);
    std::wstring mine;
    if (mode == 1) mine = L"";
    else if (mode == 2) mine = L"Player";
    else if (mode == 3) mine = custom_->text().toStdWString();
    const bool restoring = mode == 0;
    std::vector<uint64_t> states;
    std::set<uint64_t> fromMenu;                            // states found via the full-object fallback, not the normal GameState walk
    if (!restoring) {
        if (haveLocal) states.push_back(local);
        if (others_->isChecked()) allPlayerStates(states);
        // No World/GameState yet means there's no level loaded (main menu, loading screens, character select). Fall back to a full
        // object-table scan so the name still gets changed there. There's no "other players" concept before a level loads, so every
        // PlayerState found this way is treated as yours.
        if (!haveLocal) {
            rescanAllPlayerStates(*p);
            for (uint64_t ps : scannedStates_) if (UE4::plausible(ps)) { states.push_back(ps); fromMenu.insert(ps); }
        }
    }
    std::set<uint64_t> seen;
    int changed = 0, count = 0;
    bool cut = false;
    for (uint64_t ps : states) {
        if (!seen.insert(ps).second) continue;
        const bool isLocal = (haveLocal && ps == local) || fromMenu.count(ps) != 0;
        if (ue_.className(ps).find("PlayerState") == std::string::npos) continue;
        ++count;
        const std::wstring text = isLocal ? mine : (mode == 2 ? std::wstring(L"Player") : std::wstring());
        for (const char* f : kNameFields) {
            UE4::Prop pr;
            if (!ue_.find(ps, f, pr)) continue;
            want[ps + pr.offset] = text;
        }
    }
    // put back anything that should no longer be changed
    for (auto it = saved_.begin(); it != saved_.end();) {
        if (want.count(it->first)) { ++it; continue; }
        restoreString(*p, it->first, it->second);
        it = saved_.erase(it);
    }
    for (const auto& kv : want) {
        const int r = applyString(*p, kv.first, kv.second, cut);
        if (r > 0) ++changed;
    }
    if (restoring) { status_->setText(saved_.empty() ? QString() : tr("Putting your name back...")); return; }
    if (!haveLocal && count == 0) { status_->setText(tr("Looking for your player. This can take a few seconds on the main menu.")); return; }
    QString s = tr("Active. Names handled: %1.").arg(count);
    if (!haveLocal) s += " " + tr("Found on the main menu.");
    if (cut) s += " " + tr("Your real name is short, so the new name was shortened to fit.");
    status_->setText(s);
    Q_UNUSED(changed);
}

} // namespace dk
