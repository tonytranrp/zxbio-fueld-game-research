#pragma once

#include "world/player/player_state.hpp"

namespace world::player {

// The "feel" layer (Prompt 001 A6): head-bob, landing dip, boost FOV kick. Strictly RENDER-ONLY --
// it returns an offset the camera pose adds, and never touches the physical position, so every
// mechanical check (the walk-violation counter, --autofly, --verify-frame) is unaffected whether
// it runs or not. `polish_max_offset` is the hard clamp that makes that claim testable rather than
// merely intended.
struct ViewPolishSense {
    float horizontal_speed = 0.0f; // metres/second actually travelled this tick
    bool boosting = false;
};

// Advances the polish state one fixed tick and returns the eye-Y offset to add when rendering.
// `enabled == false` decays every term to zero rather than freezing it, so toggling --no-view-polish
// mid-run settles instead of locking the view at whatever offset it had.
float update_view_polish(PlayerState& state, const PlayerTuning& tuning, const ViewPolishSense& sense,
                         const StepResult& step, bool enabled, float dt) noexcept;

// The current additive FOV offset in radians (boost kick), for the lens.
[[nodiscard]] float view_polish_fov_offset(const PlayerState& state) noexcept;

} // namespace world::player
