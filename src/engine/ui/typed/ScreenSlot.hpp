#pragma once

#include "engine/ui/Screen.hpp"
#include "engine/ui/typed/ScreenDispatch.hpp"
#include "engine/ui/typed/ScreenLifecycle.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/ui/typed/ScreenTypes.hpp"
#include <memory>
#include <string_view>
#include <utility>

namespace biofuel::engine::ui::typed {

enum class SlotTransitionState : u8 {
    None,
    TransitionIn,
    TransitionOut,
};

struct ScreenTransitionRuntime {
    SlotTransitionState state = SlotTransitionState::None;
    f32 progress = 0.0f;
    TransitionPolicyData policy{};

    [[nodiscard]] f32 alpha() const noexcept {
        const auto eased = policy.easing ? policy.easing(progress) : progress;
        switch (state) {
        case SlotTransitionState::TransitionIn:
            return eased;
        case SlotTransitionState::TransitionOut:
            return 1.0f - eased;
        case SlotTransitionState::None:
            break;
        }
        return 1.0f;
    }

    [[nodiscard]] bool active() const noexcept {
        return state != SlotTransitionState::None;
    }

    [[nodiscard]] bool needsRemoval() const noexcept {
        return state == SlotTransitionState::TransitionOut && progress >= 1.0f;
    }

    void startIn(TransitionPolicyData nextPolicy) noexcept {
        policy = nextPolicy;
        state = SlotTransitionState::TransitionIn;
        progress = 0.0f;
    }

    void startOut() noexcept {
        state = SlotTransitionState::TransitionOut;
        progress = 0.0f;
    }

    [[nodiscard]] bool advance(f32 dt) noexcept {
        if (!active()) {
            return false;
        }

        if (policy.duration > 0.0f) {
            progress += dt / policy.duration;
        } else {
            progress = 1.0f;
        }

        if (progress < 1.0f) {
            return false;
        }

        progress = 1.0f;
        if (state == SlotTransitionState::TransitionIn) {
            state = SlotTransitionState::None;
            return true;
        }

        return true;
    }
};

struct ScreenSlot {
    std::unique_ptr<::biofuel::engine::ui::Screen> screen;
    const ScreenDispatch* dispatch = &bridgeScreenDispatch();
    ScreenId id = ScreenId::Unknown;
    std::string_view name = "Screen";
    ScreenTransitionRuntime transition{};

    ScreenSlot() = default;

    ScreenSlot(std::unique_ptr<::biofuel::engine::ui::Screen> nextScreen, TransitionPolicyData policy)
        : screen(std::move(nextScreen))
    {
        if (screen) {
            id = screen->screenId();
            name = screen->getName();
        }
        transition.policy = policy;
    }

    template<typename TScreen>
    static ScreenSlot typed(std::unique_ptr<TScreen> nextScreen, TransitionPolicyData policy) {
        ScreenSlot slot{std::move(nextScreen), policy};
        slot.dispatch = &typedScreenDispatch<TScreen>();
        return slot;
    }

    [[nodiscard]] ::biofuel::engine::ui::Screen* get() const noexcept {
        return screen.get();
    }

    [[nodiscard]] ::biofuel::engine::ui::Screen& operator*() const noexcept {
        return *screen;
    }

    [[nodiscard]] ::biofuel::engine::ui::Screen* operator->() const noexcept {
        return screen.get();
    }
};

} // namespace biofuel::engine::ui::typed
