#pragma once

namespace biofuel::engine::input {

class InputSystem {
public:
    // Drains Raylib input and publishes typed input/mouse/window events.
    // Returns whether any key press was consumed this poll.
    [[nodiscard]] static bool poll() noexcept;
};

} // namespace biofuel::engine::input
