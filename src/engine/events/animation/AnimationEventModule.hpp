#pragma once

#include "engine/events/animation/AnimationEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::animation {
BIOFUEL_EVENT_TAG(ScreenTransitionStarted, ::biofuel::engine::events::animation::ScreenTransitionStartedEvent);
BIOFUEL_EVENT_TAG(ScreenTransitionCompleted, ::biofuel::engine::events::animation::ScreenTransitionCompletedEvent);
} // namespace biofuel::engine::runtime::typed::animation

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(animation::ScreenTransitionStarted, "animation.screen_transition_started");
BIOFUEL_EVENT_SPEC(animation::ScreenTransitionCompleted, "animation.screen_transition_completed");
BIOFUEL_EVENT_MODULE(AnimationEventModule, ScreenEvents,
    animation::ScreenTransitionStarted,
    animation::ScreenTransitionCompleted)
} // namespace biofuel::engine::runtime::typed

