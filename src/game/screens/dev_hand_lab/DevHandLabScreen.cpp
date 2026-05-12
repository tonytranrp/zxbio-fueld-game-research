#include "DevHandLabScreen.hpp"

#ifdef BIOFUEL_ENABLE_DEV_SCREENS

#include "engine/custom/procedural/animation/HandAnimation.hpp"
#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/ik/IkTypes.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "game/screens/main_menu/MainMenuScreen.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <raymath.h>

namespace biofuel::game::screens {

namespace {

using ::biofuel::engine::graphics::Renderer;
using ::biofuel::engine::custom::procedural::hand::DefaultRobotHandPreset;
using ::biofuel::engine::custom::procedural::hand::FingerDebugState;
using ::biofuel::engine::custom::procedural::hand::HandSide;
using ::biofuel::engine::custom::procedural::hand::RobotHandRenderOptions;
using dev_hand_lab::HandLabTab;
using dev_hand_lab::SliderId;
using dev_hand_lab::TargetVisibility;

constexpr i32 PANEL_PAD = 18;
constexpr i32 CONTROL_X = 24;
constexpr i32 CONTROL_W = 300;
constexpr i32 SLIDER_H = 10;
constexpr i32 BUTTON_H = 26;
constexpr i32 TAB_Y = 104;
constexpr i32 CONTENT_Y = 162;
constexpr i32 POSE_CURL_Y = 226;
constexpr i32 POSE_SPREAD_Y = 268;
constexpr i32 POSE_WRIST_X_Y = 310;
constexpr i32 POSE_WRIST_Y_Y = 352;
constexpr i32 POSE_WRIST_Z_Y = 394;
constexpr i32 POSE_PITCH_Y = 436;
constexpr i32 POSE_YAW_Y = 478;
constexpr i32 POSE_ROLL_Y = 520;
constexpr i32 POSE_MIRROR_LEFT_Y = 562;
constexpr i32 POSE_MIRROR_RIGHT_Y = 592;
constexpr i32 POSE_RESET_Y = 622;
constexpr i32 ANIM_PLAY_Y = 222;
constexpr i32 ANIM_NEXT_Y = 252;
constexpr i32 ANIM_SPEED_Y = 306;
constexpr i32 ANIM_SCRUB_Y = 350;
constexpr i32 ANIM_LOOP_Y = 394;
constexpr i32 ANIM_MIRROR_Y = 424;
constexpr i32 IK_ITERATIONS_Y = 224;
constexpr i32 IK_TOLERANCE_Y = 268;
constexpr i32 IK_LIMITS_Y = 312;
constexpr i32 MATERIAL_SHELL_Y = 224;
constexpr i32 MATERIAL_ACCENT_Y = 268;
constexpr i32 MATERIAL_JOINT_Y = 312;
constexpr i32 MATERIAL_RESET_Y = 356;
constexpr i32 MATERIAL_RELOAD_Y = 386;
constexpr i32 MATERIAL_EXPORT_Y = 416;
constexpr i32 DEBUG_BONES_Y = 154;
constexpr i32 DEBUG_AXES_Y = 184;
constexpr i32 DEBUG_JOINTS_Y = 214;
constexpr i32 DEBUG_TARGETS_Y = 244;
constexpr i32 DEBUG_CAMERA_FRONT_Y = 314;
constexpr i32 DEBUG_CAMERA_SIDE_Y = 344;
constexpr i32 DEBUG_CAMERA_TOP_Y = 374;
constexpr i32 DEBUG_CAMERA_RESET_Y = 404;

[[nodiscard]] Rectangle sliderBounds(const i32 y) noexcept {
    return Rectangle{static_cast<f32>(CONTROL_X), static_cast<f32>(y), static_cast<f32>(CONTROL_W), static_cast<f32>(SLIDER_H)};
}

[[nodiscard]] Rectangle buttonBounds(const i32 y) noexcept {
    return Rectangle{static_cast<f32>(CONTROL_X), static_cast<f32>(y), static_cast<f32>(CONTROL_W), static_cast<f32>(BUTTON_H)};
}

[[nodiscard]] Rectangle buttonBounds(const i32 x, const i32 y, const i32 width) noexcept {
    return Rectangle{static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(width), static_cast<f32>(BUTTON_H)};
}

[[nodiscard]] bool mouseIn(const Rectangle bounds) noexcept {
    return CheckCollisionPointRec(GetMousePosition(), bounds);
}

[[nodiscard]] f32 sliderValue(const i32 y, const f32 minValue, const f32 maxValue) noexcept {
    const Rectangle bounds = sliderBounds(y);
    const f32 t = std::clamp((GetMousePosition().x - bounds.x) / bounds.width, 0.0f, 1.0f);
    return minValue + (maxValue - minValue) * t;
}

[[nodiscard]] i32 sliderIntValue(const i32 y, const i32 minValue, const i32 maxValue) noexcept {
    return std::clamp(static_cast<i32>(sliderValue(y, static_cast<f32>(minValue), static_cast<f32>(maxValue)) + 0.5f), minValue, maxValue);
}

[[nodiscard]] f32 smoothStep(const f32 value) noexcept {
    const f32 t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] Vector3 add(const Vector3 a, const Vector3 b) noexcept {
    return Vector3Add(a, b);
}

} // namespace

void DevHandLabScreen::onEnter() {
    setTransitionDuration(0.08f);
    reloadPreset();
    m_cameraState = {};
    resetHands();
    applyCamera();
}

void DevHandLabScreen::onUpdate(const f32 dt) {
    m_handEngine.animation().update(dt);
    if (m_handEngine.animation().playing()) {
        applyAnimationPose();
    }
    solveHands();
    applyCamera();
}

void DevHandLabScreen::onInput() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (auto* sm = manager()) {
            sm->queueReplace<MainMenuScreen>();
        }
        return;
    }

    updateTabHotkeys();

    if (IsKeyPressed(KEY_TAB)) {
        m_selection.hand = m_selection.hand == HandSide::Left ? HandSide::Right : HandSide::Left;
    }

    if (IsKeyPressed(KEY_ONE)) { m_selection.finger = FingerId::Thumb; }
    if (IsKeyPressed(KEY_TWO)) { m_selection.finger = FingerId::Index; }
    if (IsKeyPressed(KEY_THREE)) { m_selection.finger = FingerId::Middle; }
    if (IsKeyPressed(KEY_FOUR)) { m_selection.finger = FingerId::Ring; }
    if (IsKeyPressed(KEY_FIVE)) { m_selection.finger = FingerId::Pinky; }
    if (IsKeyPressed(KEY_SPACE)) { m_handEngine.animation().togglePlaying(); }

    updateButtons();
    updateSliders();
    updateTargetDrag();
    updateCameraInput();
}

