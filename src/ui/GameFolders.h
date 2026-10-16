#pragma once
#include <QString>

namespace dk {
namespace GameFolders {

struct Result {
    QString paks;     // ".../Dungeons/Content/Paks" (empty if not found)
    QString how;      // where it was found: "from the running game", "saved", "default location"...
};

// Looks for the game's Paks folder: a folder you picked earlier, then next to the running game's exe,
// then the usual Minecraft Launcher / Steam / Xbox install locations on every drive.
// game 0 = Minecraft Dungeons, 1 = Minecraft Dungeons II (the picked folder is only used for game 0).
Result find(const QString& exePath, int game = 0);

QString modsDir(const QString& paks);                 // <Paks>/~mods
// Turns whatever the user pasted (Paks folder, ~mods folder, or the game's install folder) into the Paks folder. "" if none.
QString resolve(const QString& pasted);
int countPaks(const QString& dir, bool recursive);    // number of .pak files

QString savedPath();
void savePath(const QString& paks);                   // remember a folder picked by hand
bool looksLikePaks(const QString& dir);               // contains at least one .pak

}
} // namespace dk
