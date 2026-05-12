#pragma once

#include "engine/core/Types.hpp"
#include <string_view>

// ------------------------------------------------------------------------------
// Animation Events - Screen transition lifecycle and effect coordination
// Fired by ScreenManager and engine animation to coordinate visual effects.
// ------------------------------------------------------------------------------

namespace biofuel::engine::events::animation {

struct ScreenTransitionStartedEvent {
    std::string_view screenName;  // Name of the screen starting transition
    bool isEntering;             // true = TransitionIn, false = TransitionOut
};

struct ScreenTransitionCompletedEvent {
    std::string_view screenName;
    bool isEntering;
};

struct ScreenOverlayFadeStartedEvent {
    u8 targetAlpha;
    f32 duration;
};

struct ScreenOverlayFadeCompletedEvent {
    u8 finalAlpha;
};

} // namespace biofuel::engine::events::animation
