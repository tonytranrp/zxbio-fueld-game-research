#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// The sea: every voxel at or below sea level that the terrain does not fill. The only liquid, so it
// is also where the swim physics live (goal 79). The albedo is the `material` debug view's color;
// both renderers replace the lit albedo with their own water shading (Shading::Water).
struct Water {
    static constexpr const char* name = "Water";
    static constexpr Color albedo{0.09f, 0.33f, 0.58f};
    static constexpr Phase phase = Phase::Liquid;
    static constexpr Shading shading = Shading::Water;
    // Upthrust at >= 1 voxel submersion is 2x gravity, so the equilibrium floats the feet half a
    // meter under the surface with the eyes above it; the drag damps the bob.
    static constexpr LiquidPhysics liquid{64.0f, 2.5f, 0.5f};
    static constexpr bool yields_to_trees = true; // a canopy leaning over the shore fills the water
    static constexpr bool overrides_terrain = false;

    // Not terrain, but under the sea: water never overrides solid ground (fill_terrain's rule).
    [[nodiscard]] static constexpr bool fills(const TerrainQuery& q) noexcept {
        return !q.below_surface() && q.submerged();
    }
};

} // namespace world::materials::defs
