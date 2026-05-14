#include "CalibrationScreen.hpp"

#include "engine/custom/procedural/pose/TrackedPoseMapping.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "game/presentation/hands/CalibrationFlowState.hpp"
#include <algorithm>
#include <cstdio>
#include <string_view>

namespace biofuel::game::screens {

namespace {
using ::biofuel::engine::graphics::Renderer;
namespace pose = ::biofuel::engine::custom::procedural::pose;

constexpr f32 CAMERA_START_TIMEOUT_SECONDS = 12.0f;

[[nodiscard]] bool confirmInputPressed() noexcept {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

[[nodiscard]] f32 clamp01(const f32 value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] f32 easeOutCubic(const f32 t) noexcept {
    const f32 inv = 1.0f - clamp01(t);
    return 1.0f - inv * inv * inv;
}

[[nodiscard]] Rectangle lerpRect(const Rectangle from, const Rectangle to, const f32 t) noexcept {
    return Rectangle{
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.width + (to.width - from.width) * t,
        from.height + (to.height - from.height) * t,
    };
}

[[nodiscard]] Rectangle fullScreenRect() noexcept {
    return Rectangle{0.0f, 0.0f, static_cast<f32>(Renderer::screenWidth()), static_cast<f32>(Renderer::screenHeight())};
}

[[nodiscard]] Rectangle introCardRect() noexcept {
    const f32 sw = static_cast<f32>(Renderer::screenWidth());
    const f32 sh = static_cast<f32>(Renderer::screenHeight());
    const f32 width = std::min(sw * 0.62f, 720.0f);
    const f32 height = width * 0.5625f;
    return Rectangle{(sw - width) * 0.5f, (sh - height) * 0.5f, width, height};
}

void drawCenteredText(const std::string_view text, const i32 y, const i32 size, const Color color) {
    const i32 width = Renderer::measureText(text, size);
    Renderer::drawText(text, (Renderer::screenWidth() - width) / 2, y, size, color);
}

} // namespace

void CalibrationScreen::onEnter() {
    m_phaseTime = 0.0f;
    m_overlayTime = 0.0f;
    m_startedCalibration = false;
    m_popQueued = false;

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    if (!tracking.featureEnabled()) {
        m_phase = Phase::FeatureDisabled;
        return;
    }
    if (!tracking.cameraConsentGranted()) {
        tracking.requestCameraAccess();
        m_phase = Phase::WaitingForConsent;
        return;
    }
    startTracking();
#else
    m_phase = Phase::FeatureDisabled;
#endif
}

void CalibrationScreen::onExit() {
    m_preview.release();
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    if (::biofuel::engine::runtime::Runtime::handTracking().previewEnabled()) {
        ::biofuel::engine::runtime::Runtime::handTracking().setPreviewEnabled(false);
    }
#endif
}

void CalibrationScreen::onUpdate(const f32 dt) {
    m_phaseTime += dt;
    m_overlayTime += dt;

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    if (m_phase == Phase::Starting || m_phase == Phase::Calibrating) {
        auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
        m_preview.update(tracking);
        if (m_phase == Phase::Starting && !tracking.running() && m_phaseTime > CAMERA_START_TIMEOUT_SECONDS) {
            tracking.setPreviewEnabled(false);
            game::presentation::hands::CalibrationFlowState::instance().fail();
            m_phase = Phase::Failed;
            m_phaseTime = 0.0f;
            return;
        }
        if (!m_startedCalibration && tracking.running()) {
            ::biofuel::engine::runtime::Runtime::handPose().startCalibration();
            m_startedCalibration = true;
            m_phase = Phase::Calibrating;
            m_phaseTime = 0.0f;
        }
        if (m_startedCalibration && ::biofuel::engine::runtime::Runtime::handPose().calibrationValid()) {
            tracking.setPreviewEnabled(false);
            m_phase = Phase::SuccessHold;
            m_phaseTime = 0.0f;
        }
    }
#endif

    if (m_phase == Phase::FeatureDisabled && m_phaseTime > 0.65f) {
        completeAndPop();
    }
    if (m_phase == Phase::SuccessHold && m_phaseTime > 0.45f) {
        m_phase = Phase::Outro;
        m_phaseTime = 0.0f;
    }
    if ((m_phase == Phase::Outro || m_phase == Phase::Cancelled || m_phase == Phase::Failed) && m_phaseTime > 0.42f) {
        if (m_phase == Phase::Outro) {
            completeAndPop();
        } else if (m_phase == Phase::Failed) {
            failAndPop();
        } else {
            cancelAndPop();
        }
    }
}

void CalibrationScreen::onRender() {
    const Rectangle full = fullScreenRect();
    const f32 progress = overlayProgress();
    const Rectangle cameraBounds = lerpRect(introCardRect(), full, progress);
    const Rectangle contentBounds = cameraContentRect(cameraBounds);

    DrawRectangleRec(full, Color{0, 0, 0, static_cast<unsigned char>(90.0f + progress * 80.0f)});
    drawCameraPreview(cameraBounds);
    drawCalibrationGuide(contentBounds);
    drawStatusCard(cameraBounds);
}

void CalibrationScreen::onInput() {
    if (m_popQueued) {
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        m_phase = Phase::Cancelled;
        m_phaseTime = 0.0f;
        game::presentation::hands::CalibrationFlowState::instance().cancel();
        return;
    }

    if (m_phase == Phase::WaitingForConsent && confirmInputPressed()) {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
        ::biofuel::engine::runtime::Runtime::handTracking().approveCameraAccess();
        startTracking();
#endif
        return;
    }

    if (m_phase == Phase::FeatureDisabled && confirmInputPressed()) {
        completeAndPop();
    }
}

void CalibrationScreen::startTracking() {
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    tracking.setPreviewEnabled(true);
    const bool started = tracking.start();
    if (!started) {
        tracking.setPreviewEnabled(false);
        game::presentation::hands::CalibrationFlowState::instance().fail();
        m_phase = Phase::Failed;
        m_phaseTime = 0.0f;
        return;
    }
    m_phase = Phase::Starting;
    m_phaseTime = 0.0f;
#endif
}

void CalibrationScreen::completeAndPop() {
    if (m_popQueued) {
        return;
    }
    game::presentation::hands::CalibrationFlowState::instance().complete();
    m_popQueued = true;
    if (auto* sm = manager()) {
        sm->queuePop();
    }
}

void CalibrationScreen::cancelAndPop() {
    if (m_popQueued) {
        return;
    }
    game::presentation::hands::CalibrationFlowState::instance().cancel();
    m_popQueued = true;
    if (auto* sm = manager()) {
        sm->queuePop();
    }
}

void CalibrationScreen::failAndPop() {
    if (m_popQueued) {
        return;
    }
    game::presentation::hands::CalibrationFlowState::instance().fail();
    m_popQueued = true;
    if (auto* sm = manager()) {
        sm->queuePop();
    }
}

Rectangle CalibrationScreen::cameraContentRect(const Rectangle bounds) const noexcept {
    const Texture2D texture = m_preview.texture();
    const f32 sourceWidth = texture.id != 0U ? static_cast<f32>(texture.width) : 16.0f;
    const f32 sourceHeight = texture.id != 0U ? static_cast<f32>(texture.height) : 9.0f;
    if (sourceWidth <= 0.0f || sourceHeight <= 0.0f || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return bounds;
    }

    const f32 sourceAspect = sourceWidth / sourceHeight;
    const f32 boundsAspect = bounds.width / bounds.height;
    f32 width = bounds.width;
    f32 height = bounds.height;
    if (boundsAspect > sourceAspect) {
        width = bounds.height * sourceAspect;
    } else {
        height = bounds.width / sourceAspect;
    }

    return Rectangle{
        bounds.x + (bounds.width - width) * 0.5f,
        bounds.y + (bounds.height - height) * 0.5f,
        width,
        height,
    };
}

void CalibrationScreen::drawCameraPreview(const Rectangle bounds) const {
    DrawRectangleRec(bounds, Color{4, 10, 13, 245});
    DrawRectangleLinesEx(bounds, 2.0f, Color{84, 236, 148, 210});

    const Texture2D texture = m_preview.texture();
    if (texture.id != 0U) {
        const Rectangle content = cameraContentRect(bounds);
        const Rectangle src{
            static_cast<f32>(texture.width),
            0.0f,
            -static_cast<f32>(texture.width),
            static_cast<f32>(texture.height)};
        DrawTexturePro(texture, src, content, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
        DrawRectangleLinesEx(content, 1.0f, Color{238, 246, 240, 95});
        DrawRectangleRec(content, Color{0, 0, 0, 35});
        return;
    }

    const i32 y = static_cast<i32>(bounds.y + bounds.height * 0.5f - 22.0f);
    if (m_phase == Phase::FeatureDisabled) {
        drawCenteredText("Hand tracking is disabled in this build", y, 22, Color{238, 232, 184, 255});
        drawCenteredText("Continuing without calibration...", y + 32, 16, Color{190, 205, 200, 255});
    } else if (m_phase == Phase::Failed) {
        drawCenteredText("Hand calibration could not start", y, 22, Color{255, 203, 84, 255});
        drawCenteredText("Returning to the main menu...", y + 32, 16, Color{190, 205, 200, 255});
    } else if (m_phase == Phase::WaitingForConsent) {
        drawCenteredText("Camera access is needed to calibrate your hands", y, 22, Color{238, 246, 240, 255});
        drawCenteredText("Press ENTER or click to allow camera access", y + 32, 16, Color{84, 236, 148, 255});
    } else {
        drawCenteredText("Camera preview warming up...", y, 22, Color{238, 246, 240, 255});
        drawCenteredText("Show both hands clearly in the camera", y + 32, 16, Color{190, 205, 200, 255});
    }
}

void CalibrationScreen::drawCalibrationGuide(const Rectangle bounds) const {
    if (m_phase != Phase::Calibrating) {
        return;
    }

    const auto wizard = ::biofuel::engine::runtime::Runtime::handPose().calibrationState();
    if (!wizard.active) {
        return;
    }

    const Vector2 target = pose::calibrationTarget(wizard.step);
    const Vector2 targetPoint{
        bounds.x + target.x * bounds.width,
        bounds.y + target.y * bounds.height,
    };
    const f32 radius = pose::calibrationTargetRadius(wizard.step) * std::min(bounds.width, bounds.height);
    const Color color = wizard.targetAcquired ? Color{84, 236, 148, 245} : Color{255, 203, 84, 235};
    DrawCircleLinesV(targetPoint, radius, color);
    DrawCircleLinesV(targetPoint, radius + 12.0f, Color{color.r, color.g, color.b, 120});
    DrawCircleV(targetPoint, 5.0f, color);
}

void CalibrationScreen::drawStatusCard(const Rectangle bounds) const {
    const Rectangle card{
        bounds.x + 32.0f,
        bounds.y + bounds.height - 116.0f,
        bounds.width - 64.0f,
        82.0f,
    };
    if (card.width < 280.0f || card.y < bounds.y) {
        return;
    }

    DrawRectangleRec(card, Color{3, 8, 10, 220});
    DrawRectangleLinesEx(card, 1.0f, Color{84, 236, 148, 165});

    std::string_view prompt = "Preparing hand calibration";
    std::string_view secondary = "Keep your hands visible inside the camera frame";
    f32 progress = 0.0f;

    if (m_phase == Phase::FeatureDisabled) {
        prompt = "Hand tracking unavailable";
        secondary = "The game will continue without calibrated hand models";
        progress = 1.0f;
    } else if (m_phase == Phase::WaitingForConsent) {
        prompt = "Camera permission";
        secondary = "Press ENTER or click to continue";
    } else if (m_phase == Phase::Starting) {
        prompt = "Starting camera";
        secondary = "Waiting for the first tracking frame";
    } else if (m_phase == Phase::Calibrating) {
        const auto wizard = ::biofuel::engine::runtime::Runtime::handPose().calibrationState();
        prompt = pose::calibrationPrompt(wizard.activeHand, wizard.step);
        secondary = pose::calibrationCaptureStatusName(
            wizard.activeHand == pose::CalibrationHandPhase::Right ? wizard.right.status : wizard.left.status);
        const auto handProgress = wizard.activeHand == pose::CalibrationHandPhase::Right ? wizard.right : wizard.left;
        progress = handProgress.requiredHoldSeconds > 0.0f
            ? clamp01(handProgress.holdSeconds / handProgress.requiredHoldSeconds)
            : 0.0f;
    } else if (m_phase == Phase::SuccessHold || m_phase == Phase::Outro) {
        prompt = "Hands calibrated";
        secondary = "Returning to the main menu";
        progress = 1.0f;
    } else if (m_phase == Phase::Cancelled) {
        prompt = "Calibration cancelled";
        secondary = "Returning to the main menu";
    } else if (m_phase == Phase::Failed) {
        prompt = "Calibration failed";
        secondary = "Camera tracking did not become ready in time";
    }

    Renderer::drawText(prompt, static_cast<i32>(card.x + 14.0f), static_cast<i32>(card.y + 12.0f), 18, Color{233, 246, 238, 255});
    Renderer::drawText(secondary, static_cast<i32>(card.x + 14.0f), static_cast<i32>(card.y + 38.0f), 13, Color{177, 212, 198, 255});
    DrawRectangleRec(Rectangle{card.x + 14.0f, card.y + 62.0f, card.width - 28.0f, 8.0f}, Color{18, 28, 30, 255});
    DrawRectangleRec(Rectangle{card.x + 14.0f, card.y + 62.0f, (card.width - 28.0f) * progress, 8.0f}, Color{84, 236, 148, 255});
}

f32 CalibrationScreen::overlayProgress() const noexcept {
    if (m_phase == Phase::Outro || m_phase == Phase::Cancelled || m_phase == Phase::Failed) {
        return 1.0f - easeOutCubic(m_phaseTime / 0.42f);
    }
    return easeOutCubic(m_overlayTime / 0.55f);
}

} // namespace biofuel::game::screens
