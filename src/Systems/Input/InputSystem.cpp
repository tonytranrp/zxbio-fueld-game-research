#include "InputSystem.hpp"
#include "Data/Data.hpp"
#include "Data/event/input/InputEvents.hpp"
#include "Data/event/mouse/MouseEvents.hpp"
#include "Data/event/window/WindowEvents.hpp"
#include <raylib.h>

namespace biofuel::systems::input {

void InputSystem::poll() {
    auto& bus = Data::eventBus();

    for (i32 key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        bus.trigger(event::input::KeyPressedEvent{key, ctrl, shift, alt});
    }

    const Vector2 mouse = GetMousePosition();
    for (const i32 btn : {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE}) {
        if (IsMouseButtonPressed(btn)) {
            bus.trigger(event::mouse::MousePressedEvent{btn, mouse.x, mouse.y});
        }
        if (IsMouseButtonReleased(btn)) {
            bus.trigger(event::mouse::MouseReleasedEvent{btn, mouse.x, mouse.y});
        }
    }

    const Vector2 scroll = GetMouseWheelMoveV();
    if (scroll.x != 0.0f || scroll.y != 0.0f) {
        bus.trigger(event::mouse::MouseScrolledEvent{scroll.x, scroll.y});
    }

    if (WindowShouldClose()) {
        bus.trigger(event::window::WindowCloseRequestedEvent{});
    }
}

} // namespace biofuel::systems::input
