#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "engine/core/LoadingTask.hpp"
#include "game/presentation/effects/ScreenBackdropController.hpp"

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// LoadingScreen — Animated loading bar displayed while engine initializes.
//
// Shown immediately after window creation. Displays a progress bar with
// status text, completing deferred init tasks (shader compilation, etc.).
// After all tasks finish AND the minimum display time (3s) elapses, it
// auto-transitions to MainMenuScreen via replace().
//
// Any key press skips the remaining minimum time once tasks are complete.
// ------------------------------------------------------------------------------
class LoadingScreen final : public ::biofuel::engine::ui::Screen {
    template<typename, typename>
    friend struct ::biofuel::engine::ui::typed::RenderElementExecutor;

public:
    LoadingScreen(i32 width, i32 height, i32 targetFps);

    void onEnter() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::game::screens::screen_id::Loading; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "LoadingScreen"; }

private:
    static constexpr f32 MIN_DISPLAY_SECONDS = 3.0f;
    static constexpr f32 DOTS_INTERVAL = 0.4f;
    static constexpr i32 BAR_WIDTH = 400;
    static constexpr i32 BAR_HEIGHT = 18;
    static constexpr i32 TITLE_SIZE = 40;
    static constexpr i32 STATUS_SIZE = 16;
    static constexpr f32 PROGRESS_LERP_SPEED = 10.0f;

    i32 m_appWidth;
    i32 m_appHeight;
    i32 m_appTargetFps;

    LoadingTaskQueue m_tasks;
    f32 m_elapsed = 0.0f;
    f32 m_displayProgress = 0.0f;
    f32 m_actualProgress = 0.0f;
    bool m_tasksDone = false;
    bool m_allowSkip = false;
    bool m_transitioned = false;
    bool m_reportedStartupMemory = false;
    game::presentation::effects::ScreenBackdropController m_backdrop;

    void buildTasks();
    void transitionToNext();
};

} // namespace biofuel::game::screens
