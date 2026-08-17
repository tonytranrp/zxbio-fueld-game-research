#pragma once

#include "engine/events/screen/ScreenEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::screen {
BIOFUEL_EVENT_TAG(TransitionOverride, ::biofuel::engine::events::screen::ScreenTransitionOverrideEvent);
BIOFUEL_EVENT_TAG(LayerOverride, ::biofuel::engine::events::screen::ScreenLayerOverrideEvent);
BIOFUEL_EVENT_TAG(DebugRenderOverride, ::biofuel::engine::events::screen::ScreenDebugRenderOverrideEvent);
} // namespace biofuel::engine::runtime::typed::screen

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(screen::TransitionOverride, "screen.transition_override");
BIOFUEL_EVENT_SPEC(screen::LayerOverride, "screen.layer_override");
BIOFUEL_EVENT_SPEC(screen::DebugRenderOverride, "screen.debug_render_override");
BIOFUEL_EVENT_MODULE(ScreenEventModule, ScreenEvents,
    screen::TransitionOverride,
    screen::LayerOverride,
    screen::DebugRenderOverride)
} // namespace biofuel::engine::runtime::typed

