#pragma once

namespace biofuel::game::screens {

class PauseController final {
public:
    static void handleGlobalInput();

private:
    [[nodiscard]] static bool canPauseCurrentScreen() noexcept;
};

} // namespace biofuel::game::screens
