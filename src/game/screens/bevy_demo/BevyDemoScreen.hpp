#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "engine/core/Types.hpp"
#include <raylib.h>

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// BevyDemoScreen -- Fullscreen tech-demo overlay for the embedded Bevy renderer.
//
// Follows the VideoScreen pattern: onRender composites the frame texture
// BevyRenderService produces each fixed-step tick (that pump itself lives in
// Application::update(), not here -- same as VideoManager's). Reachable via
// the F6 debug hotkey (see GameApp.cpp); ESC pops back off the stack.
//
// Deliberately NOT named GamePlayScreen/placed under screens/gameplay/ --
// ScreenFlowGuard.cmake hard-fails the build on either, a tripwire left after
// the 2026-08-19 voxel/farm-gameplay deletion. This is a different thing: a
// rendering-pipeline tech demo, not "the game."
// ------------------------------------------------------------------------------
class BevyDemoScreen final : public ::biofuel::engine::ui::Screen {
    template<typename, typename>
    friend struct ::biofuel::engine::ui::typed::RenderElementExecutor;

public:
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override {
        return ::biofuel::game::screens::screen_id::BevyDemo;
    }
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "BevyDemoScreen";
    }
};

} // namespace biofuel::game::screens
