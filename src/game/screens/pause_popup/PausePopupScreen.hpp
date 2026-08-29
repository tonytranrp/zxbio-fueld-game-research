#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "game/presentation/widgets/MenuHelper.hpp"
#include "game/presentation/effects/ScreenBlurEffect.hpp"
#include <array>
#include <string_view>

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// PausePopupScreen - Semi-transparent blurred overlay with Resume/Quit options
// Slides in from the right edge when ESC is pressed.
// Uses ScreenBlurEffect to blur and tint the screen behind the panel.
// ------------------------------------------------------------------------------
class PausePopupScreen final : public ::biofuel::engine::ui::Screen {
    template<typename, typename>
    friend struct ::biofuel::engine::ui::typed::RenderElementExecutor;

public:
    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::game::screens::screen_id::PausePopup; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "PausePopupScreen"; }

    // Panel dimensions — the single source of truth, shared by the input
    // hit-testing below and the render executors in the .cpp.
    static constexpr i32 PANEL_WIDTH = 420;
    static constexpr i32 PANEL_HEIGHT = 260;

private:
    static constexpr std::array<game::presentation::widgets::MenuItem, 2> s_items = {{
        {.label = "Resume",          .locked = false},
        {.label = "Quit to Desktop", .locked = false},
    }};

    static constexpr i32 TITLE_SIZE = 32;
    static constexpr i32 HINT_SIZE = 14;
    static constexpr game::presentation::widgets::MenuLayout MENU_LAYOUT = {
        .itemSpacing = 44,
        .fontSize = 22,
        .hitboxPaddingX = 12,
        .hitboxPaddingY = 4,
    };

    static constexpr f32 SLIDE_DURATION = 0.3f;
    static constexpr game::presentation::effects::BlurConfig BLUR_CONFIG = {
        .tintColor = {.r = 15, .g = 15, .b = 25, .a = 0},
        .maxTintAlpha = 108,
        .fadeInDuration = 0.32f,
        .fadeOutDuration = SLIDE_DURATION,
        .blurRadius = 2.7f,
        .captureScale = 0.42f,
        .desaturation = 0.22f,
        .vignetteStrength = 0.24f,
        .dimStrength = 0.18f,
        .blurPassCount = 2,
    };

    i32 m_selected = 0;
    f32 m_cooldown = 0.0f;

    // ScreenBlurEffect is a co-owner of rendering — it captures the screen
    // behind this popup, applies Gaussian blur, and draws the result.
    game::presentation::effects::ScreenBlurEffect m_blurEffect;

    // Panel slide state (animated via AnimationManager)
    f32 m_panelSlidePct = 1.0f;   // 0.0 = centered, 1.0 = off right edge

    bool m_animatingIn = true;    // true during slide-in, blocks input
    bool m_animatingOut = false;  // true during slide-out (ESC dismiss)
    bool m_quitting = false;      // true if dismiss should quit the app
    bool m_wantsPop = false;      // deferred pop flag to avoid re-entrancy

    // Restores the exact prior cursor-capture state on exit instead of
    // unconditionally re-capturing it -- PausePopupScreen is reachable from
    // both ExplorationScreen (cursor captured for mouse-look) and
    // MainMenuScreen (cursor free), see PauseController::canPauseCurrentScreen.
    bool m_cursorWasHidden = false;

    void activateSelected();
    void startSlideIn();
    void startSlideOut();

    [[nodiscard]] f32 panelSlideOffsetX(i32 screenWidth) const noexcept;
};

} // namespace biofuel::game::screens
