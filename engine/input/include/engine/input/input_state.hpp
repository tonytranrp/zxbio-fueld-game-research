#pragma once

#include "engine/core/math.hpp"

namespace engine::input {

// Plain snapshot of what the player is currently doing, written by the GLFW callback layer
// (glfw_input.hpp) and read by whatever movement system cares (PHASE_1_BRIEF.md §6). Deliberately
// semantic ("move_forward"), not raw keycodes: the key mapping is the callback layer's concern,
// and consumers stay testable with hand-built snapshots and portable to any future input source.
// No GLFW types anywhere in this header.
struct InputState {
    bool move_forward = false;  // W
    bool move_back = false;     // S
    bool move_left = false;     // A
    bool move_right = false;    // D
    bool move_up = false;       // Space
    bool move_down = false;     // Left Ctrl
    bool speed_boost = false;   // Left Shift
    bool look_active = false;   // right mouse button held (cursor captured while true)
    bool quit_requested = false; // Escape -- reported here; whether to actually quit is app policy
    bool pending_walk_toggle = false; // G pressed since last take_walk_toggle() (edge, not level)

    // Cursor movement in pixels accumulated since the last take_look_delta(), only while
    // look_active. Accumulation + explicit take keeps callback cadence (per event) decoupled from
    // consumption cadence (per frame).
    glm::vec2 pending_look_delta{0.0f};

    [[nodiscard]] glm::vec2 take_look_delta() noexcept {
        const glm::vec2 delta = pending_look_delta;
        pending_look_delta = glm::vec2{0.0f};
        return delta;
    }

    // Same accumulate-then-take pattern for the fly/walk mode toggle: a key PRESS is an event,
    // not a held state, and must fire exactly once per press regardless of frame timing.
    [[nodiscard]] bool take_walk_toggle() noexcept {
        const bool toggled = pending_walk_toggle;
        pending_walk_toggle = false;
        return toggled;
    }
};

} // namespace engine::input
