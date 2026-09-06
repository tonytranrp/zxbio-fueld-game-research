#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// The soil band under a non-beach surface voxel.
struct Dirt {
    static constexpr const char* name = "Dirt";
    static constexpr Color albedo{0.44f, 0.28f, 0.14f};
    static constexpr Phase phase = Phase::Solid;
    static constexpr Shading shading = Shading::Lit;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool yields_to_trees = false;
    static constexpr bool overrides_terrain = false;

    [[nodiscard]] static constexpr bool fills(const TerrainQuery& q) noexcept {
        if (!q.below_surface() || q.beach) {
            return false;
        }
        const float depth = q.depth();
        return depth >= q.voxel_edge && depth < TerrainBands::soil_depth + q.voxel_edge;
    }
};

} // namespace world::materials::defs
