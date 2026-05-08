#pragma once

#include "UI/Screen.hpp"
#include "Utils/ui/MenuHelper.hpp"
#include "AnimationController/AnimationManager.hpp"
#include "AnimationController/animation/PremadeAnimations.hpp"
#include "AnimationController/animation/Easing.hpp"
#include <array>
#include <string_view>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// PausePopupScreen - Semi-transparent overlay with Resume/Quit options
// Slides in from the right edge when ESC is pressed during gameplay.
// Uses AnimationController for in/out transitions — ScreenManager's built-in
// transition system is disabled (duration=0) since we handle visuals ourselves.
// ------------------------------------------------------------------------------
class PausePopupScreen final : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

private:
    static constexpr std::array<utils::ui::MenuItem, 2> s_items = {{
        {.label = "Resume",          .locked = false},
        {.label = "Quit to Desktop", .locked = false},
    }};

    static constexpr i32 PANEL_WIDTH = 420;
    static constexpr i32 PANEL_HEIGHT = 260;
    static constexpr i32 TITLE_SIZE = 32;
    static constexpr i32 HINT_SIZE = 14;
    static constexpr utils::ui::MenuLayout MENU_LAYOUT = {
        .itemSpacing = 44,
        .fontSize = 22,
        .hitboxPaddingX = 12,
        .hitboxPaddingY = 4,
    };

    static constexpr f32 SLIDE_DURATION = 0.3f;

    i32 m_selected = 0;
    f32 m_cooldown = 0.0f;

    // Animated values driven by AnimationManager callbacks
    u8 m_overlayAlpha = 0;        // 0→180 dark backdrop
    f32 m_panelSlidePct = 1.0f;   // 0.0 = centered, 1.0 = off right edge

    bool m_animatingIn = true;    // true during slide-in, blocks input
    bool m_animatingOut = false;  // true during slide-out (ESC dismiss)
    bool m_quitting = false;      // true if dismiss should quit the app

    void activateSelected();
    void startSlideIn();
    void startSlideOut();

    // Screen-space X offset for current slide position
    [[nodiscard]] f32 panelSlideOffsetX(i32 screenWidth) const;
};

} // namespace biofuel::ui::screens
