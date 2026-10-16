#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <QRandomGenerator>
#include <QSettings>

namespace dk {

static const int kThemeSchema = 2;   // bumped when the default palette changed, so old saved colours are dropped once

static QString rgba(const QColor& c, int a) {
    return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
}

QString Theme::toStyleSheet() const {
    QString s = QString(R"(
QWidget { background: %BG%; color: %TEXT%; font-family: "Segoe UI", sans-serif; font-size: %FS%px; }
QLabel { background: transparent; }
QLabel#Title { font-size: 22px; font-weight: 700; }
QLabel#PageTitle { font-size: 20px; font-weight: 700; }
QLabel#Tagline { font-size: 17px; font-weight: 600; }
QLabel#Slogan { font-size: %FSS%px; font-weight: 700; color: %ACCENT2%; }
QLabel#Muted { color: %MUTED%; }
QLabel#Hint { color: %MUTED%; font-size: %FSS%px; }
QLabel#Message { color: %ACCENT2%; }
QLabel#Notice { background: %PANEL%; color: %TEXT%; border: 1px solid %BORDER%; border-left: 4px solid %ACCENT%; border-radius: %R%px; padding: 10px 12px; }
QLabel#Section { color: %MUTED%; font-size: %FSS%px; font-weight: 700; padding: 12px 4px 3px 4px; }
QLabel#Name { color: %TEXT%; font-weight: 600; }

QFrame#Sidebar { background: %SIDEBAR%; border: none; border-right: 1px solid %BORDER%; }
QFrame#Sidebar QLabel, QFrame#Sidebar QPushButton { background: transparent; }
QPushButton#Nav { text-align: left; padding: 10px 14px; border: none; border-radius: %R%px; color: %MUTED%; background: transparent; }
QPushButton#Nav:hover { background: %PANEL%; color: %TEXT%; }
QPushButton#Nav:checked { background: %ACCENT_DIM%; color: %TEXT%; border-left: 3px solid %ACCENT%; }

QFrame#StatusBox { background: %PANEL%; border: 1px solid %BORDER%; border-radius: %R%px; }
QFrame#StatusBox QLabel { background: transparent; }

QFrame#Card { background: %PANEL%; border: 1px solid %BORDER%; border-radius: %R%px; }
QFrame#Card QLabel, QFrame#Card QCheckBox { background: transparent; }
QLabel#CardTitle { font-size: 14px; font-weight: 600; }
QToolButton#CollapsibleHeader { font-size: 14px; font-weight: 600; border: none; background: transparent; padding: 2px 0; color: %TEXT%; }
QToolButton#CollapsibleHeader:hover { color: %ACCENT%; }

QPushButton { background: %PANEL%; border: 1px solid %BORDER%; border-radius: %R%px; padding: 6px 14px; }
QPushButton:hover { border-color: %ACCENT%; }
QPushButton:pressed { background: %ACCENT_DIM%; }
QPushButton:disabled { color: %MUTED%; border-color: %BORDER%; }
QPushButton#Tab:checked { background: %ACCENT_DIM%; border: 1px solid %ACCENT%; color: %TEXT%; font-weight: 700; }
QPushButton#Primary { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %ACCENT%, stop:1 %ACCENT2%); border: none; color: %ONACCENT%; font-weight: 700; }
QPushButton#Primary:disabled { background: %BORDER%; color: %MUTED%; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextBrowser {
    background: %BG%; border: 1px solid %BORDER%; border-radius: %R%px; padding: 5px 8px; selection-background-color: %ACCENT%; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: %ACCENT%; }
QComboBox QAbstractItemView { background: %PANEL%; border: 1px solid %BORDER%; selection-background-color: %ACCENT%; }

QTreeWidget { background: %BG%; border: 1px solid %BORDER%; border-radius: %R%px; padding: 4px; outline: 0; }
QTreeWidget::item { padding: 4px 2px; border-radius: 4px; }
QTreeWidget::item:hover { background: %PANEL%; }
QHeaderView::section { background: %PANEL%; color: %MUTED%; border: none; border-bottom: 1px solid %BORDER%; padding: 5px 8px; }
QTreeWidget::item:selected { background: %ACCENT_DIM%; color: %TEXT%; }

QCheckBox { spacing: 10px; background: transparent; }
QCheckBox::indicator { width: 36px; height: 18px; border-radius: 9px; background: %BORDER%; border: 1px solid %BORDER%; }
QCheckBox::indicator:hover { border-color: %ACCENT%; }
QCheckBox::indicator:checked { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %ACCENT%, stop:1 %ACCENT2%); border-color: %ACCENT%; }
QCheckBox::indicator:disabled { background: %PANEL%; }
QRadioButton { spacing: 8px; }
QRadioButton::indicator { width: 16px; height: 16px; border-radius: 9px; background: %PANEL%; border: 2px solid %BORDER%; }
QRadioButton::indicator:hover { border-color: %ACCENT%; }
QRadioButton::indicator:checked { border: 2px solid %ACCENT%; background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 %ACCENT%, stop:0.55 %ACCENT2%, stop:0.62 %PANEL%, stop:1 %PANEL%); }
QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: %BORDER%; }
QSlider::sub-page:horizontal { height: 6px; border-radius: 3px; background: %ACCENT%; }
QSlider::handle:horizontal { width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; background: %ACCENT%; border: 2px solid %PANEL%; }
QSlider::handle:horizontal:hover { background: %ACCENT2%; }
QRadioButton::indicator:disabled { background: %PANEL%; border-color: %BORDER%; }

