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
// Pushed on top of the active screen when ESC is pressed.
// Uses MenuHelper for all list rendering and input handling.
// Uses AnimationController for smooth fade-in/out transitions.
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

    // Animation durations
    static constexpr f32 FADE_DURATION = 0.3f;
    static constexpr f32 SCALE_DURATION = 0.35f;

    i32 m_selected = 0;
    f32 m_cooldown = 0.0f;

    // Animated values driven by AnimationManager
    u8 m_overlayAlpha = 0;     // 0→180 for dark overlay fade-in
    f32 m_panelScale = 0.0f;  // 0→1 for panel pop-in scale
    f32 m_panelOffsetY = 30.0f; // Y offset for slide-up effect

    bool m_animatingIn = true;   // true during fade-in, false when fully visible
    bool m_animatingOut = false; // true during fade-out (ESC dismiss)
    bool m_quitting = false;     // true if dismiss should quit the app

    void activateSelected();
    void startFadeIn();
    void startFadeOut();
};

} // namespace biofuel::ui::screens