void DevHandLabScreen::onRender() {
    drawStudio();

    BeginMode3D(m_camera);
    if (m_debug.showAxes) {
        drawAxes();
    }
    drawFloorGrid();

    const bool showTargets = m_debug.targets != TargetVisibility::Hidden;
    const bool selectedOnly = m_debug.targets == TargetVisibility::Selected;
    const RobotHandRenderOptions leftOptions{
        .showBones = m_debug.showBones,
        .showTargets = showTargets && (!selectedOnly || m_selection.hand == HandSide::Left),
        .selectedTargetOnly = selectedOnly,
        .selectedFinger = m_selection.finger,
        .materials = ::biofuel::engine::custom::procedural::hand::RobotHandMaterialState{
            .shellIntensity = m_materials.shellIntensity,
            .accentIntensity = m_materials.accentIntensity,
            .jointIntensity = m_materials.jointIntensity,
        },
    };
    const RobotHandRenderOptions rightOptions{
        .showBones = m_debug.showBones,
        .showTargets = showTargets && (!selectedOnly || m_selection.hand == HandSide::Right),
        .selectedTargetOnly = selectedOnly,
        .selectedFinger = m_selection.finger,
        .materials = ::biofuel::engine::custom::procedural::hand::RobotHandMaterialState{
            .shellIntensity = m_materials.shellIntensity,
            .accentIntensity = m_materials.accentIntensity,
            .jointIntensity = m_materials.jointIntensity,
        },
    };
    m_handEngine.render(m_leftHand, leftOptions);
    m_handEngine.render(m_rightHand, rightOptions);
    EndMode3D();

    drawPanel();
}

void DevHandLabScreen::applyCamera() noexcept {
    const f32 cp = std::cos(m_cameraState.pitch);
    const Vector3 offset{
        std::sin(m_cameraState.yaw) * cp * m_cameraState.distance,
        std::sin(m_cameraState.pitch) * m_cameraState.distance,
        std::cos(m_cameraState.yaw) * cp * m_cameraState.distance,
    };
    m_camera = Camera3D{
        .position = add(m_cameraState.target, offset),
        .target = m_cameraState.target,
        .up = Vector3{0.0f, 1.0f, 0.0f},
        .fovy = 34.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
}

void DevHandLabScreen::updateCameraInput() noexcept {
    const Vector2 mouse = GetMousePosition();
    if (mouse.x < static_cast<f32>(PANEL_WIDTH) || m_dragFinger >= 0) {
        m_lastMouse = mouse;
        return;
    }

    const Vector2 delta{mouse.x - m_lastMouse.x, mouse.y - m_lastMouse.y};
    const bool pan = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)
        || (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)));
    if (pan) {
        m_cameraState.target.x -= delta.x * 0.0025f;
        m_cameraState.target.y += delta.y * 0.0025f;
    } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        m_cameraState.yaw -= delta.x * 0.008f;
        m_cameraState.pitch = std::clamp(m_cameraState.pitch + delta.y * 0.006f, -0.85f, 0.95f);
    }

    const f32 wheel = GetMouseWheelMove();
    if (wheel != 0.0f && m_dragFinger < 0) {
        m_cameraState.distance = std::clamp(m_cameraState.distance - wheel * 0.12f, 0.75f, 4.0f);
    }
    m_lastMouse = mouse;
}

void DevHandLabScreen::solveHands() noexcept {
    const ::biofuel::engine::custom::procedural::ik::IkSolveSettings settings{
        .maxIterations = m_iterations,
        .tolerance = m_debug.jointLimits ? m_tolerance : std::max(m_tolerance, 0.010f),
    };
    m_handEngine.solve(m_leftHand, m_rightHand, settings);
}

void DevHandLabScreen::resetHands() noexcept {
    m_curl = 0.10f;
    m_spread = 0.0f;
    m_tolerance = 0.002f;
    m_iterations = 16;
    m_selection = {};
    m_wrist = {};
    m_materials = dev_hand_lab::HandLabMaterialState{
        .shellIntensity = m_preset.materials.shellIntensity,
        .accentIntensity = m_preset.materials.accentIntensity,
        .jointIntensity = m_preset.materials.jointIntensity,
    };
    m_debug = {};
    m_activeTab = HandLabTab::Pose;
    m_handEngine.animation().reset();
    m_handEngine.animation().setSpeed(m_preset.animationSpeed);
    m_handEngine.resetHand(m_leftHand, m_wrist.leftOrigin);
    m_handEngine.resetHand(m_rightHand, m_wrist.rightOrigin);
    applyWristPose();
}

