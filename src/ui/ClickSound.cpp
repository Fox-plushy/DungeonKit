#include "ClickSound.h"
#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QMouseEvent>
#include <QRadioButton>
#include <QSettings>
#include <windows.h>
#include <mmsystem.h>
#include <cstdint>
#include <cstring>

namespace dk {

ClickSound& ClickSound::instance() {
    static ClickSound s;
    return s;
}

ClickSound::ClickSound() {
    QSettings st;
    mode_ = st.value("sound/mode", 0).toInt();
    if (mode_ < Off || mode_ > Combined) mode_ = Off;
    volume_ = qBound(0, st.value("sound/volume", 60).toInt(), 100);
    rebuild();
}

void ClickSound::install() {
    if (qApp) qApp->installEventFilter(&instance());
}

void ClickSound::setMode(int m) {
    mode_ = qBound(int(Off), m, int(LoudClick));
    QSettings().setValue("sound/mode", mode_);
    rebuild();
}

void ClickSound::setVolume(int v) {
    volume_ = qBound(0, v, 100);
    QSettings().setValue("sound/volume", volume_);
    rebuild();
}

void ClickSound::rebuild() {
    PlaySoundW(nullptr, nullptr, 0);                       // stop anything playing before the buffer changes
    wav_.clear();
    static const char* const names[] = {nullptr, ":/sounds/soft.wav", ":/sounds/futuristic.wav", ":/sounds/combined.wav",
                                         ":/sounds/stone.wav", ":/sounds/loud_click.wav"};
    if (mode_ == Off || !names[mode_]) return;
    QFile f(names[mode_]);
    if (!f.open(QIODevice::ReadOnly)) return;
    wav_ = f.readAll();
    // find the "data" chunk and scale its 16-bit samples (volume follows a curve, so the slider feels even)
    const double gain = (volume_ / 100.0) * (volume_ / 100.0);
    int pos = 12;
    while (pos + 8 <= wav_.size()) {
        uint32_t len = 0;
        std::memcpy(&len, wav_.constData() + pos + 4, 4);
        if (std::memcmp(wav_.constData() + pos, "data", 4) == 0) {
            const int start = pos + 8;
            const int end = qMin<qint64>(wav_.size(), qint64(start) + len);
            for (int i = start; i + 1 < end; i += 2) {
                int16_t s;
                std::memcpy(&s, wav_.constData() + i, 2);
                s = int16_t(qBound(-32768.0, s * gain, 32767.0));
                std::memcpy(wav_.data() + i, &s, 2);
            }
            return;
        }
        pos += 8 + int(len) + int(len & 1);
    }
}

void ClickSound::play() {
    if (mode_ == Off || volume_ <= 0 || wav_.isEmpty()) return;
    PlaySoundW(reinterpret_cast<LPCWSTR>(wav_.constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

bool ClickSound::eventFilter(QObject* obj, QEvent* e) {
    if (mode_ != Off && e->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::LeftButton) {
            auto* b = qobject_cast<QAbstractButton*>(obj);
            if (b && b->isEnabled() && !qobject_cast<QRadioButton*>(b)) play();      // radio buttons play their own preview
        }
    }
    return QObject::eventFilter(obj, e);
}

}  // namespace dk
