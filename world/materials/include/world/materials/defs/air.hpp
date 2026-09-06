#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// The empty voxel. Must be the first component of the registry: id 0 is "nothing here" in the brick
// occupancy mask, the node array (an air box is absent), the chunk palette default, and the shader's
// miss material.
struct Air {
    static constexpr const char* name = "Air";
    static constexpr Color albedo{0.0f, 0.0f, 0.0f}; // never sampled by a real fragment
    static constexpr Phase phase = Phase::Gas;
    static constexpr Shading shading = Shading::Lit;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool yields_to_trees = true;
    static constexpr bool overrides_terrain = false;

    // Above the surface and above the sea.
    [[nodiscard]] static constexpr bool fills(const TerrainQuery& q) noexcept {
        return !q.below_surface() && !q.submerged();
    }
};

} // namespace world::materials::defs