void DevHandLabScreen::applyWristPose() noexcept {
    m_leftHand.setWristPose(m_wrist.leftOrigin, m_wrist.pitch, m_wrist.yaw, m_wrist.roll);
    m_rightHand.setWristPose(m_wrist.rightOrigin, m_wrist.pitch, -m_wrist.yaw, -m_wrist.roll);
    m_leftHand.setCurl(m_curl);
    m_rightHand.setCurl(m_curl);
    m_leftHand.setSpread(m_spread);
    m_rightHand.setSpread(m_spread);
}

void DevHandLabScreen::applyAnimationPose() noexcept {
    const auto leftPose = m_handEngine.animation().sample(HandSide::Left);
    const auto rightPose = m_handEngine.animation().mirror() ? leftPose : m_handEngine.animation().sample(HandSide::Right);
    const f32 eased = smoothStep(m_handEngine.animation().phase());

    m_curl = leftPose.curl;
    m_spread = leftPose.spread;
    m_wrist.leftOrigin = Vector3{-0.42f, -0.18f, 0.0f};
    m_wrist.rightOrigin = Vector3{0.42f, -0.18f, 0.0f};
    m_leftHand.setWristPose(add(m_wrist.leftOrigin, leftPose.wristOffset), 0.0f, -0.18f * eased, 0.12f * std::sin(eased * 6.2831853f));
    m_rightHand.setWristPose(add(m_wrist.rightOrigin, Vector3{-rightPose.wristOffset.x, rightPose.wristOffset.y, rightPose.wristOffset.z}), 0.0f, 0.18f * eased, -0.12f * std::sin(eased * 6.2831853f));
    m_leftHand.setCurl(leftPose.curl);
    m_leftHand.setSpread(leftPose.spread);
    m_rightHand.setCurl(rightPose.curl);
    m_rightHand.setSpread(m_handEngine.animation().mirror() ? -rightPose.spread : rightPose.spread);

    for (u8 raw = 0U; raw < static_cast<u8>(FingerId::Count); ++raw) {
        const auto finger = static_cast<FingerId>(raw);
        const Vector3 leftOffset = leftPose.targetOffsets[static_cast<usize>(finger)];
        const Vector3 sampledRightOffset = rightPose.targetOffsets[static_cast<usize>(finger)];
        const Vector3 rightOffset = m_handEngine.animation().mirror()
            ? Vector3{-leftOffset.x, leftOffset.y, leftOffset.z}
            : sampledRightOffset;
        m_leftHand.moveTarget(finger, leftOffset);
        m_rightHand.moveTarget(finger, rightOffset);
    }
}

void DevHandLabScreen::mirrorTargets(const HandSide source) noexcept {
    for (u8 raw = 0; raw < static_cast<u8>(FingerId::Count); ++raw) {
        const auto finger = static_cast<FingerId>(raw);
        if (source == HandSide::Left) {
            const Vector3 left = m_leftHand.target(finger);
            m_rightHand.setTarget(finger, Vector3{-left.x, left.y, left.z});
        } else {
            const Vector3 right = m_rightHand.target(finger);
            m_leftHand.setTarget(finger, Vector3{-right.x, right.y, right.z});
        }
    }
}

void DevHandLabScreen::reloadPreset() noexcept {
    m_preset = m_handEngine.presets().load<DefaultRobotHandPreset>();
    m_handEngine.applyPreset(m_preset);
}

void DevHandLabScreen::exportPreset() const noexcept {
    auto exportPreset = m_preset;
    exportPreset.name = "dev_hand_lab_export";
    exportPreset.materials = ::biofuel::engine::custom::procedural::hand::RobotHandMaterialState{
        .shellIntensity = m_materials.shellIntensity,
        .accentIntensity = m_materials.accentIntensity,
        .jointIntensity = m_materials.jointIntensity,
    };
    exportPreset.ik = ::biofuel::engine::custom::procedural::ik::IkSolveSettings{
        .maxIterations = m_iterations,
        .tolerance = m_tolerance,
    };
    exportPreset.animationSpeed = m_handEngine.animation().speed();
    static_cast<void>(m_handEngine.presets().exportJson(
        exportPreset,
        std::filesystem::path{"assets/custom/procedural/hand/presets/dev_hand_lab_export.json"}));
}

