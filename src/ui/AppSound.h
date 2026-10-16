#pragma once
// Optional background sound for the app itself (separate from Click sounds, which only play on a button
// press). Off by default; the on/off choice and the volume are remembered between launches. Plays on
// repeat for as long as it is on, and keeps playing across pages since it is one app-wide instance.
#include <QString>

namespace dk {

class AppSound {
public:
    static AppSound& instance();
    bool enabled() const { return enabled_; }
    int volume() const { return volume_; }   // 0 - 100
    void setEnabled(bool on);
    void setVolume(int v);
private:
    AppSound();
    ~AppSound();
    AppSound(const AppSound&) = delete;
    void start();
    void stop();
    void applyVolume();
    bool enabled_ = false;
    int volume_ = 5;          // low by default, as asked - this plays continuously in the background
    bool opened_ = false;
};

}  // namespace dk
