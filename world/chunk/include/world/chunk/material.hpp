#pragma once

#include <cstdint>

namespace world::chunk {

enum class MaterialID : std::uint8_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Water = 3,
    // Decoration-only materials (TERRAIN_FIXES_BRIEF Group W): used by tree geometry appended to
    // chunk meshes, never written into voxel data.
    Wood = 4,
    Leaves = 5,
};

} // namespace world::chunk