QScrollArea { border: none; background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: %BORDER%; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: %ACCENT%; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
)");
    {   // readable text on the accent gradient: dark text on light accents, white text on dark ones
        auto lum = [](const QColor& c) { return 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF(); };
        s.replace("%ONACCENT%", (lum(accent) + lum(accent2)) / 2.0 > 0.45 ? "#0b0a0f" : "#ffffff");
    }
    s.replace("%ACCENT_DIM%", rgba(accent, 55));
    s.replace("%ACCENT2%", accent2.name());
    s.replace("%ACCENT%", accent.name());
    s.replace("%BG%", bg.name());
    s.replace("%PANEL%", panel.name());
    s.replace("%SIDEBAR%", sidebar.name());
    s.replace("%TEXT%", text.name());
    s.replace("%MUTED%", muted.name());
    s.replace("%BORDER%", border.name());
    s.replace("%R%", QString::number(radius));
    s.replace("%FSS%", QString::number(fontSize - 1));
    s.replace("%FS%", QString::number(fontSize));
    if (!customQss.isEmpty()) s += "\n/* imported */\n" + customQss;
    return s;
}

void Theme::load() {
    QSettings q;
    radius = q.value("theme/radius", radius).toInt();
    fontSize = q.value("theme/fontSize", fontSize).toInt();
    customQss = q.value("theme/custom").toString();
    if (q.value("theme/schema", 1).toInt() < kThemeSchema) return;     // old palette: use the new default
    auto col = [&](const char* k, QColor& c) { QColor v(q.value(QString("theme/") + k).toString()); if (v.isValid()) c = v; };
    col("accent", accent); col("accent2", accent2); col("bg", bg); col("panel", panel); col("sidebar", sidebar);
    col("text", text); col("muted", muted); col("border", border);
}

void Theme::save() const {
    QSettings q;
    q.setValue("theme/schema", kThemeSchema);
    q.setValue("theme/accent", accent.name()); q.setValue("theme/accent2", accent2.name());
    q.setValue("theme/bg", bg.name());
    q.setValue("theme/panel", panel.name());   q.setValue("theme/sidebar", sidebar.name());
    q.setValue("theme/text", text.name());     q.setValue("theme/muted", muted.name());
    q.setValue("theme/border", border.name());
    q.setValue("theme/radius", radius);        q.setValue("theme/fontSize", fontSize);
    q.setValue("theme/custom", customQss);
}

QStringList Theme::presetNames() { return {"Obsidian", "Void", "Forest", "Frost", "End", "Nether", "Magic"}; }
QStringList Theme::presetNames2() { return {"Sandstone", "Slime", "Oxidized", "Sunset", "Neon", "Royal", "Ash"}; }

