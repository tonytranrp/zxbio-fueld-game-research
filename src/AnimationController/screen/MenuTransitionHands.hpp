#pragma once

#include "Core/Types.hpp"
#include "Systems/Model/ModelSystem.hpp"
#include "Utils/render/Components/Camera/ShaderCamera.hpp"
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
#include "AnimationController/screen/ModelControllerOverlay.hpp"
#include <array>
#endif
#include <raylib.h>
#include <memory>

namespace biofuel::animation::screen {

// ------------------------------------------------------------------------------
// MenuTransitionHands — 3D hand model overlay for New Game / Continue transitions.
//
// Phase machine:
//   Idle → (start) → Playing → (anim done, 2.52s) → Complete (holds final frame)
//
// Renders during Playing and Complete. Stays in Complete until reset() or
// unload() is called by the owning screen's lifecycle.
// ------------------------------------------------------------------------------
class MenuTransitionHands final {
public:
    enum class Phase : u8 {
        Idle,       // loaded, not active — no rendering
        Playing,    // "action" animation running — full render
        Complete,   // animation finished, holding final frame — full render
    };
public:
    static constexpr systems::model::ModelAssetId ASSET_ID = systems::model::ModelAssetId::MenuTransitionHands;

    // Animation timing constants
    static constexpr f32 ACTION_DURATION = 2.52f;   // "action" anim clip length

    void load();
    void unload() noexcept;
    void reset() noexcept;
    void start() noexcept;
    void update(f32 dt, f32 dimensionShift, const utils::render::component::ShaderCameraState& shaderCamera) noexcept;
    void render() noexcept;

    [[nodiscard]] bool ready() const noexcept { return m_loaded; }
    [[nodiscard]] Phase phase() const noexcept { return m_phase; }
    [[nodiscard]] bool isRendering() const noexcept { return m_phase == Phase::Playing || m_phase == Phase::Complete; }

private:
    struct TransitionRenderPose {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Quaternion orientation{0.0f, 0.0f, 0.0f, 1.0f};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        Color auraTint{88, 136, 220, 180};
        Color baseTint{232, 238, 255, 220};
    };

    [[nodiscard]] TransitionRenderPose buildPose(const systems::model::ModelInstance& instance) const noexcept;
    void drawHands(const systems::model::ModelInstance& instance, const TransitionRenderPose& pose) const noexcept;

    void updateCamera(const utils::render::component::ShaderCameraState& shaderCamera) noexcept;
    void cacheUniformLocations(Shader shader) const noexcept;
    void applyShaderUniforms(const systems::model::ModelInstance& instance) const noexcept;
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    using ControllerTargets = std::array<ModelControlTarget, 5>;
    [[nodiscard]] ControllerTargets buildControllerTargets(const TransitionRenderPose& pose) noexcept;
    void updateController(const TransitionRenderPose& pose) noexcept;
    void renderController(const TransitionRenderPose& pose) noexcept;
    void applyControllerOffsets() noexcept;
#endif
    Camera3D m_camera{
        .position = Vector3{0.0f, 0.15f, 2.55f},
        .target = Vector3{0.0f, 0.0f, 0.0f},
        .up = Vector3{0.0f, 1.0f, 0.0f},
        .fovy = 30.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    std::shared_ptr<systems::model::ModelInstance> m_instance;
    bool m_loaded = false;
    Phase m_phase = Phase::Idle;
    f32 m_elapsed = 0.0f;
    f32 m_dimensionShift = 0.0f;
    f32 m_cameraYaw = 0.0f;
    f32 m_baseScale = 1.0f;
    f32 m_fingerStartY = 0.0f;
    f32 m_fingerEndY = 1.0f;
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    ModelControllerOverlay m_controller;
    Vector3 m_rootOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_cameraPositionOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_cameraTargetOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_leftHandOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_rightHandOffset{0.0f, 0.0f, 0.0f};
#endif

    mutable i32 m_timeLoc = -1;
    mutable i32 m_portalStrengthLoc = -1;
    mutable i32 m_fingerStartLoc = -1;
    mutable i32 m_fingerEndLoc = -1;
    mutable i32 m_colorALoc = -1;
    mutable i32 m_colorBLoc = -1;
    mutable i32 m_rimColorLoc = -1;
    mutable u32 m_cachedShaderId = 0;
};

} // namespace biofuel::animation::screen
