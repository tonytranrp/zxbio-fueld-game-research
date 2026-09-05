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
    // Group M (goals 81/93): real voxel materials appended AFTER the decoration pair so every
    // baked material ID (tree Wood/Leaves, the shader's water==3 / leaves==5 tests) stays valid.
    Sand = 6,  // shoreline band around sea level
    Grass = 7, // surface skin on gentle above-sea terrain
};

} // namespace world::chunk
