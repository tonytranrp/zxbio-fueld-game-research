#include "GameApp.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "game/screens/GameScreenCatalog.hpp"
#include "game/screens/loading/LoadingScreen.hpp"
#include "game/screens/pause_popup/PauseController.hpp"
#include <utility>

namespace biofuel::game::app {

::biofuel::engine::app::Application makeApplication() {
    ::biofuel::engine::app::Application::Config config{
        .title = "Biofuel Game - Fuel Farm",
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .fullscreen = false,
        .resizable = true,
        .globalInput = []() {
            ::biofuel::game::screens::PauseController::handleGlobalInput();
        },
    };

    config.startup = [](const i32 width, const i32 height, const i32 targetFps) {
        auto& screen = ::biofuel::engine::runtime::Runtime::screen();
        screen.setPolicyResolvers(
            &::biofuel::game::screens::transitionPolicyForId,
            &::biofuel::game::screens::stackPolicyForId);
        screen.push<::biofuel::game::screens::LoadingScreen>(width, height, targetFps);
    };

    return ::biofuel::engine::app::Application{std::move(config)};
}

} // namespace biofuel::game::app
