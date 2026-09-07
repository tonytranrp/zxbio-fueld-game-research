#include "world/player/view_polish.hpp"

#include <algorithm>
#include <cmath>

namespace world::player {

namespace {

constexpr float kTwoPi = 6.283185307179586f;
// Bob amplitude reaches full scale at the normal walking speed (move_speed * walk_speed_factor =
// 10 m/s with the shipped numbers). Creeping does not bob; boosting does not bob harder.
constexpr float kBobFullSpeed = 10.0f;

} // namespace

float update_view_polish(PlayerState& state, const PlayerTuning& tuning, const ViewPolishSense& sense,
                         const StepResult& step, bool enabled, float dt) noexcept {
    // Decay terms even when disabled, so toggling --no-view-polish mid-run settles the view instead
    // of freezing it at whatever offset it happened to hold.
    state.landing_dip *= std::exp(-dt / tuning.landing_dip_tau);
    const float fovTarget = (enabled && sense.boosting) ? tuning.fov_kick_radians : 0.0f;
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
        const float scale = std::clamp(sense.horizontal_speed / kBobFullSpeed, 0.0f, 1.0f);
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
