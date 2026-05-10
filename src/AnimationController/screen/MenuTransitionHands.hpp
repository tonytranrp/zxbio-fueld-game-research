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

class MenuTransitionHands final {
public:
    static constexpr systems::model::ModelAssetId ASSET_ID = systems::model::ModelAssetId::MenuTransitionHands;

    void load();
    void unload() noexcept;
    void reset() noexcept;
    void start() noexcept;
    void update(f32 dt, f32 dimensionShift, const utils::render::component::ShaderCameraState& shaderCamera) noexcept;
    void render() noexcept;

    [[nodiscard]] bool ready() const noexcept { return m_loaded; }
    [[nodiscard]] bool active() const noexcept { return m_active && m_loaded; }

private:
    struct TransitionRenderPose {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Quaternion orientation{0.0f, 0.0f, 0.0f, 1.0f};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        Color auraTint{88, 136, 220, 180};
        Color baseTint{232, 238, 255, 220};
    };

    [[nodiscard]] static f32 saturate(f32 value) noexcept;
    [[nodiscard]] static f32 easeInOutCubic(f32 value) noexcept;
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
    bool m_active = false;
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
