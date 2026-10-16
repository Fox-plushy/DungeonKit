#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace dk {

// Plain single-colour icons drawn in code (no image files). Recoloured whenever the theme changes.
namespace Icons {

enum Kind { Home, Items, Teleports, Player, Heart, Tools, Eye, Account, Customize, Information, Folder, Globe, Save, Book, Warning, Camera, Pin, Chest, Pencil };

QPixmap pixmap(Kind kind, const QColor& color, int size = 128);
QIcon navIcon(Kind kind, const QColor& normal, const QColor& checked);

}

} // namespace dk
