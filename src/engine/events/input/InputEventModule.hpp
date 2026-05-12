#pragma once

#include "engine/events/input/InputEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::input {
BIOFUEL_EVENT_TAG(KeyPressed, ::biofuel::engine::events::input::KeyPressedEvent);
BIOFUEL_EVENT_TAG(KeyReleased, ::biofuel::engine::events::input::KeyReleasedEvent);
BIOFUEL_EVENT_TAG(KeyRepeat, ::biofuel::engine::events::input::KeyRepeatEvent);
} // namespace biofuel::engine::runtime::typed::input

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(input::KeyPressed, "input.key_pressed");
BIOFUEL_EVENT_SPEC(input::KeyReleased, "input.key_released");
BIOFUEL_EVENT_SPEC(input::KeyRepeat, "input.key_repeat");
BIOFUEL_EVENT_MODULE(InputEventModule, InputEvents, input::KeyPressed, input::KeyReleased, input::KeyRepeat)
} // namespace biofuel::engine::runtime::typed

