#pragma once

#include "engine/ui/Screen.hpp"
#include "engine/ui/typed/RenderContext.hpp"
#include "engine/ui/typed/ScreenLifecycle.hpp"
#include "engine/ui/typed/ScreenModule.hpp"
#include <type_traits>

namespace biofuel::engine::ui::typed {

struct ScreenDispatch {
    using EnterFn = void(*)(::biofuel::engine::ui::Screen&, LifecycleContext&);
    using ExitFn = void(*)(::biofuel::engine::ui::Screen&, LifecycleContext&);
    using PauseFn = void(*)(::biofuel::engine::ui::Screen&, LifecycleContext&);
    using ResumeFn = void(*)(::biofuel::engine::ui::Screen&, ResumeContext&);
    using UpdateFn = void(*)(::biofuel::engine::ui::Screen&, UpdateContext&);
    using InputFn = void(*)(::biofuel::engine::ui::Screen&, InputContext&);
    using RenderFn = void(*)(::biofuel::engine::ui::Screen&, RenderContext&);

    EnterFn onEnter = nullptr;
    ExitFn onExit = nullptr;
    PauseFn onPause = nullptr;
    ResumeFn onResume = nullptr;
    UpdateFn onUpdate = nullptr;
    InputFn onInput = nullptr;
    RenderFn onRender = nullptr;
};

namespace detail {

inline void bridgeEnter(::biofuel::engine::ui::Screen& screen, LifecycleContext&) {
    screen.onEnter();
}

inline void bridgeExit(::biofuel::engine::ui::Screen& screen, LifecycleContext&) {
    screen.onExit();
}

inline void bridgePause(::biofuel::engine::ui::Screen& screen, LifecycleContext&) {
    screen.onPause();
}

inline void bridgeResume(::biofuel::engine::ui::Screen& screen, ResumeContext&) {
    screen.onResume();
}

inline void bridgeUpdate(::biofuel::engine::ui::Screen& screen, UpdateContext& context) {
    screen.onUpdate(context.deltaTime);
}

inline void bridgeInput(::biofuel::engine::ui::Screen& screen, InputContext&) {
    screen.onInput();
}

inline void bridgeRender(::biofuel::engine::ui::Screen& screen, RenderContext&) {
    screen.onRender();
}

template<typename TScreen>
void typedEnter(::biofuel::engine::ui::Screen& screen, LifecycleContext& context) {
    ScreenModule<std::remove_cvref_t<TScreen>>::onEnter(static_cast<TScreen&>(screen), context);
}

template<typename TScreen>
void typedExit(::biofuel::engine::ui::Screen& screen, LifecycleContext& context) {
    ScreenModule<std::remove_cvref_t<TScreen>>::onExit(static_cast<TScreen&>(screen), context);
}

template<typename TScreen>
void typedPause(::biofuel::engine::ui::Screen& screen, LifecycleContext& context) {
    ScreenModule<std::remove_cvref_t<TScreen>>::onPause(static_cast<TScreen&>(screen), context);
}

template<typename TScreen>
void typedResume(::biofuel::engine::ui::Screen& screen, ResumeContext& context) {
    ScreenModule<std::remove_cvref_t<TScreen>>::onResume(static_cast<TScreen&>(screen), context);
}

template<typename TScreen>
void typedUpdate(::biofuel::engine::ui::Screen& screen, UpdateContext& context) {
    ScreenModule<std::remove_cvref_t<TScreen>>::onUpdate(static_cast<TScreen&>(screen), context);
}

template<typename TScreen>
void typedInput(::biofuel::engine::ui::Screen& screen, InputContext& context) {
    ScreenModule<std::remove_cvref_t<TScreen>>::onInput(static_cast<TScreen&>(screen), context);
}

template<typename TScreen>
void typedRender(::biofuel::engine::ui::Screen& screen, RenderContext& context) {
    ScreenModule<std::remove_cvref_t<TScreen>>::onRender(static_cast<TScreen&>(screen), context);
}

} // namespace detail

[[nodiscard]] inline const ScreenDispatch& bridgeScreenDispatch() noexcept {
    static constexpr ScreenDispatch DISPATCH{
        .onEnter = detail::bridgeEnter,
        .onExit = detail::bridgeExit,
        .onPause = detail::bridgePause,
        .onResume = detail::bridgeResume,
        .onUpdate = detail::bridgeUpdate,
        .onInput = detail::bridgeInput,
        .onRender = detail::bridgeRender,
    };
    return DISPATCH;
}

template<typename TScreen>
[[nodiscard]] const ScreenDispatch& typedScreenDispatch() noexcept {
    using CleanScreen = std::remove_cvref_t<TScreen>;
    static constexpr ScreenDispatch DISPATCH{
        .onEnter = detail::typedEnter<CleanScreen>,
        .onExit = detail::typedExit<CleanScreen>,
        .onPause = detail::typedPause<CleanScreen>,
        .onResume = detail::typedResume<CleanScreen>,
        .onUpdate = detail::typedUpdate<CleanScreen>,
        .onInput = detail::typedInput<CleanScreen>,
        .onRender = detail::typedRender<CleanScreen>,
    };
    return DISPATCH;
}

} // namespace biofuel::engine::ui::typed
