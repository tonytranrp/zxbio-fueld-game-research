#pragma once

#include "engine/events/window/WindowEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::window {
BIOFUEL_EVENT_TAG(Focused, ::biofuel::engine::events::window::WindowFocusedEvent);
BIOFUEL_EVENT_TAG(Minimized, ::biofuel::engine::events::window::WindowMinimizedEvent);
BIOFUEL_EVENT_TAG(CloseRequested, ::biofuel::engine::events::window::WindowCloseRequestedEvent);
} // namespace biofuel::engine::runtime::typed::window

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(window::Focused, "window.focused");
BIOFUEL_EVENT_SPEC(window::Minimized, "window.minimized");
BIOFUEL_EVENT_SPEC(window::CloseRequested, "window.close_requested");
BIOFUEL_EVENT_MODULE(WindowEventModule, InputEvents, window::Focused, window::Minimized, window::CloseRequested)
} // namespace biofuel::engine::runtime::typed

