#include "Splash.h"

#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QFontMetrics>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QScreen>
#include <QShowEvent>
#include <algorithm>
#include <cmath>
#include "../Version.h"
#include "Logo.h"
#include "I18n.h"
#include "TitleBar.h"

namespace dk {

namespace {
struct Step { const char* label; const char* tag; double weight; };
const Step kSteps[] = {
    {"Searching Steam libraries",     "Steam",    0.24},
    {"Searching Xbox app folders",    "Xbox",     0.24},
    {"Searching Minecraft Launcher",  "Launcher", 0.20},
    {"Locating save folders",         "Saves",    0.24},
    {"Done",                          "Done",     0.08},
};
// folders shown while "searching" (the screen is cosmetic; the real search is on the Home page)
const char* const kPaths[][5] = {
    {"C:\\Program Files (x86)\\Steam\\steamapps\\common\\MinecraftDungeons",
     "C:\\Program Files\\Steam\\steamapps\\common\\MinecraftDungeons",
     "D:\\SteamLibrary\\steamapps\\common\\MinecraftDungeons",
     "E:\\SteamLibrary\\steamapps\\common\\MinecraftDungeons",
     "C:\\Program Files (x86)\\Steam\\steamapps\\libraryfolders.vdf"},
    {"C:\\XboxGames\\Minecraft Dungeons\\Content",
     "D:\\XboxGames\\Minecraft Dungeons\\Content",
     "C:\\Program Files\\WindowsApps\\Microsoft.Lovika_*",
     "E:\\XboxGames\\Minecraft Dungeons\\Content",
     "C:\\XboxGames\\Minecraft Dungeons II\\Content"},
    {"C:\\Program Files (x86)\\Minecraft Launcher",
     "C:\\Program Files (x86)\\Minecraft Launcher\\game",
     "%APPDATA%\\.minecraft_dungeons",
     "%LOCALAPPDATA%\\Packages\\Microsoft.4297127D64EC6_*",
     "C:\\Program Files\\Minecraft Launcher"},
    {"%LOCALAPPDATA%\\Dungeons\\Saved\\SaveGames",
     "%USERPROFILE%\\Saved Games\\Mojang Studios\\Dungeons",
     "%LOCALAPPDATA%\\Dungeons\\Saved\\Config\\WindowsNoEditor",
     "%USERPROFILE%\\Documents\\My Games\\Dungeons",
     "%LOCALAPPDATA%\\Dungeons\\Saved\\Logs"},
};
constexpr int kStepCount = 5;
}

SplashDialog::SplashDialog(const Theme& theme, QWidget* parent) : QDialog(parent), t_(theme) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent;");
    setModal(true);
    setFixedSize(540, 330);
    logo_ = Logo::pixmap(112, t_.accent, Logo::recolorEnabled());
    totalMs_ = 5000 + (int)QRandomGenerator::global()->bounded(5001);      // 5 - 10 seconds
    connect(&timer_, &QTimer::timeout, this, [this]() { tick(); });
}

void SplashDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    if (QScreen* s = QGuiApplication::primaryScreen())
        move(s->availableGeometry().center() - rect().center());
    clock_.start();
    timer_.start(16);
}

double SplashDialog::progress() const {
    return std::min(1.0, clock_.elapsed() / double(totalMs_));
}

void SplashDialog::tick() {
    if (!finishing_ && progress() >= 1.0) {
        finishing_ = true;
        QTimer::singleShot(600, this, [this]() { timer_.stop(); accept(); });   // hold on "Done" briefly
    }
    update();
}

void SplashDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
    const double r = std::max(6, t_.radius + 6);

    // card
    QPainterPath card;
    card.addRoundedRect(box, r, r);
    p.fillPath(card, t_.bg);
    QLinearGradient sheen(box.topLeft(), box.bottomRight());
    sheen.setColorAt(0, QColor(t_.accent.red(), t_.accent.green(), t_.accent.blue(), 34));
    sheen.setColorAt(1, QColor(0, 0, 0, 0));
    p.fillPath(card, sheen);
    p.setPen(QPen(t_.border, 1.5));
    p.drawPath(card);

    // overall progress -> which step we are in
    const double prog = finishing_ ? 1.0 : progress();
    double acc = 0;
    int cur = kStepCount - 1;
    for (int i = 0; i < kStepCount; ++i) {
        if (prog < acc + kSteps[i].weight || i == kStepCount - 1) { cur = i; break; }
        acc += kSteps[i].weight;
    }
    if (prog >= 1.0) cur = kStepCount;                              // everything finished

    // glow + spinning logo
    const QPointF c(width() / 2.0, 92);
    QRadialGradient glow(c, 90);
    glow.setColorAt(0, QColor(t_.accent.red(), t_.accent.green(), t_.accent.blue(), 90));
    glow.setColorAt(1, QColor(t_.accent.red(), t_.accent.green(), t_.accent.blue(), 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(c, 90, 90);
    p.save();
    p.translate(c);
    p.rotate(std::fmod(clock_.elapsed() * 0.14, 360.0));
    p.drawPixmap(QRectF(-56, -56, 112, 112), logo_, QRectF(logo_.rect()));
    p.restore();

    // title
    QFont tf("Segoe UI", 16, QFont::Bold);
    p.setFont(tf);
    p.setPen(t_.text);
    p.drawText(QRectF(0, 152, width(), 28), Qt::AlignCenter, QString::fromLatin1(DK_APP_NAME "  v" DK_APP_VERSION));

    // current step text with animated dots
    QFont sf("Segoe UI", 10);
    p.setFont(sf);
    QString label = cur >= kStepCount ? I18n::tr(kSteps[kStepCount - 1].label)
                                      : I18n::tr(kSteps[cur].label);
    if (cur < kStepCount - 1) label += QString(int(clock_.elapsed() / 350) % 4, QChar('.'));
    p.setPen(t_.accent2);
    p.drawText(QRectF(50, 196, 380, 20), Qt::AlignLeft | Qt::AlignVCenter, label);
    p.setPen(t_.muted);
    p.drawText(QRectF(width() - 130, 196, 80, 20), Qt::AlignRight | Qt::AlignVCenter,
               QString("%1%").arg(int(prog * 100)));

    // progress bar
    const QRectF bar(50, 222, width() - 100, 8);
    p.setPen(Qt::NoPen);
    p.setBrush(t_.panel);
    p.drawRoundedRect(bar, 4, 4);
    if (prog > 0) {
        QRectF fill = bar;
        fill.setWidth(std::max(8.0, bar.width() * prog));
        QLinearGradient g(fill.topLeft(), fill.topRight());
        g.setColorAt(0, t_.accent);
        g.setColorAt(1, t_.accent2);
        p.setBrush(g);
        p.drawRoundedRect(fill, 4, 4);
    }

    // step chips
    QFont cf("Segoe UI", 8);
    p.setFont(cf);
    const double slot = (width() - 100) / double(kStepCount);
    for (int i = 0; i < kStepCount; ++i) {
        const double x = 50 + slot * i;
        const QPointF dot(x + 7, 262);
        const bool done = i < cur || cur >= kStepCount;
        const bool now = i == cur;
        if (done) {
            p.setPen(Qt::NoPen);
            p.setBrush(t_.accent);
            p.drawEllipse(dot, 6, 6);
            p.setPen(QPen(t_.bg, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(QPointF(dot.x() - 3, dot.y()), QPointF(dot.x() - 0.8, dot.y() + 2.4));
            p.drawLine(QPointF(dot.x() - 0.8, dot.y() + 2.4), QPointF(dot.x() + 3, dot.y() - 2.6));
        } else {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(now ? t_.accent2 : t_.border, 1.6));
            p.drawEllipse(dot, 5.5, 5.5);
            if (now) {
                p.setPen(Qt::NoPen);
                p.setBrush(t_.accent2);
                p.drawEllipse(dot, 2.2 + 0.8 * std::sin(clock_.elapsed() / 120.0), 2.2 + 0.8 * std::sin(clock_.elapsed() / 120.0));
            }
        }
        p.setPen(done ? t_.text : now ? t_.accent2 : t_.muted);
        p.drawText(QRectF(x + 18, 252, slot - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, I18n::tr(kSteps[i].tag));
    }

    // the folder being "checked" right now
    if (cur < kStepCount - 1) {
        const int pi = int(clock_.elapsed() / 260) % 5;
        QFont mf("Consolas", 8);
        p.setFont(mf);
        p.setPen(t_.muted);
        const QString path = QString::fromUtf8(kPaths[cur][pi]);
        p.drawText(QRectF(50, 288, width() - 100, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(mf).elidedText(I18n::tr("Checking") + " " + path, Qt::ElideMiddle, int(width() - 100)));
    } else {
        p.setFont(QFont("Segoe UI", 8));
        p.setPen(t_.accent2);
        p.drawText(QRectF(50, 288, width() - 100, 18), Qt::AlignLeft | Qt::AlignVCenter, I18n::tr("Ready. Pick your game on the Home page."));
    }
}

} // namespace dk
