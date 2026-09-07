#include "world/player/view_polish.hpp"

#include <algorithm>
#include <cmath>

namespace world::player {

namespace {

constexpr float kTwoPi = 6.283185307179586f;

} // namespace

float update_view_polish(PlayerState& state, const PlayerTuning& tuning, const ViewPolishSense& sense,
                         const StepResult& step, bool enabled, float dt) noexcept {
    // Decay terms even when disabled, so toggling --no-view-polish mid-run settles the view instead
    // of freezing it at whatever offset it happened to hold.
    state.landing_dip *= std::exp(-dt / tuning.landing_dip_tau);
    // Goal 234: the FOV kick follows ACTUAL SPEED, not the Shift key. With sprint now a ramp
    // rather than an instant multiply, keying the kick off the flag would snap the lens open a
    // second before the body got there -- and would keep it open while sprinting into a wall at
    // zero speed. Zero at walking pace, full at sprint, linear between.
    const float sprintBand = std::max(tuning.sprint_speed - tuning.walk_speed, 1.0e-3f);
    const float sprintFraction =
        std::clamp((sense.horizontal_speed - tuning.walk_speed) / sprintBand, 0.0f, 1.0f);
    const float fovTarget = enabled ? tuning.fov_kick_radians * sprintFraction : 0.0f;
    state.fov_kick += (fovTarget - state.fov_kick) * (1.0f - std::exp(-dt / tuning.fov_kick_tau));

    if (!enabled) {
        return 0.0f;
    }

    if (step.landed) {
        // One-shot dip proportional to how hard you hit, clamped so a fall from orbit does not
        // slam the camera through the floor.
        state.landing_dip =
            std::min(tuning.landing_dip_max, step.impact_speed * tuning.landing_dip_per_speed);
    }

    float offset = -state.landing_dip;

    // Head-bob is keyed to DISTANCE walked, not wall-clock: the pace stays constant whether the
    // frame rate does or not, and stopping mid-stride leaves the phase where it was.
    if (state.mode == MoveMode::Walk && state.stance == Stance::Grounded) {
        state.bob_distance += sense.horizontal_speed * dt;
        const float scale = std::clamp(sense.horizontal_speed / tuning.walk_speed, 0.0f, 1.0f);
        offset += std::sin(state.bob_distance * tuning.bob_frequency * kTwoPi) * tuning.bob_amplitude * scale;
    }

    // The clamp A6's Check asserts against: whatever the terms sum to, polish never moves the eye
    // more than this from the physical pose.
    return std::clamp(offset, -tuning.polish_max_offset, tuning.polish_max_offset);
}

float view_polish_fov_offset(const PlayerState& state) noexcept {
    return state.fov_kick;
}

} // namespace world::player
