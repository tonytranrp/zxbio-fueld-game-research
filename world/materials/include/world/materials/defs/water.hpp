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
    // Upthrust at >= 1 voxel submersion is 2x gravity -- i.e. a NET +1 g upward when fully under --
    // so the equilibrium floats the feet half a meter under the surface with the eyes above it; the
    // drag damps the bob.
    //
    // 19.62 = 2 x 9.81. This number is a RATIO to gravity dressed as an absolute, and goal 233
    // caught it being one: when gravity moved from -32 to Earth's -9.81 this constant stayed 64,
    // the net upthrust went from +32 to +54.2 m/s^2, and the swimmer was fired out of the water
    // and onto a 0.5 m shore lip within a hundred ticks. `world/player`'s own test pins the
    // relationship (it can see both numbers; this header deliberately cannot), so the pair cannot
    // drift apart again silently.
    //
    // The departure from reality, named: a real human is very nearly neutrally buoyant -- body
    // density ~985 kg/m^3 against water's 1000 gives a net upthrust near 0.015 g, which would take
    // the better part of a minute to surface from. +1 g is a game-scale choice, and it is the SAME
    // choice that shipped before; only its spelling changed.
    static constexpr LiquidPhysics liquid{19.62f, 2.5f, 0.5f};
    static constexpr bool yields_to_trees = true; // a canopy leaning over the shore fills the water
    static constexpr bool overrides_terrain = false;

    // Not terrain, but under the sea: water never overrides solid ground (fill_terrain's rule).
    [[nodiscard]] static constexpr bool fills(const TerrainQuery& q) noexcept {
        return !q.below_surface() && q.submerged();
    }
};

} // namespace world::materials::defs
