#pragma once

#include "UI/Screen.hpp"
#include "Utils/ui/MenuHelper.hpp"
#include <array>
#include <string_view>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// MainMenuScreen - Title screen with navigable menu
// Uses MenuHelper for all list rendering and input handling.
// ------------------------------------------------------------------------------
class MainMenuScreen final : public Screen {
public:
    void onEnter() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

private:
    static constexpr std::array<utils::ui::MenuItem, 3> s_items = {{
        {.label = "New Game", .locked = false},
        {.label = "Continue", .locked = true},
        {.label = "Quit",     .locked = false},
    }};

    static constexpr i32 TITLE_SIZE = 48;
    static constexpr i32 SUBTITLE_SIZE = 18;
    static constexpr utils::ui::MenuLayout MENU_LAYOUT = {
        .itemSpacing = 48,
        .fontSize = 26,
        .hitboxPaddingX = 16,
        .hitboxPaddingY = 4,
    };

    i32 m_selected = 0;
    f32 m_cooldown = 0.0f;
    f32 m_titlePulse = 0.0f;

    void activateSelected();
    [[nodiscard]] bool isLocked(i32 index) const;
};

} // namespace biofuel::ui::screens
