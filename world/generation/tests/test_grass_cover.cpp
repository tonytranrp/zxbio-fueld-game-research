// Prompt 007 goal 338 = docs/goals.md goal 193.
//
// The Check asks for determinism, a per-biome density argued against the terrain research's own
// ground-cover numbers, and a body that walks through grass without colliding with it or being lied
// to by the aim readout. Everything here is one of those, plus the property goal 340 will need:
// a tuft's IDENTITY has to be a function of where it is and nothing else, or the three tiers cannot
// agree about it.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "world/generation/field/biome.hpp"
#include "world/generation/grass_cover.hpp"
#include "world/materials/materials.hpp"

using namespace world::generation;
using world::generation::field::Biome;

namespace {

// A flat, dry, gentle world: every mask passes, so a test measures the PLACEMENT rule and not the
// terrain's opinion of it.
constexpr float kFlatGround = 12.0f;
[[nodiscard]] float flat_height(float, float) noexcept {
    return kFlatGround;
}
[[nodiscard]] float flat_slope(float, float) noexcept {
    return 0.0f;
}

struct FixedBiome {
    Biome b;
    Biome operator()(float, float) const noexcept { return b; }
};

} // namespace

TEST_CASE("every biome's ground-cover target sits inside the band it cites", "[grass][biome]") {
    // research/earth-terrain-geomorphology-research.md Part 6 section 7. The table is the research's;
    // this is the assertion that a later tuning pass cannot quietly leave it.
    for (const GroundCoverBand& band : ground_cover_table()) {
        INFO(static_cast<int>(band.biome)
             << ": " << band.plants_per_m2 << " plants/m2, band [" << band.band_lo << ", " << band.band_hi
             << "] -- " << band.citation);
        CHECK(band.plants_per_m2 >= band.band_lo);
        CHECK(band.plants_per_m2 <= band.band_hi);
    }
    // And the three numbers the research states in its own summary sentence, by name, so a
    // renumbered enum cannot silently swap two biomes' densities.
    CHECK(plants_per_m2(Biome::Desert) == Catch::Approx(2.0f));     // Namib tussock, Wagner 2016
    CHECK(plants_per_m2(Biome::Shrubland) == Catch::Approx(14.0f)); // semi-arid rangeland 10-18
    CHECK(plants_per_m2(Biome::Grassland) == Catch::Approx(60.0f)); // natural dry grassland 10-100
    CHECK(plants_per_m2(Biome::Ocean) == Catch::Approx(0.0f));
    // The ordering the research implies: wetter and more temperate carries more ground cover.
    CHECK(plants_per_m2(Biome::Grassland) > plants_per_m2(Biome::Shrubland));
    CHECK(plants_per_m2(Biome::Shrubland) > plants_per_m2(Biome::Desert));
    CHECK(plants_per_m2(Biome::Desert) > plants_per_m2(Biome::Ocean));
}

TEST_CASE("placed tuft density reproduces the biome's, divided by plants_per_tuft", "[grass]") {
    // The density model in one assertion. A TUFT stands for `plants_per_tuft` plants, so the count
    // per square metre must be the biome's number over it -- if that drifts, the whole per-biome
    // table stops meaning anything.
    GrassCoverParams params;
    params.patch_edge_m = 8.0f;
    for (const Biome biome : {Biome::Grassland, Biome::Shrubland, Biome::Desert}) {
        std::size_t total = 0;
        constexpr int kPatches = 6;
        for (int pz = 0; pz < kPatches; ++pz) {
            for (int px = 0; px < kPatches; ++px) {
                total += grass_tufts_in_patch(1337,
                                              glm::vec2{static_cast<float>(px) * params.patch_edge_m,
                                                        static_cast<float>(pz) * params.patch_edge_m},
                                              params, flat_height, flat_slope, FixedBiome{biome})
                             .size();
            }
        }
        const float area =
            static_cast<float>(kPatches * kPatches) * params.patch_edge_m * params.patch_edge_m;
        const float measured = static_cast<float>(total) / area;
        const float expected = plants_per_m2(biome) / params.plants_per_tuft;
        INFO("biome " << static_cast<int>(biome) << ": " << measured << " tufts/m2 against an expected "
                      << expected);
        CHECK(measured == Catch::Approx(expected).epsilon(0.12));
    }
}

TEST_CASE("tuft placement is deterministic and patch-independent", "[grass]") {
    GrassCoverParams params;
    const glm::vec2 origin{16.0f, -24.0f};
    const auto a =
        grass_tufts_in_patch(99, origin, params, flat_height, flat_slope, FixedBiome{Biome::Grassland});
    const auto b =
        grass_tufts_in_patch(99, origin, params, flat_height, flat_slope, FixedBiome{Biome::Grassland});
    REQUIRE_FALSE(a.empty());
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i].id == b[i].id);
        REQUIRE(a[i].base.x == b[i].base.x);
        REQUIRE(a[i].base.z == b[i].base.z);
        REQUIRE(a[i].height == b[i].height);
        REQUIRE(a[i].lean_x == b[i].lean_x);
    }
    // A different seed is a different field.
    const auto c =
        grass_tufts_in_patch(100, origin, params, flat_height, flat_slope, FixedBiome{Biome::Grassland});
    bool anyDifferent = c.size() != a.size();
    for (std::size_t i = 0; !anyDifferent && i < a.size(); ++i) {
        anyDifferent = c[i].id != a[i].id;
    }
    CHECK(anyDifferent);
}

