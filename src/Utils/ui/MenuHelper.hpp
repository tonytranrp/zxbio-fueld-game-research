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

// ------------------------------------------------------------------------------
// MenuHitResult - Result of mouse hit-testing a menu
// ------------------------------------------------------------------------------
struct MenuHitResult {
    i32 hoveredIndex = -1;
    bool clicked = false;
};

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

} // namespace biofuel::utils::ui
