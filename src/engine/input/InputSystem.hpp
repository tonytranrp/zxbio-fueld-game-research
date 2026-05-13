#pragma once

namespace biofuel::engine::input {

class InputSystem {
public:
    static void poll() noexcept;
    [[nodiscard]] static bool keyPressedThisPoll() noexcept;
};

} // namespace biofuel::engine::input
