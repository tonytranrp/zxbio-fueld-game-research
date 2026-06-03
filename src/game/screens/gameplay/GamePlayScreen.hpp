#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "engine/world/voxel/VoxelWorld.hpp"
#include "game/gameplay/world3d/FirstPersonController.hpp"
#include "game/presentation/hands/HandModelOverlay.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::game::screens {

// =============================================================================
// GamePlayScreen — a walkable, infinite, Minecraft-style voxel world.
//
// Streams chunks of blocky terrain around the player as they walk, and lets
// them move, sprint, look, and jump through it in first person.
// =============================================================================
class GamePlayScreen final : public ::biofuel::engine::ui::Screen {
public:
    GamePlayScreen();
    ~GamePlayScreen() noexcept override;

    void onEnter() override;
    void onExit() override;
    void onPause() override;
    void onResume() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::game::screens::screen_id::GamePlay; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "GamePlayScreen"; }

private:
    void renderSky() const;
    void renderHud() const;
    void releaseCursor() noexcept;
    void captureCursor() noexcept;

    ::biofuel::engine::world::voxel::VoxelWorld m_voxels;
    ::biofuel::game::gameplay::world3d::FirstPersonController m_player;
    ::biofuel::game::presentation::hands::HandModelOverlay m_handOverlay;
    bool m_cursorCaptured = false;
};

} // namespace biofuel::game::screens
