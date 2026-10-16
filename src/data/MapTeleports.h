#pragma once
#include <cstddef>

namespace dk {

struct TeleportSpot { const char* name; float x, y, z; };
struct TeleportGroup { const char* name; const TeleportSpot* spots; size_t count; };

// Mission / area teleport points from the original table.
const TeleportGroup* teleportGroups(size_t& count);

} // namespace dk
