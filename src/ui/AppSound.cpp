#include "AppSound.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

namespace dk {

namespace {

const wchar_t* kAlias = L"DungeonKitAppSound";

void mci(const std::wstring& cmd) {
    mciSendStringW(cmd.c_str(), nullptr, 0, nullptr);
}

// The sound ships embedded as a Qt resource (so it's one file, inside the exe), but MCI needs a real path
// on disk to open. Extracted once per run into the temp folder and reused from there.
QString extractedPath() {
    static QString path;
    if (!path.isEmpty()) return path;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString out = dir + "/DungeonKit_AppSound.mp3";
    if (!QFile::exists(out)) {
        QFile in(":/sounds/app_futuristic_pursuit.mp3");
        if (!in.open(QIODevice::ReadOnly)) return QString();
        const QByteArray data = in.readAll();
        QFile f(out);
        if (!f.open(QIODevice::WriteOnly)) return QString();
        f.write(data);
        f.close();
    }
    path = out;
    return path;
}

}  // namespace

AppSound& AppSound::instance() {
    static AppSound s;
    return s;
}

AppSound::AppSound() {
    QSettings st;
    enabled_ = st.value("appsound/enabled", false).toBool();
    volume_ = qBound(0, st.value("appsound/volume", 5).toInt(), 100);
    if (enabled_) start();
}

AppSound::~AppSound() {
    stop();
}

void AppSound::setEnabled(bool on) {
    if (on == enabled_) return;
    enabled_ = on;
    QSettings().setValue("appsound/enabled", enabled_);
    if (enabled_) start(); else stop();
}

void AppSound::setVolume(int v) {
    volume_ = qBound(0, v, 100);
    QSettings().setValue("appsound/volume", volume_);
    if (opened_) applyVolume();
}

void AppSound::start() {
    const QString path = extractedPath();
    if (path.isEmpty()) return;
    const std::wstring alias = kAlias;
    mci(L"close " + alias);   // in case one was already open (e.g. a previous attempt this run)
    const QString native = QDir::toNativeSeparators(path);
    mci(L"open \"" + native.toStdWString() + L"\" type mpegvideo alias " + alias);
    opened_ = true;
    applyVolume();
    mci(L"play " + alias + L" repeat");
}

void AppSound::stop() {
    if (!opened_) return;
    const std::wstring alias = kAlias;
    mci(L"stop " + alias);
    mci(L"close " + alias);
    opened_ = false;
}

void AppSound::applyVolume() {
    // MCI volume runs 0-1000; the slider is 0-100. Squared curve so the low end (this plays quietly by
    // default) doesn't feel like it jumps straight to silence or straight to loud.
    const double g = (volume_ / 100.0) * (volume_ / 100.0);
    const std::wstring alias = kAlias;
    mci(L"setaudio " + alias + L" volume to " + std::to_wstring((long)(g * 1000)));
}

}  // namespace dk
