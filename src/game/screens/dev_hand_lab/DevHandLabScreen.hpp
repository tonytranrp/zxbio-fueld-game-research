#pragma once

#ifdef BIOFUEL_ENABLE_DEV_SCREENS

#include "engine/ui/Screen.hpp"
#include "engine/custom/procedural/hand/RobotHandModule.hpp"
#include "game/screens/dev_hand_lab/HandLabTypes.hpp"
#include <raylib.h>
#include <string_view>

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
#include "engine/custom/procedural/hand/HandTrackingRetarget.hpp"
#include "engine/vision/hand_tracking/HandTrackingTypes.hpp"
#endif

namespace biofuel::game::screens {

class DevHandLabScreen final : public ::biofuel::engine::ui::Screen {
public:
    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override {
        return ::biofuel::engine::ui::typed::ScreenId::DevHandLab;
    }
    [[nodiscard]] std::string_view getName() const noexcept override { return "DevHandLabScreen"; }

private:
    using HandEngine = ::biofuel::engine::custom::procedural::hand::RobotHandModule<::biofuel::engine::custom::procedural::hand::BiofuelRobotHands>;
    using LeftHand = HandEngine::LeftHand;
    using RightHand = HandEngine::RightHand;
    using HandSide = ::biofuel::engine::custom::procedural::hand::HandSide;
    using TrackedHandPose = ::biofuel::engine::custom::procedural::hand::TrackedRobotHandPose;

    Camera3D m_camera{};
    dev_hand_lab::HandLabCameraState m_cameraState{};
    dev_hand_lab::HandLabWristPose m_wrist{};
    HandEngine m_handEngine{};
    ::biofuel::engine::custom::procedural::hand::RobotHandPreset m_preset{};
    LeftHand m_leftHand;
    RightHand m_rightHand;
    Vector2 m_lastMouse{0.0f, 0.0f};
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    Texture2D m_previewTexture{};
    u64 m_previewTextureSequence = 0U;
    f32 m_previewUploadCooldown = 0.0f;
    TrackedHandPose m_trackedLeft{};
    TrackedHandPose m_trackedRight{};
    ::biofuel::engine::custom::procedural::hand::MappedTrackedHands m_trackingMapped{};
    ::biofuel::engine::custom::procedural::hand::HandTrackingRetargeter m_handRetargeter{};
#endif

    void applyCamera() noexcept;
    void updateCameraInput() noexcept;
    void updateHandTracking(f32 dt) noexcept;
    void resetHands() noexcept;
    void applyWristPose() noexcept;
    void reloadPreset() noexcept;
    void drawStudio() const noexcept;
    void drawFloorGrid() const noexcept;
    void drawStatusHud() const noexcept;
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    void startHandTrackingWithPreview() noexcept;
    void drawLiveTrackingOverlay() const noexcept;
    void drawHandLandmarkOverlay(Rectangle previewBounds, const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame) const noexcept;
    void drawCalibrationGuide(Rectangle previewBounds) const noexcept;
    void applyTrackedFrame(const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame, f32 dt) noexcept;
    void resetTrackingCalibration() noexcept;
    void updatePreviewTexture(f32 dt) noexcept;
    void unloadPreviewTexture() noexcept;
#endif
};

} // namespace biofuel::game::screens

#endif // BIOFUEL_ENABLE_DEV_SCREENS