void DevHandLabScreen::updateTargetDrag() noexcept {
    const Vector2 mouse = GetMousePosition();
    if (mouse.x < static_cast<f32>(PANEL_WIDTH)) {
        m_lastMouse = mouse;
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        HandSide side = HandSide::Left;
        const i32 finger = pickFingerTarget(mouse, side);
        if (finger >= 0) {
            m_handEngine.animation().setPlaying(false);
            m_dragFinger = finger;
            m_selection.hand = side;
            m_selection.finger = static_cast<FingerId>(finger);
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        m_dragFinger = -1;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && m_dragFinger >= 0) {
        const Vector2 delta{mouse.x - m_lastMouse.x, mouse.y - m_lastMouse.y};
        moveSelectedTarget(Vector3{delta.x * 0.0025f, -delta.y * 0.0025f, 0.0f});
    }

    const f32 wheel = GetMouseWheelMove();
    if (wheel != 0.0f && m_dragFinger >= 0) {
        moveSelectedTarget(Vector3{0.0f, 0.0f, wheel * 0.025f});
    }

    m_lastMouse = mouse;
}

void DevHandLabScreen::updateSliders() noexcept {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        m_activeSlider = SliderId::None;
        const auto hit = [](const i32 y) noexcept { return mouseIn(sliderBounds(y)); };
        switch (m_activeTab) {
        case HandLabTab::Pose:
            if (hit(POSE_CURL_Y)) { m_activeSlider = SliderId::Curl; }
            else if (hit(POSE_SPREAD_Y)) { m_activeSlider = SliderId::Spread; }
            else if (hit(POSE_WRIST_X_Y)) { m_activeSlider = SliderId::WristX; }
            else if (hit(POSE_WRIST_Y_Y)) { m_activeSlider = SliderId::WristY; }
            else if (hit(POSE_WRIST_Z_Y)) { m_activeSlider = SliderId::WristZ; }
            else if (hit(POSE_PITCH_Y)) { m_activeSlider = SliderId::WristPitch; }
            else if (hit(POSE_YAW_Y)) { m_activeSlider = SliderId::WristYaw; }
            else if (hit(POSE_ROLL_Y)) { m_activeSlider = SliderId::WristRoll; }
            break;
        case HandLabTab::Animation:
            if (hit(ANIM_SPEED_Y)) { m_activeSlider = SliderId::AnimationSpeed; }
            else if (hit(ANIM_SCRUB_Y)) { m_activeSlider = SliderId::AnimationScrub; }
            break;
        case HandLabTab::IK:
            if (hit(IK_ITERATIONS_Y)) { m_activeSlider = SliderId::Iterations; }
            else if (hit(IK_TOLERANCE_Y)) { m_activeSlider = SliderId::Tolerance; }
            break;
        case HandLabTab::Materials:
            if (hit(MATERIAL_SHELL_Y)) { m_activeSlider = SliderId::ShellIntensity; }
            else if (hit(MATERIAL_ACCENT_Y)) { m_activeSlider = SliderId::AccentIntensity; }
            else if (hit(MATERIAL_JOINT_Y)) { m_activeSlider = SliderId::JointIntensity; }
            break;
        case HandLabTab::Debug:
        case HandLabTab::Count:
            break;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        m_activeSlider = SliderId::None;
    }

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        return;
    }

    switch (m_activeSlider) {
    case SliderId::Curl:
        m_handEngine.animation().setPlaying(false);
        m_curl = sliderValue(POSE_CURL_Y, 0.0f, 1.0f);
        m_handEngine.animation().setScrub(m_handEngine.animation().phase());
        m_leftHand.setCurl(m_curl);
        m_rightHand.setCurl(m_curl);
        break;
    case SliderId::Spread:
        m_handEngine.animation().setPlaying(false);
        m_spread = sliderValue(POSE_SPREAD_Y, -1.0f, 1.0f);
        m_leftHand.setSpread(m_spread);
        m_rightHand.setSpread(m_spread);
        break;
    case SliderId::WristX:
        m_handEngine.animation().setPlaying(false);
        m_wrist.leftOrigin.x = sliderValue(POSE_WRIST_X_Y, -0.70f, -0.10f);
        m_wrist.rightOrigin.x = -m_wrist.leftOrigin.x;
        applyWristPose();
        break;
    case SliderId::WristY:
        m_handEngine.animation().setPlaying(false);
        m_wrist.leftOrigin.y = sliderValue(POSE_WRIST_Y_Y, -0.48f, 0.08f);
        m_wrist.rightOrigin.y = m_wrist.leftOrigin.y;
        applyWristPose();
        break;
    case SliderId::WristZ:
        m_handEngine.animation().setPlaying(false);
        m_wrist.leftOrigin.z = sliderValue(POSE_WRIST_Z_Y, -0.35f, 0.35f);
        m_wrist.rightOrigin.z = m_wrist.leftOrigin.z;
        applyWristPose();
        break;
    case SliderId::WristPitch:
        m_handEngine.animation().setPlaying(false);
        m_wrist.pitch = sliderValue(POSE_PITCH_Y, -0.8f, 0.8f);
        applyWristPose();
        break;
    case SliderId::WristYaw:
        m_handEngine.animation().setPlaying(false);
        m_wrist.yaw = sliderValue(POSE_YAW_Y, -0.8f, 0.8f);
        applyWristPose();
        break;
    case SliderId::WristRoll:
        m_handEngine.animation().setPlaying(false);
        m_wrist.roll = sliderValue(POSE_ROLL_Y, -0.8f, 0.8f);
        applyWristPose();
        break;
    case SliderId::AnimationSpeed:
        m_handEngine.animation().setSpeed(sliderValue(ANIM_SPEED_Y, 0.1f, 3.0f));
        break;
    case SliderId::AnimationScrub:
        m_handEngine.animation().setPlaying(false);
        m_handEngine.animation().setScrub(sliderValue(ANIM_SCRUB_Y, 0.0f, 1.0f));
        applyAnimationPose();
        break;
    case SliderId::Iterations:
        m_iterations = sliderIntValue(IK_ITERATIONS_Y, 4, 40);
        break;
    case SliderId::Tolerance:
        m_tolerance = sliderValue(IK_TOLERANCE_Y, 0.0005f, 0.020f);
        break;
    case SliderId::ShellIntensity:
        m_materials.shellIntensity = sliderValue(MATERIAL_SHELL_Y, 0.55f, 1.35f);
        break;
    case SliderId::AccentIntensity:
        m_materials.accentIntensity = sliderValue(MATERIAL_ACCENT_Y, 0.45f, 1.55f);
        break;
    case SliderId::JointIntensity:
        m_materials.jointIntensity = sliderValue(MATERIAL_JOINT_Y, 0.55f, 1.55f);
        break;
    case SliderId::None:
        break;
    }
}

