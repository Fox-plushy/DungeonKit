#include "WidgetHideCard.h"
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMutexLocker>
#include <QSettings>
#include <QThread>
#include <QVBoxLayout>
#include "../core/Process.h"

namespace dk {

WidgetHideCard::WidgetHideCard(const Spec& spec, std::function<Process*()> proc, QWidget* parent, bool embedded) : QFrame(parent), spec_(spec), proc_(std::move(proc)) {
    if (!embedded) setObjectName("Card");
    auto* v = new QVBoxLayout(this);
    if (embedded) v->setContentsMargins(0, 4, 0, 4); else v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    auto* title = new QLabel(spec_.title);
    if (embedded) title->setStyleSheet("font-weight: 600;"); else title->setObjectName("CardTitle");
    v->addWidget(title);
    auto* hint = new QLabel(spec_.hint);
    hint->setObjectName("Hint");
    hint->setWordWrap(true);
    v->addWidget(hint);
    auto* row = new QHBoxLayout;
    auto* ml = new QLabel("Show as");
    mode_ = new QComboBox;
    mode_->addItem("Show it", 0);
    mode_->addItem("Hide it completely", 1);
    mode_->addItem("Default", 2);
    mode_->addItem("Custom", 3);
    custom_ = new QLineEdit;
    custom_->setMaxLength(24);
    custom_->setPlaceholderText("Custom text");
    row->addWidget(ml);
    row->addWidget(mode_);
    row->addWidget(custom_, 1);
    v->addLayout(row);
    status_ = new QLabel;
    status_->setObjectName("Hint");
    status_->setWordWrap(true);
    v->addWidget(status_);

    QSettings s;
    mode_->setCurrentIndex(qBound(0, s.value(spec_.settingsKey + "/mode", 0).toInt(), 3));
    custom_->setText(s.value(spec_.settingsKey + "/custom", "").toString());
    custom_->setEnabled(mode_->currentIndex() == 3);
    connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { custom_->setEnabled(mode_->currentIndex() == 3); saveSettings(); pending_ = 14; });
    connect(custom_, &QLineEdit::textChanged, this, [this]() { saveSettings(); pending_ = 14; });
    connect(&timer_, &QTimer::timeout, this, [this]() { tick(); });
    timer_.start(700);
}

void WidgetHideCard::saveSettings() {
    QSettings s;
    s.setValue(spec_.settingsKey + "/mode", mode_->currentIndex());
    s.setValue(spec_.settingsKey + "/custom", custom_->text());
}

bool WidgetHideCard::readFString(Process& p, uint64_t addr, uint64_t& data, int32_t& num, int32_t& max, std::wstring& text) {
    data = ue_.rd64(addr);
    num = 0;
    max = 0;
    ue_.rd(addr + 8, num);
    ue_.rd(addr + 12, max);
    if (!UE4::plausible(data) || num < 0 || num > 256 || max < num || max > 4096) return false;
    text.clear();
    if (num > 1) {
        std::vector<uint16_t> u((size_t)num, 0);
        if (!p.read(data, u.data(), (size_t)num * 2)) return false;
        for (int i = 0; i < num && u[(size_t)i]; ++i) {
            if (u[(size_t)i] < 0x20) return false;           // not text
            text.push_back((wchar_t)u[(size_t)i]);
        }
    }
    return true;
}

// A text block's Text is an FText: a pointer to shared text data. The shown string sits somewhere inside it, either as an FString or behind a pointer.
// Look for something that reads like a short string.
uint64_t WidgetHideCard::findTextString(Process& p, uint64_t textBlock) {
    UE4::Prop pr;
    if (!ue_.find(textBlock, "Text", pr)) return 0;
    const uint64_t td = ue_.rd64(textBlock + pr.offset);
    if (!UE4::plausible(td)) return 0;
    for (uint64_t off = 0; off <= 0x80; off += 8) {
        uint64_t d; int32_t n, m; std::wstring t;
        if (readFString(p, td + off, d, n, m, t) && n >= 2) return td + off;
        const uint64_t ptr = ue_.rd64(td + off);
        if (UE4::plausible(ptr) && readFString(p, ptr, d, n, m, t) && n >= 2) return ptr;
    }
    return 0;
}

void WidgetHideCard::setText(Process& p, uint64_t addr, const std::wstring& want) {
    uint64_t data; int32_t num, max; std::wstring cur;
    if (!readFString(p, addr, data, num, max, cur)) return;
    if (cur == want) return;
    auto it = savedText_.find(addr);
    const bool ours = it != savedText_.end() && it->second.data == data && it->second.written == cur;
    if (!ours) { SavedText s; s.data = data; s.original = cur; savedText_[addr] = s; it = savedText_.find(addr); }
    if (max < 1) return;
    const std::wstring text = want.substr(0, (size_t)(max - 1));
    std::vector<uint16_t> u(text.size() + 1, 0);
    for (size_t i = 0; i < text.size(); ++i) u[i] = (uint16_t)text[i];
    if (!p.write(data, u.data(), u.size() * 2)) return;
    const int32_t newNum = (int32_t)text.size() + 1;
    p.write(addr + 8, &newNum, sizeof newNum);
    it->second.written = text;
}

