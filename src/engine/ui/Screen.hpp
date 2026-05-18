#pragma once

#include "engine/core/Types.hpp"
#include "engine/ui/typed/ScreenTypes.hpp"

namespace biofuel {
class LoadingTaskQueue;
}

namespace biofuel::engine::ui {

namespace typed {
template<typename, typename>
struct RenderElementExecutor;
}

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

    // Easing function type for transitions (matches engine animation::Easing)
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

    // ---- Async loading (called by ScreenManager before onEnter) ----
    // Screens with heavy init should override this to register tasks.
    // ScreenManager processes tasks before calling onEnter().
    virtual void buildLoadingTasks(::biofuel::LoadingTaskQueue& tasks) { static_cast<void>(tasks); }

    // ---- Accessors ----
    [[nodiscard]] ScreenManager* manager() const noexcept { return m_manager; }
    [[nodiscard]] virtual typed::ScreenId screenId() const noexcept { return typed::ScreenId::Unknown; }
    [[nodiscard]] virtual std::string_view getName() const noexcept { return "Screen"; }

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
    const auto eased = [this](const f32 value) noexcept {
        return m_transitionEasing ? m_transitionEasing(value) : value;
    };

    if (m_transitionState == TransitionState::TransitionIn) {
        return eased(m_transitionProgress);
    }
    if (m_transitionState == TransitionState::TransitionOut) {
        return 1.0f - eased(m_transitionProgress);
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

inline void Screen::startTransitionIn() {
    m_transitionState = TransitionState::TransitionIn;
    m_transitionProgress = 0.0f;
}

inline void Screen::startTransitionOut() {
    m_transitionState = TransitionState::TransitionOut;
    m_transitionProgress = 0.0f;
}

} // namespace biofuel::engine::ui
