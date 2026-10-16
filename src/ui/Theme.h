#pragma once
#include <QColor>
#include <QString>
#include <QStringList>

namespace dk {

struct Theme {
    // Default look: blackish grey with a hint of purple.
    QColor accent  {0x8b, 0x5c, 0xf6};
    QColor accent2 {0xc0, 0xa0, 0xff};
    QColor bg      {0x0b, 0x0a, 0x0f};
    QColor panel   {0x13, 0x11, 0x19};
    QColor sidebar {0x0d, 0x0c, 0x12};
    QColor text    {0xe7, 0xe3, 0xf0};
    QColor muted   {0x8a, 0x84, 0x99};
    QColor border  {0x25, 0x21, 0x2f};
    int radius   = 8;
    int fontSize = 13;
    QString customQss;          // imported .qss / .css, appended after the generated sheet

    QString toStyleSheet() const;
    void load();                // from QSettings
    void save() const;

    // Colour presets (radius, font size and imported QSS are left alone by the caller).
    static QStringList presetNames2();                // second row: Sandstone, Slime, Oxidized, Sunset, Neon, Royal, Ash
    static QStringList presetNames();                 // Obsidian, Void, Forest, Frost, End, Nether, Magic
    static Theme preset(const QString& name);
    static Theme randomized(const Theme& keepShape);       // random dark colours; corner size, font size and imported style are kept
};

} // namespace dk
