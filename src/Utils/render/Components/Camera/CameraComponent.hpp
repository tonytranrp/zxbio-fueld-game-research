#pragma once

#include "Utils/render/Components/ComponentModule.hpp"
#include "Utils/render/Components/Camera/ShaderCamera.hpp"
#include "Utils/render/ShaderManager.hpp"

namespace biofuel::utils::render::component {

// ==============================================================================
// CameraComponent — Shader component for camera perspective manipulation
// ==============================================================================
//
// Wraps a ShaderCameraController and exposes it through the ComponentModule
// interface. When applied to a shader, it automatically writes the camera
// uniforms (uCameraYaw, uCameraOffsetX, uCameraOffsetY).
//
// Uniform locations are cached on first apply() to avoid per-frame GL lookups.
//
// Usage via ComponentManager:
//   auto camera = std::make_unique<CameraComponent>();
//   camera->controller().setTarget(...);
//   components.add(std::move(camera));
//
// Usage standalone:
//   CameraComponent cam;
//   cam.controller().setTarget(ShaderCameraState{.yaw = -0.3f}, 2.0f);
//   cam.update(dt);
//   cam.apply(shader);
//
// ==============================================================================

class CameraComponent final : public ComponentModule {
public:
    // Uniform name constants for camera GLSL uniforms
    static constexpr std::string_view UNIFORM_CAMERA_YAW       = "uCameraYaw";
    static constexpr std::string_view UNIFORM_CAMERA_OFFSET_X  = "uCameraOffsetX";
    static constexpr std::string_view UNIFORM_CAMERA_OFFSET_Y  = "uCameraOffsetY";

    static constexpr std::string_view COMPONENT_NAME = "camera";

    // ---- ComponentModule interface ----
    [[nodiscard]] std::string_view name() const noexcept override {
        return COMPONENT_NAME;
    }

    void reset() noexcept override {
        m_controller.reset();
    }

    void update(f32 dt) noexcept override {
        m_controller.update(dt);
    }

    void apply(Shader shader) const noexcept override {
        cacheLocations(shader);

        const auto& state = m_controller.current();
        ShaderManager::setValue(shader, m_yawLoc, &state.yaw, SHADER_UNIFORM_FLOAT);
        ShaderManager::setValue(shader, m_offsetXLoc, &state.offsetX, SHADER_UNIFORM_FLOAT);
        ShaderManager::setValue(shader, m_offsetYLoc, &state.offsetY, SHADER_UNIFORM_FLOAT);
    }

    [[nodiscard]] bool isActive() const noexcept override {
        return m_controller.isAnimating();
    }

    // ---- Camera-specific API ----
    [[nodiscard]] ShaderCameraController& controller() noexcept { return m_controller; }
    [[nodiscard]] const ShaderCameraController& controller() const noexcept { return m_controller; }

private:
    ShaderCameraController m_controller;

    // Cached uniform locations — lazily resolved on first apply().
    // Mutable because apply() is const but caching is an implementation detail.
    mutable i32 m_yawLoc     = -1;
    mutable i32 m_offsetXLoc = -1;
    mutable i32 m_offsetYLoc = -1;
    mutable u32 m_cachedShaderId = 0;

    void cacheLocations(Shader shader) const noexcept {
        // Re-cache if shader changed (e.g. hot-reload or different shader)
        if (m_cachedShaderId == shader.id) {
            return;
        }
        m_yawLoc     = ShaderManager::getLocation(shader, UNIFORM_CAMERA_YAW);
        m_offsetXLoc = ShaderManager::getLocation(shader, UNIFORM_CAMERA_OFFSET_X);
        m_offsetYLoc = ShaderManager::getLocation(shader, UNIFORM_CAMERA_OFFSET_Y);
        m_cachedShaderId = shader.id;
    }
};

} // namespace biofuel::utils::render::component
