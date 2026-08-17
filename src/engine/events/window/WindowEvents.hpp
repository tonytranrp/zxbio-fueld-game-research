#pragma once

// ------------------------------------------------------------------------------
// Window Events - Close requests
// ------------------------------------------------------------------------------
namespace biofuel::engine::events::window {

struct WindowCloseRequestedEvent {
    // Can be intercepted to show "are you sure?" dialog
};

} // namespace biofuel::engine::events::window
