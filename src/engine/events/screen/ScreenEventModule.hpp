#pragma once

#include "engine/events/screen/ScreenEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::screen {
BIOFUEL_EVENT_TAG(Resized, ::biofuel::engine::events::screen::ScreenResizedEvent);
BIOFUEL_EVENT_TAG(FullscreenToggled, ::biofuel::engine::events::screen::FullscreenToggledEvent);
BIOFUEL_EVENT_TAG(TransitionOverride, ::biofuel::engine::events::screen::ScreenTransitionOverrideEvent);
BIOFUEL_EVENT_TAG(LayerOverride, ::biofuel::engine::events::screen::ScreenLayerOverrideEvent);
BIOFUEL_EVENT_TAG(DebugRenderOverride, ::biofuel::engine::events::screen::ScreenDebugRenderOverrideEvent);
} // namespace biofuel::engine::runtime::typed::screen

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(screen::Resized, "screen.resized");
BIOFUEL_EVENT_SPEC(screen::FullscreenToggled, "screen.fullscreen_toggled");
BIOFUEL_EVENT_SPEC(screen::TransitionOverride, "screen.transition_override");
BIOFUEL_EVENT_SPEC(screen::LayerOverride, "screen.layer_override");
BIOFUEL_EVENT_SPEC(screen::DebugRenderOverride, "screen.debug_render_override");
BIOFUEL_EVENT_MODULE(ScreenEventModule, ScreenEvents,
    screen::Resized,
    screen::FullscreenToggled,
    screen::TransitionOverride,
    screen::LayerOverride,
    screen::DebugRenderOverride)
} // namespace biofuel::engine::runtime::typed

