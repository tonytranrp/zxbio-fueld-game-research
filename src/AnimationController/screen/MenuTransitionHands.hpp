#pragma once

#include "Core/Types.hpp"
#include "Systems/Model/ModelSystem.hpp"
#include "Utils/render/Components/Camera/ShaderCamera.hpp"
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
    void render() const noexcept;

    [[nodiscard]] bool ready() const noexcept { return m_loaded; }
    [[nodiscard]] bool active() const noexcept { return m_active && m_loaded; }

private:
    [[nodiscard]] static f32 saturate(f32 value) noexcept;
    [[nodiscard]] static f32 easeOutCubic(f32 value) noexcept;
    [[nodiscard]] static f32 easeInOutCubic(f32 value) noexcept;

    void updateCamera(const utils::render::component::ShaderCameraState& shaderCamera) noexcept;
    void cacheUniformLocations(Shader shader) const noexcept;
    void applyShaderUniforms() const noexcept;
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
