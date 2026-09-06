#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// Tree trunks. Placed by tree_placement.hpp's implicit shapes, never by the terrain bands; the trunk
// is sunk half a meter into the ground on purpose, so it overrides even solid terrain when
// voxelized.
struct Wood {
    static constexpr const char* name = "Wood";
    static constexpr Color albedo{0.36f, 0.22f, 0.09f};
    static constexpr Phase phase = Phase::Solid;
    static constexpr Shading shading = Shading::Lit;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool yields_to_trees = false;
    static constexpr bool overrides_terrain = true;

    [[nodiscard]] static constexpr bool fills(const TerrainQuery&) noexcept { return false; }
};

} // namespace world::materials::defs
