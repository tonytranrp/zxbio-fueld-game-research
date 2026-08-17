#pragma once

#include "engine/events/mouse/MouseEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::mouse {
BIOFUEL_EVENT_TAG(Pressed, ::biofuel::engine::events::mouse::MousePressedEvent);
BIOFUEL_EVENT_TAG(Released, ::biofuel::engine::events::mouse::MouseReleasedEvent);
BIOFUEL_EVENT_TAG(Scrolled, ::biofuel::engine::events::mouse::MouseScrolledEvent);
} // namespace biofuel::engine::runtime::typed::mouse

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(mouse::Pressed, "mouse.pressed");
BIOFUEL_EVENT_SPEC(mouse::Released, "mouse.released");
BIOFUEL_EVENT_SPEC(mouse::Scrolled, "mouse.scrolled");
BIOFUEL_EVENT_MODULE(MouseEventModule, InputEvents, mouse::Pressed, mouse::Released, mouse::Scrolled)
} // namespace biofuel::engine::runtime::typed

