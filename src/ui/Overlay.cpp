#include "Overlay.h"
#include <QLabel>
#include <QVBoxLayout>

namespace dk {

StatOverlay::StatOverlay(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput | Qt::NoDropShadowWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    label_ = new QLabel;
    label_->setTextFormat(Qt::RichText);
    l->addWidget(label_);
    restyle();
}

void StatOverlay::setLinesHtml(const QString& html) {
    label_->setText(html);
    adjustSize();
    setVisible(!html.isEmpty());
}

void StatOverlay::setScale(double s) {
    scale_ = s < 0.5 ? 0.5 : (s > 3.0 ? 3.0 : s);
    restyle();
    adjustSize();
}

void StatOverlay::restyle() {
    const int padV = (int)(8 * scale_), padH = (int)(16 * scale_);
    const int font = (int)(16 * scale_);
    const int radius = (int)(8 * scale_);
    label_->setStyleSheet(QString("background: rgba(0,0,0,140); padding: %1px %2px; border-radius: %3px; "
                                   "font-size: %4px; font-weight: 600; font-family: 'Segoe UI';")
                           .arg(padV).arg(padH).arg(radius).arg(font));
}

void StatOverlay::trackWindow(long x, long y, long w, long h, int corner, int offsetX, int offsetY) {
    if (!isVisible()) return;
    adjustSize();
    const int mx = (int)(20 * scale_), my = (int)(20 * scale_);
    int px = (int)x + mx, py = (int)y + my;
    if (corner == 1 || corner == 3) px = (int)x + (int)w - width() - mx;
    if (corner == 2 || corner == 3) py = (int)y + (int)h - height() - my;
    move(px + offsetX, py + offsetY);
}

} // namespace dk
