#include "PauseController.hpp"
#include "PausePopupScreen.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/ui/Screen.hpp"
#include "engine/ui/ScreenManager.hpp"
#include <raylib.h>

namespace biofuel::game::screens {

void PauseController::handleGlobalInput() {
    if (!IsKeyPressed(KEY_ESCAPE)) {
        return;
    }
    if (!canPauseCurrentScreen()) {
        return;
    }

    auto& screens = ::biofuel::engine::runtime::Runtime::screen();
    screens.queuePush<PausePopupScreen>();
}

bool PauseController::canPauseCurrentScreen() noexcept {
    auto& screens = ::biofuel::engine::runtime::Runtime::screen();
    if (screens.isTransitioning()) {
        return false;
    }

    const auto* current = screens.currentScreen();
    if (current == nullptr) {
        return false;
    }

    using enum ::biofuel::engine::ui::typed::ScreenId;
    switch (current->screenId()) {
    case Loading:
    case PausePopup:
    case Calibration:
    case Idle:
    case Video:
    case Unknown:
    case Count:
#ifdef BIOFUEL_ENABLE_DEV_SCREENS
    case DevHandLab:
#endif
        return false;
    case MainMenu:
    case Join:
    case GamePlay:
        return true;
    }

    return false;
}

} // namespace biofuel::game::screens
