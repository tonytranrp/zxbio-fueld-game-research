#include "MenuHelper.hpp"
#include "Utils/render/Render.hpp"

namespace biofuel::utils::ui {
using biofuel::utils::render::Renderer;

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
            std::string{items[i].label},
            centerX,
            itemY,
            layout.fontSize,
            color
        );

        if (items[i].locked) {
            const i32 labelW = MeasureText(items[i].label.data(), layout.fontSize);
            Renderer::drawText(
                "(locked)",
                centerX + labelW / 2 + 12,
                itemY + 8,
                14,
                layout.colorLockedLabel
            );
        }

        if (i == selectedIndex && !items[i].locked) {
            const i32 labelW = MeasureText(items[i].label.data(), layout.fontSize);
            Renderer::drawText(
                "\x10",
                centerX - labelW / 2 - 28,
                itemY + 2,
                layout.fontSize - 4,
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
        do {
            selectedIndex = (selectedIndex + dir + itemCount) % itemCount;
        } while (items[selectedIndex].locked);
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
        const i32 labelW = MeasureText(items[i].label.data(), layout.fontSize);

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

} // namespace biofuel::utils::ui
