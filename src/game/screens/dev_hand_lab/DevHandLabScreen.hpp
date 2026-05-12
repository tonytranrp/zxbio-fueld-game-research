#pragma once

#ifdef BIOFUEL_ENABLE_DEV_SCREENS

#include "engine/ui/Screen.hpp"
#include "engine/custom/procedural/hand/RobotHandModule.hpp"
#include "game/screens/dev_hand_lab/HandLabTypes.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::game::screens {

class DevHandLabScreen final : public ::biofuel::engine::ui::Screen {
public:
    void onEnter() override;
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
    using FingerId = ::biofuel::engine::custom::procedural::hand::FingerId;
    using HandSide = ::biofuel::engine::custom::procedural::hand::HandSide;

    static constexpr i32 PANEL_WIDTH = 348;

    Camera3D m_camera{};
    dev_hand_lab::HandLabCameraState m_cameraState{};
    dev_hand_lab::HandLabSelection m_selection{};
    dev_hand_lab::HandLabWristPose m_wrist{};
    dev_hand_lab::HandLabMaterialState m_materials{};
    dev_hand_lab::HandLabDebugState m_debug{};
    HandEngine m_handEngine{};
    ::biofuel::engine::custom::procedural::hand::RobotHandPreset m_preset{};
    LeftHand m_leftHand;
    RightHand m_rightHand;
    dev_hand_lab::HandLabTab m_activeTab = dev_hand_lab::HandLabTab::Pose;
    dev_hand_lab::SliderId m_activeSlider = dev_hand_lab::SliderId::None;
    i32 m_dragFinger = -1;
    Vector2 m_lastMouse{0.0f, 0.0f};
    f32 m_curl = 0.18f;
    f32 m_spread = 0.0f;
    f32 m_tolerance = 0.002f;
    i32 m_iterations = 16;

    void applyCamera() noexcept;
    void updateCameraInput() noexcept;
    void solveHands() noexcept;
    void resetHands() noexcept;
    void applyWristPose() noexcept;
    void applyAnimationPose() noexcept;
    void mirrorTargets(HandSide source) noexcept;
    void reloadPreset() noexcept;
    void exportPreset() const noexcept;
    void updateTargetDrag() noexcept;
    void updateSliders() noexcept;
    void updateButtons() noexcept;
    void updateTabHotkeys() noexcept;
    void drawStudio() const noexcept;
    void drawFloorGrid() const noexcept;
    void drawAxes() const noexcept;
    void drawPanel() const noexcept;
    void drawTabs() const noexcept;
    void drawPoseTab(i32 y) const noexcept;
    void drawAnimationTab(i32 y) const noexcept;
    void drawIkTab(i32 y) const noexcept;
    void drawMaterialsTab(i32 y) const noexcept;
    void drawDebugTab(i32 y) const noexcept;
    void drawSlider(i32 y, std::string_view label, f32 value, f32 minValue, f32 maxValue, dev_hand_lab::SliderId slider) const noexcept;
    void drawButton(i32 y, std::string_view label, bool active = false) const noexcept;
    void drawButtonAt(i32 x, i32 y, i32 width, std::string_view label, bool active = false) const noexcept;
    void drawStatusLine(i32 y) const noexcept;

    [[nodiscard]] Vector3 selectedTarget() const noexcept;
    void moveSelectedTarget(Vector3 delta) noexcept;
    [[nodiscard]] i32 pickFingerTarget(Vector2 mouse, HandSide& side) const noexcept;
    [[nodiscard]] static std::string_view fingerName(FingerId finger) noexcept;
    [[nodiscard]] static std::string_view handName(HandSide hand) noexcept;
};

} // namespace biofuel::game::screens

#endif // BIOFUEL_ENABLE_DEV_SCREENS