void DevHandLabScreen::updateButtons() noexcept {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }

    const Vector2 mouse = GetMousePosition();
    if (mouse.x > static_cast<f32>(PANEL_WIDTH)) {
        return;
    }

    const auto setTabIfHit = [this](const HandLabTab tab, const i32 x, const i32 y) {
        if (mouseIn(buttonBounds(x, y, 52))) {
            m_activeTab = tab;
            return true;
        }
        return false;
    };

    if (setTabIfHit(HandLabTab::Pose, 20, TAB_Y) || setTabIfHit(HandLabTab::Animation, 78, TAB_Y) ||
        setTabIfHit(HandLabTab::IK, 136, TAB_Y) || setTabIfHit(HandLabTab::Materials, 194, TAB_Y) ||
        setTabIfHit(HandLabTab::Debug, 252, TAB_Y)) {
        return;
    }

    switch (m_activeTab) {
    case HandLabTab::Pose:
        if (mouseIn(buttonBounds(POSE_MIRROR_LEFT_Y))) { mirrorTargets(HandSide::Left); }
        else if (mouseIn(buttonBounds(POSE_MIRROR_RIGHT_Y))) { mirrorTargets(HandSide::Right); }
        else if (mouseIn(buttonBounds(POSE_RESET_Y))) { resetHands(); }
        break;
    case HandLabTab::Animation:
        if (mouseIn(buttonBounds(ANIM_PLAY_Y))) { m_handEngine.animation().togglePlaying(); }
        else if (mouseIn(buttonBounds(ANIM_NEXT_Y))) {
            m_handEngine.animation().selectNext();
            applyAnimationPose();
        }
        else if (mouseIn(buttonBounds(ANIM_LOOP_Y))) { m_handEngine.animation().toggleLoop(); }
        else if (mouseIn(buttonBounds(ANIM_MIRROR_Y))) {
            m_handEngine.animation().toggleMirror();
            applyAnimationPose();
        }
        break;
    case HandLabTab::IK:
        if (mouseIn(buttonBounds(IK_LIMITS_Y))) { m_debug.jointLimits = !m_debug.jointLimits; }
        break;
    case HandLabTab::Materials:
        if (mouseIn(buttonBounds(MATERIAL_RESET_Y))) { m_materials = {}; }
        else if (mouseIn(buttonBounds(MATERIAL_RELOAD_Y))) { reloadPreset(); resetHands(); }
        else if (mouseIn(buttonBounds(MATERIAL_EXPORT_Y))) { exportPreset(); }
        break;
    case HandLabTab::Debug:
        if (mouseIn(buttonBounds(DEBUG_BONES_Y))) { m_debug.showBones = !m_debug.showBones; }
        else if (mouseIn(buttonBounds(DEBUG_AXES_Y))) { m_debug.showAxes = !m_debug.showAxes; }
        else if (mouseIn(buttonBounds(DEBUG_JOINTS_Y))) { m_debug.showJointNames = !m_debug.showJointNames; }
        else if (mouseIn(buttonBounds(DEBUG_TARGETS_Y))) {
            m_debug.targets = m_debug.targets == TargetVisibility::Hidden
                ? TargetVisibility::All
                : (m_debug.targets == TargetVisibility::All ? TargetVisibility::Selected : TargetVisibility::Hidden);
        }
        else if (mouseIn(buttonBounds(DEBUG_CAMERA_FRONT_Y))) { m_cameraState = dev_hand_lab::HandLabCameraState{}; }
        else if (mouseIn(buttonBounds(DEBUG_CAMERA_SIDE_Y))) {
            m_cameraState = dev_hand_lab::HandLabCameraState{.target = Vector3{0.0f, 0.02f, 0.0f}, .yaw = 1.5707963f, .pitch = 0.12f, .distance = 2.25f};
        }
        else if (mouseIn(buttonBounds(DEBUG_CAMERA_TOP_Y))) {
            m_cameraState = dev_hand_lab::HandLabCameraState{.target = Vector3{0.0f, 0.02f, 0.0f}, .yaw = 0.0f, .pitch = 1.15f, .distance = 2.35f};
        }
        else if (mouseIn(buttonBounds(DEBUG_CAMERA_RESET_Y))) { m_cameraState = dev_hand_lab::HandLabCameraState{}; }
        break;
    case HandLabTab::Count:
        break;
    }
}

void DevHandLabScreen::updateTabHotkeys() noexcept {
    if (IsKeyPressed(KEY_F1)) { m_activeTab = HandLabTab::Pose; }
    if (IsKeyPressed(KEY_F2)) { m_activeTab = HandLabTab::Animation; }
    if (IsKeyPressed(KEY_F3)) { m_activeTab = HandLabTab::IK; }
    if (IsKeyPressed(KEY_F4)) { m_activeTab = HandLabTab::Materials; }
    if (IsKeyPressed(KEY_F5)) { m_activeTab = HandLabTab::Debug; }
}

void DevHandLabScreen::drawStudio() const noexcept {
    DrawRectangleGradientV(0, 0, Renderer::screenWidth(), Renderer::screenHeight(), Color{5, 10, 13, 255}, Color{13, 31, 29, 255});
    DrawCircleGradient(
        Renderer::screenWidth() / 2 + 120,
        Renderer::screenHeight() / 2 - 40,
        static_cast<f32>(Renderer::screenHeight()) * 0.52f,
        Color{30, 112, 72, 70},
        Color{5, 10, 13, 0});
    DrawRectangle(0, Renderer::screenHeight() - 108, Renderer::screenWidth(), 108, Color{2, 6, 8, 125});
}

void DevHandLabScreen::drawFloorGrid() const noexcept {
    constexpr f32 floorY = -0.37f;
    constexpr i32 lineCount = 12;
    constexpr f32 spacing = 0.16f;
    for (i32 index = -lineCount; index <= lineCount; ++index) {
        const f32 p = static_cast<f32>(index) * spacing;
        const Color line = index == 0 ? Color{76, 192, 130, 115} : Color{88, 126, 120, 58};
        DrawLine3D(Vector3{p, floorY, -1.9f}, Vector3{p, floorY, 1.9f}, line);
        DrawLine3D(Vector3{-1.9f, floorY, p}, Vector3{1.9f, floorY, p}, line);
    }
}

