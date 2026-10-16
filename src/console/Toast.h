#pragma once
// A small popup shown over the game window so you can see the console DLL did something. It is a plain Win32 window
// (topmost, click-through, never takes focus), so it shows over windowed and borderless games. It may not show over
// a game in true exclusive fullscreen; the log in DungeonKit always has the result.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace toast {

struct Data {
    std::wstring title, body;
    bool good = true;
    int slot = 0;                 // 0 = top, 1 = below that, ...
    DWORD startTick = 0;
    HINSTANCE inst = nullptr;
};

inline int scaled(int px) {
    HDC dc = GetDC(nullptr);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return MulDiv(px, dpi > 0 ? dpi : 96, 96);
}

inline BOOL CALLBACK findGameWindow(HWND h, LPARAM param) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r;
    if (!GetWindowRect(h, &r)) return TRUE;
    const long area = (r.right - r.left) * (r.bottom - r.top);
    HWND* best = reinterpret_cast<HWND*>(param);
    RECT br{};
    if (*best) GetWindowRect(*best, &br);
    if (!*best || area > (br.right - br.left) * (br.bottom - br.top)) *best = h;       // the biggest visible window of the game
    return TRUE;
}

inline LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Data* d = reinterpret_cast<Data*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(24, 22, 34));
        FillRect(dc, &rc, bg);
        DeleteObject(bg);
        if (d) {
            RECT bar = rc;
            bar.right = bar.left + scaled(6);
            HBRUSH ac = CreateSolidBrush(d->good ? RGB(110, 200, 120) : RGB(225, 100, 100));
            FillRect(dc, &bar, ac);
            DeleteObject(ac);
            SetBkMode(dc, TRANSPARENT);
            HFONT big = CreateFontW(-scaled(19), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            HFONT small = CreateFontW(-scaled(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            const int pad = scaled(18);
            HGDIOBJ old = SelectObject(dc, big);
            SetTextColor(dc, RGB(243, 243, 250));
            RECT t = rc;
            t.left += pad; t.top += scaled(12); t.right -= scaled(10);
            DrawTextW(dc, d->title.c_str(), -1, &t, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(dc, small);
            SetTextColor(dc, RGB(190, 190, 210));
            RECT b = rc;
            b.left += pad; b.top += scaled(42); b.right -= scaled(10);
            DrawTextW(dc, d->body.c_str(), -1, &b, DT_LEFT | DT_TOP | DT_WORDBREAK);
            SelectObject(dc, old);
            DeleteObject(big);
            DeleteObject(small);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_TIMER: {
        if (!d) return 0;
        const DWORD age = GetTickCount() - d->startTick;
        if (age >= 5600) { DestroyWindow(h); return 0; }
        if (age > 5000) SetLayeredWindowAttributes(h, 0, (BYTE)(235 * (5600 - age) / 600), LWA_ALPHA);      // fade out
        return 0;
    }
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

inline DWORD WINAPI thread(LPVOID p) {
    Data* d = static_cast<Data*>(p);
    static const wchar_t* kClass = L"DungeonKitToast";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = d->inst;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);                               // fails harmlessly if it already exists

    const int w = scaled(430), h = scaled(86);
    HWND game = nullptr;
    EnumWindows(findGameWindow, reinterpret_cast<LPARAM>(&game));
    RECT area{};
    if (game) GetWindowRect(game, &area);
    else { area.left = 0; area.top = 0; area.right = GetSystemMetrics(SM_CXSCREEN); area.bottom = GetSystemMetrics(SM_CYSCREEN); }
    const int x = area.left + ((area.right - area.left) - w) / 2;
    const int y = area.top + scaled(36) + d->slot * (h + scaled(10));

    HWND win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                               kClass, L"DungeonKit", WS_POPUP, x, y, w, h, nullptr, nullptr, d->inst, nullptr);
    if (!win) { delete d; return 0; }
    d->startTick = GetTickCount();
    SetWindowLongPtrW(win, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));
    SetLayeredWindowAttributes(win, 0, 235, LWA_ALPHA);
    SetWindowRgn(win, CreateRoundRectRgn(0, 0, w + 1, h + 1, scaled(16), scaled(16)), TRUE);
    SetTimer(win, 1, 50, nullptr);
    ShowWindow(win, SW_SHOWNOACTIVATE);
    SetWindowPos(win, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    UpdateWindow(win);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    delete d;
    return 0;
}

// Shows the popup for about 5 seconds and returns at once.
inline void show(HINSTANCE inst, const wchar_t* title, const wchar_t* body, bool good, int slot = 0) {
    Data* d = new Data;
    d->title = title;
    d->body = body;
    d->good = good;
    d->slot = slot;
    d->inst = inst;
    HANDLE t = CreateThread(nullptr, 0, thread, d, 0, nullptr);
    if (t) CloseHandle(t); else delete d;
}

} // namespace toast
