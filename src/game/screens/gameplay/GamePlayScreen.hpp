#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "engine/world/voxel/VoxelWorld.hpp"
#include "engine/world/voxel/VoxelVolume.hpp"
#include "engine/graphics/RenderSurface.hpp"
#include "game/gameplay/world3d/FirstPersonController.hpp"
#include <raylib.h>
#include <array>
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
    void renderRaster();
    void renderRaymarch();
    void loadRaymarchShader();
    void renderHud() const;
    void releaseCursor() noexcept;
    void captureCursor() noexcept;

    ::biofuel::engine::world::voxel::VoxelWorld m_voxels;
    ::biofuel::engine::world::voxel::VoxelVolume m_volume;
    ::biofuel::game::gameplay::world3d::FirstPersonController m_player;

    // Raymarched-voxel renderer (John Lin style); toggle with F6.
    ::biofuel::engine::graphics::RenderSurface m_rayTarget;
    Shader m_rayShader{};
    bool m_rayShaderReady = false;
    bool m_raymarchMode = true;
    std::array<i32, 16> m_rayLoc{};   // cached uniform locations

    bool m_cursorCaptured = false;
};

} // namespace biofuel::game::screens
