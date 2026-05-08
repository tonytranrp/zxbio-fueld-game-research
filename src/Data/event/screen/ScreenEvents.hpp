#pragma once

// ------------------------------------------------------------------------------
// Screen Events - Resolution changes, fullscreen toggles
// ------------------------------------------------------------------------------
namespace biofuel::event::screen {

struct ScreenResizedEvent {
    int width;
    int height;
    int prevWidth;
    int prevHeight;
};

struct FullscreenToggledEvent {
    bool fullscreen;
};

} // namespace biofuel::event::screen
