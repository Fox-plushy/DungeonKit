#pragma once
#include <QDialog>
#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include "Theme.h"

namespace dk {

// Start-up loading screen. It is purely cosmetic: a short animated sequence (5 to 10 seconds)
// that runs before the main window opens. exec() returns when it is finished.
class SplashDialog : public QDialog {
    Q_OBJECT
public:
    explicit SplashDialog(const Theme& theme, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    void tick();
    double progress() const;

    Theme t_;
    QPixmap logo_;
    QTimer timer_;
    QElapsedTimer clock_;
    int totalMs_ = 7000;
    bool finishing_ = false;
};

} // namespace dk
