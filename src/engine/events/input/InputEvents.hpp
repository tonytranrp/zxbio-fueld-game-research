#pragma once

#include "engine/core/Types.hpp"

// ------------------------------------------------------------------------------
// Input Events - Keyboard and gamepad input
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::input {

struct KeyPressedEvent {
    i32 key = 0;    // Raylib key code (e.g. KEY_SPACE)
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct KeyReleasedEvent {
    i32 key = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

} // namespace biofuel::engine::events::input
