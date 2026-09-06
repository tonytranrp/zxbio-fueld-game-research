#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>

#include "world/materials/materials.hpp"

using namespace world::materials;

namespace {

// fill_terrain's band rule as it was written before Group AC (integer depth, 1 m voxels), kept
// verbatim as the oracle: the registry's per-component predicates must reproduce it exactly.
MaterialID old_chunk_rule(std::int32_t worldY, std::int32_t surfaceHeight, std::int32_t seaLevel, bool beach,
                          bool grassy) {
    constexpr std::int32_t kSoilDepth = 3;
    if (worldY <= surfaceHeight) {
        const std::int32_t depth = surfaceHeight - worldY;
        if (depth == 0) {
            return beach ? MaterialID::Sand : (grassy ? MaterialID::Grass : MaterialID::Stone);
        }
        if (depth <= kSoilDepth) {
            return beach ? MaterialID::Sand : MaterialID::Dirt;
        }
        return MaterialID::Stone;
    }
    if (worldY <= seaLevel) {
        return MaterialID::Water;
    }
    return MaterialID::Air;
}

// TerrainSampler::column_material as it was written before Group AC (meters, any voxel edge).
MaterialID old_sampler_rule(float surfaceHeight, bool beach, bool grassy, float voxelBottom, float voxelEdge,
                            float seaLevel) {
    constexpr float kSoilDepth = 3.0f;
    if (voxelBottom <= surfaceHeight) {
        const float depth = surfaceHeight - voxelBottom;
        if (depth < voxelEdge) {
            return beach ? MaterialID::Sand : (grassy ? MaterialID::Grass : MaterialID::Stone);
        }
        if (depth < kSoilDepth + voxelEdge) {
            return beach ? MaterialID::Sand : MaterialID::Dirt;
        }
        return MaterialID::Stone;
    }
    if (voxelBottom <= seaLevel) {
        return MaterialID::Water;
    }
    return MaterialID::Air;
}

} // namespace

// The composition's own invariants are compile-time facts, stated as such.
static_assert(static_cast<std::size_t>(MaterialID::Air) == 0);
static_assert(static_cast<std::size_t>(MaterialID::Stone) == Registry::index_of<defs::Stone>());
static_assert(static_cast<std::size_t>(MaterialID::Grass) == kMaterialCount - 1);
static_assert(Registry::table.size() == kMaterialCount);
static_assert(properties_of(MaterialID::Water).is_liquid());
static_assert(!properties_of(MaterialID::Water).is_solid());
static_assert(std::string_view{name_of(MaterialID::Leaves)} == "Leaves");

TEST_CASE("every material is reachable by its enumerator and named after its component", "[materials]") {
    CHECK(std::string_view{name_of(MaterialID::Air)} == "Air");
    CHECK(std::string_view{name_of(MaterialID::Stone)} == "Stone");
    CHECK(std::string_view{name_of(MaterialID::Dirt)} == "Dirt");
    CHECK(std::string_view{name_of(MaterialID::Water)} == "Water");
    CHECK(std::string_view{name_of(MaterialID::Wood)} == "Wood");
    CHECK(std::string_view{name_of(MaterialID::Leaves)} == "Leaves");
    CHECK(std::string_view{name_of(MaterialID::Sand)} == "Sand");
    CHECK(std::string_view{name_of(MaterialID::Grass)} == "Grass");
    for (std::size_t i = 0; i < kMaterialCount; ++i) {
        const auto id = static_cast<MaterialID>(i);
        CHECK(std::string_view{name_of(id)}.size() > 0);
        for (std::size_t j = i + 1; j < kMaterialCount; ++j) {
            CHECK(std::string_view{name_of(id)} != std::string_view{name_of(static_cast<MaterialID>(j))});
        }
    }
}

TEST_CASE("is_occupied reproduces the mesh-extraction occupancy rule", "[materials]") {
    // Everything that needs a mesh boundary against open air: solid or liquid. Foliage is not a
    // voxel on the mesh path (canopies are appended geometry), so it is not occupied there.
    CHECK_FALSE(is_occupied(MaterialID::Air));
    CHECK(is_occupied(MaterialID::Stone));
    CHECK(is_occupied(MaterialID::Dirt));
    CHECK(is_occupied(MaterialID::Water));
    CHECK(is_occupied(MaterialID::Wood));
    CHECK_FALSE(is_occupied(MaterialID::Leaves));
    CHECK(is_occupied(MaterialID::Sand));
    CHECK(is_occupied(MaterialID::Grass));
}

TEST_CASE("is_solid excludes water and foliage (gameplay collision, not mesh occupancy)", "[materials]") {
    CHECK_FALSE(properties_of(MaterialID::Air).is_solid());
    CHECK_FALSE(properties_of(MaterialID::Water).is_solid());
    CHECK_FALSE(properties_of(MaterialID::Leaves).is_solid());
    CHECK(properties_of(MaterialID::Stone).is_solid());
    CHECK(properties_of(MaterialID::Grass).is_solid());
    CHECK(properties_of(MaterialID::Wood).is_solid());
}

