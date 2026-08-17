#pragma once

#include "engine/events/window/WindowEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::window {
BIOFUEL_EVENT_TAG(CloseRequested, ::biofuel::engine::events::window::WindowCloseRequestedEvent);
} // namespace biofuel::engine::runtime::typed::window

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(window::CloseRequested, "window.close_requested");
BIOFUEL_EVENT_MODULE(WindowEventModule, InputEvents, window::CloseRequested)
} // namespace biofuel::engine::runtime::typed

