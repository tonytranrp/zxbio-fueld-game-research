#include "MenuHelper.hpp"
#include "Utils/render/Render.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace biofuel::utils::ui {
using biofuel::utils::render::Renderer;

namespace {

constexpr i32 VERTICAL_ACCENT_BAR_WIDTH = 14;
constexpr i32 VERTICAL_ACCENT_BAR_HEIGHT = 2;
constexpr i32 VERTICAL_ACCENT_SQUARE = 4;

constexpr f32 HORIZONTAL_SLOT_VISIBILITY = 1.35f;
constexpr std::array<i32, 5> HORIZONTAL_RELATIVE_OFFSETS = {-2, -1, 0, 1, 2};

std::array<HorizontalMenuItemVisualState, HORIZONTAL_RELATIVE_OFFSETS.size()> g_horizontalStates{};

[[nodiscard]] i32 wrapIndex(const i32 index, const i32 itemCount) noexcept {
    return (index % itemCount + itemCount) % itemCount;
}

[[nodiscard]] f32 saturate(const f32 value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] f32 easeOutCubic(const f32 value) noexcept {
    const f32 t = saturate(value) - 1.0f;
    return t * t * t + 1.0f;
}

[[nodiscard]] i32 lerpInt(const i32 a, const i32 b, const f32 t) noexcept {
    return static_cast<i32>(std::lround(static_cast<f32>(a) + static_cast<f32>(b - a) * saturate(t)));
}

[[nodiscard]] Color lerpColor(const Color a, const Color b, const f32 t) noexcept {
    const f32 pct = saturate(t);
    auto channel = [pct](const u8 lhs, const u8 rhs) -> u8 {
        return static_cast<u8>(std::lround(static_cast<f32>(lhs) +
            (static_cast<f32>(rhs) - static_cast<f32>(lhs)) * pct));
    };

    return Color{
        channel(a.r, b.r),
        channel(a.g, b.g),
        channel(a.b, b.b),
        channel(a.a, b.a)
    };
}

void rebuildHorizontalStates(
    std::span<const MenuItem> items,
    const i32 selectedIndex,
    const i32 centerX,
    const i32 centerY,
    const HorizontalMenuLayout& layout,
    const HorizontalMenuMotion& motion)
{
    for (auto& state : g_horizontalStates) {
        state = {};
    }

    if (items.empty()) {
        return;
    }

    const i32 itemCount = static_cast<i32>(items.size());
    for (i32 slot = 0; slot < static_cast<i32>(HORIZONTAL_RELATIVE_OFFSETS.size()); ++slot) {
        const i32 offset = HORIZONTAL_RELATIVE_OFFSETS[slot];
        const f32 slotOffset = static_cast<f32>(offset) + motion.slotShift;
        if (std::abs(slotOffset) > HORIZONTAL_SLOT_VISIBILITY) {
            continue;
        }

        const i32 itemIndex = wrapIndex(selectedIndex + offset, itemCount);
        auto& state = g_horizontalStates[slot];
        const f32 emphasis = easeOutCubic(1.0f - saturate(std::abs(slotOffset)));
        const i32 fontSize = lerpInt(layout.sideFontSize, layout.centerFontSize, emphasis);

        state.itemIndex = itemIndex;
        state.slotOffset = slotOffset;
        state.emphasis = emphasis;
        state.locked = items[itemIndex].locked;
        state.visible = true;
        state.fontSize = fontSize;
        state.centerX = centerX + static_cast<i32>(std::lround(slotOffset * static_cast<f32>(layout.sideOffsetX)));
        state.baselineY = centerY + static_cast<i32>(std::lround(std::abs(slotOffset) * static_cast<f32>(layout.sideOffsetY)));
        state.hitWidth = Renderer::measureText(items[itemIndex].label, state.fontSize);
        const Color sideColor = state.locked ? layout.colorSideLocked : layout.colorSide;
        state.color = lerpColor(sideColor, layout.colorSelected, emphasis);
    }

    auto* focusedState = &g_horizontalStates.front();
    f32 nearestSlotDistance = 999.0f;
    for (auto& state : g_horizontalStates) {
        if (!state.visible) {
            continue;
        }

        const f32 slotDistance = std::abs(state.slotOffset);
        if (slotDistance < nearestSlotDistance) {
            nearestSlotDistance = slotDistance;
            focusedState = &state;
        }
    }

    for (auto& state : g_horizontalStates) {
        state.selected = (&state == focusedState) && state.visible;
    }
}

} // namespace

