#pragma once

#include "engine/core/Types.hpp"

// ------------------------------------------------------------------------------
// Mouse Events - Mouse clicks, scroll
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::mouse {

struct MousePressedEvent {
    i32 button = 0; // MouseButton enum
    f32 x = 0.0f;
    f32 y = 0.0f;
};

struct MouseReleasedEvent {
    i32 button = 0;
    f32 x = 0.0f;
    f32 y = 0.0f;
};

struct MouseScrolledEvent {
    f32 scrollX = 0.0f;
    f32 scrollY = 0.0f;
};

} // namespace biofuel::engine::events::mouse
