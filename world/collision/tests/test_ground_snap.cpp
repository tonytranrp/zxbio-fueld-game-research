// The step DOWN (SweepParams::ground_snap), and the defect it exists for.
//
// Every other sweep test in this folder stands a body on a half-space floor or walks it into a box.
// The surface the game actually has is a STAIRCASE -- a heightfield quantised to the voxel grid --
// and nothing in the repository drove a body across one before this file. That gap is why the
// defect below survived: `walk_violations` and `inside_solid` are both blind to it (during the
// failure the body is ABOVE the surface and inside nothing), so the two assertions every walking
// scenario carries pass throughout.
//
// The defect, stated as arithmetic: `move_and_slide_once` resolves the vertical axis BEFORE the
// horizontal one, so `grounded` describes the floor under where the body WAS, and the only downward
// motion a tick carries is `vertical_velocity * dt` -- from rest, g*dt^2 = 2.7 mm. The ground under
// a moving body drops by `v*dt*tan(slope)`. Contact therefore survives only while
//
//     slope <= atan(g*dt / v)
//
// which is 6.66 degrees at the 1.4 m/s walk and 1.34 degrees at the 7 m/s sprint. Measured terrain
// slope at the metre scale is ~25 degrees, so the body left the ground on essentially every descent
// and fell until the terrain caught up -- roughly half a metre of air at sprint.

#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "world/collision/aabb_sweep.hpp"

using world::collision::Aabb;
using world::collision::ground_snap_for;
using world::collision::move_and_slide;
using world::collision::SweepParams;
using world::collision::SweepResult;

namespace {

// A descending staircase: solid below a surface that drops one `riser` every `tread` of +X. This is
// what a voxelised heightfield of slope atan(riser/tread) actually is.
struct StairWorld {
    float riser = 0.0078125f; // one finest voxel
    float tread = 0.0167f;    // riser/tread = tan(25 degrees)
    float top = 0.0f;         // surface height at x = 0

    [[nodiscard]] float surface_at(float x) const { return top - std::floor(x / tread) * riser; }

    [[nodiscard]] bool overlaps_solid(const Aabb& box) const {
        // Sample the surface across the box's own footprint: a step boundary inside the box must
        // count, or the body tunnels through the corner of a stair.
        for (int i = 0; i <= 8; ++i) {
            const float t = static_cast<float>(i) / 8.0f;
            const float x = box.min.x + (box.max.x - box.min.x) * t;
            if (box.min.y < surface_at(x)) {
                return true;
            }
        }
        return false;
    }
};
static_assert(world::collision::SolidQuery<StairWorld>);

constexpr float kDt = 1.0f / 60.0f;
constexpr float kGravity = -9.81f;
constexpr float kHalfWidth = 0.3f;
constexpr float kHeight = 1.75f;

Aabb body_at(float x, float feetY) {
    return Aabb::upright(glm::vec3{x, feetY, 0.0f}, kHalfWidth, kHeight);
}

// Walks a body down the staircase for `ticks`, counting how many ticks ended ungrounded and how far
// the feet ever got above the surface under them.
struct WalkResult {
    int ungrounded = 0;
    float worstGapMetres = 0.0f;
};

WalkResult walk_down(const StairWorld& world, const SweepParams& params, float speed, int ticks) {
    WalkResult out;
    // Start resting exactly on the surface at x = 0.
    glm::vec3 feet{0.0f, world.surface_at(0.0f), 0.0f};
    float verticalVelocity = 0.0f;
    for (int i = 0; i < ticks; ++i) {
        verticalVelocity += kGravity * kDt;
        const glm::vec3 wanted{speed * kDt, verticalVelocity * kDt, 0.0f};
        const SweepResult moved = move_and_slide(world, body_at(feet.x, feet.y), wanted, params);
        feet += moved.delta;
        if (moved.grounded) {
            verticalVelocity = 0.0f;
        } else {
            ++out.ungrounded;
        }
        // The surface under the body's own centre, which is what "on the ground" means here.
        const float gap = feet.y - world.surface_at(feet.x);
        out.worstGapMetres = std::max(out.worstGapMetres, gap);
    }
    return out;
}

} // namespace