Theme Theme::preset(const QString& name) {
    Theme t;                                   // Obsidian = the defaults above
    auto set = [&](const char* accent, const char* accent2, const char* bg, const char* panel,
                   const char* sidebar, const char* text, const char* muted, const char* border) {
        t.accent = QColor(accent); t.accent2 = QColor(accent2); t.bg = QColor(bg); t.panel = QColor(panel);
        t.sidebar = QColor(sidebar); t.text = QColor(text); t.muted = QColor(muted); t.border = QColor(border);
    };
    if (name == "Void")        // black, grey and white
        set("#d4d4d8", "#ffffff", "#050505", "#0f0f10", "#080808", "#f2f2f2", "#8a8a8f", "#27272a");
    else if (name == "Forest") // green and light green
        set("#22a35a", "#8be3a5", "#060b08", "#0d1511", "#08100b", "#e6f5ea", "#7fa38b", "#1a3324");
    else if (name == "Magic")  // maroon, white and pink
        set("#b0224a", "#ff8fc0", "#0c0508", "#1a0a10", "#10060a", "#fff0f6", "#b88a9b", "#45142a");
    else if (name == "End")    // purple and black
        set("#9333ea", "#d08bff", "#07040d", "#10081c", "#09050f", "#efe4ff", "#9279b8", "#2a1648");
    else if (name == "Nether") // orange, yellow, red, black
        set("#ff6a1a", "#ffc533", "#0d0605", "#190d09", "#110907", "#ffeddc", "#b48066", "#43190f");
    else if (name == "Sandstone") // desert gold on warm brown-black
        set("#d9a441", "#f3d58f", "#0e0a06", "#1a140c", "#120d07", "#f7eddb", "#a8957a", "#3b2d18");
    else if (name == "Slime")     // acid lime on green-black
        set("#9be21f", "#d8ff72", "#060a03", "#0e1507", "#0a1005", "#f0ffd9", "#8aa070", "#25380f");
    else if (name == "Oxidized")  // copper with teal patina
        set("#e0763d", "#3fc4b0", "#080e0e", "#101a1a", "#0b1313", "#e8f4f1", "#7ea29c", "#1d3a38");
    else if (name == "Sunset")    // coral and amber over a dusky plum
        set("#ff6b6b", "#ffd166", "#100a14", "#1c1223", "#150d1a", "#fff1e6", "#b49aa8", "#3c2147");
    else if (name == "Neon")      // hot magenta and electric cyan
        set("#ff2bd6", "#00f0ff", "#05030c", "#0e0820", "#08051a", "#f4eaff", "#8d7fb8", "#2b1458");
    else if (name == "Royal")     // royal blue and periwinkle on midnight navy
        set("#4f6bff", "#a5b4fc", "#040716", "#0a1030", "#060a22", "#e8ecff", "#7882b8", "#17205a");
    else if (name == "Ash")       // warm grey and ivory
        set("#c9b8a3", "#efe3d1", "#0d0c0b", "#1a1816", "#111009", "#f4efe8", "#9a9084", "#35312c");
    else if (name == "Frost")  // blue, light blue, cyan
        set("#3b9cff", "#67e8f9", "#060a11", "#0d1520", "#08101a", "#e4f3ff", "#7a9ebc", "#183049");
    return t;
}

Theme Theme::randomized(const Theme& keep) {
    auto rnd = [](double lo, double hi) { return lo + (hi - lo) * QRandomGenerator::global()->generateDouble(); };
    auto hsv = [](double h, double s, double v) {
        h -= std::floor(h);
        return QColor::fromHsvF(h, std::min(1.0, std::max(0.0, s)), std::min(1.0, std::max(0.0, v)));
    };
    const double h = rnd(0.0, 1.0);
    // the second accent is a neighbour on the colour wheel, or sometimes the opposite side
    const double h2 = QRandomGenerator::global()->bounded(4) == 0 ? h + 0.5 + rnd(-0.06, 0.06) : h + rnd(0.04, 0.12) * (QRandomGenerator::global()->bounded(2) ? 1 : -1);
    const double sat = rnd(0.55, 0.9), bgSat = rnd(0.35, 0.8);
    Theme t;
    t.accent  = hsv(h, sat, rnd(0.82, 1.0));
    t.accent2 = hsv(h2, rnd(0.3, 0.6), 1.0);
    t.bg      = hsv(h, bgSat, rnd(0.04, 0.07));
    t.panel   = hsv(h, bgSat, rnd(0.09, 0.13));
    t.sidebar = hsv(h, bgSat, rnd(0.06, 0.09));
    t.border  = hsv(h, bgSat * 0.8, rnd(0.2, 0.3));
    t.text    = hsv(h, 0.07, 0.96);
    t.muted   = hsv(h, 0.25, 0.66);
    t.radius = keep.radius; t.fontSize = keep.fontSize; t.customQss = keep.customQss;
    return t;
}

} // namespace dk