void DevHandLabScreen::drawAxes() const noexcept {
    DrawLine3D(Vector3{-1.4f, 0.0f, 0.0f}, Vector3{1.4f, 0.0f, 0.0f}, Color{220, 72, 86, 210});
    DrawLine3D(Vector3{0.0f, -0.8f, 0.0f}, Vector3{0.0f, 0.8f, 0.0f}, Color{86, 220, 130, 210});
    DrawLine3D(Vector3{0.0f, 0.0f, -1.4f}, Vector3{0.0f, 0.0f, 1.4f}, Color{82, 144, 255, 210});
}

void DevHandLabScreen::drawPanel() const noexcept {
    DrawRectangle(0, 0, PANEL_WIDTH, Renderer::screenHeight(), Color{5, 9, 13, 250});
    DrawRectangle(0, 0, PANEL_WIDTH, 136, Color{12, 24, 24, 250});
    DrawLine(PANEL_WIDTH, 0, PANEL_WIDTH, Renderer::screenHeight(), Color{74, 202, 126, 180});

    Renderer::drawText("ROBOT HAND LAB", PANEL_PAD, 22, 18, Color{235, 246, 240, 255});
    Renderer::drawText("ESC returns to Main Menu", PANEL_PAD, 50, 13, Color{141, 162, 160, 255});
    Renderer::drawText("RMB orbit  |  Shift+RMB pan  |  wheel zoom", PANEL_PAD, 72, 12, Color{141, 162, 160, 255});

    drawTabs();
    switch (m_activeTab) {
    case HandLabTab::Pose: drawPoseTab(CONTENT_Y); break;
    case HandLabTab::Animation: drawAnimationTab(CONTENT_Y); break;
    case HandLabTab::IK: drawIkTab(CONTENT_Y); break;
    case HandLabTab::Materials: drawMaterialsTab(CONTENT_Y); break;
    case HandLabTab::Debug: drawDebugTab(CONTENT_Y); break;
    case HandLabTab::Count: break;
    }

    drawStatusLine(Renderer::screenHeight() - 28);
}

void DevHandLabScreen::drawTabs() const noexcept {
    for (usize index = 0; index < dev_hand_lab::DefaultHandLabTabs::values.size(); ++index) {
        const HandLabTab tab = dev_hand_lab::DefaultHandLabTabs::values[index];
        const i32 x = 20 + static_cast<i32>(index) * 58;
        const Rectangle bounds = buttonBounds(x, TAB_Y, 54);
        const bool active = tab == m_activeTab;
        DrawRectangleRec(bounds, active ? Color{44, 133, 78, 255} : Color{22, 32, 38, 255});
        DrawRectangleLinesEx(bounds, 1.0f, active ? Color{126, 242, 164, 235} : Color{68, 88, 96, 220});
        Renderer::drawText(dev_hand_lab::tabName(tab), x + 9, TAB_Y + 7, 12, Color{224, 238, 232, 255});
    }
}

void DevHandLabScreen::drawPoseTab(const i32 y) const noexcept {
    char line[160]{};
    std::snprintf(line, sizeof(line), "Selected: %s %.*s", handName(m_selection.hand).data(), static_cast<int>(fingerName(m_selection.finger).size()), fingerName(m_selection.finger).data());
    Renderer::drawText(line, CONTROL_X, y, 15, Color{112, 235, 150, 255});

    const Vector3 target = selectedTarget();
    std::snprintf(line, sizeof(line), "target { %.2f, %.2f, %.2f }", target.x, target.y, target.z);
    Renderer::drawText(line, CONTROL_X, y + 22, 13, Color{191, 210, 204, 255});
    Renderer::drawText("Tab hand. 1-5 finger. Drag amber target.", CONTROL_X, y + 44, 12, Color{141, 162, 160, 255});

    drawSlider(POSE_CURL_Y, "Curl", m_curl, 0.0f, 1.0f, SliderId::Curl);
    drawSlider(POSE_SPREAD_Y, "Spread", m_spread, -1.0f, 1.0f, SliderId::Spread);
    drawSlider(POSE_WRIST_X_Y, "Wrist X", m_wrist.leftOrigin.x, -0.70f, -0.10f, SliderId::WristX);
    drawSlider(POSE_WRIST_Y_Y, "Wrist Y", m_wrist.leftOrigin.y, -0.48f, 0.08f, SliderId::WristY);
    drawSlider(POSE_WRIST_Z_Y, "Wrist Z", m_wrist.leftOrigin.z, -0.35f, 0.35f, SliderId::WristZ);
    drawSlider(POSE_PITCH_Y, "Pitch", m_wrist.pitch, -0.8f, 0.8f, SliderId::WristPitch);
    drawSlider(POSE_YAW_Y, "Yaw", m_wrist.yaw, -0.8f, 0.8f, SliderId::WristYaw);
    drawSlider(POSE_ROLL_Y, "Roll", m_wrist.roll, -0.8f, 0.8f, SliderId::WristRoll);
    drawButton(POSE_MIRROR_LEFT_Y, "Mirror Left To Right");
    drawButton(POSE_MIRROR_RIGHT_Y, "Mirror Right To Left");
    drawButton(POSE_RESET_Y, "Reset Lab Pose");
}

