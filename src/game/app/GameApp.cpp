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
        .targetFps = 0,  // 0 = uncapped framerate (SetTargetFPS(0) disables the limiter)
        .fullscreen = false,
        .resizable = true,
        .globalInput = []() {
            ::biofuel::game::screens::PauseController::handleGlobalInput();

            // Debug overlay hotkeys (work in all build configs):
            //   F3 = toggle the whole overlay
            //   F4 = toggle the Memory panel (sorted resource breakdown)
            //   F5 = toggle the Assets panel
            //   F7 = toggle the Frame Timing panel (FPS / frame ms)
            // NOTE: F6 is intentionally NOT used here -- GamePlayScreen already
            // binds F6 to its rasterized/raymarched voxel-renderer toggle, and a
            // single frame's IsKeyPressed(KEY_F6) would otherwise fire both.
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