void WidgetHideCard::setVisible(Process& p, uint64_t obj, bool hide) {
    UE4::Prop pr;
    if (!ue_.find(obj, "Visibility", pr)) return;
    const uint64_t a = obj + pr.offset;
    uint8_t cur = 0;
    if (!ue_.rd(a, cur)) return;
    auto it = savedVis_.find(a);
    if (hide) {
        if (cur == 1) return;                                // Collapsed
        if (it == savedVis_.end()) { SavedVis s; s.original = cur; s.written = 1; savedVis_[a] = s; }
        const uint8_t one = 1;
        p.write(a, &one, 1);
    } else if (it != savedVis_.end()) {
        if (cur == it->second.written) p.write(a, &it->second.original, 1);
        savedVis_.erase(it);
    }
}

void WidgetHideCard::restoreAll(Process* p) {
    if (p) {
        for (const auto& kv : savedText_) {
            uint64_t data; int32_t num, max; std::wstring cur;
            if (!readFString(*p, kv.first, data, num, max, cur) || data != kv.second.data || cur != kv.second.written) continue;
            if ((int)kv.second.original.size() + 1 > max) continue;
            std::vector<uint16_t> u(kv.second.original.size() + 1, 0);
            for (size_t i = 0; i < kv.second.original.size(); ++i) u[i] = (uint16_t)kv.second.original[i];
            p->write(data, u.data(), u.size() * 2);
            const int32_t newNum = (int32_t)kv.second.original.size() + 1;
            p->write(kv.first + 8, &newNum, sizeof newNum);
        }
        for (const auto& kv : savedVis_) {
            uint8_t cur = 0;
            if (ue_.rd(kv.first, cur) && cur == kv.second.written) p->write(kv.first, &kv.second.original, 1);
        }
    }
    savedText_.clear();
    savedVis_.clear();
}

void WidgetHideCard::rescan(Process&) {
    {
        QMutexLocker l(&scanLock_);
        if (scanDone_) { widgets_ = scanResult_; scanDone_ = false; scanning_ = false; }
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
    const std::string cls = spec_.widgetClass.toStdString();
    QThread* th = QThread::create([this, arr, cls]() {
        std::vector<uint64_t> found;
        const int lim = arr.num < 2000000 ? arr.num : 2000000;
        for (int i = 0; i < lim; ++i) {
            const uint64_t o = ue_.objectAt(arr, i);
            if (!UE4::plausible(o)) continue;
            if (ue_.className(o) != cls) continue;
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

void WidgetHideCard::tick() {
    if (!auto_) { if (pending_ <= 0) return; --pending_; }
    Process* p = proc_ ? proc_() : nullptr;
    const int mode = mode_->currentData().toInt();
    if (!p || !p->attached() || !p->alive()) {
        savedText_.clear();
        savedVis_.clear();
        widgets_.clear();
        pid_ = 0;
        status_->setText(mode == 0 ? QString() : tr("Waiting for Minecraft Dungeons II. Pick the game on Home and load into the game."));
        return;
    }
    if (pid_ != p->pid()) { savedText_.clear(); savedVis_.clear(); widgets_.clear(); pid_ = p->pid(); }
    if (mode == 0 && savedText_.empty() && savedVis_.empty()) { status_->setText(QString()); return; }
    if (!ue_.ready() || ueFor_ != p->pid()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now < nextInit_) return;
        std::string err;
        if (!ue_.init(*p, err)) { nextInit_ = now + 3000; status_->setText(tr("The game is not ready yet.")); return; }
        ueFor_ = p->pid();
    }
    if (mode == 0) { restoreAll(p); status_->setText(QString()); return; }

    // keep only widgets that are still the right kind of object
    std::vector<uint64_t> live;
    const std::string cls = spec_.widgetClass.toStdString();
    for (uint64_t w : widgets_) if (UE4::plausible(w) && ue_.className(w) == cls) live.push_back(w);
    if (live.size() != widgets_.size()) widgets_ = live;
    rescan(*p);

    int textFound = 0;
    for (uint64_t w : widgets_) {
        if (mode == 1) setVisible(*p, w, true);
        else setVisible(*p, w, false);
        for (const std::string& prop : spec_.textProps) {
            uint64_t tb = 0;
            if (!ue_.readPtr(w, prop.c_str(), tb) || !UE4::plausible(tb)) continue;
            if (mode == 1) setVisible(*p, tb, true); else setVisible(*p, tb, false);
            const uint64_t str = findTextString(*p, tb);
            if (!str) continue;
            ++textFound;
            const std::wstring want = mode == 1 ? std::wstring() : mode == 2 ? spec_.defaultText.toStdWString() : custom_->text().toStdWString();
            setText(*p, str, want);
        }
    }
    if (widgets_.empty()) status_->setText(tr("Looking for it. Open the screen that shows it (for example the Tab screen). It may not be found."));
    else status_->setText(tr("Found %1 widget(s), text located in %2. If the screen does not change, the game has not redrawn it yet.").arg((int)widgets_.size()).arg(textFound));
}

} // namespace dk
