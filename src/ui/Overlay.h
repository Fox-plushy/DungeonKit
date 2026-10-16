#pragma once
// A small see-through window that stays on top of the game and shows a short, colored stack of live stats
// (health, damage, speed, the active quest, ...). It never takes focus or the mouse - it is purely informational -
// and tracks the game's own window rect so it stays put whether the game is windowed, maximized or moved. It shows
// over the game in windowed or borderless windowed mode; it may not show over a game in true exclusive fullscreen.
#include <QWidget>

class QLabel;

namespace dk {

class StatOverlay : public QWidget {
public:
    explicit StatOverlay(QWidget* parent = nullptr);

    // One pre-formatted line per stat, already colored (see Mcd2Pages' overlayTick). Empty = nothing is shown
    // and the window hides itself.
    void setLinesHtml(const QString& html);

    void setScale(double s);                 // 0.5 .. 3.0: a size multiplier for the whole overlay

    // Moves the overlay to one corner of the game's window rect, then nudges it by (offsetX, offsetY) pixels - so a
    // panel that tracks the window correctly but sits slightly off from where you'd like it can be dragged into place.
    // corner: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right.
    void trackWindow(long x, long y, long w, long h, int corner, int offsetX = 0, int offsetY = 0);

private:
    void restyle();
    QLabel* label_ = nullptr;
    double scale_ = 1.0;
};

} // namespace dk
