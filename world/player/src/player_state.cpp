#include "world/player/player_state.hpp"

#include <algorithm>
#include <cmath>

namespace world::player {

float target_ground_speed(const PlayerIntent& intent, const PlayerTuning& tuning) noexcept {
    return intent.boost ? tuning.sprint_speed : tuning.walk_speed;
}

float directional_speed_factor(float forward, float right, const PlayerTuning& tuning) noexcept {
    // Scale the two components of a UNIT direction anisotropically and take the length. Straight
    // ahead gives 1, straight back `back_speed_factor`, straight sideways `lateral_speed_factor`,
    // and every diagonal an ellipse between them -- one expression, no state machine.
    const float alongFactor = forward >= 0.0f ? 1.0f : tuning.back_speed_factor;
    const float f = forward * alongFactor;
    const float r = right * tuning.lateral_speed_factor;
    return std::sqrt(f * f + r * r);
}

void accelerate_ground(glm::vec2& velocity, const glm::vec2& target, const PlayerTuning& tuning,
                       float dt) noexcept {
    const glm::vec2 gap = target - velocity;
    const float distance = glm::length(gap);
    if (distance <= 1.0e-6f) {
        velocity = target;
        return;
    }
    // Speeding up or slowing down is decided by whether the target is FASTER than the current
    // speed, not by whether there is input: turning at speed is a direction change at constant
    // magnitude and should not be charged the braking rate.
    const float rate =
        glm::length(target) >= glm::length(velocity) ? tuning.ground_accel : tuning.ground_decel;
    const float step = rate * dt;
    velocity = step >= distance ? target : velocity + gap * (step / distance);
}

glm::vec2 update_slide(PlayerState& state, const PlayerTuning& tuning, const WorldSense& sense, bool grounded,
                       float dt) noexcept {
    const bool tooSteep = grounded && sense.ground_slope_radians > tuning.max_walk_slope_radians;
    if (!tooSteep) {
        // Bleed off at the braking rate. Not instant: stepping from a 41 degree face onto a 39
        // degree one should not stop the body dead, and a hard reset here is what would make the
        // climb/slide boundary jitter.
        state.slide_speed = std::max(0.0f, state.slide_speed - tuning.ground_decel * dt);
    } else {
        const float t = sense.ground_slope_radians;
        const float mu = std::tan(tuning.max_walk_slope_radians);
        const float along = std::abs(tuning.gravity) * (std::sin(t) - mu * std::cos(t));
        state.slide_speed = std::min(state.slide_speed + std::max(along, 0.0f) * dt, tuning.max_slide_speed);
    }
    return state.slide_speed <= 0.0f ? glm::vec2{0.0f} : -sense.ground_uphill * state.slide_speed;
}

glm::vec3 wish_velocity(const PlayerIntent& intent, const PlayerTuning& tuning, MoveMode mode,
                        float yawRadians, float pitchRadians, float moveSpeed) noexcept {
    constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};
    glm::vec3 wish{0.0f};
    float speed = moveSpeed;

    if (mode == MoveMode::Fly) {
        // Full free-flight: "forward" includes pitch, vertical strafe along world up. Rebuilt from
        // the two angles rather than taken from the transform so this stays a pure function.
        const float cp = std::cos(pitchRadians);
        const glm::vec3 forward{-std::sin(yawRadians) * cp, std::sin(pitchRadians),
                                -std::cos(yawRadians) * cp};
        const glm::vec3 right{std::cos(yawRadians), 0.0f, -std::sin(yawRadians)};
        if (intent.forward) {
            wish += forward;
        }
        if (intent.back) {
            wish -= forward;
        }
        if (intent.right) {
            wish += right;
        }
        if (intent.left) {
            wish -= right;
        }
        if (intent.up) {
            wish += kWorldUp;
        }
        if (intent.down) {
            wish -= kWorldUp;
        }
    } else {
        // Walk follows YAW only -- looking at the ground must not slow you down -- and never takes
        // a vertical component from input; that axis belongs to gravity, the jump, and buoyancy.
        const glm::vec3 forward{-std::sin(yawRadians), 0.0f, -std::cos(yawRadians)};
        const glm::vec3 right{std::cos(yawRadians), 0.0f, -std::sin(yawRadians)};
        if (intent.forward) {
            wish += forward;
        }
        if (intent.back) {
            wish -= forward;
        }
        if (intent.right) {
            wish += right;
        }
        if (intent.left) {
            wish -= right;
        }
        // Walk speed comes from the tuning in m/s (goal 232), NOT from the spectator's move_speed
        // times a factor. `moveSpeed` is the fly camera's number and the walking path ignores it.
        if (glm::length(wish) > 1.0e-6f) {
            const glm::vec3 unit = glm::normalize(wish);
            const float alongForward = glm::dot(unit, forward);
            const float alongRight = glm::dot(unit, right);
            speed = target_ground_speed(intent, tuning) *
                    directional_speed_factor(alongForward, alongRight, tuning);
        }
        return glm::length(wish) > 1.0e-6f ? glm::normalize(wish) * speed : glm::vec3{0.0f};
    }

