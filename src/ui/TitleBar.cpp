#include "TitleBar.h"

#include <QWidget>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace dk {

void applyTitleBarColors(QWidget* window, unsigned bgRgb, unsigned textRgb, unsigned borderRgb) {
#ifdef _WIN32
    if (!window) return;
    using Fn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static Fn setAttr = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (HMODULE m = LoadLibraryW(L"dwmapi.dll"))
            setAttr = reinterpret_cast<Fn>(GetProcAddress(m, "DwmSetWindowAttribute"));
    }
    if (!setAttr) return;
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return;

    auto colorref = [](unsigned rgb) -> COLORREF {          // 0xRRGGBB -> 0x00BBGGRR
        return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    };
    const unsigned r = (bgRgb >> 16) & 0xFF, g = (bgRgb >> 8) & 0xFF, b = bgRgb & 0xFF;
    const BOOL dark = (r * 299 + g * 587 + b * 114) / 1000 < 128;
    setAttr(hwnd, 20, &dark, sizeof(dark));                 // DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 1809+)
    setAttr(hwnd, 19, &dark, sizeof(dark));                 // same flag on early Win10 builds
    COLORREF cap = colorref(bgRgb), txt = colorref(textRgb), brd = colorref(borderRgb);
    setAttr(hwnd, 35, &cap, sizeof(cap));                   // DWMWA_CAPTION_COLOR (Win11)
    setAttr(hwnd, 36, &txt, sizeof(txt));                   // DWMWA_TEXT_COLOR
    setAttr(hwnd, 34, &brd, sizeof(brd));                   // DWMWA_BORDER_COLOR
#else
    (void)window; (void)bgRgb; (void)textRgb; (void)borderRgb;
#endif
}

} // namespace dk
