#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "game/screens/exploration/ExplorationLevel.hpp"
#include "engine/character/CharacterController3D.hpp"
#include "engine/character/FirstPersonCamera.hpp"
#include "engine/graphics/ViewmodelPass.hpp"
#include "engine/models/ModelSystem.hpp"
#include "engine/ui/Screen.hpp"
#include <memory>
#include <string_view>

namespace biofuel::game::screens {

// -----------------------------------------------------------------------------
// ExplorationScreen - milestone 1: a first-person, realistic (non-voxel)
// walkable 3D environment. WASD + mouse-look, Rapier kinematic capsule
// collision, no weapons/combat/inventory yet. Reached from the main menu's
// post-dismiss dimension-shift completing (see MainMenuScreen).
// -----------------------------------------------------------------------------
class ExplorationScreen final : public ::biofuel::engine::ui::Screen {
    template<typename, typename>
    friend struct ::biofuel::engine::ui::typed::RenderElementExecutor;

public:
    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return screen_id::Exploration; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "ExplorationScreen"; }

    [[nodiscard]] const ExplorationLevel& level() const noexcept { return m_level; }
    [[nodiscard]] const engine::character::CharacterController3D& character() const noexcept { return m_character; }
    [[nodiscard]] const engine::character::FirstPersonCamera& firstPersonCamera() const noexcept { return m_camera; }
    [[nodiscard]] const std::shared_ptr<engine::models::ModelInstance>& handsInstance() const noexcept { return m_handsInstance; }
    [[nodiscard]] engine::graphics::ViewmodelPass& viewmodelPass() noexcept { return m_viewmodelPass; }

private:
    ExplorationLevel m_level;
    engine::character::CharacterController3D m_character;
    engine::character::FirstPersonCamera m_camera;
    std::shared_ptr<engine::models::ModelInstance> m_handsInstance;
    engine::graphics::ViewmodelPass m_viewmodelPass;
    bool m_cursorCaptured = false;
};

} // namespace biofuel::game::screens
