#include "GameApp.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/debug/DebugOverlayService.hpp"
#include "game/screens/GameScreenCatalog.hpp"
#include "game/screens/loading/LoadingScreen.hpp"
#include "game/screens/pause_popup/PauseController.hpp"
#include <raylib.h>
#include <utility>

namespace biofuel::game::app {

::biofuel::engine::app::Application makeApplication() {
    ::biofuel::engine::app::Application::Config config{
        .title = "Biofuel Game - Fuel Farm",
        .width = 1280,
        .height = 720,
        // Vsync caps rendering to the display's real refresh rate; targetFps is
        // a backstop in case vsync doesn't engage (e.g. some borderless-window
        // driver configurations). Previously both were disabled/uncapped, which
        // measured 700-3000fps redrawing identical frames for no visual benefit.
        .targetFps = 240,
        .fullscreen = false,
        .resizable = true,
        .vsync = true,
        .globalInput = []() {
            ::biofuel::game::screens::PauseController::handleGlobalInput();

            // Debug overlay hotkeys (work in all build configs):
            //   F3 = toggle the whole overlay
            //   F4 = toggle the Memory panel (sorted resource breakdown)
            //   F5 = toggle the Assets panel
            //   F7 = toggle the Frame Timing panel (FPS / frame ms)
            // F6 is free (the voxel world that used to bind it was removed).
            auto& overlay = ::biofuel::engine::runtime::Runtime::debugOverlay();
            if (IsKeyPressed(KEY_F3)) {
                overlay.toggle();
            }
            if (IsKeyPressed(KEY_F4)) {
                overlay.setPanelEnabled<::biofuel::engine::debug::MemoryTelemetryDebugPanel>(
                    !overlay.panelEnabled<::biofuel::engine::debug::MemoryTelemetryDebugPanel>());
            }
            if (IsKeyPressed(KEY_F5)) {
                overlay.setPanelEnabled<::biofuel::engine::debug::AssetDebugPanel>(
                    !overlay.panelEnabled<::biofuel::engine::debug::AssetDebugPanel>());
            }
            if (IsKeyPressed(KEY_F7)) {
                overlay.setPanelEnabled<::biofuel::engine::debug::FrameTimingDebugPanel>(
                    !overlay.panelEnabled<::biofuel::engine::debug::FrameTimingDebugPanel>());
            }
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
