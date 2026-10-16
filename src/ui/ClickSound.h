#pragma once
// Click sounds: every enabled button plays a short sound when it is pressed. The choice and the volume are remembered.
#include <QByteArray>
#include <QObject>

namespace dk {

class ClickSound : public QObject {
    Q_OBJECT
public:
    enum Mode { Off = 0, Soft = 1, Futuristic = 2, Combined = 3, Stone = 4, LoudClick = 5 };
    static ClickSound& instance();
    static void install();                 // call once after the QApplication exists
    int mode() const { return mode_; }
    int volume() const { return volume_; }  // 0 - 100
    void setMode(int m);
    void setVolume(int v);
    void play();                            // plays the current sound (does nothing when Off)
protected:
    bool eventFilter(QObject* obj, QEvent* e) override;
private:
    ClickSound();
    void rebuild();                         // reload the chosen sound and scale it to the volume
    int mode_ = Off, volume_ = 60;
    QByteArray wav_;                        // must stay alive while it plays
};

}  // namespace dk
