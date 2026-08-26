#include "PauseController.hpp"
#include "PausePopupScreen.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/ui/Screen.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "game/screens/GameScreenIds.hpp"
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
    constexpr auto Loading = screen_id::Loading;
    constexpr auto PausePopup = screen_id::PausePopup;
    constexpr auto Idle = screen_id::Idle;
    constexpr auto Video = screen_id::Video;
    constexpr auto MainMenu = screen_id::MainMenu;
    constexpr auto Exploration = screen_id::Exploration;
    switch (current->screenId()) {
    case Loading:
    case PausePopup:
    case Idle:
    case Video:
    case Unknown:
    case Count:
        return false;
    case MainMenu:
        return true;
    case Exploration:
        return true;
    }

    return false;
}

} // namespace biofuel::game::screens
