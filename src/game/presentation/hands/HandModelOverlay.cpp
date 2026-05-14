#include "game/presentation/hands/HandModelOverlay.hpp"

#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/runtime/Runtime.hpp"
#include <raymath.h>

namespace biofuel::game::presentation::hands {

namespace {
using ::biofuel::engine::custom::procedural::hand::DefaultRobotHandPreset;
using ::biofuel::engine::custom::procedural::hand::RobotHandRenderOptions;
} // namespace

void ensureModelOnlyHandTracking() noexcept {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    auto& handPose = ::biofuel::engine::runtime::Runtime::handPose();
    if (!handPose.calibrationValid()) {
        return;
    }

    auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    tracking.setPreviewEnabled(false);
    if (tracking.cameraConsentGranted() && !tracking.running()) {
        (void)tracking.start();
    }
#endif
}

void HandModelOverlay::onEnter() {
    ensureReady();
    m_left.reset();
    m_right.reset();
}

void HandModelOverlay::onExit() noexcept {
    m_hands.clearResources();
    m_ready = false;
    m_left.reset();
    m_right.reset();
}

void HandModelOverlay::update(
    const f32 dt,
    const bool manualLeft,
    const bool manualRight) noexcept
{
    const auto& handPose = ::biofuel::engine::runtime::Runtime::handPose();
    const bool calibrated = handPose.calibrationValid();
    const auto& leftPose = handPose.leftPose();
    const auto& rightPose = handPose.rightPose();

    m_left.update(calibrated && leftPose.valid, leftPose.confidence, manualLeft, dt);
    m_right.update(calibrated && rightPose.valid, rightPose.confidence, manualRight, dt);
}

void HandModelOverlay::render() {
    const auto& handPose = ::biofuel::engine::runtime::Runtime::handPose();
    if (!handPose.calibrationValid()) {
        return;
    }
    if (!m_left.visible() && !m_right.visible()) {
        return;
    }

    ensureReady();
    BeginMode3D(overlayCamera());
    renderPose(handPose.leftPose(), m_left);
    renderPose(handPose.rightPose(), m_right);
    EndMode3D();
}

void HandModelOverlay::ensureReady() {
    if (m_ready) {
        return;
    }
    m_preset = m_hands.presets().load<DefaultRobotHandPreset>();
    m_hands.applyPreset(m_preset);
    m_ready = true;
}

void HandModelOverlay::renderPose(
    const TrackedPose& pose,
    const HandPresenceAnimator& animator)
{
    if (!pose.valid || !animator.visible()) {
        return;
    }

    RobotHandRenderOptions options{};
    options.showTargets = false;
    options.materials = m_preset.materials;
    options.materials.opacity = animator.alpha();
    m_hands.renderTracked(pose, options);
}

Camera3D HandModelOverlay::overlayCamera() const noexcept {
    return Camera3D{
        .position = Vector3{0.0f, 0.16f, 1.95f},
        .target = Vector3{0.0f, 0.08f, 0.0f},
        .up = Vector3{0.0f, 1.0f, 0.0f},
        .fovy = 34.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
}

} // namespace biofuel::game::presentation::hands
