#pragma once

#include "engine/core/Types.hpp"
#include <string_view>

// ------------------------------------------------------------------------------
// Animation Events - Screen transition lifecycle
// Fired by ScreenManager and engine animation to coordinate screen changes.
// ------------------------------------------------------------------------------

namespace biofuel::engine::events::animation {

struct ScreenTransitionStartedEvent {
    std::string_view screenName;  // Name of the screen starting transition
    bool isEntering = false;      // true = TransitionIn, false = TransitionOut
};

struct ScreenTransitionCompletedEvent {
    std::string_view screenName;
    bool isEntering = false;
};

} // namespace biofuel::engine::events::animation
