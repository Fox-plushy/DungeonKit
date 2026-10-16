#include "GameFolders.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace dk {
namespace GameFolders {

bool looksLikePaks(const QString& dir) {
    QDir d(dir);
    return d.exists() && !d.entryList(QStringList() << "*.pak", QDir::Files).isEmpty();
}

QString savedPath() { return QSettings().value("paths/paks").toString(); }
void savePath(const QString& paks) { QSettings().setValue("paths/paks", paks); }

int countPaks(const QString& dir, bool recursive) {
    int n = 0;
    QDirIterator it(dir, QStringList() << "*.pak", QDir::Files,
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while (it.hasNext()) { it.next(); ++n; }
    return n;
}

QString modsDir(const QString& paks) { return QDir(paks).filePath("~mods"); }

// Folders (relative to some install root) where the Paks directory can sit.
static const char* kRel[] = {"Content/Paks", "Dungeons/Content/Paks", "Content/Dungeons/Content/Paks"};

static QString checkRoot(const QString& root) {
    for (const char* r : kRel) {
        const QString p = QDir(root).filePath(QString::fromLatin1(r));
        if (looksLikePaks(p)) return QDir::cleanPath(p);
    }
    return QString();
}

QString resolve(const QString& pasted) {
    QString s = pasted.trimmed();
    if (s.size() >= 2 && s.startsWith('"') && s.endsWith('"')) s = s.mid(1, s.size() - 2);   // "Copy as path"
    s = QDir::cleanPath(s.trimmed());
    if (s.isEmpty() || !QDir(s).exists()) return QString();
    if (QFileInfo(s).fileName().compare("~mods", Qt::CaseInsensitive) == 0) s = QFileInfo(s).dir().absolutePath();
    if (looksLikePaks(s)) return QDir::cleanPath(s);
    return checkRoot(s);
}

Result find(const QString& exePath, int game) {
    // 1. folder picked by hand
    const QString saved = savedPath();
    if (game == 0 && !saved.isEmpty() && looksLikePaks(saved)) return {QDir::cleanPath(saved), "folder you picked"};

    // 2. walk up from the running game's exe
    if (!exePath.isEmpty()) {
        QDir d = QFileInfo(exePath).dir();
        for (int i = 0; i < 9; ++i) {
            const QString p = checkRoot(d.absolutePath());
            if (!p.isEmpty()) return {p, "found from the running game"};
            if (!d.cdUp()) break;
        }
    }

    // 3. usual install locations
    QStringList roots;
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty() && game == 0) roots << appData + "/.minecraft_dungeons";     // Minecraft Launcher
    for (const QFileInfo& drive : QDir::drives()) {
        const QString r = drive.absoluteFilePath();                                     // "C:/"
        if (game == 1) {
            roots << r + "XboxGames/Minecraft Dungeons II"
                  << r + "Program Files (x86)/Steam/steamapps/common/Minecraft Dungeons II"
                  << r + "Program Files/Steam/steamapps/common/Minecraft Dungeons II"
                  << r + "SteamLibrary/steamapps/common/Minecraft Dungeons II"
                  << r + "Games/Minecraft Dungeons II";
            continue;
        }
        roots << r + "XboxGames/Minecraft Dungeons"
              << r + "Program Files (x86)/Steam/steamapps/common/MinecraftDungeons"
              << r + "Program Files/Steam/steamapps/common/MinecraftDungeons"
              << r + "SteamLibrary/steamapps/common/MinecraftDungeons"
              << r + "Steam/steamapps/common/MinecraftDungeons"
              << r + "Program Files (x86)/Minecraft Dungeons"
              << r + "Games/Minecraft Dungeons";
    }
    for (const QString& root : roots) {
        const QString p = checkRoot(root);
        if (!p.isEmpty()) return {p, "found at a usual install location"};
    }
    return {};
}

}
} // namespace dk
