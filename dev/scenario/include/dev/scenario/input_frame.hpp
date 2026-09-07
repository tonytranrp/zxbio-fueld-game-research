#pragma once

#include "engine/core/math.hpp"
#include "world/player/player_state.hpp"

namespace dev::scenario {

// One fixed tick's worth of input. `intent` is world::player::PlayerIntent -- the controller's own
// vocabulary, already independent of GLFW -- rather than a second input type this module would
// then have to translate. That is deliberate and load-bearing: the harness hands this straight to
// the same step_player() the app calls, so there is no path by which a scripted run can exercise a
// simulation the player cannot reach.
struct InputFrame {
    world::player::PlayerIntent intent{};
    // Mouse look runs at RENDER cadence in the app (a 144 Hz mouse must not be sampled at 60 Hz),
    // but a script has no mouse: `look` segments produce their turn as a per-tick pixel delta and
    // the harness applies it exactly where GLFW's would land.
    glm::vec2 look_delta_pixels{0.0f};

    [[nodiscard]] friend bool operator==(const InputFrame&, const InputFrame&) = default;
};

} // namespace dev::scenario
