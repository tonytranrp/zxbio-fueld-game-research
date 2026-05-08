#pragma once

#include "Core/Types.hpp"

namespace biofuel::ui {

class ScreenManager;

// ------------------------------------------------------------------------------
// Screen - Abstract base for all game screens
// Owned exclusively by ScreenManager. Screens are non-copyable, non-movable.
// ------------------------------------------------------------------------------
class Screen {
    friend class ScreenManager;

public:
    enum class TransitionState : u8 {
        None,
        TransitionIn,
        TransitionOut
    };

    // Easing function type for transitions (matches AnimationController::Easing)
    using EasingFn = f32(*)(f32);

    Screen() = default;
    virtual ~Screen() noexcept = default;

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;
    Screen(Screen&&) = delete;
    Screen& operator=(Screen&&) = delete;

    // ---- Lifecycle (called by ScreenManager) ----
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onPause() {}
    virtual void onResume() {}
    virtual void onUpdate(f32 deltaTime) = 0;
    virtual void onRender() = 0;
    virtual void onInput() {}

    // ---- Accessors ----
    [[nodiscard]] ScreenManager* manager() const noexcept { return m_manager; }

    [[nodiscard]] f32 transitionAlpha() const noexcept;
    [[nodiscard]] bool needsRemoval() const noexcept;
    [[nodiscard]] bool isTransitioning() const noexcept;

    [[nodiscard]] bool passthroughRender() const noexcept { return m_passthroughRender; }
    [[nodiscard]] bool passthroughUpdate() const noexcept { return m_passthroughUpdate; }
    [[nodiscard]] bool passthroughInput() const noexcept { return m_passthroughInput; }

    void setRenderPassthrough(bool v) noexcept { m_passthroughRender = v; }
    void setUpdatePassthrough(bool v) noexcept { m_passthroughUpdate = v; }
    void setInputPassthrough(bool v) noexcept { m_passthroughInput = v; }

    // ---- Transition configuration ----
    void setTransitionDuration(f32 seconds) noexcept { m_transitionDuration = seconds; }
    void setTransitionEasing(EasingFn fn) noexcept { m_transitionEasing = fn; }

protected:
    void startTransitionIn();
    void startTransitionOut();

private:
    void setManager(ScreenManager* mgr) noexcept { m_manager = mgr; }

    ScreenManager* m_manager = nullptr;

    TransitionState m_transitionState = TransitionState::None;
    f32 m_transitionProgress = 0.0f;
    f32 m_transitionDuration = 0.5f;
    EasingFn m_transitionEasing = nullptr; // null = use ScreenManager default

    bool m_passthroughRender = false;
    bool m_passthroughUpdate = false;
    bool m_passthroughInput = false;
};

// ---- Inline implementations ----

inline f32 Screen::transitionAlpha() const noexcept {
    if (m_transitionState == TransitionState::TransitionIn) {
        return m_transitionProgress;
    }
    if (m_transitionState == TransitionState::TransitionOut) {
        return 1.0f - m_transitionProgress;
    }
    return 1.0f;
}

inline bool Screen::needsRemoval() const noexcept {
    return m_transitionState == TransitionState::TransitionOut
        && m_transitionProgress >= 1.0f;
}

inline bool Screen::isTransitioning() const noexcept {
    return m_transitionState != TransitionState::None;
}

} // namespace biofuel::ui