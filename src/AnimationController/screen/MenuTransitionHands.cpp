#include "MenuTransitionHands.hpp"
#include "Data/Data.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "AnimationController/animation/Easing.hpp"
#include <algorithm>
#include <cmath>
#include <span>
#include <string_view>
#include <spdlog/spdlog.h>
#include <raymath.h>
#include <rlgl.h>

namespace biofuel::animation::screen {

namespace {

constexpr f32 BASE_SCALE_MULTIPLIER = 1.18f;
constexpr f32 BASE_HAND_Y = -0.30f;
constexpr f32 BASE_HAND_Z = 0.18f;
constexpr f32 ROOT_TRANSLATION_Y_WEIGHT = 0.24f;
constexpr f32 ROOT_TRANSLATION_Z_WEIGHT = 0.12f;
constexpr f32 HAND_CONTROL_X_OFFSET = 0.30f;
constexpr f32 HAND_CONTROL_Y_OFFSET = -0.06f;
constexpr f32 HAND_CONTROL_Z_OFFSET = 0.03f;
constexpr Vector3 BASE_CAMERA_POSITION{0.0f, -0.14f, 2.22f};
constexpr Vector3 BASE_CAMERA_TARGET{0.0f, -0.18f, 0.08f};
constexpr f32 BASE_CAMERA_FOVY = 32.0f;
constexpr f32 CAMERA_YAW_POSITION_WEIGHT = 0.12f;
constexpr f32 CAMERA_YAW_TARGET_WEIGHT = 0.10f;
constexpr f32 CAMERA_SHIFT_Y_WEIGHT = 0.01f;
constexpr f32 CAMERA_SHIFT_Z_WEIGHT = -0.06f;
constexpr f32 HAND_SHIFT_Y_WEIGHT = 0.01f;
constexpr f32 HAND_SHIFT_Z_WEIGHT = -0.02f;
constexpr f32 HAND_FLOAT_AMPLITUDE = 0.010f;
constexpr f32 HAND_FLOAT_FREQUENCY = 1.18f;
constexpr f32 HAND_FLOAT_WEIGHT = 0.18f;

struct BoneOffsetWeight {
    std::string_view boneName;
    f32 weight = 1.0f;
};

constexpr BoneOffsetWeight LEFT_HAND_CONTROL_BONES[] = {
    {.boneName = "shoulder.l", .weight = 0.05f},
    {.boneName = "bicep.l", .weight = 0.10f},
    {.boneName = "forearm.l", .weight = 0.48f},
    {.boneName = "forearm.Twist0.l", .weight = 0.10f},
    {.boneName = "forearm.Twist1.l", .weight = 0.08f},
    {.boneName = "wrist.l", .weight = 0.19f},
};

constexpr BoneOffsetWeight RIGHT_HAND_CONTROL_BONES[] = {
    {.boneName = "shoulder.r", .weight = 0.05f},
    {.boneName = "bicep.r", .weight = 0.10f},
    {.boneName = "forearm.r", .weight = 0.48f},
    {.boneName = "forearm.Twist0.r", .weight = 0.10f},
    {.boneName = "forearm.Twist1.r", .weight = 0.08f},
    {.boneName = "wrist.r", .weight = 0.19f},
};

class BackfaceCullingScope final {
public:
    BackfaceCullingScope() noexcept {
        rlDisableBackfaceCulling();
    }

    ~BackfaceCullingScope() noexcept {
        rlEnableBackfaceCulling();
    }

