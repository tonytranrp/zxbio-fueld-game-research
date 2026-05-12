#pragma once

#include "engine/core/Types.hpp"

// ------------------------------------------------------------------------------
// Input Events - Keyboard and gamepad input
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::input {

struct KeyPressedEvent {
    i32 key;        // Raylib key code (e.g. KEY_SPACE)
    bool ctrl;
    bool shift;
    bool alt;
};

struct KeyReleasedEvent {
    i32 key;
    bool ctrl;
    bool shift;
    bool alt;
};

struct KeyRepeatEvent {
    i32 key;
};

} // namespace biofuel::engine::events::input