void DevHandLabScreen::drawAnimationTab(const i32 y) const noexcept {
    char line[160]{};
    std::snprintf(line, sizeof(line), "Clip: %.*s", static_cast<int>(m_handEngine.animation().currentName().size()), m_handEngine.animation().currentName().data());
    Renderer::drawText(line, CONTROL_X, y, 15, Color{112, 235, 150, 255});
    std::snprintf(line, sizeof(line), "Phase %.2f | Speed %.2fx", m_handEngine.animation().phase(), m_handEngine.animation().speed());
    Renderer::drawText(line, CONTROL_X, y + 22, 13, Color{191, 210, 204, 255});
    Renderer::drawText("Demo playback is paused while editing pose controls.", CONTROL_X, y + 44, 12, Color{141, 162, 160, 255});
    drawButton(ANIM_PLAY_Y, m_handEngine.animation().playing() ? "Pause Demo (Space)" : "Play Demo (Space)", m_handEngine.animation().playing());
    drawButton(ANIM_NEXT_Y, "Next Demo Clip");
    drawSlider(ANIM_SPEED_Y, "Speed", m_handEngine.animation().speed(), 0.1f, 3.0f, SliderId::AnimationSpeed);
    drawSlider(ANIM_SCRUB_Y, "Scrub", m_handEngine.animation().phase(), 0.0f, 1.0f, SliderId::AnimationScrub);
    drawButton(ANIM_LOOP_Y, m_handEngine.animation().loop() ? "Loop: ON" : "Loop: OFF", m_handEngine.animation().loop());
    drawButton(ANIM_MIRROR_Y, m_handEngine.animation().mirror() ? "Mirror: ON" : "Mirror: OFF", m_handEngine.animation().mirror());
}

void DevHandLabScreen::drawIkTab(const i32 y) const noexcept {
    const auto debug = m_selection.hand == HandSide::Left ? m_leftHand.debugFingers() : m_rightHand.debugFingers();
    const auto selected = debug[static_cast<usize>(m_selection.finger)];
    char line[160]{};
    std::snprintf(line, sizeof(line), "%s %.4f | %d iter", selected.solve.reached ? "Reached" : "Error", selected.solve.error, selected.solve.iterations);
    Renderer::drawText(line, CONTROL_X, y, 15, selected.solve.reached ? Color{112, 235, 150, 255} : Color{245, 170, 100, 255});
    Renderer::drawText("Tighten tolerance only if the hand is already stable.", CONTROL_X, y + 28, 12, Color{141, 162, 160, 255});
    drawSlider(IK_ITERATIONS_Y, "Iterations", static_cast<f32>(m_iterations), 4.0f, 40.0f, SliderId::Iterations);
    drawSlider(IK_TOLERANCE_Y, "Tolerance", m_tolerance, 0.0005f, 0.020f, SliderId::Tolerance);
    drawButton(IK_LIMITS_Y, m_debug.jointLimits ? "Joint Limits: ON" : "Joint Limits: LOOSE", m_debug.jointLimits);

    i32 rowY = 362;
    for (const auto& finger : debug) {
        std::snprintf(line, sizeof(line), "%.*s  %.4f  %d", static_cast<int>(fingerName(finger.id).size()), fingerName(finger.id).data(), finger.solve.error, finger.solve.iterations);
        Renderer::drawText(line, CONTROL_X, rowY, 13, finger.solve.reached ? Color{174, 236, 190, 255} : Color{246, 174, 124, 255});
        rowY += 20;
    }
}

void DevHandLabScreen::drawMaterialsTab(const i32 y) const noexcept {
    Renderer::drawText("Palette: Biofuel Green Robot", CONTROL_X, y, 15, Color{112, 235, 150, 255});
    Renderer::drawText("Procedural material tint controls.", CONTROL_X, y + 22, 13, Color{141, 162, 160, 255});
    drawSlider(MATERIAL_SHELL_Y, "Shell", m_materials.shellIntensity, 0.55f, 1.35f, SliderId::ShellIntensity);
    drawSlider(MATERIAL_ACCENT_Y, "Green Accent", m_materials.accentIntensity, 0.45f, 1.55f, SliderId::AccentIntensity);
    drawSlider(MATERIAL_JOINT_Y, "Graphite Joint", m_materials.jointIntensity, 0.55f, 1.55f, SliderId::JointIntensity);
    drawButton(MATERIAL_RESET_Y, "Reset Materials");
    drawButton(MATERIAL_RELOAD_Y, "Reload JSON Preset");
    drawButton(MATERIAL_EXPORT_Y, "Export JSON Preset");
}

void DevHandLabScreen::drawDebugTab(const i32 y) const noexcept {
    static_cast<void>(y);
    drawButton(DEBUG_BONES_Y, m_debug.showBones ? "Bones: ON" : "Bones: OFF", m_debug.showBones);
    drawButton(DEBUG_AXES_Y, m_debug.showAxes ? "Axes: ON" : "Axes: OFF", m_debug.showAxes);
    drawButton(DEBUG_JOINTS_Y, m_debug.showJointNames ? "Joint Names: ON" : "Joint Names: OFF", m_debug.showJointNames);
    drawButton(DEBUG_TARGETS_Y, "Cycle Targets", m_debug.targets != TargetVisibility::Hidden);
    drawButton(DEBUG_CAMERA_FRONT_Y, "Camera: Front");
    drawButton(DEBUG_CAMERA_SIDE_Y, "Camera: Side");
    drawButton(DEBUG_CAMERA_TOP_Y, "Camera: Top");
    drawButton(DEBUG_CAMERA_RESET_Y, "Camera: Reset");

    char line[180]{};
    std::snprintf(line, sizeof(line), "Target visibility: %.*s", static_cast<int>(dev_hand_lab::targetVisibilityName(m_debug.targets).size()), dev_hand_lab::targetVisibilityName(m_debug.targets).data());
    Renderer::drawText(line, CONTROL_X, 462, 13, Color{191, 210, 204, 255});
    std::snprintf(line, sizeof(line), "Camera yaw %.2f pitch %.2f dist %.2f", m_cameraState.yaw, m_cameraState.pitch, m_cameraState.distance);
    Renderer::drawText(line, CONTROL_X, 486, 13, Color{191, 210, 204, 255});
    std::snprintf(line, sizeof(line), "FPS %d", GetFPS());
    Renderer::drawText(line, CONTROL_X, 510, 13, Color{112, 235, 150, 255});

#ifdef BIOFUEL_DEBUG_MEMORY_STATS
    const auto processMemory = ::biofuel::engine::debug::MemoryTelemetry::processMemory();
    std::snprintf(line, sizeof(line), "RAM %.1f MiB / Private %.1f MiB",
        static_cast<double>(processMemory.workingSetBytes) / (1024.0 * 1024.0),
        static_cast<double>(processMemory.privateBytes) / (1024.0 * 1024.0));
    Renderer::drawText(line, CONTROL_X, 534, 13, Color{191, 210, 204, 255});
#endif
}

