#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// The shoreline: surface AND soil band of every column whose surface is within the beach band of
// sea level (goal 81's design -- sand runs the whole soil depth, so a beach never shows dirt).
struct Sand {
    static constexpr const char* name = "Sand";
    static constexpr Color albedo{0.78f, 0.70f, 0.46f};
    static constexpr Phase phase = Phase::Solid;
    static constexpr Shading shading = Shading::Lit;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool yields_to_trees = false;
    static constexpr bool overrides_terrain = false;

    [[nodiscard]] static constexpr bool fills(const TerrainQuery& q) noexcept {
        return q.below_surface() && q.beach && q.depth() < TerrainBands::soil_depth + q.voxel_edge;
    }
};

} // namespace world::materials::defs
