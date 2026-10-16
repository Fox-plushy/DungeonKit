#pragma once
#include <QList>
#include <QString>
#include <QStringList>

namespace dk {
namespace SaveBackup {

enum Game { Dungeons = 0, Dungeons2 = 1 };
QString gameName(int game);                // "Minecraft Dungeons" / "Minecraft Dungeons II"

// One folder that can be backed up. `game` says which game it belongs to; `mods` is true for the ~mods folder.
struct Source { QString label; QString path; int game = Dungeons; bool mods = false; };

// The ~mods folders (found by the caller, because they depend on the install). Empty = not known.
void setModsFolders(const QString& dungeons, const QString& dungeons2);

// Every place that is looked at, including ones that do not exist on this computer (for the debug report).
struct Candidate { QString label; QString path; bool exists = false; int game = Dungeons; bool mods = false; };
QList<Candidate> candidates();

// The folders that exist on this computer. The ~mods folders are left out when includeMods is false.
QList<Source> sources(bool includeMods = true);

QString savedPath(int game = Dungeons);    // folder picked by hand ("" if none)
void savePath(const QString& dir, int game = Dungeons);

// <Desktop>/DungeonKit Backups
QString backupRoot();

struct Info { QString appVersion; QString gameVersion[2]; };   // written into each backup so it explains itself

struct Result {
    bool ok = false;
    QStringList folders;  // the new backup folders (one per game)
    int files = 0;
    qint64 bytes = 0;
    QString error;
};

// Copies the sources into <backupRoot>/<Game> - <date time> - v<game version>/<label>/ , one folder per game.
// Originals are only read.
Result backup(const QList<Source>& from, const Info& info, QStringList* log = nullptr);

int existingBackups();                     // how many backup folders are already on the desktop

} // namespace SaveBackup
} // namespace dk
