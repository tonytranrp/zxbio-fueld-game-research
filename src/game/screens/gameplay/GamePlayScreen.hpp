#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/physics/PhysicsTypes.hpp"
#include "engine/ui/Screen.hpp"
#include "game/gameplay/FarmState.hpp"
#include "game/gameplay/WorldPhysicsIntegration.hpp"
#include "game/presentation/hands/HandModelOverlay.hpp"
#include "game/presentation/sprites/NekoCat.hpp"
#include <memory>
#include <string_view>

namespace biofuel::engine::physics {
class PhysicsSystem;
class PhysicsWorld2D;
} // namespace biofuel::engine::physics

namespace biofuel::game::screens {

class GamePlayScreen final : public ::biofuel::engine::ui::Screen {
public:
    GamePlayScreen();
    ~GamePlayScreen() noexcept override;

    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::game::screens::screen_id::GamePlay; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "GamePlayScreen"; }

private:
    void ensureHandTrackingForModelOverlay();
    void initPhysicsWorld();
    void shutdownPhysicsWorld() noexcept;
    void createPlayerBody();
    void syncNekoCatFromPhysics() noexcept;
    void applyWASDVelocity(f32 dt) noexcept;

    game::presentation::hands::HandModelOverlay m_handOverlay;
    presentation::sprites::NekoCat m_neko;

    // ---- Physics integration ----
    std::unique_ptr<::biofuel::engine::physics::PhysicsSystem> m_physicsSystem;
    ::biofuel::game::gameplay::WorldPhysicsIntegration m_worldPhysics;
    std::unique_ptr<::biofuel::game::gameplay::FarmState> m_farmState;
    ::biofuel::engine::physics::PhysicsBody2D m_playerBody{};

    f32 m_metersToPixels = 64.0f;
    f32 m_playerSpeed = 5.0f; // meters per second
};

} // namespace biofuel::game::screens
