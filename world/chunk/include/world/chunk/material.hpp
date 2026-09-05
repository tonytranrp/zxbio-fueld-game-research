#pragma once

#include <cstddef>
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

// The one place this count is computed -- block_type.hpp's kBlockTable and pso_terrain.cpp's GPU
// palette buffer both size themselves from this instead of each hand-tracking the enum's high
// value (Group P, voxel-representation-redesign.md §6). Appending a material only means adding an
// enum value here and a kBlockTable row in block_type.hpp; this updates itself.
inline constexpr std::size_t kMaterialCount = static_cast<std::size_t>(MaterialID::Grass) + 1;

} // namespace world::chunk
