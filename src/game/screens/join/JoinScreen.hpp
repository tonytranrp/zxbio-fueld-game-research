#pragma once

#include "engine/ui/Screen.hpp"
#include <raylib.h>

namespace biofuel::game::screens {

class JoinScreen final : public ::biofuel::engine::ui::Screen {
public:
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::engine::ui::typed::ScreenId::Join; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "JoinScreen"; }

private:
    static constexpr i32 BUTTON_WIDTH = 180;
    static constexpr i32 BUTTON_HEIGHT = 52;
    static constexpr i32 BUTTON_FONT_SIZE = 24;
    static constexpr i32 TITLE_FONT_SIZE = 34;
    static constexpr i32 HINT_FONT_SIZE = 15;

    bool m_gameplayQueued = false;

    [[nodiscard]] Rectangle joinButtonBounds() const noexcept;
    void activateJoin();
};

} // namespace biofuel::game::screens