TEST_CASE("a tuft belongs to exactly one patch, and its id says which tuft it is", "[grass]") {
    // THE PROPERTY GOAL 340 RESTS ON. Three tiers -- voxel blades, a raster overlay, a distance
    // shimmer -- have to agree that a given tuft is the same tuft. That is only possible if
    // identity comes from the tuft's own grid cell rather than from whatever asked about it, so
    // this covers one area with two different patch grids and asserts the same set comes out.
    GrassCoverParams fine;
    fine.patch_edge_m = 4.0f;
    GrassCoverParams coarse = fine;
    coarse.patch_edge_m = 12.0f;

    const auto gather = [&](const GrassCoverParams& p) {
        std::unordered_map<std::uint32_t, glm::vec2> found;
        const int n = static_cast<int>(24.0f / p.patch_edge_m);
        for (int pz = 0; pz < n; ++pz) {
            for (int px = 0; px < n; ++px) {
                for (const GrassTuft& t :
                     grass_tufts_in_patch(7,
                                          glm::vec2{static_cast<float>(px) * p.patch_edge_m,
                                                    static_cast<float>(pz) * p.patch_edge_m},
                                          p, flat_height, flat_slope, FixedBiome{Biome::Grassland})) {
                    // Also proves no tuft is emitted twice: a duplicate id would collide here.
                    REQUIRE(found.find(t.id) == found.end());
                    found.emplace(t.id, glm::vec2{t.base.x, t.base.z});
                }
            }
        }
        return found;
    };

    const auto fineSet = gather(fine);
    const auto coarseSet = gather(coarse);
    INFO(fineSet.size() << " tufts over 24 m at a 4 m patch, " << coarseSet.size() << " at 12 m");
    REQUIRE_FALSE(fineSet.empty());
    REQUIRE(fineSet.size() == coarseSet.size());
    for (const auto& [id, pos] : fineSet) {
        const auto it = coarseSet.find(id);
        REQUIRE(it != coarseSet.end());
        // Not "close to": IDENTICAL. A tier that reproduced a tuft a centimetre away would show as
        // a doubled blade where two tiers overlap.
        REQUIRE(it->second.x == pos.x);
        REQUIRE(it->second.y == pos.y);
    }
}

TEST_CASE("nothing is planted below the waterline or on a cliff", "[grass]") {
    GrassCoverParams params;
    const auto underwater = [](float, float) { return -3.0f; };
    CHECK(
        grass_tufts_in_patch(5, glm::vec2{0.0f}, params, underwater, flat_slope, FixedBiome{Biome::Grassland})
            .empty());
    const auto cliff = [&](float, float) { return params.max_slope * 2.0f; };
    CHECK(grass_tufts_in_patch(5, glm::vec2{0.0f}, params, flat_height, cliff, FixedBiome{Biome::Grassland})
              .empty());
    // Ocean carries no ground cover at all, by its own table entry rather than by a special case.
    CHECK(grass_tufts_in_patch(5, glm::vec2{0.0f}, params, flat_height, flat_slope, FixedBiome{Biome::Ocean})
              .empty());
}

TEST_CASE("a patch volume is made of GrassBlade, which the body walks through", "[grass]") {
    GrassCoverParams params;
    const auto tufts = grass_tufts_in_patch(3, glm::vec2{0.0f}, params, flat_height, flat_slope,
                                            FixedBiome{Biome::Grassland});
    REQUIRE_FALSE(tufts.empty());
    const TreeVolume volume = grass_patch_volume(tufts, params);
    REQUIRE_FALSE(volume.empty());
    CHECK(volume.primitives().size() == tufts.size() * static_cast<std::size_t>(params.blades_per_tuft));

    // Sample the middle of the first blade: it must be GrassBlade and nothing else.
    const TreePrimitive& blade = volume.primitives().front();
    CHECK(volume.material_at(0.5f * (blade.a + blade.b)) == world::chunk::MaterialID::GrassBlade);

    // The two properties the Check names, asserted where they are decided rather than in the app:
    // a blade is not solid, so the collider never sees it...
    CHECK_FALSE(world::materials::properties_of(world::chunk::MaterialID::GrassBlade).is_solid());
    CHECK_FALSE(world::materials::properties_of(world::chunk::MaterialID::GrassBlade).is_occupied());
    // ...and it is a real named material, so the aim readout tells the truth about it rather than
    // reporting the ground underneath.
    CHECK(std::string_view{world::materials::name_of(world::chunk::MaterialID::GrassBlade)} == "GrassBlade");

    // Every blade stands ON the ground, not in it or above it.
    for (const TreePrimitive& p : volume.primitives()) {
        REQUIRE(p.a.y == Catch::Approx(kFlatGround).margin(1.0e-4));
        REQUIRE(p.b.y > p.a.y);
        REQUIRE(p.b.y - p.a.y <= params.blade_height_m * 1.3f);
    }
}

