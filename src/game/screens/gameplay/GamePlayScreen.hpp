#pragma once

#include "engine/ui/Screen.hpp"

namespace biofuel::game::screens {

class GamePlayScreen final : public ::biofuel::engine::ui::Screen {
public:
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::engine::ui::typed::ScreenId::GamePlay; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "GamePlayScreen"; }
};

} // namespace biofuel::game::screens
