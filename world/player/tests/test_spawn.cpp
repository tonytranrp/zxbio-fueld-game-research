#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "world/player/spawn.hpp"
#include "world/player/tuning.hpp"

using Catch::Matchers::WithinAbs;

namespace {

// A height field with an explicit shape, so each of the four steps can be asserted in isolation
// rather than against the real generator (which would make this a terrain test).
struct FlatGround {
    float height;
    [[nodiscard]] float height_at(float, float) const noexcept { return height; }
};

// Falls `slope` metres per metre in +x, so the uphill corner of a body's footprint is genuinely
// higher than its centre -- the case a point sample gets wrong.
struct Ramp {
    float slope;
    [[nodiscard]] float height_at(float x, float) const noexcept { return -slope * x; }
};

static_assert(world::player::HeightSampler<FlatGround>);
static_assert(world::player::HeightSampler<Ramp>);

constexpr float kVoxel = 0.0078125f; // 2^-7, the shipped finest voxel
constexpr float kHalf = world::player::kDefaultTuning.body_half_width;

} // namespace

TEST_CASE("ground_feet_height snaps up to the voxel grid and clears it", "[player][spawn]") {
    // A height deliberately NOT on a voxel boundary: 10.0 / 0.0078125 = 1280 exactly, so offset it.
    const FlatGround ground{10.0f + kVoxel * 0.5f};
    const float feet = world::player::ground_feet_height(ground, 0.0f, 0.0f, kVoxel);

    // Above the surface, never on or below it -- feet AT the analytic height sit inside the top
    // solid voxel, which is the bug this arithmetic exists to prevent.
    REQUIRE(feet > ground.height);
    // Snapped up to a grid line, then two voxels of clearance: the result is a multiple of the
    // voxel edge.
    REQUIRE_THAT(std::fmod(feet, kVoxel), WithinAbs(0.0f, 1.0e-5f));
    // ...and the clearance is the stated two voxels past the snap, not one and not ten.
    const float snapped = std::ceil(ground.height / kVoxel) * kVoxel;
    REQUIRE_THAT(feet - snapped, WithinAbs(2.0f * kVoxel, 1.0e-6f));
}

TEST_CASE("ground_feet_height takes the maximum over the body footprint", "[player][spawn]") {
    // The measured case: terrain falling ~0.6 m per metre. The centre column is at 0, the uphill
    // corner of a 0.6 m-wide box is half_width further back and therefore higher.
    const Ramp ramp{0.6f};
    const float feet = world::player::ground_feet_height(ramp, 0.0f, 0.0f, kVoxel);

    const float uphill = ramp.height_at(-kHalf, 0.0f);
    REQUIRE(uphill > ramp.height_at(0.0f, 0.0f)); // the fixture is actually sloped
    // Clears the UPHILL corner, not just the centre. A point sample would land ~0.19 m low here,
    // which is goal 228's "181 of 181 ticks inside solid" in a scenario where nothing moves.
    REQUIRE(feet > uphill);
    REQUIRE(feet - uphill < 4.0f * kVoxel); // ...and only just: the snap + clearance, nothing more
}

TEST_CASE("ground_feet_height clamps to sea level", "[player][spawn]") {
    // Standing "on the ground" over open water means standing on the water, not on a sea floor
    // 60 m down.
    const FlatGround seafloor{-60.0f};
    const float feet = world::player::ground_feet_height(seafloor, 0.0f, 0.0f, kVoxel);
    REQUIRE(feet >= world::player::kSeaLevelWorld);
    REQUIRE_THAT(feet, WithinAbs(world::player::kSeaLevelWorld + 2.0f * kVoxel, 1.0e-6f));
}

TEST_CASE("ground_feet_height scales with the voxel edge it is given", "[player][spawn]") {
    // The mesh path passes 1 m and the svo path 2^-7; the clearance is in VOXELS, so the two
    // answers differ by the grid they snap to rather than by a hardcoded margin.
    const FlatGround ground{10.3f};
    const float fine = world::player::ground_feet_height(ground, 0.0f, 0.0f, kVoxel);
    const float coarse = world::player::ground_feet_height(ground, 0.0f, 0.0f, 1.0f);
    REQUIRE(coarse > fine);
    REQUIRE_THAT(coarse, WithinAbs(11.0f + 2.0f, 1.0e-6f)); // ceil(10.3) + 2 m
    REQUIRE(fine - ground.height < 0.05f);                  // the fine grid barely lifts the body
}
