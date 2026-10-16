#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace dk {

enum class Platform { Unknown = 0, Launcher = 1, Store = 2, Steam = 3, Gdk = 4 };

struct GameVersion {
    uint64_t moduleSize;
    Platform platform;
    int      number;        // internal build number used by the old table (29 = 1.17.0.0)
    const char* name;       // "1.17.0.0"
    int      procRule = 0;  // 0 any, 1 exactly one Dungeons process running, 2 more than one
};

const char* platformName(Platform p);

// Look up by main-module size (what the cheat table used getModuleSize() for).
std::optional<GameVersion> identifyVersion(uint64_t moduleSize, int runningDungeonsProcesses);

} // namespace dk
