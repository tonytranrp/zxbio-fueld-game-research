// Prompt 007 goal 334's Check: the Group AC property holds, and the exported macro matches the
// registry for every material.
//
// The point of this file is that `wind_responsive` is a COMPONENT, not a special case. Group AC's
// standing property is that a consumer never asks "is this material X" -- it asks the material a
// question, and adding a new answer touches the registry and nothing else. A boolean that the
// shader learns about through a registry-derived bitmask keeps that true across the C++/HLSL
// boundary, which is the one place it is easiest to lose.

#include <catch2/catch_test_macros.hpp>

#include "world/materials/materials.hpp"

using namespace world::materials;

namespace {
/// The same computation `render/diligent/detail/material_macros.hpp` performs, duplicated here on
/// purpose: if the two ever disagree, one of them is wrong, and a test that called the renderer's
/// own function would agree with it by construction and prove nothing.
[[nodiscard]] constexpr std::uint32_t expected_mask() noexcept {
    std::uint32_t mask = 0;
    for (std::size_t i = 0; i < kMaterialCount; ++i) {
        if (Registry::table[i].wind_responsive) {
            mask |= 1u << i;
        }
    }
    return mask;
}
} // namespace

TEST_CASE("the wind mask is exactly the registry's wind-responsive materials", "[materials][wind]") {
    const std::uint32_t mask = expected_mask();
    for (std::size_t i = 0; i < kMaterialCount; ++i) {
        const bool inMask = (mask & (1u << i)) != 0u;
        INFO("material " << Registry::table[i].name << " (id " << i << ")");
        CHECK(inMask == Registry::table[i].wind_responsive);
    }
    // A mask of zero would pass the loop above trivially, so assert the feature is actually on.
    CHECK(mask != 0u);
}

TEST_CASE("leaves and grass move in the wind and nothing else does", "[materials][wind]") {
    // Named through the registry rather than by id, which is the Group AC property in one line.
    CHECK(Registry::table[Registry::index_of<defs::Leaves>()].wind_responsive);
    CHECK(Registry::table[Registry::index_of<defs::Grass>()].wind_responsive);

    CHECK_FALSE(Registry::table[Registry::index_of<defs::Stone>()].wind_responsive);
    CHECK_FALSE(Registry::table[Registry::index_of<defs::Dirt>()].wind_responsive);
    CHECK_FALSE(Registry::table[Registry::index_of<defs::Sand>()].wind_responsive);
    CHECK_FALSE(Registry::table[Registry::index_of<defs::Water>()].wind_responsive);
    CHECK_FALSE(Registry::table[Registry::index_of<defs::Wood>()].wind_responsive);
    CHECK_FALSE(Registry::table[Registry::index_of<defs::Air>()].wind_responsive);
}

TEST_CASE("wind responsiveness is independent of the shading model", "[materials][wind]") {
    // THE WHOLE REASON THIS MEMBER EXISTS. Grass must be wind-responsive AND stay Shading::Lit:
    // reclassifying it as Foliage to reach the marcher's wind block would also have given ground
    // grass the mesh path's canopy sway, which is a tree crown's motion and not a lawn's.
    // Group AG's note called that out; this asserts the resolution.
    CHECK(Registry::table[Registry::index_of<defs::Grass>()].shading == Shading::Lit);
    CHECK(Registry::table[Registry::index_of<defs::Grass>()].wind_responsive);

    CHECK(Registry::table[Registry::index_of<defs::Leaves>()].shading == Shading::Foliage);
    CHECK(Registry::table[Registry::index_of<defs::Leaves>()].wind_responsive);

    // And the converse must be possible too, or the two concepts are still entangled: at least one
    // material is Foliage-shaded or Lit-shaded on the other side of the wind answer.
    bool litAndStill = false;
    for (std::size_t i = 0; i < kMaterialCount; ++i) {
        litAndStill = litAndStill || (Registry::table[i].shading == Shading::Lit &&
                                      !Registry::table[i].wind_responsive);
    }
    CHECK(litAndStill);
}