TEST_CASE("only Water is a liquid, and it carries the swim physics", "[materials]") {
    for (std::size_t i = 0; i < kMaterialCount; ++i) {
        const auto id = static_cast<MaterialID>(i);
        const bool expected = (id == MaterialID::Water);
        CHECK(properties_of(id).is_liquid() == expected);
        CHECK((properties_of(id).liquid.buoyancy_acceleration > 0.0f) == expected);
        CHECK((properties_of(id).liquid.drag > 0.0f) == expected);
        CHECK((properties_of(id).liquid.swim_equilibrium_depth > 0.0f) == expected);
    }
    // The equilibrium the camera test (test_spectator_camera.cpp) expects: upthrust at full
    // submersion exceeds gravity (32 world units/s^2), so a body floats.
    CHECK(properties_of(MaterialID::Water).liquid.buoyancy_acceleration > 32.0f);
}

TEST_CASE("only Water is shaded as water and only Leaves as foliage", "[materials]") {
    for (std::size_t i = 0; i < kMaterialCount; ++i) {
        const auto id = static_cast<MaterialID>(i);
        const Shading expected = id == MaterialID::Water    ? Shading::Water
                                 : id == MaterialID::Leaves ? Shading::Foliage
                                                            : Shading::Lit;
        CHECK(properties_of(id).shading == expected);
    }
    // The macro names the shaders test against exist for every model and are distinct.
    CHECK(std::string_view{shading_macro_name(Shading::Lit)} == "MAT_SHADING_LIT");
    CHECK(std::string_view{shading_macro_name(Shading::Water)} == "MAT_SHADING_WATER");
    CHECK(std::string_view{shading_macro_name(Shading::Foliage)} == "MAT_SHADING_FOLIAGE");
}

TEST_CASE("tree voxelization priority: air and water yield, the trunk overrides, terrain wins otherwise",
          "[materials]") {
    // The rule terrain_sampler.cpp used to spell inline:
    //   terrainSolid = current != Air && current != Water; replace = !terrainSolid || tree == Wood
    for (std::size_t c = 0; c < kMaterialCount; ++c) {
        for (const MaterialID tree : {MaterialID::Wood, MaterialID::Leaves}) {
            const auto current = static_cast<MaterialID>(c);
            const bool terrainSolid = current != MaterialID::Air && current != MaterialID::Water;
            const bool expected = !terrainSolid || tree == MaterialID::Wood;
            CHECK(tree_replaces(current, tree) == expected);
        }
    }
}

TEST_CASE("exactly one component claims every terrain voxel", "[materials]") {
    // Surface heights and voxel bottoms straddle the surface, the soil band, and sea level, at the
    // chunk edge (1 m) and the two sparse-brick extremes; beach/grassy in every combination.
    for (const float edge : {1.0f, 0.25f, 0.0078125f}) {
        for (int s = -6; s <= 6; ++s) {
            const float surface = static_cast<float>(s) * 0.7f;
            for (int b = -60; b <= 60; ++b) {
                const float bottom = static_cast<float>(b) * 0.15f;
                for (const bool beach : {false, true}) {
                    for (const bool grassy : {false, true}) {
                        const TerrainQuery q{surface, bottom, edge, 0.0f, beach, grassy};
                        REQUIRE(Registry::terrain_claims(q) == 1);
                    }
                }
            }
        }
    }
}

TEST_CASE("terrain_material reproduces fill_terrain's integer band rule at 1 m", "[materials]") {
    constexpr std::int32_t seaLevel = 0;
    for (std::int32_t surface = -8; surface <= 12; ++surface) {
        for (std::int32_t worldY = -12; worldY <= 16; ++worldY) {
            for (const bool beach : {false, true}) {
                for (const bool grassy : {false, true}) {
                    const TerrainQuery q{static_cast<float>(surface),
                                         static_cast<float>(worldY),
                                         1.0f,
                                         static_cast<float>(seaLevel),
                                         beach,
                                         grassy};
                    REQUIRE(terrain_material(q) == old_chunk_rule(worldY, surface, seaLevel, beach, grassy));
                }
            }
        }
    }
}

TEST_CASE("terrain_material reproduces the sampler's meter band rule at sub-meter voxels", "[materials]") {
    constexpr float seaLevel = 0.0f;
    for (const float edge : {0.5f, 0.125f, 0.0078125f}) {
        for (int s = -10; s <= 10; ++s) {
            const float surface = static_cast<float>(s) * 0.37f;
            for (int b = -80; b <= 80; ++b) {
                const float bottom = static_cast<float>(b) * 0.11f;
                for (const bool beach : {false, true}) {
                    for (const bool grassy : {false, true}) {
                        const TerrainQuery q{surface, bottom, edge, seaLevel, beach, grassy};
                        REQUIRE(terrain_material(q) ==
                                old_sampler_rule(surface, beach, grassy, bottom, edge, seaLevel));
                    }
                }
            }
        }
    }
}

TEST_CASE("the band constants are the ones the world was built with", "[materials]") {
    // Changing these changes the shipped world (every capture, the sampler-vs-fill equivalence
    // test); they moved here from three hand-mirrored copies, they did not change.
    CHECK(TerrainBands::beach_band == 1.75f);
    CHECK(TerrainBands::soil_depth == 3.0f);
    CHECK(TerrainBands::grass_max_slope == 1.9f);
    CHECK(TerrainBands::is_beach(1.75f, 0.0f));
    CHECK_FALSE(TerrainBands::is_beach(1.76f, 0.0f));
    CHECK(TerrainBands::is_grassy(false, 1.9f));
    CHECK_FALSE(TerrainBands::is_grassy(false, 1.91f));
    CHECK_FALSE(TerrainBands::is_grassy(true, 0.0f));
}
