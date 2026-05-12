#pragma once

// ------------------------------------------------------------------------------
// Window Events - Focus, minimize, close requests
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::window {

struct WindowFocusedEvent {
    bool focused;
};

struct WindowMinimizedEvent {
    bool minimized;
};

struct WindowCloseRequestedEvent {
    // Can be intercepted to show "are you sure?" dialog
};

} // namespace biofuel::engine::events::window
