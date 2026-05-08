#pragma once

// ------------------------------------------------------------------------------
// Mouse Events - Mouse movement, clicks, scroll
// ------------------------------------------------------------------------------
namespace biofuel::event::mouse {

struct MouseMovedEvent {
    float x;
    float y;
    float deltaX;
    float deltaY;
};

struct MousePressedEvent {
    int button;     // MouseButton enum
    float x;
    float y;
};

struct MouseReleasedEvent {
    int button;
    float x;
    float y;
};

struct MouseScrolledEvent {
    float scrollX;
    float scrollY;
};

} // namespace biofuel::event::mouse