void DevHandLabScreen::drawSlider(
    const i32 y,
    const std::string_view label,
    const f32 value,
    const f32 minValue,
    const f32 maxValue,
    const SliderId slider) const noexcept
{
    static_cast<void>(slider);
    char text[96]{};
    std::snprintf(text, sizeof(text), "%.*s: %.2f", static_cast<int>(label.size()), label.data(), value);
    Renderer::drawText(text, CONTROL_X, y - 18, 12, Color{211, 226, 220, 255});

    const Rectangle bounds = sliderBounds(y);
    DrawRectangleRec(Rectangle{bounds.x, bounds.y - 1.0f, bounds.width, bounds.height + 2.0f}, Color{13, 21, 26, 255});
    DrawRectangleRec(bounds, Color{35, 45, 52, 255});
    const f32 denom = std::max(maxValue - minValue, 0.0001f);
    const f32 t = std::clamp((value - minValue) / denom, 0.0f, 1.0f);
    DrawRectangle(static_cast<i32>(bounds.x), static_cast<i32>(bounds.y), static_cast<i32>(bounds.width * t), static_cast<i32>(bounds.height), Color{62, 207, 122, 255});
    DrawCircle(static_cast<i32>(bounds.x + bounds.width * t), static_cast<i32>(bounds.y + bounds.height * 0.5f), 6.0f, Color{255, 196, 72, 255});
}

void DevHandLabScreen::drawButton(const i32 y, const std::string_view label, const bool active) const noexcept {
    const Rectangle bounds = buttonBounds(y);
    drawButtonAt(static_cast<i32>(bounds.x), static_cast<i32>(bounds.y), static_cast<i32>(bounds.width), label, active);
}

void DevHandLabScreen::drawButtonAt(const i32 x, const i32 y, const i32 width, const std::string_view label, const bool active) const noexcept {
    const Rectangle bounds{static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(width), static_cast<f32>(BUTTON_H)};
    const Color fill = active ? Color{38, 104, 68, 245} : Color{24, 34, 43, 245};
    DrawRectangleRec(Rectangle{bounds.x + 1.0f, bounds.y + 2.0f, bounds.width, bounds.height}, Color{0, 0, 0, 90});
    DrawRectangleRec(bounds, fill);
    DrawRectangleLinesEx(bounds, 1.0f, active ? Color{118, 236, 154, 205} : Color{76, 96, 104, 190});
    Renderer::drawText(label, static_cast<i32>(bounds.x) + 12, static_cast<i32>(bounds.y) + 7, 13, Color{232, 242, 238, 255});
}

void DevHandLabScreen::drawStatusLine(const i32 y) const noexcept {
    char line[180]{};
    std::snprintf(line, sizeof(line), "%s %.*s | %.*s | %s",
        handName(m_selection.hand).data(),
        static_cast<int>(fingerName(m_selection.finger).size()),
        fingerName(m_selection.finger).data(),
        static_cast<int>(m_handEngine.animation().currentName().size()),
        m_handEngine.animation().currentName().data(),
        m_handEngine.animation().playing() ? "playing" : "paused");
    Renderer::drawText(line, PANEL_PAD, y, 13, Color{112, 235, 150, 255});
}

Vector3 DevHandLabScreen::selectedTarget() const noexcept {
    return m_selection.hand == HandSide::Left
        ? m_leftHand.target(m_selection.finger)
        : m_rightHand.target(m_selection.finger);
}

void DevHandLabScreen::moveSelectedTarget(const Vector3 delta) noexcept {
    if (m_selection.hand == HandSide::Left) {
        m_leftHand.moveTarget(m_selection.finger, delta);
    } else {
        m_rightHand.moveTarget(m_selection.finger, delta);
    }
}

i32 DevHandLabScreen::pickFingerTarget(const Vector2 mouse, HandSide& side) const noexcept {
    i32 bestFinger = -1;
    f32 bestDistance = 20.0f;
    const auto testHand = [&](const auto& hand, const HandSide handSide) {
        for (const FingerDebugState& finger : hand.debugFingers()) {
            const Vector2 screen = GetWorldToScreen(finger.target, m_camera);
            const f32 dx = screen.x - mouse.x;
            const f32 dy = screen.y - mouse.y;
            const f32 distance = std::sqrt(dx * dx + dy * dy);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestFinger = static_cast<i32>(finger.id);
                side = handSide;
            }
        }
    };

    testHand(m_leftHand, HandSide::Left);
    testHand(m_rightHand, HandSide::Right);
    return bestFinger;
}

std::string_view DevHandLabScreen::fingerName(const FingerId finger) noexcept {
    switch (finger) {
    case FingerId::Thumb: return "Thumb";
    case FingerId::Index: return "Index";
    case FingerId::Middle: return "Middle";
    case FingerId::Ring: return "Ring";
    case FingerId::Pinky: return "Pinky";
    case FingerId::Count: break;
    }
    return "Unknown";
}

std::string_view DevHandLabScreen::handName(const HandSide hand) noexcept {
    switch (hand) {
    case HandSide::Left: return "Left";
    case HandSide::Right: return "Right";
    }
    return "Unknown";
}

} // namespace biofuel::game::screens

#endif // BIOFUEL_ENABLE_DEV_SCREENS