void renderVerticalMenu(
    std::span<const MenuItem> items,
    const i32 selectedIndex,
    const i32 centerX,
    const i32 startY,
    const MenuLayout& layout)
{
    for (i32 i = 0; i < static_cast<i32>(items.size()); ++i) {
        const i32 itemY = startY + i * layout.itemSpacing;

        Color color = layout.colorNormal;
        if (items[i].locked) {
            color = layout.colorLocked;
        } else if (i == selectedIndex) {
            color = layout.colorSelected;
        }

        Renderer::drawTextCentered(
            items[i].label,
            centerX,
            itemY,
            layout.fontSize,
            color
        );

        if (items[i].locked) {
            const i32 labelW = Renderer::measureText(items[i].label, layout.fontSize);
            Renderer::drawText(
                "(locked)",
                centerX + labelW / 2 + 12,
                itemY + 8,
                14,
                layout.colorLockedLabel
            );
        }

        if (i == selectedIndex && !items[i].locked) {
            const i32 labelW = Renderer::measureText(items[i].label, layout.fontSize);
            const i32 accentX = centerX - labelW / 2 - 30;
            const i32 accentY = itemY + layout.fontSize / 2;
            Renderer::drawRect(
                accentX,
                accentY - VERTICAL_ACCENT_BAR_HEIGHT / 2,
                VERTICAL_ACCENT_BAR_WIDTH,
                VERTICAL_ACCENT_BAR_HEIGHT,
                layout.colorSelected
            );
            Renderer::drawRect(
                accentX + VERTICAL_ACCENT_BAR_WIDTH + 4,
                accentY - VERTICAL_ACCENT_SQUARE / 2,
                VERTICAL_ACCENT_SQUARE,
                VERTICAL_ACCENT_SQUARE,
                layout.colorSelected
            );
        }
    }
}

bool navigateVerticalMenu(
    i32& selectedIndex,
    const i32 itemCount,
    f32& cooldownTimer,
    const f32 dt,
    std::span<const MenuItem> items,
    const MenuLayout& layout)
{
    if (cooldownTimer > 0.0f) {
        cooldownTimer -= dt;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (items.empty() || !items[selectedIndex].locked) {
            return true;
        }
        return false;
    }

    if (cooldownTimer > 0.0f) {
        return false;
    }

    i32 dir = 0;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        dir = -1;
    } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        dir = 1;
    } else {
        return false;
    }

    if (!items.empty()) {
        i32 cycleCount = 0;
        do {
            selectedIndex = (selectedIndex + dir + itemCount) % itemCount;
            ++cycleCount;
        } while (items[selectedIndex].locked && cycleCount < itemCount);
    } else {
        selectedIndex = (selectedIndex + dir + itemCount) % itemCount;
    }

    cooldownTimer = layout.keyRepeatDelay;
    return false;
}

MenuHitResult hitTestVerticalMenu(
    std::span<const MenuItem> items,
    const i32 centerX,
    const i32 startY,
    const MenuLayout& layout)
{
    const Vector2 mouse = GetMousePosition();
    MenuHitResult result;

    for (i32 i = 0; i < static_cast<i32>(items.size()); ++i) {
        if (items[i].locked) {
            continue;
        }

        const i32 itemY = startY + i * layout.itemSpacing;
        const i32 labelW = Renderer::measureText(items[i].label, layout.fontSize);

        const Rectangle hitbox = {
            static_cast<f32>(centerX - labelW / 2 - layout.hitboxPaddingX),
            static_cast<f32>(itemY - layout.hitboxPaddingY),
            static_cast<f32>(labelW + layout.hitboxPaddingX * 2),
            static_cast<f32>(layout.fontSize + layout.hitboxPaddingY * 2)
        };

        if (CheckCollisionPointRec(mouse, hitbox)) {
            result.hoveredIndex = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                result.clicked = true;
            }
            return result;
        }
    }

    return result;
}

bool navigateHorizontalMenu(
    i32& selectedIndex,
    const i32 itemCount,
    f32& cooldownTimer,
    std::span<const MenuItem> items,
    const HorizontalMenuLayout& layout)
{
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (items.empty() || !items[selectedIndex].locked) {
            return true;
        }
        return false;
    }

    if (cooldownTimer > 0.0f) {
        return false;
    }

    i32 dir = 0;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        dir = -1;
    } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        dir = 1;
    } else {
        return false;
    }

    if (!items.empty()) {
        i32 cycleCount = 0;
        do {
            selectedIndex = wrapIndex(selectedIndex + dir, itemCount);
            ++cycleCount;
        } while (items[selectedIndex].locked && cycleCount < itemCount);
    } else {
        selectedIndex = wrapIndex(selectedIndex + dir, itemCount);
    }

    cooldownTimer = layout.keyRepeatDelay;
    return false;
}

