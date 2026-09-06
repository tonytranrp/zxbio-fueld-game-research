#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// Tree canopies. Placed by tree_placement.hpp's lobes, never by the terrain bands. Foliage: a ray
// hits it and the sparse-brick path stores it, but a body walks through it and it is not a floor.
// The mesh path sways it in the wind (Shading::Foliage); the svo path shades it lit.
struct Leaves {
    static constexpr const char* name = "Leaves";
    static constexpr Color albedo{0.20f, 0.50f, 0.12f};
    static constexpr Phase phase = Phase::Foliage;
    static constexpr Shading shading = Shading::Foliage;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool yields_to_trees = false;   // a second canopy does not carve the first
    static constexpr bool overrides_terrain = false; // terrain wins: no carving grass out of a hillside

    [[nodiscard]] static constexpr bool fills(const TerrainQuery&) noexcept { return false; }
};

} // namespace world::materials::defs