    if (intent.boost) {
        speed *= tuning.fly_boost_factor;
    }
    const float length = glm::length(wish);
    if (length <= 0.0f) {
        return glm::vec3{0.0f};
    }
    return wish / length * speed; // normalized: diagonals are not faster
}

JumpOutcome update_jump(PlayerState& state, const PlayerTuning& tuning, bool jumpPressed, float dt) noexcept {
    JumpOutcome outcome;

    // A press is remembered for jump_buffer_time. This is what makes a jump entered slightly before
    // landing fire on touchdown instead of being swallowed by the frame it arrived in.
    if (jumpPressed) {
        state.jump_buffer_remaining = tuning.jump_buffer_time;
    }

    // Coyote credit is refreshed every grounded tick (see step_player) and spent while airborne, so
    // it only ever counts down here.
    state.coyote_remaining = std::max(0.0f, state.coyote_remaining - dt);
    state.jump_buffer_remaining = std::max(0.0f, state.jump_buffer_remaining - dt);

    const bool mayJump = state.stance == Stance::Grounded || state.coyote_remaining > 0.0f;
    if (mayJump && state.jump_buffer_remaining > 0.0f) {
        state.vertical_velocity = tuning.jump_speed;
        state.stance = Stance::Airborne;
        state.coyote_remaining = 0.0f;      // one jump per departure from the ground
        state.jump_buffer_remaining = 0.0f; // consumed, not merely expired
        outcome.jumped = true;
    }
    return outcome;
}

void integrate_swim(PlayerState& state, const PlayerTuning& tuning, const PlayerIntent& intent,
                    float submersion, float dt) noexcept {
    // Passive equilibrium first (goal 79's model, unchanged): upthrust proportional to submersion
    // up to one voxel, damped by drag, floats the feet swim_equilibrium_depth under the surface.
    state.vertical_velocity += (tuning.gravity + kWaterPhysics.buoyancy_acceleration * submersion) * dt;
    state.vertical_velocity *= std::exp(-kWaterPhysics.drag * dt);

    // Then the swimmer overrides it (A5). A direct velocity, not an acceleration: buoyancy at full
    // submersion is +32 m/s^2 net, so any thrust small enough to feel like swimming would simply
    // lose to it. Releasing the key hands the column straight back to the equilibrium above.
    const float kick = static_cast<float>(intent.up ? 1 : 0) - static_cast<float>(intent.down ? 1 : 0);
    if (kick != 0.0f) {
        state.vertical_velocity = kick * tuning.swim_speed;
    }
}

void update_eye_smoothing(PlayerState& state, const PlayerTuning& tuning, float physicalEyeY,
                          float previousPhysicalEyeY, float dt) noexcept {
    if (state.stance != Stance::Grounded || tuning.eye_smooth_tau <= 0.0f) {
        state.eye_smooth_offset = 0.0f; // a fall must never leave the view trailing the body
        return;
    }
    // The rendered eye follows the physical one with a first-order lag. Writing that in terms of
    // the OFFSET (rendered - physical) collapses to one line: if the body moved by d this tick and
    // the offset was o, the new offset is (o - d) * exp(-dt/tau). Standing still it decays to zero;
    // a step-up of d puts the view d below the body and it catches up smoothly.
    const float moved = physicalEyeY - previousPhysicalEyeY;
    const float decay = std::exp(-dt / tuning.eye_smooth_tau);
    state.eye_smooth_offset = (state.eye_smooth_offset - moved) * decay;
    state.eye_smooth_offset =
        std::clamp(state.eye_smooth_offset, -tuning.eye_smooth_max_lag, tuning.eye_smooth_max_lag);
}

} // namespace world::player