void renderHorizontalCarousel(
    std::span<const MenuItem> items,
    const i32 selectedIndex,
    const i32 centerX,
    const i32 centerY,
    const HorizontalMenuLayout& layout,
    const HorizontalMenuMotion& motion)
{
    rebuildHorizontalStates(items, selectedIndex, centerX, centerY, layout, motion);

    for (const auto& state : g_horizontalStates) {
        if (!state.visible) {
            continue;
        }

        Renderer::drawTextCentered(
            items[state.itemIndex].label,
            state.centerX,
            state.baselineY,
            state.fontSize,
            state.color
        );

        if (state.locked) {
            Renderer::drawTextCentered(
                "LOCKED",
                state.centerX,
                state.baselineY + state.fontSize + 2,
                layout.lockedLabelFontSize,
                lerpColor(layout.colorLockedLabel, layout.colorSelectedGlow, state.emphasis * 0.25f)
            );
        }

        if (!state.selected || state.locked) {
            continue;
        }

        const i32 underlineWidth = lerpInt(layout.underlineWidth / 2, layout.underlineWidth, state.emphasis);
        const i32 underlineHeight = std::max(1, lerpInt(1, layout.underlineHeight, state.emphasis));
        const i32 accentWidth = std::max(8, lerpInt(layout.accentWidth / 2, layout.accentWidth, state.emphasis));
        const i32 accentHeight = std::max(1, lerpInt(1, layout.accentHeight, state.emphasis));
        const i32 accentGap = lerpInt(layout.accentGap / 2, layout.accentGap, state.emphasis);

        const i32 underlineX = state.centerX - underlineWidth / 2;
        const i32 underlineY = state.baselineY + state.fontSize + layout.underlineOffsetY;
        Renderer::drawRect(
            underlineX,
            underlineY,
            underlineWidth,
            underlineHeight,
            lerpColor(layout.colorSide, layout.colorSelected, state.emphasis)
        );

        const i32 accentY = state.baselineY + state.fontSize / 2;
        const i32 leftAccentX = state.centerX - state.hitWidth / 2 - accentGap - accentWidth;
        const i32 rightAccentX = state.centerX + state.hitWidth / 2 + accentGap;

        Renderer::drawRect(
            leftAccentX,
            accentY - accentHeight / 2,
            accentWidth,
            accentHeight,
            lerpColor(layout.colorSide, layout.colorSelectedGlow, state.emphasis)
        );
        Renderer::drawRect(
            rightAccentX,
            accentY - accentHeight / 2,
            accentWidth,
            accentHeight,
            lerpColor(layout.colorSide, layout.colorSelectedGlow, state.emphasis)
        );
    }
}

HorizontalMenuHitResult hitTestHorizontalCarousel(
    std::span<const MenuItem> items,
    const i32 selectedIndex,
    const i32 centerX,
    const i32 centerY,
    const HorizontalMenuLayout& layout,
    const HorizontalMenuMotion& motion)
{
    rebuildHorizontalStates(items, selectedIndex, centerX, centerY, layout, motion);

    const Vector2 mouse = GetMousePosition();
    HorizontalMenuHitResult result;

    for (const auto& state : g_horizontalStates) {
        if (!state.visible || state.locked) {
            continue;
        }

        const Rectangle hitbox = {
            static_cast<f32>(state.centerX - state.hitWidth / 2 - layout.hitboxPaddingX),
            static_cast<f32>(state.baselineY - layout.hitboxPaddingY),
            static_cast<f32>(state.hitWidth + layout.hitboxPaddingX * 2),
            static_cast<f32>(state.fontSize + layout.hitboxPaddingY * 2)
        };

        if (CheckCollisionPointRec(mouse, hitbox)) {
            result.hoveredIndex = state.itemIndex;
            result.clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            return result;
        }
    }

    return result;
}

std::span<const HorizontalMenuItemVisualState> horizontalMenuVisualStates() {
    return std::span<const HorizontalMenuItemVisualState>{g_horizontalStates};
}

} // namespace biofuel::utils::ui
