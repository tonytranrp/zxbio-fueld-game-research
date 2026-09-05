#pragma once

#include <array>
#include <cstddef>

#include "world/chunk/material.hpp"

namespace world::chunk {

// Group P (Voxel Representation Redesign, research/voxel-representation-redesign.md §6): every
// real property a material has, in one place. A constexpr table, not a runtime registry with
// virtual dispatch -- the material set is small and known at compile time, and this project has
// no mod/plugin content pipeline (§6.3), so a Block class hierarchy would buy indirection nothing
// here needs. Extend this struct when a new property is needed; never add a new scattered
// `== MaterialID::X` comparison elsewhere instead.
struct BlockProperties {
    float color[3];       // linear-space albedo; pso_terrain.cpp derives its GPU palette buffer from
                          // this directly (single source of truth -- see that file).
    bool is_solid;        // participates in ground collision/the walk-mode clamp; false for Air AND
                          // Water (water is handled by buoyancy, not the solid-ground path).
    bool is_liquid;       // buoyancy applies (swimming, goal 79), not ground-clamp.
    bool supports_growth; // reserved: intended as a future tree/decoration placement gate. NOT
                          // wired to any consumer yet -- direct reading of tree_decoration.cpp
                          // found placement is driven purely by HeightmapGenerator (height/slope),
                          // which has no voxel/material access at all today, so there is no
                          // existing "supports_growth"-shaped comparison to migrate. The value
                          // below is a real, defensible guess (matches terrain_fill.cpp's own
                          // grassy/beach split), kept honest here rather than silently invented.
    float hardness;       // reserved: mining/destruction, not built yet.
};

// One row per MaterialID, in enum order -- kept in sync by the static_assert below, not by
// comment discipline. Air's color is never sampled by a real fragment (index 0 is always void)
// but keeps every table direct-indexed by MaterialID with no offset arithmetic anywhere.
inline constexpr std::array<BlockProperties, kMaterialCount> kBlockTable = {{
    /* Air    */ {{0.00f, 0.00f, 0.00f}, false, false, false, 0.0f},
    /* Stone  */ {{0.52f, 0.49f, 0.44f}, true, false, false, 1.5f},
    /* Dirt   */ {{0.44f, 0.28f, 0.14f}, true, false, true, 0.5f},
    /* Water  */ {{0.09f, 0.33f, 0.58f}, false, true, false, 0.0f},
    /* Wood   */ {{0.36f, 0.22f, 0.09f}, true, false, false, 1.0f},
    /* Leaves */ {{0.20f, 0.50f, 0.12f}, false, false, false, 0.2f},
    /* Sand   */ {{0.78f, 0.70f, 0.46f}, true, false, false, 0.4f},
    /* Grass  */ {{0.23f, 0.48f, 0.13f}, true, false, true, 0.6f},
}};
static_assert(kBlockTable.size() == kMaterialCount,
              "every MaterialID needs a kBlockTable row -- add one when the enum grows");

constexpr const BlockProperties& properties_of(MaterialID id) {
    return kBlockTable[static_cast<std::size_t>(id)];
}

// Derived, not stored: "does this voxel need a mesh boundary against open air" is exactly
// solid-or-liquid -- Air is the only material that is neither. Replaces the old scattered
// `m != MaterialID::Air` occupancy idiom that used to live as a local `is_solid()` helper inside
// mesh_extractor.cpp; that name was already a slight misnomer (it treated Water as "solid" too,
// which is correct for meshing -- water needs a surface against air -- but wrong for gameplay
// collision, where Water must NOT be solid). Two real, different questions now have two honestly
// named answers instead of one overloaded one.
constexpr bool is_occupied(MaterialID id) {
    return properties_of(id).is_solid || properties_of(id).is_liquid;
}

} // namespace world::chunk
