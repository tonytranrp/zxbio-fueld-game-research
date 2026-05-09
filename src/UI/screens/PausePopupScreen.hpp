#pragma once

#include "UI/Screen.hpp"
#include "Utils/ui/MenuHelper.hpp"
#include "AnimationController/screen/ScreenBlurEffect.hpp"
#include <array>
#include <string_view>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// PausePopupScreen - Semi-transparent blurred overlay with Resume/Quit options
// Slides in from the right edge when ESC is pressed.
// Uses ScreenBlurEffect to blur and tint the screen behind the panel.
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
    static constexpr animation::screen::BlurConfig BLUR_CONFIG = {
        .tintColor = {.r = 15, .g = 15, .b = 25, .a = 0},
        .maxTintAlpha = 120,
        .fadeInDuration = 0.3f,
        .fadeOutDuration = 0.3f,
        .blurRadius = 3.0f,
    };

    i32 m_selected = 0;
    f32 m_cooldown = 0.0f;

    // ScreenBlurEffect is a co-owner of rendering — it captures the screen
    // behind this popup, applies Gaussian blur, and draws the result.
    animation::screen::ScreenBlurEffect m_blurEffect;

    // Panel slide state (animated via AnimationManager)
    f32 m_panelSlidePct = 1.0f;   // 0.0 = centered, 1.0 = off right edge

    bool m_animatingIn = true;    // true during slide-in, blocks input
    bool m_animatingOut = false;  // true during slide-out (ESC dismiss)
    bool m_quitting = false;      // true if dismiss should quit the app

    void activateSelected();
    void startSlideIn();
    void startSlideOut();

    [[nodiscard]] f32 panelSlideOffsetX(i32 screenWidth) const noexcept;
};

} // namespace biofuel::ui::screens
