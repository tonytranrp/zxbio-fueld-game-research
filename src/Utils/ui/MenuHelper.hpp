#pragma once

#include "Core/Types.hpp"
#include <raylib.h>
#include <string_view>
#include <span>

namespace biofuel::utils::ui {

// ------------------------------------------------------------------------------
// MenuItem - A single entry in a vertical menu list
// ------------------------------------------------------------------------------
struct MenuItem {
    std::string_view label;
    bool locked = false;
};

// ------------------------------------------------------------------------------
// Visual layout constants for menu rendering
// ------------------------------------------------------------------------------
struct MenuLayout {
    i32 itemSpacing = 48;
    i32 fontSize = 26;
    i32 hitboxPaddingX = 16;
    i32 hitboxPaddingY = 4;
    Color colorNormal = LIGHTGRAY;
    Color colorSelected = YELLOW;
    Color colorLocked = {60, 60, 60, 255};
    Color colorLockedLabel = {80, 80, 80, 255};
    f32 keyRepeatDelay = 0.12f;
};

struct HorizontalMenuLayout {
    i32 sideOffsetX = 210;
    i32 sideOffsetY = 14;
    i32 centerFontSize = 28;
    i32 sideFontSize = 22;
    i32 lockedLabelFontSize = 13;
    i32 hitboxPaddingX = 18;
    i32 hitboxPaddingY = 10;
    i32 underlineWidth = 78;
    i32 underlineHeight = 3;
    i32 underlineOffsetY = 12;
    i32 accentGap = 20;
    i32 accentWidth = 18;
    i32 accentHeight = 2;
    Color colorSelected = {200, 155, 60, 255};
    Color colorSelectedGlow = {255, 205, 90, 255};
    Color colorSide = {120, 125, 140, 220};
    Color colorSideLocked = {75, 78, 90, 220};
    Color colorLockedLabel = {95, 98, 112, 220};
    f32 keyRepeatDelay = 0.12f;
};

struct HorizontalMenuMotion {
    f32 slotShift = 0.0f;
};

struct HorizontalMenuItemVisualState {
    i32 itemIndex = -1;
    i32 centerX = 0;
    i32 baselineY = 0;
    i32 fontSize = 0;
    i32 hitWidth = 0;
    f32 slotOffset = 0.0f;
    f32 emphasis = 0.0f;
    bool selected = false;
    bool locked = false;
    bool visible = false;
    Color color = BLANK;
};

// ------------------------------------------------------------------------------
// MenuHitResult - Result of mouse hit-testing a menu
// ------------------------------------------------------------------------------
struct MenuHitResult {
    i32 hoveredIndex = -1;
    bool clicked = false;
};

using HorizontalMenuHitResult = MenuHitResult;

// ------------------------------------------------------------------------------
// MenuHelper - Free-function utilities for vertical menu lists
// Eliminates duplicated navigation/rendering across MainMenu and PausePopup.
// ------------------------------------------------------------------------------

// Renders a vertical menu centered at (cx, startY).
void renderVerticalMenu(
    std::span<const MenuItem> items,
    i32 selectedIndex,
    i32 centerX,
    i32 startY,
    const MenuLayout& layout = {}
);

// Handles keyboard navigation (Up/Down/W/S). Updates selectedIndex in place
// using a cooldown timer. Checks Enter/Space for activation.
// Returns true if the user activated the current item.
[[nodiscard]] bool navigateVerticalMenu(
    i32& selectedIndex,
    i32 itemCount,
    f32& cooldownTimer,
    f32 dt,
    std::span<const MenuItem> items = {},
    const MenuLayout& layout = {}
);

// Checks if the mouse is hovering over or clicking on menu items.
[[nodiscard]] MenuHitResult hitTestVerticalMenu(
    std::span<const MenuItem> items,
    i32 centerX,
    i32 startY,
    const MenuLayout& layout = {}
);

[[nodiscard]] bool navigateHorizontalMenu(
    i32& selectedIndex,
    i32 itemCount,
    f32& cooldownTimer,
    std::span<const MenuItem> items = {},
    const HorizontalMenuLayout& layout = {}
);

void renderHorizontalCarousel(
    std::span<const MenuItem> items,
    i32 selectedIndex,
    i32 centerX,
    i32 centerY,
    const HorizontalMenuLayout& layout = {},
    const HorizontalMenuMotion& motion = {}
);

[[nodiscard]] HorizontalMenuHitResult hitTestHorizontalCarousel(
    std::span<const MenuItem> items,
    i32 selectedIndex,
    i32 centerX,
    i32 centerY,
    const HorizontalMenuLayout& layout = {},
    const HorizontalMenuMotion& motion = {}
);

[[nodiscard]] std::span<const HorizontalMenuItemVisualState> horizontalMenuVisualStates();

} // namespace biofuel::utils::ui
