#pragma once

#include "UI/Screen.hpp"
#include "Utils/ui/MenuHelper.hpp"
#include <array>
#include <string_view>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// MainMenuScreen - Title screen with horizontal menu bar at bottom
// Clean layout: title top-left, menu bottom-middle, version footer
// ------------------------------------------------------------------------------
class MainMenuScreen final : public Screen {
public:
    void onEnter() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

private:
    // ---- Menu items ----
    static constexpr std::array<utils::ui::MenuItem, 3> s_items = {{
        {.label = "New Game", .locked = false},
        {.label = "Continue", .locked = true},
        {.label = "Quit",     .locked = false},
    }};

    // ---- Color palette ----
    static constexpr Color COLOR_BG              = {15, 15, 25, 255};   // Dark background
    static constexpr Color COLOR_GOLD             = {200, 155, 60, 255}; // Gold accent (selected)
    static constexpr Color COLOR_GOLD_DIM         = {140, 110, 40, 255}; // Dimmer gold for arrows
    static constexpr Color COLOR_WARM_HI          = {255, 200, 80, 255}; // Warm highlight (pulse peak)
    static constexpr Color COLOR_GRAY_DIM         = {90, 90, 100, 255};  // Unselected items
    static constexpr Color COLOR_GRAY_LOCKED       = {55, 55, 65, 255};  // Locked item text
    static constexpr Color COLOR_GRAY_LOCKED_LABEL = {80, 80, 90, 255};  // Locked "(locked)" label
    static constexpr Color COLOR_VERSION           = {60, 60, 70, 255};  // Version/footer text

    // ---- Title area layout (top-left) ----
    static constexpr i32 TITLE_X                = 40;
    static constexpr i32 TITLE_Y                = 30;
    static constexpr i32 TITLE_FONT_SIZE        = 40;
    static constexpr i32 SUBTITLE_FONT_SIZE     = 16;
    static constexpr i32 HINTS_FONT_SIZE        = 13;
    static constexpr i32 TITLE_SUBTITLE_GAP     = 10;  // Pixels between title baseline and subtitle
    static constexpr i32 SUBTITLE_HINTS_GAP     = 8;   // Pixels between subtitle baseline and hints
    static constexpr f32 TITLE_PULSE_SPEED      = 1.8f;
    static constexpr f32 TITLE_PULSE_MIN        = 225.0f;
    static constexpr f32 TITLE_PULSE_RANGE      = 30.0f;

    // ---- Menu bar layout (bottom-middle) ----
    static constexpr i32 MENU_BAR_Y_OFFSET      = 100; // From bottom of screen
    static constexpr i32 MENU_FONT_SIZE          = 24;  // Uniform size for all items
    static constexpr i32 MENU_ITEM_SPACING       = 180; // Pixels between item centers
    static constexpr i32 UNDERLINE_WIDTH         = 70;
    static constexpr i32 UNDERLINE_HEIGHT         = 3;
    static constexpr i32 UNDERLINE_OFFSET_Y      = 8;   // Below text baseline
    static constexpr i32 ARROW_FONT_SIZE         = 22;
    static constexpr i32 ARROW_OFFSET_X         = 10;   // Pixels from item text edge to arrow

    // ---- Footer layout ----
    static constexpr i32 FOOTER_FONT_SIZE        = 12;
    static constexpr i32 FOOTER_MARGIN_X        = 10;
    static constexpr i32 FOOTER_BOTTOM_OFFSET   = 25;

    // ---- Navigation ----
    static constexpr f32 KEY_REPEAT_DELAY       = 0.12f;

    // ---- State ----
    i32 m_selected  = 0;
    f32 m_cooldown  = 0.0f;
    f32 m_titlePulse = 0.0f;

    // ---- Methods ----
    void activateSelected();
    [[nodiscard]] bool isLocked(i32 index) const;

    // ---- Menu bar rendering (single horizontal row at bottom) ----
    void renderMenuBar(i32 centerX, i32 barY) const;

    // ---- Menu bar mouse hit-testing ----
    [[nodiscard]] i32 hitTestMenuBar(i32 centerX, i32 barY) const;

    // ---- Horizontal menu navigation ----
    [[nodiscard]] bool navigateMenu();
};

} // namespace biofuel::ui::screens