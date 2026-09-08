#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "engine/core/math.hpp"
#include "world/generation/field/biome.hpp"
#include "world/generation/tree_volume.hpp"

namespace world::generation {

// Ground cover as VOXELS (Prompt 007 goal 338 = docs/goals.md goal 193).
//
// THIS REOPENS A DECIDED-AGAINST GOAL, and it is worth saying so rather than quietly contradicting
// the list. Goal 40 -- "grass ground-cover geometry" -- was closed as "needs instancing+textures".
// That verdict was about a RASTER overlay, and it was right about one: goal 339 is that overlay and
// it does need both. What is new is `research/grass-rendering-research.md`, which ranks
// "grass AS voxels in the SVO, finest levels only" as this engine's #2 option and points out that
// it needs no new pipeline at all -- the octree already does distance culling by construction, and
// a voxel blade inherits the march's shadows, AO, grain and materials for free. The research is
// equally clear about the two costs, and both are accepted here rather than argued away:
// vegetation is the worst-case SVO content class (Laine & Karras), and there is no published way to
// animate stored ray-marched voxels, so these blades do not move.
//
// ---------------------------------------------------------------------------------------------
// A TUFT IS NOT A PLANT, AND THE DIFFERENCE IS THE WHOLE DENSITY MODEL
// ---------------------------------------------------------------------------------------------
//
// `research/earth-terrain-geomorphology-research.md` Part 6 §7 measures ground cover in plants per
// square metre, and the numbers are far larger than a renderer can place one-for-one: 10-100/m^2
// for natural dry grassland, 10-18/m^2 for semi-arid rangeland, ~2/m^2 for desert tussock grass,
// and 940-2,836 TILLERS/m^2 for a managed temperate sward (tillers, not plants -- the research is
// explicit that the two are not the same, and that it could not pin a field measurement at the
// 10,000/m^2 the parent anchor claimed).
//
// At 60 plants/m^2 a 24 m ring holds 108,600 of them; at ~180 voxels each that is 19.5 million
// voxels of grass, which is not a budget, it is a refusal. So a TUFT stands for `plants_per_tuft`
// real plants, and the density that reaches the voxelizer is the biome's plant density divided by
// it. That is a level-of-detail device and it is named as one: the biome numbers below are the
// research's, unscaled, and the scaling happens in one place where it can be read.
struct GroundCoverBand {
    field::Biome biome;
    float plants_per_m2; ///< the target this generator places, before `plants_per_tuft`
    float band_lo;       ///< the measured range, kept beside the target so it can be argued with
    float band_hi;
    std::string_view citation;
};

/// The per-biome table, straight from Part 6 §7. Two entries are honestly weaker than the rest and
/// say so in their citation: temperate-forest understory is measured as COVER (46-57%) rather than
/// as a plant count, and boreal moss fraction the research explicitly flagged as "not fabricated"
/// -- both are stated as derived rather than measured.
[[nodiscard]] std::span<const GroundCoverBand> ground_cover_table() noexcept;
[[nodiscard]] const GroundCoverBand& ground_cover_of(field::Biome b) noexcept;
[[nodiscard]] float plants_per_m2(field::Biome b) noexcept;

struct GrassCoverParams {
    /// How many real plants one voxel tuft stands for. The LOD device described above; 20 puts a
    /// temperate grassland at 3 tufts/m^2, and a 24 m ring at ~5,400 tufts / ~27,000 blades, which
    /// sits inside the 32K-131K blades the grass research calls a production range.
    float plants_per_tuft = 20.0f;
    /// A blade is a thin, slightly leaning capsule. 0.28 m is a real unmown sward; the radius is
    /// two finest voxels, because one is a dotted line at 7.8 mm.
    float blade_height_m = 0.28f;
    float blade_radius_m = 0.016f;
    float tuft_radius_m = 0.09f;
    int blades_per_tuft = 5;
    /// Steeper than this and nothing is placed -- the same central-difference slope the tree mask
    /// uses, so a cliff face has no lawn on it.
    float max_slope = 2.0f;
    /// Patch edge, metres. A patch is one `TreeVolume`, which is what lets grass ride the sampler's
    /// existing tree acceleration structure instead of needing a second one.
    float patch_edge_m = 4.0f;
};

/// One tuft: where it is, how tall, and the id that makes it the SAME tuft in every tier
/// (goal 340). The id is a hash of the tuft's integer grid cell, so it is stable under any
/// re-derivation and identical on the CPU and in a tool.
struct GrassTuft {
    glm::vec3 base{0.0f};
    float height = 0.0f;
    float lean_x = 0.0f;
    float lean_z = 0.0f;
    std::uint32_t id = 0;
};

/// Every tuft whose base falls inside the patch `[origin, origin + patch_edge)^2`, for a biome and
/// a ground-height function. Deterministic in (seed, patch, biome): the same patch produces the
/// same tufts in the same order in every process.
///
/// `height_at` is the analytic surface; `slope_at` its central-difference slope. Both are passed as
/// callables so this module needs neither the heightmap nor the field.
template <typename HeightFn, typename SlopeFn, typename BiomeFn>
[[nodiscard]] std::vector<GrassTuft>
grass_tufts_in_patch(int seed, glm::vec2 origin, const GrassCoverParams& params, HeightFn&& height_at,
                     SlopeFn&& slope_at, BiomeFn&& biome_at);

/// The patch as a volume the sampler can voxelize, with every primitive carrying `GrassBlade`.
[[nodiscard]] TreeVolume grass_patch_volume(std::span<const GrassTuft> tufts, const GrassCoverParams& params);

} // namespace world::generation

#include "world/generation/detail/grass_cover.inl"
