#pragma once

#include "UI/Screen.hpp"
#include "Core/LoadingTask.hpp"

namespace biofuel::ui::screens {

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
class LoadingScreen final : public Screen {
public:
    void onEnter() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

private:
    static constexpr f32 MIN_DISPLAY_SECONDS = 3.0f;
    static constexpr i32 BAR_WIDTH = 400;
    static constexpr i32 BAR_HEIGHT = 18;
    static constexpr i32 TITLE_SIZE = 40;
    static constexpr i32 STATUS_SIZE = 16;
    static constexpr f32 PROGRESS_LERP_SPEED = 10.0f;

    LoadingTaskQueue m_tasks;
    f32 m_elapsed = 0.0f;
    f32 m_displayProgress = 0.0f;
    f32 m_actualProgress = 0.0f;
    bool m_tasksDone = false;
    bool m_allowSkip = false;

    void buildTasks();
    void transitionToNext();
};

} // namespace biofuel::ui::screens
