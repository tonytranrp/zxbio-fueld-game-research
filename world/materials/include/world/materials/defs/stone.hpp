#pragma once

#include "world/materials/material_def.hpp"

namespace world::materials::defs {

// Bedrock: everything below the soil band, and the exposed surface of a slope too steep for grass.
struct Stone {
    static constexpr const char* name = "Stone";
    static constexpr Color albedo{0.52f, 0.49f, 0.44f};
    static constexpr Phase phase = Phase::Solid;
    static constexpr Shading shading = Shading::Lit;
    static constexpr LiquidPhysics liquid{};
    static constexpr bool yields_to_trees = false;
    static constexpr bool overrides_terrain = false;

    [[nodiscard]] static constexpr bool fills(const TerrainQuery& q) noexcept {
        if (!q.below_surface()) {
            return false;
        }
        const float depth = q.depth();
        const bool steepSurface = !q.beach && depth < q.voxel_edge && !q.grassy;
        const bool belowSoil = depth >= TerrainBands::soil_depth + q.voxel_edge;
        return steepSurface || belowSoil;
    }
};

} // namespace world::materials::defs
