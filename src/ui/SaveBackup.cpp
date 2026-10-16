#include "SaveBackup.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace dk {
namespace SaveBackup {

static QString g_mods[2];

QString gameName(int game) { return game == Dungeons2 ? "Minecraft Dungeons II" : "Minecraft Dungeons"; }
void setModsFolders(const QString& dungeons, const QString& dungeons2) { g_mods[0] = dungeons; g_mods[1] = dungeons2; }

QString savedPath(int game) { return QSettings().value(game == Dungeons2 ? "paths/saves2" : "paths/saves").toString(); }
void savePath(const QString& dir, int game) { QSettings().setValue(game == Dungeons2 ? "paths/saves2" : "paths/saves", dir); }

QList<Candidate> candidates() {
    QList<Candidate> out;
    auto add = [&](int game, const QString& label, const QString& path, bool mods = false) {
        const QString p = QDir::cleanPath(path);
        if (p.isEmpty() || p == ".") return;
        for (const Candidate& s : out) if (s.path.compare(p, Qt::CaseInsensitive) == 0) return;
        out.append({label, p, QDir(p).exists(), game, mods});
    };
    const QString home = QDir::homePath();
    const QString local = qEnvironmentVariable("LOCALAPPDATA");

    // ---- Minecraft Dungeons
    add(Dungeons, "Saves - Launcher or Steam", home + "/Saved Games/Mojang Studios/Dungeons");
    if (!local.isEmpty()) add(Dungeons, "Saves - Xbox app", local + "/Packages/Microsoft.Lovika_8wekyb3d8bbwe/SystemAppData/wgs");
    if (!savedPath(Dungeons).isEmpty()) add(Dungeons, "Saves - folder you picked", savedPath(Dungeons));
    if (!g_mods[0].isEmpty()) add(Dungeons, "Mods (~mods)", g_mods[0], true);

    // ---- Minecraft Dungeons II (the usual places for a game of this kind; whatever exists is used)
    {
        const QDir mojang(home + "/Saved Games/Mojang Studios");
        for (const QString& d : mojang.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            if (d.compare("Dungeons", Qt::CaseInsensitive) != 0) add(Dungeons2, "Saves - Saved Games", mojang.filePath(d));
        if (!QDir(mojang.filePath("Dungeons2")).exists()) add(Dungeons2, "Saves - Saved Games", mojang.filePath("Dungeons2"));
    }
    if (!local.isEmpty()) {
        for (const char* n : {"Dungeons", "Dungeons2", "MinecraftDungeons2", "Spicewood"})
            add(Dungeons2, "Saves - AppData", local + "/" + n + "/Saved/SaveGames");
        const QDir pk(local + "/Packages");
        for (const QString& d : pk.entryList(QStringList() << "*Dungeons*" << "*Mojang*" << "*Minecraft*", QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (d.startsWith("Microsoft.Lovika", Qt::CaseInsensitive)) continue;        // that one is the first game
            add(Dungeons2, "Saves - Xbox app", pk.filePath(d) + "/SystemAppData/wgs");
            add(Dungeons2, "Saves - Xbox app", pk.filePath(d) + "/LocalState");
        }
    }
    if (!savedPath(Dungeons2).isEmpty()) add(Dungeons2, "Saves - folder you picked", savedPath(Dungeons2));
    if (!g_mods[1].isEmpty()) add(Dungeons2, "Mods (~mods)", g_mods[1], true);
    return out;
}

QList<Source> sources(bool includeMods) {
    QList<Source> out;
    for (const Candidate& c : candidates())
        if (c.exists && (includeMods || !c.mods)) out.append({c.label, c.path, c.game, c.mods});
    return out;
}

QString backupRoot() {
    QString desk = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desk.isEmpty()) desk = QDir::homePath() + "/Desktop";
    return QDir(desk).filePath("DungeonKit Backups");
}

static QString safeName(QString s) {
    for (QChar& c : s) if (QString("<>:\"/\\|?*").contains(c)) c = '-';
    return s.trimmed();
}

Result backup(const QList<Source>& from, const Info& info, QStringList* log) {
    Result r;
    auto L = [&](const QString& line) { if (log) log->append(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + line); };
    L("Backup started");
    if (from.isEmpty()) { r.error = "No save folder to back up."; L("Stopped: no save folder to back up"); return r; }
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH.mm");
    for (int game : {(int)Dungeons, (int)Dungeons2}) {
        QList<Source> mine;
        for (const Source& s : from) if (s.game == game) mine.append(s);
        if (mine.isEmpty()) continue;
        QString name = gameName(game) + " - " + stamp;
        if (!info.gameVersion[game].isEmpty()) name += " - v" + safeName(info.gameVersion[game]);
        QString dest = QDir(backupRoot()).filePath(name);
        for (int n = 2; QDir(dest).exists() && n < 100; ++n) dest = QDir(backupRoot()).filePath(name + " (" + QString::number(n) + ")");   // two backups in one minute
        L("WRITE  create folder  " + QDir::toNativeSeparators(dest));
        if (!QDir().mkpath(dest)) { r.error = "Could not create the backup folder on your Desktop."; L("FAILED  could not create that folder"); return r; }
        r.folders << dest;
        int gameFiles = 0;
        QStringList summary;
        for (const Source& s : mine) {
            const QDir src(s.path);
            const QString base = QDir(dest).filePath(safeName(s.label));
            L("READ   folder  " + QDir::toNativeSeparators(s.path) + "   (" + s.label + ")");
            QDirIterator it(s.path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
            int here = 0;
            while (it.hasNext()) {
                const QString f = it.next();
                const QString rel = src.relativeFilePath(f);
                const QString to = QDir(base).filePath(rel);
                if (!QDir().mkpath(QFileInfo(to).absolutePath())) {
                    r.error = "Could not create a folder inside the backup.";
                    L("FAILED  could not create  " + QDir::toNativeSeparators(QFileInfo(to).absolutePath()));
                    return r;
                }
                if (!QFile::copy(f, to)) {
                    r.error = "Could not copy " + QDir::toNativeSeparators(f);
                    L("FAILED  copy  " + QDir::toNativeSeparators(f));
                    return r;
                }
                const qint64 sz = QFileInfo(f).size();
                L("COPY   " + QDir::toNativeSeparators(f) + "  ->  " + QDir::toNativeSeparators(to) + "   (" + QString::number(sz) + " bytes)");
                r.files++;
                gameFiles++;
                here++;
                r.bytes += sz;
            }
            summary << QString("%1: %2 file(s)  (from %3)").arg(safeName(s.label)).arg(here).arg(QDir::toNativeSeparators(s.path));
            L(QString("READ   done, %1 file(s) from this folder; the originals were not changed").arg(here));
        }
        // a short note inside the backup that says what it is
        QFile note(QDir(dest).filePath("Backup info.txt"));
        if (note.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream t(&note);
            t << "Game:          " << gameName(game) << "\n"
              << "Game version:  " << (info.gameVersion[game].isEmpty() ? QString("unknown (the game was not running)") : info.gameVersion[game]) << "\n"
              << "Backed up:     " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n"
              << "DungeonKit:    " << info.appVersion << "\n\n"
              << "What is inside (one folder each):\n";
            for (const QString& l : summary) t << "  " << l << "\n";
            t << "\nTo restore: close the game, then copy the files from the matching folder back to the original location.\n";
        }
        L(QString("Folder done: %1 file(s) for %2").arg(gameFiles).arg(gameName(game)));
    }
    if (r.files == 0) { r.error = "The save folders were empty, so nothing was backed up."; L("Stopped: the folders were empty"); return r; }
    r.ok = true;
    L(QString("Backup finished: %1 file(s), %2 bytes").arg(r.files).arg(r.bytes));
    return r;
}

int existingBackups() {
    const QDir d(backupRoot());
    return d.exists() ? d.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() : 0;
}

} // namespace SaveBackup
} // namespace dk
