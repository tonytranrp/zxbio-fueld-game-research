#pragma once

#include <algorithm>
#include <cmath>

#include "world/collision/aabb_sweep.hpp"
#include "world/collision/solid_query.hpp"
#include "world/player/player_state.hpp"

namespace world::player {

// One fixed simulation tick, swept against any world that answers `overlaps_solid` (Prompt 001
// Group A). A template over world::collision::SolidQuery for the same reason the sweep itself is
// one: the tests drive a plane-and-wall fake, the app drives TerrainCollider, and a future query
// over the sparse-brick octree (goal 173) slots in without touching this file.
//
// `eyePosition` is the CAMERA position (the feet sit tuning.eye_height below it) -- the same
// convention app::SpectatorCameraState has always used, kept so the transform means one thing
// everywhere.
//
// Order within a tick, and why:
//   1. horizontal wish        -- what the player asked for
//   2. water sense            -- decides Swimming before the vertical integrator runs
//   3. vertical integration   -- gravity / buoyancy+drag+dive / fly strafe
//   4. jump                   -- an initial VELOCITY, so it must land before the sweep
//   5. sweep + apply          -- the world's answer, the only thing that moves the body
//   6. stance + landing       -- derived from the sweep, feeds the next tick's coyote credit
//   7. eye smoothing          -- render-only, on the exact positions from step 5
template <collision::SolidQuery Q>
StepResult step_player(const Q& query, PlayerState& state, const PlayerTuning& tuning,
                       const PlayerIntent& intent, const WorldSense& sense, glm::vec3& eyePosition,
                       float yawRadians, float pitchRadians, float moveSpeed, float dt) {
    StepResult result;
    const float previousEyeY = eyePosition.y;
    const Stance previousStance = state.stance;
    // A query that declares itself an open world (nothing is solid, anywhere) is the --noclip and
    // collision-free-test path, and it is the ONLY case that still gets the analytic floor below.
    // See the note at that floor for why every other case lost it.
    constexpr bool query_is_open_world = requires { typename Q::open_world_tag; };

    const glm::vec3 wish = wish_velocity(intent, tuning, state.mode, yawRadians, pitchRadians, moveSpeed);
    glm::vec3 delta{0.0f};
    if (state.mode == MoveMode::Fly) {
        delta = wish * dt; // the tool responds instantly; only the body has inertia
    } else {
        // Goal 234: the body accelerates. `wish` is the TARGET velocity in m/s; the ramp is what
        // turns Shift from an instant x4 into a sprint you have to build up to (0.70 s to 7 m/s at
        // 10 m/s^2) and let go of (0.50 s back to a stop at 14).
        accelerate_ground(state.horizontal_velocity, glm::vec2{wish.x, wish.z}, tuning, dt);
        delta = glm::vec3{state.horizontal_velocity.x, 0.0f, state.horizontal_velocity.y} * dt;
    }

    if (state.mode == MoveMode::Fly) {
        // Fly keeps its old meaning exactly: no gravity, no stance, vertical strafe along world up
        // (wish_velocity already folded Space/Ctrl in). Collision still applies.
        const glm::vec3 feet = eyePosition - glm::vec3{0.0f, tuning.eye_height, 0.0f};
        const collision::Aabb body =
            collision::Aabb::upright(feet, tuning.body_half_width, tuning.body_height);
        collision::SweepParams sweep;
        sweep.step_height = 0.0f;
        eyePosition += collision::move_and_slide(query, body, delta, sweep).delta;
        state.stance = Stance::Airborne;
        state.vertical_velocity = 0.0f;
        state.coyote_remaining = 0.0f;
        state.jump_buffer_remaining = 0.0f;
        state.eye_smooth_offset = 0.0f;
        return result;
    }

    // --- water ------------------------------------------------------------------------------------
    // Swimming needs a genuinely submerged column, not just feet below the plane: standing in a
    // puddle on ground above sea level is walking, and always was.
    const float feetY = eyePosition.y - tuning.eye_height;
    const bool inWater = feetY < sense.water_surface_y && sense.ground_height < sense.water_surface_y;
    const float submersion = inWater ? std::min(sense.water_surface_y - feetY, 1.0f) : 0.0f;

    // --- vertical ----------------------------------------------------------------------------------
    if (inWater) {
        state.stance = Stance::Swimming;
        integrate_swim(state, tuning, intent, submersion, dt);
    } else {
        state.vertical_velocity += tuning.gravity * dt;
    }

    // --- jump --------------------------------------------------------------------------------------
    // Swimming ignores the jump machinery entirely (Space is a swim kick down there, handled
    // above); on land the state machine owns Space.
    if (!inWater) {
        result.jumped = update_jump(state, tuning, intent.jump_pressed, dt).jumped;
    } else if (intent.jump_pressed) {
        state.jump_buffer_remaining = tuning.jump_buffer_time; // buffered for the moment you surface
    }

    delta.y += state.vertical_velocity * dt;

    // --- sweep -------------------------------------------------------------------------------------
    const glm::vec3 feet = eyePosition - glm::vec3{0.0f, tuning.eye_height, 0.0f};
    const collision::Aabb body = collision::Aabb::upright(feet, tuning.body_half_width, tuning.body_height);
    collision::SweepParams sweep;
    sweep.step_height = tuning.step_height;
    const collision::SweepResult moved = collision::move_and_slide(query, body, delta, sweep);
    eyePosition += moved.delta;
    result.stepped_up = moved.stepped_up;
    result.started_inside = moved.started_inside;

    // --- swim-to-shore assist (A5) -------------------------------------------------------------------
    // rlVoxel's "liquid pop-up": swimming into a bank is the one case where a body that is doing
    // everything right still gets stuck, because buoyancy holds it at a surface *below* the lip.
    // If the horizontal move was refused and the same body one probe-height higher is free, kick
    // upward -- the next tick's horizontal move then succeeds on its own.
    if (state.stance == Stance::Swimming && (moved.blocked_x || moved.blocked_z)) {
        const collision::Aabb lifted =
            body.translated(moved.delta + glm::vec3{0.0f, tuning.shore_pop_probe, 0.0f});
        if (!query.overlaps_solid(lifted)) {
            state.vertical_velocity = std::max(state.vertical_velocity, tuning.shore_pop_impulse);
            result.shore_popped = true;
        }
    }

    // --- stance ---------------------------------------------------------------------------------------
    if (moved.grounded || (moved.blocked_y && delta.y < 0.0f)) {
        if (previousStance != Stance::Grounded) {
            result.landed = true;
            result.impact_speed = std::abs(state.vertical_velocity);
        }
        state.stance = Stance::Grounded;
        state.vertical_velocity = 0.0f;
        state.coyote_remaining = tuning.coyote_time; // refreshed while standing; spent once airborne
    } else if (moved.blocked_y && delta.y > 0.0f) {
        state.vertical_velocity = 0.0f; // bumped the head
        if (!inWater) {
            state.stance = Stance::Airborne;
        }
    } else if (!inWater) {
        state.stance = Stance::Airborne;
    }

    // --- the analytic backstop, and why it is gone --------------------------------------------------
    // There used to be a clamp here: eyePosition.y >= sense.ground_height + eye_height, with a
    // comment saying it "should never fire" with collision on and that it was "the one thing
    // standing between a query gap and a fall through the world."
    //
    // Both halves of that were true, and together they made it a liability. A save that should
    // never fire, firing silently, is a bug detector wired to a mute button: with the old 16 m
    // cached collider, a fast camera left the cache, the query said air, and the clamp quietly
    // put the body back on the analytic surface -- so the query gap that IS the "I clip through
    // blocks" complaint never showed up as anything at all.
    //
    // Prompt 003 goal 228 removes it. The octree query has no edge (world/collision/
    // octree_collider.hpp), so a gap provably cannot come from running out of cache; and the
    // caller now COUNTS ticks that end with the body inside solid and logs the first one with its
    // position. A hidden save became a visible bug, which is the only form in which it can be
    // fixed.
    //
    // `sense.ground_height` is still used above -- it is what tells the swimmer whether the column
    // is genuinely submerged -- it just no longer teleports the body.
    if (query_is_open_world) {
        // --noclip and the collision-free tests still need a floor, or the body falls forever.
        // This is the ONLY remaining use, and it is explicitly the no-collision path.
        const float standingEyeY = sense.ground_height + tuning.eye_height;
        if (eyePosition.y <= standingEyeY) {
            eyePosition.y = standingEyeY;
            state.vertical_velocity = 0.0f;
            if (state.stance != Stance::Swimming) {
                if (previousStance == Stance::Airborne) {
                    result.landed = true;
                }
                state.stance = Stance::Grounded;
                state.coyote_remaining = tuning.coyote_time;
            }
        }
    }

    update_eye_smoothing(state, tuning, eyePosition.y, previousEyeY, dt);
    return result;
}

} // namespace world::player
