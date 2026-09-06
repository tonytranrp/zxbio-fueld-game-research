#pragma once

namespace world::materials {

// The constants every terrain band rule shares. Before Group AC these lived as three hand-mirrored
// copies (terrain_fill.cpp, terrain_sampler.hpp, aim_query.cpp) with "update together" comments;
// this is the one place they are written.
struct TerrainBands {
    static constexpr float beach_band = 1.75f;     // surface <= sea + band: a beach column (sand, no grass)
    static constexpr float soil_depth = 3.0f;      // meters of soil under the surface voxel
    static constexpr float grass_max_slope = 1.9f; // steeper columns read as exposed rock

    [[nodiscard]] static constexpr bool is_beach(float surfaceHeight, float seaLevel) noexcept {
        return surfaceHeight <= seaLevel + beach_band;
    }
    [[nodiscard]] static constexpr bool is_grassy(bool beach, float slope) noexcept {
        return !beach && slope <= grass_max_slope;
    }
};

// One voxel of one terrain column, as a material's band predicate sees it. Units are meters; the
// chunk path passes voxel_edge 1 and its own (truncated integer) surface height, the sparse-brick
// path passes sub-meter edges -- the rules are the same function of (depth, edge) either way, which
// is what keeps the two worlds byte-identical at 1 m (test_terrain_sampler.cpp).
struct TerrainQuery {
    float surface_height; // the column's surface height
    float voxel_bottom;   // height of the voxel's bottom face
    float voxel_edge;     // voxel size: the surface band is exactly one voxel thick at any resolution
    float sea_level;
    bool beach;  // TerrainBands::is_beach(...) as the caller computed it
    bool grassy; // TerrainBands::is_grassy(...) as the caller computed it

    [[nodiscard]] constexpr bool below_surface() const noexcept { return voxel_bottom <= surface_height; }
    [[nodiscard]] constexpr float depth() const noexcept { return surface_height - voxel_bottom; }
    [[nodiscard]] constexpr bool submerged() const noexcept { return voxel_bottom <= sea_level; }
};

} // namespace world::materials
