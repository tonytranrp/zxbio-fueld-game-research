#pragma once

#include "Core/Types.hpp"

// ------------------------------------------------------------------------------
// Mouse Events - Mouse movement, clicks, scroll
// ------------------------------------------------------------------------------
namespace biofuel::event::mouse {

struct MouseMovedEvent {
    f32 x;
    f32 y;
    f32 deltaX;
    f32 deltaY;
};

struct MousePressedEvent {
    i32 button;     // MouseButton enum
    f32 x;
    f32 y;
};

struct MouseReleasedEvent {
    i32 button;
    f32 x;
    f32 y;
};

struct MouseScrolledEvent {
    f32 scrollX;
    f32 scrollY;
};

} // namespace biofuel::event::mouse
