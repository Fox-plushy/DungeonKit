#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace dk {
namespace Logo {

// The DungeonKit logo. When `recolor` is on, its hue follows the theme's accent colour
// (brightness and transparency are kept, so the shading stays the same). A "logo.png"
// placed next to the exe replaces the built-in image and is never recoloured.
QPixmap pixmap(int size, const QColor& accent, bool recolor);
QIcon icon(const QColor& accent, bool recolor);

bool recolorEnabled();                 // saved setting, on by default
void setRecolorEnabled(bool on);

}
} // namespace dk
