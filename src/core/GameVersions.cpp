#include "GameVersions.h"

namespace dk {

static const GameVersion kVersions[] = {
#include "GameVersions.inc"
};

const char* platformName(Platform p) {
    switch (p) {
        case Platform::Launcher: return "Minecraft Launcher";
        case Platform::Store:    return "Windows Store / Xbox App";
        case Platform::Steam:    return "Steam";
        case Platform::Gdk:      return "Xbox App / Microsoft Store (GDK)";
        default:                 return "Unknown";
    }
}

std::optional<GameVersion> identifyVersion(uint64_t moduleSize, int procs) {
    for (const auto& v : kVersions) {
        if (v.moduleSize != moduleSize) continue;
        if (v.procRule == 1 && procs != 1) continue;
        if (v.procRule == 2 && procs <= 1) continue;
        return v;
    }
    return std::nullopt;
}

} // namespace dk
