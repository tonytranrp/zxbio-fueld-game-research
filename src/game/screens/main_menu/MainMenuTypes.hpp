#pragma once

#include "engine/core/Types.hpp"
#include "engine/animation/Easing.hpp"
#include "game/presentation/widgets/MenuHelper.hpp"

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// IntroPhase — Controls text fade-in sequence
// ------------------------------------------------------------------------------
enum class IntroPhase {
    WaitingForTransition,   // crossfade in progress, bg shader only
    TitleFade,              // title fading in
    SubtitleFade,           // subtitle fading in
    HintsFade,              // controls hint fading in
    MenuFade,               // menu bar fading in
    Done,                   // all elements visible, input active
};

// ------------------------------------------------------------------------------
// TextFade — Fade-in state for a single text element
// ------------------------------------------------------------------------------
struct TextFade {
    f32 delay    = 0.0f;   // seconds after previous element starts
    f32 duration = 0.5f;   // how long this fade takes
    f32 elapsed  = 0.0f;   // time since this fade started

    [[nodiscard]] f32 alpha() const noexcept {
        if (elapsed <= 0.0f) return 0.0f;
        const f32 t = (elapsed >= duration) ? 1.0f : elapsed / duration;
        return ::biofuel::engine::animation::Easing::easeOutCubic(t);
    }
};

// ------------------------------------------------------------------------------
// UIDismissState — Dismiss animation state
// UI elements slide off-screen in a staggered cascade when an action is selected.
// ------------------------------------------------------------------------------
struct UIDismissState {
    bool active  = false;
    f32  elapsed = 0.0f;

    static constexpr i32 ELEM_MENU   = 0;
    static constexpr i32 ELEM_TITLE  = 1;
    static constexpr i32 ELEM_HINTS  = 2;
    static constexpr i32 ELEM_FOOTER = 3;
    static constexpr i32 ELEM_COUNT  = 4;

    static constexpr f32 STAGGER_DELAY = 0.06f;
    static constexpr f32 ELEM_DURATION = 0.40f;

    [[nodiscard]] static constexpr f32 totalDuration() noexcept {
        return STAGGER_DELAY * static_cast<f32>(ELEM_COUNT - 1) + ELEM_DURATION;
    }

    [[nodiscard]] f32 progress(i32 elementIndex) const noexcept {
        if (!active) return 0.0f;
        const f32 elemStart  = static_cast<f32>(elementIndex) * STAGGER_DELAY;
        const f32 elemElapsed = elapsed - elemStart;
        if (elemElapsed <= 0.0f) return 0.0f;
        const f32 t = (elemElapsed >= ELEM_DURATION) ? 1.0f : elemElapsed / ELEM_DURATION;
        return ::biofuel::engine::animation::Easing::easeInCubic(t);
    }

    [[nodiscard]] bool isDone() const noexcept {
        return active && elapsed >= totalDuration();
    }
};

// ------------------------------------------------------------------------------
// MenuSlideState — Horizontal carousel slide animation state
// ------------------------------------------------------------------------------
struct MenuSlideState {
    i32 direction = 0;
    f32 elapsed   = 0.0f;
    f32 duration  = 0.22f;

    [[nodiscard]] bool active() const noexcept {
        return direction != 0 && elapsed < duration;
    }

    [[nodiscard]] f32 progress() const noexcept {
        if (direction == 0 || duration <= 0.0f) {
            return 1.0f;
        }
        return elapsed >= duration ? 1.0f : elapsed / duration;
    }

    [[nodiscard]] game::presentation::widgets::HorizontalMenuMotion motion() const noexcept {
        const f32 t = ::biofuel::engine::animation::Easing::easeOutCubic(progress());
        return game::presentation::widgets::HorizontalMenuMotion{
            .slotShift = static_cast<f32>(direction) * (1.0f - t)
        };
    }
};

} // namespace biofuel::game::screens
