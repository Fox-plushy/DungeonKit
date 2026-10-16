#include "Logo.h"

#include <QApplication>
#include <QImage>
#include <QSettings>
#include <algorithm>

namespace dk {
namespace Logo {

static QImage baseImage() {
    static QImage img;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        QImage custom(QApplication::applicationDirPath() + "/logo.png");
        img = custom.isNull() ? QImage(":/logo.png") : custom;
        img = img.convertToFormat(QImage::Format_ARGB32);
    }
    return img;
}

static bool usingCustom() {
    static int c = -1;
    if (c < 0) c = QImage(QApplication::applicationDirPath() + "/logo.png").isNull() ? 0 : 1;
    return c == 1;
}

static QImage tint(const QImage& src, const QColor& accent) {
    QImage out = src;
    const float hue = accent.hslHueF() < 0 ? 0.f : accent.hslHueF();
    // Saturation follows the accent: vivid themes give a vivid logo, grey themes a grey one.
    const float satScale = std::min(1.0f, accent.hslSaturationF() * 1.15f);
    for (int y = 0; y < out.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const QColor c = QColor::fromRgba(line[x]);
            const float a = c.alphaF();
            if (a <= 0.f) continue;
            const float s = std::min(1.0f, std::max(0.0f, c.hslSaturationF() * satScale));
            QColor n = QColor::fromHslF(hue, s, c.lightnessF(), a);
            line[x] = n.rgba();
        }
    }
    return out;
}

QPixmap pixmap(int size, const QColor& accent, bool recolor) {
    static QRgb cachedKey = 0;
    static bool cachedValid = false;
    static QImage cached;
    QImage base = baseImage();
    if (base.isNull()) return QPixmap();
    if (recolor && !usingCustom()) {
        if (!cachedValid || cachedKey != accent.rgb()) {
            cached = tint(base, accent);
            cachedKey = accent.rgb();
            cachedValid = true;
        }
        base = cached;
    }
    return QPixmap::fromImage(base.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QIcon icon(const QColor& accent, bool recolor) {
    QIcon ic;
    for (int s : {16, 24, 32, 48, 64, 128}) ic.addPixmap(pixmap(s, accent, recolor));
    return ic;
}

bool recolorEnabled() { return QSettings().value("logo/recolor", true).toBool(); }
void setRecolorEnabled(bool on) { QSettings().setValue("logo/recolor", on); }

}
} // namespace dk