    BackfaceCullingScope(const BackfaceCullingScope&) = delete;
    BackfaceCullingScope& operator=(const BackfaceCullingScope&) = delete;
};

[[nodiscard]] constexpr Vector3 scaled(const Vector3 value, const f32 weight) noexcept {
    return Vector3{value.x * weight, value.y * weight, value.z * weight};
}

void applyWeightedBoneOffsets(
    systems::model::ModelInstance& instance,
    const std::span<const BoneOffsetWeight> weights,
    const Vector3 offset)
{
    for (const auto& weight : weights) {
        instance.setBoneTranslationOffset(weight.boneName, scaled(offset, weight.weight));
    }
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
    m_baseScale = BASE_SCALE_MULTIPLIER * metrics.unitScale;
    m_fingerStartY = metrics.localBounds.min.y + metrics.size.y * 0.44f;
    m_fingerEndY = metrics.localBounds.min.y + metrics.size.y * 0.92f;
    m_instance->setAnimationState("idle");
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    applyControllerOffsets();
#endif
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
    m_phase = Phase::Idle;
    m_elapsed = 0.0f;
    m_dimensionShift = 0.0f;
    m_cameraYaw = 0.0f;
    m_camera.position = BASE_CAMERA_POSITION;
    m_baseScale = std::max(m_baseScale, 0.0001f);
    m_timeLoc = -1;
    m_portalStrengthLoc = -1;
    m_fingerStartLoc = -1;
    m_fingerEndLoc = -1;
    m_colorALoc = -1;
    m_colorBLoc = -1;
    m_rimColorLoc = -1;
    m_cachedShaderId = 0;
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    m_controller.reset();
    m_rootOffset = Vector3{0.0f, 0.0f, 0.0f};
    m_cameraPositionOffset = Vector3{0.0f, 0.0f, 0.0f};
    m_cameraTargetOffset = Vector3{0.0f, 0.0f, 0.0f};
    m_leftHandOffset = Vector3{0.0f, 0.0f, 0.0f};
    m_rightHandOffset = Vector3{0.0f, 0.0f, 0.0f};
#endif
    m_camera.target = BASE_CAMERA_TARGET;
    m_camera.up = Vector3{0.0f, 1.0f, 0.0f};
    m_camera.fovy = BASE_CAMERA_FOVY;
    m_camera.projection = CAMERA_PERSPECTIVE;
    if (m_instance) {
        m_instance->resetAnimation();
        m_instance->setAnimationState("idle");
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
        applyControllerOffsets();
#endif
    }
}

void MenuTransitionHands::start() noexcept {
    if (!m_loaded || !m_instance) {
        return;
    }

    m_phase = Phase::Playing;
    m_elapsed = 0.0f;
    m_instance->playAction("action", 0.12f);
}

void MenuTransitionHands::update(
    const f32 dt,
    const f32 dimensionShift,
    const utils::render::component::ShaderCameraState& shaderCamera) noexcept
{
    if (m_phase == Phase::Idle || !m_loaded || !m_instance) {
        return;
    }

    m_elapsed += dt;

    // Auto-transition: Playing → Complete after action clip finishes
    if (m_phase == Phase::Playing && m_elapsed >= ACTION_DURATION) {
        m_phase = Phase::Complete;
    }

    m_dimensionShift = std::max(m_dimensionShift, std::clamp(dimensionShift, 0.0f, 1.0f));
    m_cameraYaw = shaderCamera.yaw;
    updateCamera(shaderCamera);
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    const TransitionRenderPose pose = buildPose(*m_instance);
    updateController(pose);
    applyControllerOffsets();
#endif
}

void MenuTransitionHands::render() noexcept {
    if (!isRendering() || !m_loaded || !m_instance) {
        return;
    }

#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    applyControllerOffsets();
    m_instance->update(0.0f);
#endif

    const TransitionRenderPose pose = buildPose(*m_instance);

    BeginMode3D(m_camera);
    drawHands(*m_instance, pose);
    EndMode3D();
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    renderController(pose);
#endif
}

MenuTransitionHands::TransitionRenderPose MenuTransitionHands::buildPose(
    const systems::model::ModelInstance& instance) const noexcept
{
    const auto& keyframe = instance.keyframeState();
    const f32 shift = animation::Easing::easeInOutCubic(m_dimensionShift);
    const f32 awareness = instance.keyframeScalar("awareness", 0.0f);
    const f32 auraBias = instance.keyframeScalar("aura_bias", 0.12f);
    const f32 portalBias = instance.keyframeScalar("portal_bias", 0.20f);
    const f32 floatLift = std::sin(m_elapsed * HAND_FLOAT_FREQUENCY) * HAND_FLOAT_AMPLITUDE;

    TransitionRenderPose pose;
    pose.position = Vector3{
        m_cameraYaw * 0.06f
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
            + m_rootOffset.x
#endif
        ,
        BASE_HAND_Y + keyframe.rootTranslation.y * ROOT_TRANSLATION_Y_WEIGHT + floatLift * HAND_FLOAT_WEIGHT + shift * HAND_SHIFT_Y_WEIGHT
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
            + m_rootOffset.y
#endif
        ,
        BASE_HAND_Z + keyframe.rootTranslation.z * ROOT_TRANSLATION_Z_WEIGHT + shift * HAND_SHIFT_Z_WEIGHT
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
            + m_rootOffset.z
#endif
    };

    const Quaternion baseOrientation = QuaternionFromEuler(
        (28.0f + awareness * 2.0f) * DEG2RAD,
        m_cameraYaw * 0.08f,
        0.0f
    );
    pose.orientation = QuaternionNormalize(QuaternionMultiply(baseOrientation, keyframe.rootRotation));
    pose.scale = Vector3{
        m_baseScale * keyframe.rootScale.x,
        m_baseScale * keyframe.rootScale.y,
        m_baseScale * keyframe.rootScale.z
    };

    const f32 auraAlpha = 6.0f + auraBias * 10.0f + shift * 6.0f;
    const f32 baseAlpha = 255.0f;
    pose.auraTint = Color{
        static_cast<u8>(84 + awareness * 8.0f),
        static_cast<u8>(98 + auraBias * 8.0f),
        static_cast<u8>(178 + portalBias * 8.0f),
        static_cast<u8>(std::clamp(auraAlpha, 0.0f, 255.0f))
    };
    pose.baseTint = Color{
        static_cast<u8>(252),
        static_cast<u8>(252),
        static_cast<u8>(255),
        static_cast<u8>(std::clamp(baseAlpha, 0.0f, 255.0f))
    };
    return pose;
}

void MenuTransitionHands::drawHands(
    const systems::model::ModelInstance& instance,
    const TransitionRenderPose& pose) const noexcept
{
    Vector3 axis = Vector3{0.0f, 1.0f, 0.0f};
    f32 angle = 0.0f;
    QuaternionToAxisAngle(pose.orientation, &axis, &angle);
    const f32 angleDegrees = angle * RAD2DEG;

    applyShaderUniforms(instance);
    const BackfaceCullingScope doubleSidedHands;
    instance.draw(systems::model::ModelRenderState{
        .position = pose.position,
        .rotationAxis = axis,
        .scale = Vector3{pose.scale.x * 1.005f, pose.scale.y * 1.005f, pose.scale.z * 1.005f},
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

void MenuTransitionHands::updateCamera(
    const utils::render::component::ShaderCameraState& shaderCamera) noexcept
{
    const f32 shift = animation::Easing::easeInOutCubic(m_dimensionShift);
    m_camera.position = Vector3{
        shaderCamera.yaw * CAMERA_YAW_POSITION_WEIGHT,
        BASE_CAMERA_POSITION.y + shift * CAMERA_SHIFT_Y_WEIGHT,
        BASE_CAMERA_POSITION.z + shift * CAMERA_SHIFT_Z_WEIGHT
    };
    m_camera.target = Vector3{
        shaderCamera.yaw * CAMERA_YAW_TARGET_WEIGHT,
        BASE_CAMERA_TARGET.y,
        BASE_CAMERA_TARGET.z
    };
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    m_camera.position.x += m_cameraPositionOffset.x;
    m_camera.position.y += m_cameraPositionOffset.y;
    m_camera.position.z += m_cameraPositionOffset.z;
    m_camera.target.x += m_cameraTargetOffset.x;
    m_camera.target.y += m_cameraTargetOffset.y;
    m_camera.target.z += m_cameraTargetOffset.z;
#endif
}

#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
MenuTransitionHands::ControllerTargets MenuTransitionHands::buildControllerTargets(const TransitionRenderPose& pose) noexcept {
    const Vector3 rootBase{
        pose.position.x - m_rootOffset.x,
        pose.position.y - m_rootOffset.y,
        pose.position.z - m_rootOffset.z,
    };
    const Vector3 cameraPositionBase{
        m_camera.position.x - m_cameraPositionOffset.x,
        m_camera.position.y - m_cameraPositionOffset.y,
        m_camera.position.z - m_cameraPositionOffset.z,
    };
    const Vector3 cameraTargetBase{
        m_camera.target.x - m_cameraTargetOffset.x,
        m_camera.target.y - m_cameraTargetOffset.y,
        m_camera.target.z - m_cameraTargetOffset.z,
    };

    const u64 instanceId = m_instance ? m_instance->instanceId() : 0;
    return ControllerTargets{{
        ModelControlTarget{
            .name = "hands.root",
            .instanceId = instanceId,
            .baseWorldPosition = rootBase,
            .runtimeOffset = &m_rootOffset,
            .screenHalf = ModelControlScreenHalf::Any,
            .color = Color{255, 226, 86, 255},
        },
        ModelControlTarget{
            .name = "camera.position",
            .instanceId = instanceId,
            .baseWorldPosition = cameraPositionBase,
            .runtimeOffset = &m_cameraPositionOffset,
            .screenHalf = ModelControlScreenHalf::Any,
            .color = Color{114, 196, 255, 255},
        },
        ModelControlTarget{
            .name = "camera.target",
            .instanceId = instanceId,
            .baseWorldPosition = cameraTargetBase,
            .runtimeOffset = &m_cameraTargetOffset,
            .screenHalf = ModelControlScreenHalf::Any,
            .color = Color{160, 255, 178, 255},
        },
        ModelControlTarget{
            .name = "left.hand",
            .instanceId = instanceId,
            .boneName = "wrist.l",
            .baseWorldPosition = Vector3{
                pose.position.x - HAND_CONTROL_X_OFFSET,
                pose.position.y + HAND_CONTROL_Y_OFFSET,
                pose.position.z + HAND_CONTROL_Z_OFFSET
            },
            .runtimeOffset = &m_leftHandOffset,
            .screenHalf = ModelControlScreenHalf::Left,
            .color = Color{255, 120, 120, 255},
        },
        ModelControlTarget{
            .name = "right.hand",
            .instanceId = instanceId,
            .boneName = "wrist.r",
            .baseWorldPosition = Vector3{
                pose.position.x + HAND_CONTROL_X_OFFSET,
                pose.position.y + HAND_CONTROL_Y_OFFSET,
                pose.position.z + HAND_CONTROL_Z_OFFSET
            },
            .runtimeOffset = &m_rightHandOffset,
            .screenHalf = ModelControlScreenHalf::Right,
            .color = Color{140, 180, 255, 255},
        },
    }};
}

void MenuTransitionHands::updateController(const TransitionRenderPose& pose) noexcept {
    auto targets = buildControllerTargets(pose);
    m_controller.update(std::span<ModelControlTarget>(targets.data(), targets.size()), m_camera);
}

void MenuTransitionHands::renderController(const TransitionRenderPose& pose) noexcept {
    auto targets = buildControllerTargets(pose);
    m_controller.render(std::span<const ModelControlTarget>(targets.data(), targets.size()), m_camera);
}

void MenuTransitionHands::applyControllerOffsets() noexcept {
    if (!m_instance) {
        return;
    }

    applyWeightedBoneOffsets(*m_instance, LEFT_HAND_CONTROL_BONES, m_leftHandOffset);
    applyWeightedBoneOffsets(*m_instance, RIGHT_HAND_CONTROL_BONES, m_rightHandOffset);
}
#endif

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

void MenuTransitionHands::applyShaderUniforms(const systems::model::ModelInstance& instance) const noexcept {
    const Shader shader = instance.shader();
    if (!IsShaderValid(shader)) {
        return;
    }

    cacheUniformLocations(shader);

    const f32 portalStrength = std::clamp(
        instance.keyframeScalar("portal_bias", 0.18f) + animation::Easing::easeInOutCubic(m_dimensionShift) * 0.50f,
        0.0f,
        1.45f
    );
    const f32 colorA[3] = {0.04f, 0.20f, 0.44f};
    const f32 colorB[3] = {0.18f, 0.66f, 1.00f};
    const f32 rimColor[3] = {0.20f, 0.86f, 1.00f};

    utils::render::ShaderManager::setValue(shader, m_timeLoc, &m_elapsed, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_portalStrengthLoc, &portalStrength, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_fingerStartLoc, &m_fingerStartY, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_fingerEndLoc, &m_fingerEndY, SHADER_UNIFORM_FLOAT);
    utils::render::ShaderManager::setValue(shader, m_colorALoc, colorA, SHADER_UNIFORM_VEC3);
    utils::render::ShaderManager::setValue(shader, m_colorBLoc, colorB, SHADER_UNIFORM_VEC3);
    utils::render::ShaderManager::setValue(shader, m_rimColorLoc, rimColor, SHADER_UNIFORM_VEC3);
}

} // namespace biofuel::animation::screen
