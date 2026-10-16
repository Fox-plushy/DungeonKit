#pragma once
class QWidget;

namespace dk {

// Colours the native window title bar to match the theme (Windows 11 for exact colours;
// Windows 10 only gets the dark/light switch). Safe to call repeatedly and on any platform.
void applyTitleBarColors(QWidget* window, unsigned bgRgb, unsigned textRgb, unsigned borderRgb);

}
