#include "MenuTransitionHands.hpp"
#include "Data/Data.hpp"
#include "Utils/render/ShaderManager.hpp"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <raymath.h>

namespace biofuel::animation::screen {

namespace {

constexpr f32 HAND_LAUNCH_DELAY = 0.16f;
constexpr f32 HAND_SETTLE_DURATION = 3.1f;
constexpr f32 HAND_PI = 3.14159265f;

[[nodiscard]] constexpr Quaternion identityQuaternion() noexcept {
    return Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
}

} // namespace

void MenuTransitionHands::load() {
    if (m_loaded) {
        return;
    }

    auto leftInstance = Data::models().createInstance(ASSET_ID);
    auto rightInstance = Data::models().createInstance(ASSET_ID);
    if (!leftInstance || !rightInstance || !leftInstance->ready() || !rightInstance->ready()) {
        spdlog::warn("MenuTransitionHands: model instances unavailable");
        return;
    }

    m_leftInstance = std::move(leftInstance);
    m_rightInstance = std::move(rightInstance);
    const auto& metrics = m_leftInstance->metrics();
    m_baseScale = 1.05f * metrics.unitScale;
    m_fingerStartY = metrics.localBounds.min.y + metrics.size.y * 0.38f;
    m_fingerEndY = metrics.localBounds.min.y + metrics.size.y * 0.95f;
    m_leftInstance->setAnimationState("idle");
    m_rightInstance->setAnimationState("idle");
    m_loaded = true;
}

void MenuTransitionHands::unload() noexcept {
    if (!m_loaded) {
        return;
    }

    m_leftInstance.reset();
    m_rightInstance.reset();
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
    m_sideLoc = -1;
    m_cachedShaderId = 0;
    m_camera.target = Vector3{0.0f, 0.0f, 0.0f};
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.fovy = 30.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
    if (m_leftInstance) {
        m_leftInstance->resetAnimation();
        m_leftInstance->setAnimationState("idle");
    }
    if (m_rightInstance) {
        m_rightInstance->resetAnimation();
        m_rightInstance->setAnimationState("idle");
    }
}

void MenuTransitionHands::start() noexcept {
    if (!m_loaded) {
        return;
    }

    m_active = true;
    m_elapsed = 0.0f;
    if (m_leftInstance) {
        m_leftInstance->playAction("action", 0.10f);
    }
    if (m_rightInstance) {
        m_rightInstance->playAction("action", 0.18f);
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
    if (!m_active || !m_loaded || !m_leftInstance || !m_rightInstance) {
        return;
    }

    const HandRenderPose leftPose = buildHandPose(*m_leftInstance, -1.0f, 0.0f);
    const HandRenderPose rightPose = buildHandPose(*m_rightInstance, 1.0f, 0.12f);

    BeginMode3D(m_camera);
    drawHand(*m_leftInstance, leftPose);
    drawHand(*m_rightInstance, rightPose);
    EndMode3D();
}

MenuTransitionHands::HandRenderPose MenuTransitionHands::buildHandPose(
    const systems::model::ModelInstance& instance,
    const f32 sideSign,
    const f32 phaseOffset) const noexcept
{
    const auto& keyframe = instance.keyframeState();
    const f32 shift = easeInOutCubic(m_dimensionShift);
    const f32 settle = easeInOutCubic(saturate((m_elapsed - phaseOffset) / HAND_SETTLE_DURATION));
    const f32 delayT = saturate((m_elapsed - phaseOffset - HAND_LAUNCH_DELAY) / 0.42f);
    const f32 anticipation = easeInOutSine(delayT);
    const f32 awareness = instance.keyframeScalar("awareness", 0.0f);
    const f32 auraBias = instance.keyframeScalar("aura_bias", 0.15f);
    const f32 portalBias = instance.keyframeScalar("portal_bias", 0.20f);
    const f32 floatLift = std::sin((m_elapsed + phaseOffset) * 1.15f) * 0.012f;

    HandRenderPose pose;
    pose.position = Vector3{
        sideSign * (0.46f + keyframe.rootTranslation.x * (0.92f + 0.08f * awareness)) + m_cameraYaw * 0.18f,
        -0.74f + keyframe.rootTranslation.y * 0.78f + floatLift + shift * 0.05f - (1.0f - anticipation) * 0.06f,
        0.18f + keyframe.rootTranslation.z * 0.48f - shift * 0.08f - awareness * 0.03f
    };

    const Quaternion baseOrientation = QuaternionFromEuler(
        (-12.0f + awareness * 3.0f) * DEG2RAD,
        sideSign * (-18.0f + settle * 12.0f + awareness * 6.0f) * DEG2RAD,
        sideSign * (22.0f - awareness * 10.0f) * DEG2RAD
    );
    pose.orientation = QuaternionNormalize(QuaternionMultiply(baseOrientation, keyframe.rootRotation));

    pose.scale = Vector3{
        m_baseScale * keyframe.rootScale.x * sideSign,
        m_baseScale * keyframe.rootScale.y,
        m_baseScale * keyframe.rootScale.z
    };

    const f32 auraAlpha = 92.0f + auraBias * 95.0f + shift * 42.0f;
    const f32 baseAlpha = 180.0f + auraBias * 48.0f + portalBias * 18.0f;
    pose.auraTint = Color{
        static_cast<u8>(82 + awareness * 44.0f),
        static_cast<u8>(126 + auraBias * 38.0f),
        static_cast<u8>(214 + portalBias * 30.0f),
        static_cast<u8>(std::clamp(auraAlpha, 0.0f, 255.0f))
    };
    pose.baseTint = Color{
        static_cast<u8>(220 + awareness * 18.0f),
        static_cast<u8>(232 + auraBias * 14.0f),
        static_cast<u8>(255),
        static_cast<u8>(std::clamp(baseAlpha, 0.0f, 255.0f))
    };
    return pose;
}

void MenuTransitionHands::drawHand(
    const systems::model::ModelInstance& instance,
    const HandRenderPose& pose) const noexcept
{
    Vector3 axis = Vector3{0.0f, 1.0f, 0.0f};
    f32 angle = 0.0f;
    QuaternionToAxisAngle(pose.orientation, &axis, &angle);
    const f32 angleDegrees = angle * RAD2DEG;

    applyShaderUniforms(instance, (pose.scale.x < 0.0f) ? -1.0f : 1.0f);
    instance.draw(systems::model::ModelRenderState{
        .position = pose.position,
        .rotationAxis = axis,
        .scale = Vector3{pose.scale.x * 1.04f, pose.scale.y * 1.04f, pose.scale.z * 1.04f},
        .rotationDegrees = angleDegrees,
        .tint = pose.auraTint,
        .visible = true,
    });
    instance.draw(systems::model::ModelRenderState{
        .position = pose.position,
        .rotationAxis = axis,
        .scale = pose.scale,
        .rotationDegrees = angleDegrees,
        .tint = pose.baseTint,
        .visible = true,
    });
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

f32 MenuTransitionHands::easeInOutSine(const f32 value) noexcept {
    const f32 t = saturate(value);
    return -(std::cos(HAND_PI * t) - 1.0f) * 0.5f;
}

void MenuTransitionHands::updateCamera(
    const utils::render::component::ShaderCameraState& shaderCamera) noexcept
{
    const f32 shift = easeInOutCubic(m_dimensionShift);
    m_camera.position = Vector3{
        shaderCamera.yaw * 0.30f,
        0.08f + shift * 0.05f,
        3.35f - shift * 0.22f
    };
    m_camera.target = Vector3{
        shaderCamera.yaw * 0.35f,
        -0.38f,
        0.04f
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
    m_sideLoc = utils::render::ShaderManager::getLocation(shader, "uSideSign");
    m_cachedShaderId = shader.id;
}

void MenuTransitionHands::applyShaderUniforms(
    const systems::model::ModelInstance& instance,
    const f32 sideSign) const noexcept
{
    const Shader shader = instance.shader();
    if (!IsShaderValid(shader)) {
        return;
    }

    cacheUniformLocations(shader);

    const f32 portalStrength = std::clamp(
        instance.keyframeScalar("portal_bias", 0.18f) + easeInOutCubic(m_dimensionShift) * 0.55f,
        0.0f,
        1.45f);
    const f32 colorA[3] = {0.20f, 0.28f, 0.44f};
    const f32 colorB[3] = {0.90f, 0.95f, 1.00f};
    const f32 rimColor[3] = {0.72f, 0.60f, 0.98f};

    utils::render::ShaderManager::setValue(shader, m_timeLoc, &m_elapsed, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_portalStrengthLoc, &portalStrength, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_fingerStartLoc, &m_fingerStartY, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_fingerEndLoc, &m_fingerEndY, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_colorALoc, colorA, SHADER_UNIFORM_VEC3);
    utils::render::ShaderManager::setValue(shader, m_colorBLoc, colorB, SHADER_UNIFORM_VEC3);
    utils::render::ShaderManager::setValue(shader, m_rimColorLoc, rimColor, SHADER_UNIFORM_VEC3);
    utils::render::ShaderManager::setValue(shader, m_sideLoc, &sideSign, SHADER_UNIFORM_FLOAT);
}

} // namespace biofuel::animation::screen
