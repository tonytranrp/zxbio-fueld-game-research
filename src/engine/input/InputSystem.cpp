#include "InputSystem.hpp"
#include "engine/events/input/InputEvents.hpp"
#include "engine/events/mouse/MouseEvents.hpp"
#include "engine/events/window/WindowEvents.hpp"
#include "engine/runtime/typed/Events.hpp"
#include <raylib.h>

namespace biofuel::engine::input {

bool InputSystem::poll() noexcept {
    bool keyPressedThisPoll = false;
    for (i32 key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
        keyPressedThisPoll = true;
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::input::KeyPressed>({key, ctrl, shift, alt});
    }

    const Vector2 mouse = GetMousePosition();
    for (const i32 btn : {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE}) {
        if (IsMouseButtonPressed(btn)) {
            ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::mouse::Pressed>({btn, static_cast<f32>(mouse.x), static_cast<f32>(mouse.y)});
        }
        if (IsMouseButtonReleased(btn)) {
            ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::mouse::Released>({btn, static_cast<f32>(mouse.x), static_cast<f32>(mouse.y)});
        }
    }

    const Vector2 scroll = GetMouseWheelMoveV();
    if (scroll.x != 0.0f || scroll.y != 0.0f) {
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::mouse::Scrolled>({static_cast<f32>(scroll.x), static_cast<f32>(scroll.y)});
    }

    if (WindowShouldClose()) {
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::window::CloseRequested>();
    }

    return keyPressedThisPoll;
}

} // namespace biofuel::engine::input