TEST_CASE("the derived step-down distance covers one tick of the worst legal descent",
          "[collision][sweep][ground_snap]") {
    // One tick of the shipped sprint down the steepest ground the walk limit still allows, plus the
    // step-up budget as the surface-irregularity margin. The number must EXCEED the descent it has
    // to cover, which is the whole reason it is derived rather than chosen.
    constexpr float kSprint = 7.0f;
    constexpr float kMaxSlope = 0.6981317f; // 40 degrees
    constexpr float kStepHeight = 0.04f;
    const float snap = ground_snap_for(kSprint, kDt, kMaxSlope, kStepHeight);
    const float descentPerTick = kSprint * kDt * std::tan(kMaxSlope);

    CHECK(snap > descentPerTick);
    CHECK(snap > kStepHeight); // a step-up-sized budget cannot follow a slope, only clear a riser
    CHECK(snap == Catch::Approx(descentPerTick + kStepHeight));
}

TEST_CASE("without the step down, a walking body free-falls down a 25 degree staircase",
          "[collision][sweep][ground_snap]") {
    // This is the SHIPPED behaviour before the fix, pinned so the defect cannot return quietly.
    const StairWorld world;
    SweepParams params;
    params.step_height = 0.04f;
    params.ground_snap = 0.0f; // the old sweep: a step up and no step down

    const WalkResult walk = walk_down(world, params, 1.4f, 240);
    const WalkResult sprint = walk_down(world, params, 7.0f, 240);

    // The threshold is atan(g*dt/v): 6.66 degrees walking, 1.34 sprinting, against a 25 degree
    // slope. Both are far under it, so both leave the ground and stay off it.
    CHECK(walk.ungrounded > 100);
    CHECK(sprint.ungrounded > 100);
    // And the body ends up visibly in the air -- centimetres at a walk, decimetres at a sprint.
    CHECK(walk.worstGapMetres > 0.01f);
    CHECK(sprint.worstGapMetres > 0.10f);
}

TEST_CASE("with the step down, the body stays on a 25 degree staircase at both speeds",
          "[collision][sweep][ground_snap]") {
    const StairWorld world;
    SweepParams params;
    params.step_height = 0.04f;
    params.ground_snap = ground_snap_for(7.0f, kDt, 0.6981317f, 0.04f);
    params.grounded_hint = true;

    for (const float speed : {1.4f, 7.0f}) {
        const WalkResult r = walk_down(world, params, speed, 240);
        INFO("speed " << speed << " m/s: ungrounded " << r.ungrounded << " ticks, worst gap "
                      << r.worstGapMetres << " m");
        // Contact is continuous: the body may not float for a single tick.
        CHECK(r.ungrounded == 0);
        // And it sits no higher than a box of this width geometrically must on this slope. A
        // 0.6 m footprint on a descending face rests on its UPHILL edge, so its underside is
        // `half_width * tan(slope)` above the surface under its centre -- 0.14 m here, and that is
        // the box's shape, not air. One riser of margin on top for the quantisation itself.
        const float footprintLift = kHalfWidth * (world.riser / world.tread);
        CHECK(r.worstGapMetres < footprintLift + 2.0f * world.riser);
    }
}

TEST_CASE("the step down does not fire for a body that was not already in contact",
          "[collision][sweep][ground_snap]") {
    // A jump, a fall and a dive must not be cut short by the floor reaching up. `grounded_hint` is
    // the caller's statement that the body was on the ground; without it the probe is skipped.
    const StairWorld world;
    SweepParams params;
    params.step_height = 0.04f;
    params.ground_snap = 0.5f;
    params.grounded_hint = false;

    // A body half a metre up, falling one tick's worth. It must move by exactly what it asked for.
    const float feetY = world.surface_at(0.0f) + 0.5f;
    const glm::vec3 wanted{0.0f, -0.01f, 0.0f};
    const SweepResult moved = move_and_slide(world, body_at(0.0f, feetY), wanted, params);

    CHECK(moved.delta.y == Catch::Approx(-0.01f));
    CHECK_FALSE(moved.grounded);
}

TEST_CASE("a body resting on flat ground is unaffected by the step down", "[collision][sweep][ground_snap]") {
    // The probe runs every grounded tick, so it must be a no-op on level ground rather than
    // dragging the body down into the surface.
    StairWorld world;
    world.riser = 0.0f; // flat
    SweepParams params;
    params.step_height = 0.04f;
    params.ground_snap = 0.138f;
    params.grounded_hint = true;

    const WalkResult r = walk_down(world, params, 1.4f, 120);
    CHECK(r.ungrounded == 0);
    CHECK(r.worstGapMetres < 1.0e-3f);
}
