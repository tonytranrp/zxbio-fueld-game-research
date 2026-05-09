#pragma once

#include "Core/Types.hpp"

// ------------------------------------------------------------------------------
// Screen Events - Resolution changes, fullscreen toggles
// ------------------------------------------------------------------------------
namespace biofuel::event::screen {

struct ScreenResizedEvent {
    i32 width;
    i32 height;
    i32 prevWidth;
    i32 prevHeight;
};

struct FullscreenToggledEvent {
    bool fullscreen;
};

} // namespace biofuel::event::screen
