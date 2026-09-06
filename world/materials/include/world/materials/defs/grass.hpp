#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// The surface skin of gentle, above-beach terrain: one voxel thick at any resolution.
struct Grass {
    static constexpr const char* name = "Grass";
    static constexpr Color albedo{0.23f, 0.48f, 0.13f};
    static constexpr Phase phase = Phase::Solid;
    static constexpr Shading shading = Shading::Lit;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool yields_to_trees = false;
    static constexpr bool overrides_terrain = false;

    [[nodiscard]] static constexpr bool fills(const TerrainQuery& q) noexcept {
        return q.below_surface() && !q.beach && q.grassy && q.depth() < q.voxel_edge;
    }
};

} // namespace world::materials::defs
