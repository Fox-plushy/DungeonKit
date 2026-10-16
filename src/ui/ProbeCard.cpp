#include "ProbeCard.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include "../core/Injector.h"
#include "../core/Process.h"

namespace dk {

ProbeCard::ProbeCard(const Spec& spec, std::function<Process*()> proc, QWidget* parent) : QFrame(parent), spec_(spec), proc_(std::move(proc)) {
    setObjectName("Card");
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    auto* title = new QLabel(spec_.title);
    title->setObjectName("CardTitle");
    v->addWidget(title);
    auto* hint = new QLabel(spec_.hint);
    hint->setObjectName("Hint");
    hint->setWordWrap(true);
    v->addWidget(hint);

    auto* row = new QHBoxLayout;
    auto* go = new QPushButton(spec_.button);
    auto* refresh = new QPushButton("Refresh log");
    auto* folder = new QPushButton("Open folder");
    QPushButton* close = spec_.closeButton ? new QPushButton("Close game") : nullptr;
    for (auto* b : {go, refresh, folder, close}) { if (b) { b->setCursor(Qt::PointingHandCursor); row->addWidget(b); } }
    row->addStretch(1);
    v->addLayout(row);
    extra_ = new QVBoxLayout;
    extra_->setSpacing(8);
    v->addLayout(extra_);

    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMinimumHeight(180);
    log_->setPlaceholderText("The log shows here after it runs.");
    v->addWidget(log_);

    connect(go, &QPushButton::clicked, this, [this]() { inject(); });
    connect(refresh, &QPushButton::clicked, this, [this]() { showLog(); });
    connect(folder, &QPushButton::clicked, this, [this]() { openFolder(); });
    if (close) connect(close, &QPushButton::clicked, this, [this]() { closeGame(); });
}

void ProbeCard::addExtra(QWidget* w) { extra_->addWidget(w); }

QString ProbeCard::dllDir() const { return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/Probe"); }

QString ProbeCard::logPath() const {
    const QString beside = dllDir() + "/" + spec_.log;
    if (QFileInfo::exists(beside)) return beside;
    const QString tmp = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + spec_.log);
    return QFileInfo::exists(tmp) ? tmp : beside;
}

void ProbeCard::showLog() {
    QFile f(logPath());
    if (!f.open(QIODevice::ReadOnly)) { log_->setPlainText("No log yet."); return; }
    QByteArray all = f.readAll();
    if (all.size() > 20000) all = all.right(20000);                 // the end is the newest
    log_->setPlainText(QString::fromUtf8(all));
    log_->moveCursor(QTextCursor::End);
}

void ProbeCard::inject() {
    Process* p = proc_ ? proc_() : nullptr;
    if (!p || !p->attached() || !p->alive()) { emit message("Start the game first and pick it under Target game."); return; }
    const QString dll = dllDir() + "/" + spec_.dll;
    if (!QFileInfo::exists(dll)) { emit message("The DLL was not found in the Probe folder next to DungeonKit.exe. Build the project again."); return; }
    if (moduleLoaded(p->pid(), spec_.dll.toStdWString().c_str())) { emit message("The DLL is already loaded in the game. Restart the game to run it again."); showLog(); return; }
    std::string err;
    if (!injectDll(p->pid(), QDir::toNativeSeparators(dll).toStdWString(), err)) { emit message(QString::fromStdString(err)); return; }
    if (!moduleLoaded(p->pid(), spec_.dll.toStdWString().c_str())) { emit message("The loader ran, but the DLL is not in the game."); return; }
    emit message("DLL loaded. Wait a few seconds, then read the log.");
    for (int ms : {1500, 4000, 9000}) QTimer::singleShot(ms, this, [this]() { showLog(); });
}

void ProbeCard::openFolder() {
    if (!QFileInfo(dllDir()).isDir()) { emit message("The Probe folder was not found next to DungeonKit.exe."); return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dllDir()));
}

// Ends every running Minecraft Dungeons / Minecraft Dungeons II process by name (Dungeons.exe, Dungeons-Win64-Shipping.exe,
// Dungeons-WinGDK-Shipping.exe), the same way Home > Target game > Close game processes does. That covers every Minecraft
// Dungeons II app (Microsoft Store/Xbox App, Steam, and the Minecraft Launcher) without needing to know which one is running -
// useful here because closing the game normally does not work while the console is injected.
void ProbeCard::closeGame() {
    const auto a = QMessageBox::question(this, "DungeonKit",
        "End every Minecraft Dungeons and Minecraft Dungeons II process? Unsaved game progress will be lost.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (a != QMessageBox::Yes) return;
    int failed = 0;
    const int ended = Process::terminateGameProcesses(failed);
    if (failed > 0) emit message(QString("Ended %1 game process(es), %2 could not be ended. Try running DungeonKit as administrator.").arg(ended).arg(failed));
    else if (ended > 0) emit message(QString("Ended %1 game process(es).").arg(ended));
    else emit message("No game processes were running.");
}

} // namespace dk