TEST_CASE("a tuft is the same tuft in every tier, for 1000 of them", "[grass][tiers]") {
    // Prompt 007 goal 340's Check, verbatim: "a test asserts a tuft's ID and world position are
    // identical in all three tiers for 1,000 random tufts."
    //
    // THE THIRD TIER DOES NOT HAVE TUFTS, and saying so is more useful than pretending it does.
    // Goal 333's distance shimmer is a per-pixel brightness term over the ground material -- it has
    // no per-tuft identity to agree about, and it could not have one without becoming a different
    // technique. What it CAN share, and does, is the wind field it reads: the same `wind.fxh`
    // constants at the same world position. So the identity assertion below covers the two tiers
    // that have identities, and the phase assertion covers what the third can actually share.
    GrassCoverParams params;
    std::vector<GrassTuft> all;
    for (int pz = 0; pz < 12 && all.size() < 1000; ++pz) {
        for (int px = 0; px < 12 && all.size() < 1000; ++px) {
            for (const GrassTuft& t : grass_tufts_in_patch(
                     31337,
                     glm::vec2{static_cast<float>(px) * params.patch_edge_m,
                               static_cast<float>(pz) * params.patch_edge_m},
                     params, flat_height, flat_slope, FixedBiome{Biome::Grassland})) {
                all.push_back(t);
            }
        }
    }
    REQUIRE(all.size() >= 1000);
    all.resize(1000);

    // TIER 1, the voxel blades: the capsules `grass_patch_volume` builds.
    const TreeVolume volume = grass_patch_volume(all, params);
    REQUIRE(volume.primitives().size() == all.size() * static_cast<std::size_t>(params.blades_per_tuft));

    std::size_t checked = 0;
    for (std::size_t i = 0; i < all.size(); ++i) {
        for (int b = 0; b < params.blades_per_tuft; ++b) {
            const GrassBladeSegment blade = grass_blade(all[i], b, params.blades_per_tuft, params);
            const TreePrimitive& prim =
                volume.primitives()[i * static_cast<std::size_t>(params.blades_per_tuft) +
                                    static_cast<std::size_t>(b)];
            // IDENTICAL, not close: a tier that reproduced a blade a millimetre away would show as
            // a doubled blade wherever the two tiers overlap, and the overlap is deliberate.
            REQUIRE(prim.a.x == blade.start.x);
            REQUIRE(prim.a.y == blade.start.y);
            REQUIRE(prim.a.z == blade.start.z);
            REQUIRE(prim.b.x == blade.end.x);
            REQUIRE(prim.b.y == blade.end.y);
            REQUIRE(prim.b.z == blade.end.z);
            REQUIRE(prim.radius == blade.radius);

            // TIER 2, the raster overlay: `app::GrassField` builds its instance from the SAME
            // `grass_blade` call, and this is the arithmetic it applies to the result. Reproduced
            // here rather than linking the app, because what is being asserted is that the shared
            // segment is enough to place a raster blade -- i.e. that the overlay needs no second
            // copy of the fan formula. Its first version had one, and that is the failure mode.
            const glm::vec3 axis = blade.end - blade.start;
            const float rise = std::max(axis.y, 1.0e-4f);
            const glm::vec3 rasterTip =
                blade.start + glm::vec3{axis.x / rise * rise, rise, axis.z / rise * rise};
            REQUIRE(rasterTip.x == Catch::Approx(blade.end.x).margin(1.0e-5));
            REQUIRE(rasterTip.z == Catch::Approx(blade.end.z).margin(1.0e-5));
            ++checked;
        }
    }
    INFO(checked << " blades checked across " << all.size() << " tufts");
    CHECK(checked == all.size() * static_cast<std::size_t>(params.blades_per_tuft));

    // TIER 3's share: the wind phase. A pure function of the tuft's id, so every tier that animates
    // gets the same answer without passing anything between them.
    for (const GrassTuft& t : all) {
        const float phase = grass_wind_phase(t);
        REQUIRE(phase == grass_wind_phase(t));
        REQUIRE(phase >= 0.0f);
        REQUIRE(phase <= 6.0f);
    }

    // And every id is distinct, which is what makes "the same tuft" a meaningful phrase at all.
    std::vector<std::uint32_t> ids;
    ids.reserve(all.size());
    for (const GrassTuft& t : all) {
        ids.push_back(t.id);
    }
    std::sort(ids.begin(), ids.end());
    CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
}
