#include "MenuTransitionHands.hpp"
#include "Data/Data.hpp"
#include "Utils/render/ShaderManager.hpp"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <raymath.h>

namespace biofuel::animation::screen {

namespace {

constexpr f32 HAND_APPEAR_DELAY = 0.08f;
constexpr f32 HAND_APPEAR_DURATION = 1.45f;
constexpr f32 HAND_SETTLE_DURATION = 2.6f;
constexpr f32 HAND_IDLE_GESTURE_SPEED = 1.65f;
constexpr f32 HAND_PI = 3.14159265f;

[[nodiscard]] f32 saturateValue(const f32 value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

void MenuTransitionHands::load() {
    if (m_loaded) {
        return;
    }

    auto instance = Data::models().createInstance(ASSET_ID);
    if (!instance || !instance->ready()) {
        spdlog::warn("MenuTransitionHands: model instance unavailable");
        return;
    }

    m_instance = std::move(instance);
    const auto& metrics = m_instance->metrics();
    m_baseScale = 1.15f * metrics.unitScale;
    m_fingerStartY = metrics.localBounds.min.y + metrics.size.y * 0.40f;
    m_fingerEndY = metrics.localBounds.min.y + metrics.size.y * 0.92f;
    m_instance->setAnimationState("idle");
    m_loaded = true;
}

void MenuTransitionHands::unload() noexcept {
    if (!m_loaded) {
        return;
    }

    m_instance.reset();
    m_loaded = false;
    m_baseScale = 1.0f;
    reset();
}

void MenuTransitionHands::reset() noexcept {
    m_active = false;
    m_elapsed = 0.0f;
    m_dimensionShift = 0.0f;
    m_cameraYaw = 0.0f;
    m_camera.position = Vector3{0.0f, 0.15f, 2.55f};
    m_baseScale = std::max(m_baseScale, 0.0001f);
    m_timeLoc = -1;
    m_portalStrengthLoc = -1;
    m_fingerStartLoc = -1;
    m_fingerEndLoc = -1;
    m_colorALoc = -1;
    m_colorBLoc = -1;
    m_rimColorLoc = -1;
    m_cachedShaderId = 0;
    m_camera.target = Vector3{0.0f, 0.0f, 0.0f};
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.fovy = 30.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
}

void MenuTransitionHands::start() noexcept {
    if (!m_loaded) {
        return;
    }

    m_active = true;
    m_elapsed = 0.0f;
    if (m_instance) {
        m_instance->playAction("action", 0.12f);
    }
}

void MenuTransitionHands::update(
    const f32 dt,
    const f32 dimensionShift,
    const utils::render::component::ShaderCameraState& shaderCamera) noexcept
{
    if (!m_active || !m_loaded) {
        return;
    }

    m_elapsed += dt;
    m_dimensionShift = std::max(m_dimensionShift, saturate(dimensionShift));
    m_cameraYaw = shaderCamera.yaw;
    updateCamera(shaderCamera);
}

void MenuTransitionHands::render() const noexcept {
    if (!m_active || !m_loaded) {
        return;
    }

    const f32 appearT = saturate((m_elapsed - HAND_APPEAR_DELAY) / HAND_APPEAR_DURATION);
    const f32 settleT = saturate(m_elapsed / HAND_SETTLE_DURATION);
    const f32 appear = easeOutCubic(appearT);
    const f32 settle = easeInOutCubic(settleT);
    const f32 shift = easeInOutCubic(m_dimensionShift);
    const auto& animator = m_instance->animator();
    const bool actionState = (animator.currentState() == "action");
    const f32 stateT = animator.stateProgress();
    const f32 transitionT = animator.transitionProgress();
    const f32 actionArc = actionState ? std::sin(stateT * HAND_PI) : 0.0f;
    const f32 actionStrength = actionArc * (0.7f + 0.3f * transitionT);
    const f32 idleStrength = actionState ? 0.0f : 0.16f;
    const f32 gesture = (std::sin(m_elapsed * HAND_IDLE_GESTURE_SPEED) * 0.5f + 0.5f)
        * (0.45f + idleStrength * 0.55f) + actionStrength * 0.28f;
    const f32 floatOffset = std::sin(m_elapsed * 1.2f) * (0.018f + actionStrength * 0.018f);
    const f32 reachForward = actionStrength * 0.20f;
    const f32 lift = actionStrength * 0.10f;
    const f32 spread = actionStrength * 0.12f;

    const Vector3 position = {
        m_cameraYaw * (0.32f + spread * 0.18f),
        -1.32f + appear * 0.28f + floatOffset + shift * 0.04f + lift,
        0.16f - appear * 0.12f - shift * 0.08f + gesture * 0.03f - reachForward
    };
    const f32 scaleValue = m_baseScale * (1.05f + settle * 0.08f + actionStrength * 0.10f);
    const Vector3 scale = {
        scaleValue * (1.0f + spread * 0.10f),
        scaleValue * (1.0f - actionStrength * 0.05f),
        scaleValue
    };
    const Vector3 rotationAxis = {0.36f, 1.0f, 0.0f};
    const f32 yawDegrees = -m_cameraYaw * 55.0f + std::sin(m_elapsed * 0.85f) * 4.0f - 7.0f
        + actionStrength * 8.5f;
    const f32 auraAlpha = 70.0f + appear * 65.0f + shift * 35.0f + actionStrength * 52.0f;
    const f32 baseAlpha = 125.0f + appear * 95.0f + actionStrength * 26.0f;

    applyShaderUniforms();

    BeginMode3D(m_camera);
    m_instance->draw(systems::model::ModelRenderState{
        .position = position,
        .rotationAxis = rotationAxis,
        .scale = Vector3{scale.x * 1.06f, scale.y * 1.06f, scale.z * 1.06f},
        .rotationDegrees = yawDegrees,
        .tint = Color{88, 136, 220, static_cast<u8>(std::clamp(auraAlpha, 0.0f, 255.0f))},
        .visible = true,
    });
    m_instance->draw(systems::model::ModelRenderState{
        .position = position,
        .rotationAxis = rotationAxis,
        .scale = scale,
        .rotationDegrees = yawDegrees,
        .tint = Color{232, 238, 255, static_cast<u8>(std::clamp(baseAlpha, 0.0f, 255.0f))},
        .visible = true,
    });
    EndMode3D();
}

f32 MenuTransitionHands::saturate(const f32 value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

f32 MenuTransitionHands::easeOutCubic(const f32 value) noexcept {
    const f32 t = saturate(value) - 1.0f;
    return t * t * t + 1.0f;
}

f32 MenuTransitionHands::easeInOutCubic(const f32 value) noexcept {
    const f32 t = saturate(value);
    return (t < 0.5f)
        ? (4.0f * t * t * t)
        : (1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f);
}

void MenuTransitionHands::updateCamera(
    const utils::render::component::ShaderCameraState& shaderCamera) noexcept
{
    const f32 shift = easeInOutCubic(m_dimensionShift);
    m_camera.position = Vector3{
        shaderCamera.yaw * 0.35f,
        0.18f + shift * 0.03f,
        3.65f - shift * 0.18f
    };
    m_camera.target = Vector3{
        shaderCamera.yaw * 0.42f,
        -0.46f,
        0.0f
    };
}

void MenuTransitionHands::cacheUniformLocations(const Shader shader) const noexcept {
    if (m_cachedShaderId == shader.id) {
        return;
    }

    m_timeLoc = utils::render::ShaderManager::getLocation(shader, "uTime");
    m_portalStrengthLoc = utils::render::ShaderManager::getLocation(shader, "uPortalStrength");
    m_fingerStartLoc = utils::render::ShaderManager::getLocation(shader, "uFingerStartY");
    m_fingerEndLoc = utils::render::ShaderManager::getLocation(shader, "uFingerEndY");
    m_colorALoc = utils::render::ShaderManager::getLocation(shader, "uColorA");
    m_colorBLoc = utils::render::ShaderManager::getLocation(shader, "uColorB");
    m_rimColorLoc = utils::render::ShaderManager::getLocation(shader, "uRimColor");
    m_cachedShaderId = shader.id;
}

void MenuTransitionHands::applyShaderUniforms() const noexcept {
    if (!m_instance || !m_instance->ready()) {
        return;
    }

    const Shader shader = m_instance->shader();
    if (!IsShaderValid(shader)) {
        return;
    }

    cacheUniformLocations(shader);

    const f32 appearT = saturate((m_elapsed - HAND_APPEAR_DELAY) / HAND_APPEAR_DURATION);
    const auto& animator = m_instance->animator();
    const bool actionState = (animator.currentState() == "action");
    const f32 actionArc = actionState ? std::sin(animator.stateProgress() * HAND_PI) : 0.0f;
    const f32 portalStrength = std::max(easeOutCubic(appearT), m_dimensionShift)
        + actionArc * (0.22f + 0.18f * saturateValue(animator.transitionProgress()));
    const f32 colorA[3] = {0.24f, 0.34f, 0.56f};
    const f32 colorB[3] = {0.88f, 0.93f, 1.0f};
    const f32 rimColor[3] = {0.72f, 0.56f, 0.98f};

    utils::render::ShaderManager::setValue(shader, m_timeLoc, &m_elapsed, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_portalStrengthLoc, &portalStrength, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_fingerStartLoc, &m_fingerStartY, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_fingerEndLoc, &m_fingerEndY, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_colorALoc, colorA, SHADER_UNIFORM_VEC3);
    utils::render::ShaderManager::setValue(shader, m_colorBLoc, colorB, SHADER_UNIFORM_VEC3);
    utils::render::ShaderManager::setValue(shader, m_rimColorLoc, rimColor, SHADER_UNIFORM_VEC3);
}

} // namespace biofuel::animation::screen
