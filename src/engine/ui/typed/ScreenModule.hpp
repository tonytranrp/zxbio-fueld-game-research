#pragma once

#include "engine/ui/typed/RenderContext.hpp"
#include "engine/ui/typed/ScreenLifecycle.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include <type_traits>

namespace biofuel::engine::ui::typed {

template<typename TScreen>
struct ScreenState {};

template<typename TScreen>
struct ScreenModule {
    using Screen = TScreen;
    using State = ScreenState<TScreen>;

    static void onEnter(TScreen& screen, LifecycleContext&) {
        screen.onEnter();
    }

    static void onExit(TScreen& screen, LifecycleContext&) {
        screen.onExit();
    }

    static void onPause(TScreen& screen, LifecycleContext&) {
        screen.onPause();
    }

    static void onResume(TScreen& screen, ResumeContext& context) {
        if constexpr (requires(TScreen& typedScreen, ResumeContext& context) {
            typedScreen.onResume(context);
        }) {
            screen.onResume(context);
        } else {
            screen.onResume();
        }
    }

    static void onUpdate(TScreen& screen, UpdateContext& context) {
        screen.onUpdate(context.deltaTime);
    }

    static void onInput(TScreen& screen, InputContext&) {
        screen.onInput();
    }

    static void onRender(TScreen& screen, RenderContext& context) {
        if constexpr (requires(TScreen& typedScreen, RenderContext& renderContext) {
            typedScreen.onRender(renderContext);
        }) {
            screen.onRender(context);
        } else {
            screen.onRender();
        }
    }
};

} // namespace biofuel::engine::ui::typed
