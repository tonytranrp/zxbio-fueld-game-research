#pragma once

// The template half of grass_cover.hpp. Separate file rather than inline in the header because the
// placement rule is thirty lines of arithmetic and the header is the contract.

#include <cmath>

namespace world::generation {

namespace detail {

// splitmix32, the same shape `tree_placement.cpp` uses. Stable across compilers -- which is what
// makes a tuft's id the SAME number in the voxelizer, in a raster overlay and in a test (goal 340).
[[nodiscard]] inline std::uint32_t grass_hash(std::uint32_t x) noexcept {
    x += 0x9e3779b9u;
    x = (x ^ (x >> 16)) * 0x85ebca6bu;
    x = (x ^ (x >> 13)) * 0xc2b2ae35u;
    return x ^ (x >> 16);
}

[[nodiscard]] inline std::uint32_t grass_cell_id(int seed, std::int32_t cx, std::int32_t cz) noexcept {
    return grass_hash(static_cast<std::uint32_t>(seed) ^ grass_hash(static_cast<std::uint32_t>(cx)) ^
                      (grass_hash(static_cast<std::uint32_t>(cz)) << 1u));
}

[[nodiscard]] inline float grass_unit(std::uint32_t h) noexcept {
    return static_cast<float>(h & 0xFFFFFFu) / 16777215.0f;
}

} // namespace detail

template <typename HeightFn, typename SlopeFn, typename BiomeFn>
std::vector<GrassTuft> grass_tufts_in_patch(int seed, glm::vec2 origin, const GrassCoverParams& params,
                                            HeightFn&& height_at, SlopeFn&& slope_at, BiomeFn&& biome_at) {
    std::vector<GrassTuft> out;
    // The biome is sampled ONCE per patch, at its centre. A 4 m patch is far below the macro
    // field's 16 m cell, so sampling per tuft would cost a lookup each to get the same answer --
    // and it would put a biome boundary inside a patch, which is a seam the tiers would then have
    // to agree about across (goal 340).
    const glm::vec2 centre = origin + glm::vec2{0.5f * params.patch_edge_m};
    const field::Biome biome = biome_at(centre.x, centre.y);
    const float perM2 = plants_per_m2(biome) / std::max(params.plants_per_tuft, 1.0f);
    if (perM2 <= 0.0f) {
        return out;
    }

    // A JITTERED GRID, not rejection sampling: the cell a tuft lives in is what gives it a stable
    // identity, and the identity is what the three tiers have to agree on. Cell edge from the
    // density, so one tuft per cell reproduces it exactly.
    const float cell = 1.0f / std::sqrt(perM2);
    const auto cx0 = static_cast<std::int32_t>(std::floor(origin.x / cell));
    const auto cx1 = static_cast<std::int32_t>(std::floor((origin.x + params.patch_edge_m) / cell));
    const auto cz0 = static_cast<std::int32_t>(std::floor(origin.y / cell));
    const auto cz1 = static_cast<std::int32_t>(std::floor((origin.y + params.patch_edge_m) / cell));
    out.reserve(static_cast<std::size_t>((cx1 - cx0 + 1) * (cz1 - cz0 + 1)));

    for (std::int32_t cz = cz0; cz <= cz1; ++cz) {
        for (std::int32_t cx = cx0; cx <= cx1; ++cx) {
            const std::uint32_t id = detail::grass_cell_id(seed, cx, cz);
            const float jx = detail::grass_unit(id);
            const float jz = detail::grass_unit(detail::grass_hash(id ^ 0x51u));
            const float x = (static_cast<float>(cx) + 0.15f + 0.70f * jx) * cell;
            const float z = (static_cast<float>(cz) + 0.15f + 0.70f * jz) * cell;
            // Half-open on both axes so a tuft belongs to exactly one patch and no boundary tuft is
            // placed twice or dropped.
            if (x < origin.x || x >= origin.x + params.patch_edge_m || z < origin.y ||
                z >= origin.y + params.patch_edge_m) {
                continue;
            }
            if (slope_at(x, z) > params.max_slope) {
                continue;
            }
            const float ground = height_at(x, z);
            if (ground <= 0.05f) {
                continue; // no lawn below the waterline
            }
            GrassTuft tuft;
            tuft.id = id;
            tuft.base = glm::vec3{x, ground, z};
            // +/-25% height variation and a lean of up to ~11 degrees, both from the tuft's own id,
            // so a field is not a bed of identical spikes and every tier can reproduce it.
            tuft.height = params.blade_height_m * (0.75f + 0.5f * detail::grass_unit(detail::grass_hash(id ^ 0xA3u)));
            tuft.lean_x = 0.2f * (detail::grass_unit(detail::grass_hash(id ^ 0x7Du)) * 2.0f - 1.0f);
            tuft.lean_z = 0.2f * (detail::grass_unit(detail::grass_hash(id ^ 0x1Fu)) * 2.0f - 1.0f);
            out.push_back(tuft);
        }
    }
    return out;
}

} // namespace world::generation
