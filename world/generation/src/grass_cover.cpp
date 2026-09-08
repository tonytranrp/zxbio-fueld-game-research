#include "world/generation/grass_cover.hpp"

#include <algorithm>
#include <array>

namespace world::generation {

namespace {

using field::Biome;

// research/earth-terrain-geomorphology-research.md Part 6 §7, "Non-tree ground cover". The two
// entries whose citation says DERIVED are exactly the two the research measured as something other
// than a plant count, and they are labelled rather than dressed up as measurements.
constexpr std::array<GroundCoverBand, static_cast<std::size_t>(Biome::Count)> kBands{{
    {Biome::Ocean, 0.0f, 0.0f, 0.0f, "no ground cover below the waterline"},
    {Biome::Beach, 1.0f, 0.0f, 5.0f, "DERIVED: strandline is bare to sparse; no count in the research"},
    {Biome::Wetland, 80.0f, 10.0f, 100.0f,
     "natural dry-grassland band applied to a wet meadow, upper half (Part 6 section 7)"},
    {Biome::Grassland, 60.0f, 10.0f, 100.0f,
     "natural dry grassland 10-100 plants/m2 (Part 6 section 7's own summary line)"},
    {Biome::Shrubland, 14.0f, 10.0f, 18.0f,
     "semi-arid African rangeland 10-18 plants/m2, basal cover 49-73% (Frontiers 2021)"},
    {Biome::TemperateForest, 30.0f, 10.0f, 100.0f,
     "DERIVED from understory COVER 46-57% (Spies & Franklin 1991); the research gives no count"},
    {Biome::BorealForest, 20.0f, 0.0f, 100.0f,
     "DERIVED: moss/lichen dominant but the research explicitly flagged the fraction as not pinned"},
    {Biome::Alpine, 5.0f, 0.0f, 20.0f,
     "glacier-foreland succession: ~10% ground cover at year 50 (Fickert 2017)"},
    {Biome::Desert, 2.0f, 1.0f, 5.0f,
     "Namib escarpment Stipagrostis ~2 tussocks/m2 at 120 mm MAP (Wagner et al. 2016)"},
}};

} // namespace

std::span<const GroundCoverBand> ground_cover_table() noexcept {
    return {kBands.data(), kBands.size()};
}

const GroundCoverBand& ground_cover_of(Biome b) noexcept {
    const auto i = static_cast<std::size_t>(b);
    return kBands[i < kBands.size() ? i : 0];
}

float plants_per_m2(Biome b) noexcept {
    return ground_cover_of(b).plants_per_m2;
}

TreeVolume grass_patch_volume(std::span<const GrassTuft> tufts, const GrassCoverParams& params) {
    if (tufts.empty()) {
        return {};
    }
    // A tuft is `blades_per_tuft` capsules fanning out of one point, and it is expressed as a
    // SKELETON so it can go through `TreeVolume` unchanged. That is the whole reason grass costs no
    // new acceleration structure, no new box classifier and no new voxelizer: it is the same
    // capsule-and-ball volume a tree is, with a different material on it.
    TreeSkeleton skeleton;
    skeleton.segments.reserve(tufts.size() * static_cast<std::size_t>(params.blades_per_tuft));
    const auto blades = std::max(1, params.blades_per_tuft);
    for (const GrassTuft& tuft : tufts) {
        for (int b = 0; b < blades; ++b) {
            // Blades fan around the tuft's centre at a fixed angular spacing, offset by the tuft's
            // own id -- deterministic, and no two neighbouring tufts point the same way.
            const float turn = 6.2831853f * (static_cast<float>(b) / static_cast<float>(blades) +
                                             static_cast<float>(tuft.id & 0xFFu) / 256.0f);
            const float rx = params.tuft_radius_m * std::cos(turn);
            const float rz = params.tuft_radius_m * std::sin(turn);
            SkeletonSegment seg;
            seg.parent = -1; // every blade is its own root; nothing walks this skeleton's hierarchy
            seg.start = tuft.base + glm::vec3{rx * 0.25f, 0.0f, rz * 0.25f};
            seg.end = tuft.base +
                      glm::vec3{rx + tuft.lean_x * tuft.height, tuft.height, rz + tuft.lean_z * tuft.height};
            seg.radius = params.blade_radius_m;
            seg.leaf_count = 0.0f; // no leaf clouds: a blade IS the geometry
            skeleton.segments.push_back(seg);
        }
    }
    TreeVolumeParams vp;
    vp.min_branch_radius = params.blade_radius_m;
    vp.wood_material = world::chunk::MaterialID::GrassBlade;
    vp.leaf_material = world::chunk::MaterialID::GrassBlade;
    return TreeVolume{skeleton, vp};
}

} // namespace world::generation
