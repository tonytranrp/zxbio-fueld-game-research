#pragma once

// ------------------------------------------------------------------------------
// Input Events - Keyboard and gamepad input
// ------------------------------------------------------------------------------
namespace biofuel::event::input {

struct KeyPressedEvent {
    int key;        // Raylib key code (e.g. KEY_SPACE)
    bool ctrl;
    bool shift;
    bool alt;
};

struct KeyReleasedEvent {
    int key;
    bool ctrl;
    bool shift;
    bool alt;
};

struct KeyRepeatEvent {
    int key;
};

} // namespace biofuel::event::input
