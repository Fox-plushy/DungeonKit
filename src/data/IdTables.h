#pragma once
#include <cstddef>
#include <cstdint>

namespace dk {

struct IdName { int32_t id; const char* name; };
struct IdList { const IdName* items; size_t count; };
struct VersionLists { IdList enchants, items, armor; };

// Name tables for a game build (internal build number from GameVersions: 29 = 1.17.0.0).
// Unknown numbers fall back to the newest tables.
VersionLists listsForVersion(int version);
IdList rarityList();
IdList tierList();

} // namespace dk
