#pragma once

#ifdef BIOFUEL_ENABLE_DEV_SCREENS

#include "engine/ui/Screen.hpp"
#include "engine/custom/procedural/hand/HandPhysicsInteraction.hpp"
#include "engine/custom/procedural/hand/RobotHandModule.hpp"
#include "game/screens/dev_hand_lab/HandLabTypes.hpp"
#include "game/presentation/hands/HandPreviewTexture.hpp"
#include <limits>
#include <raylib.h>
#include <string_view>

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
#include "engine/custom/procedural/hand/HandTrackingRetarget.hpp"
#include "engine/vision/hand_tracking/HandTrackingTypes.hpp"
#endif

namespace biofuel::game::screens {

class DevHandLabScreen final : public ::biofuel::engine::ui::Screen {
public:
    ~DevHandLabScreen() override;

    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    void buildLoadingTasks(::biofuel::LoadingTaskQueue& tasks) override;

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
    ::biofuel::engine::custom::procedural::hand::HandPhysicsInteraction3D m_handPhysics{};
    ::biofuel::engine::custom::procedural::hand::RobotHandPreset m_preset{};
    LeftHand m_leftHand;
    RightHand m_rightHand;
    Vector2 m_lastMouse{0.0f, 0.0f};
    bool m_loadingComplete = false;
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    ::biofuel::game::presentation::hands::HandPreviewTexture m_previewTexture;
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
    void updateHandPhysics(f32 dt) noexcept;
    void drawStudio() const noexcept;
    void drawFloorGrid() const noexcept;
    void drawPhysicsProps() const noexcept;
    void drawStatusHud() const noexcept;
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    void startHandTrackingWithPreview() noexcept;
    void drawLiveTrackingOverlay() const noexcept;
    void drawHandLandmarkOverlay(Rectangle previewBounds, const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame) const noexcept;
    void drawCalibrationGuide(Rectangle previewBounds) const noexcept;
    void applyTrackedFrame(const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame, f32 dt) noexcept;
    void resetTrackingCalibration() noexcept;
    void updatePreviewTexture() noexcept;
#endif
};

} // namespace biofuel::game::screens

#endif // BIOFUEL_ENABLE_DEV_SCREENS
