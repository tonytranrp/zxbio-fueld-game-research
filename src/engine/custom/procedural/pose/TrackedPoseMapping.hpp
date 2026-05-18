#pragma once

#include "engine/custom/procedural/pose/ProceduralPosePhysics.hpp"
#include "engine/core/Types.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <raylib.h>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::pose {

enum class MirrorPolicy : u8 {
    Camera,
    Selfie,
};

enum class StageLayoutPolicy : u8 {
    Shared,
    Adaptive,
    FixedLanes,
};

enum class CalibrationWizardStep : u8 {
    Inactive,
    Center,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Near,
    Far,
    Complete,
};

enum class CalibrationHandPhase : u8 {
    Left,
    Right,
    Complete,
};

enum class MappingState : u8 {
    Idle,
    Calibrating,
    Ready,
};

enum class CalibrationCaptureStatus : u8 {
    Missing,
    OutsideTarget,
    Unstable,
    Capturing,
    Captured,
};

struct CalibrationHandProgress {
    bool detected = false;
    bool targetAcquired = false;
    bool sampleCaptured = false;
    f32 holdSeconds = 0.0f;
    f32 requiredHoldSeconds = 0.45f;
    f32 targetError = 1.0f;
    CalibrationCaptureStatus status = CalibrationCaptureStatus::Missing;
};

struct CameraFrameSpace {
    u16 width = 0U;
    u16 height = 0U;
    MirrorPolicy mirror = MirrorPolicy::Selfie;

    [[nodiscard]] bool valid() const noexcept {
        return width > 0U && height > 0U;
    }

    [[nodiscard]] f32 aspectRatio() const noexcept {
        return height == 0U ? 1.0f : static_cast<f32>(width) / static_cast<f32>(height);
    }
};

struct StageVolume {
    PoseBounds full{
        .min = Vector3{-0.82f, -0.24f, -0.46f},
        .max = Vector3{0.82f, 0.56f, 0.46f},
    };
    PoseBounds left{
        .min = Vector3{-0.82f, -0.24f, -0.46f},
        .max = Vector3{0.82f, 0.56f, 0.46f},
    };
    PoseBounds right{
        .min = Vector3{-0.82f, -0.24f, -0.46f},
        .max = Vector3{0.82f, 0.56f, 0.46f},
    };
};

struct CalibrationWizardState {
    bool active = false;
    bool cameraChanged = false;
    CalibrationHandPhase activeHand = CalibrationHandPhase::Left;
    CalibrationWizardStep step = CalibrationWizardStep::Inactive;
    f32 holdSeconds = 0.0f;
    f32 requiredHoldSeconds = 0.45f;
    bool targetAcquired = false;
    f32 targetError = 1.0f;
    CalibrationHandProgress left{};
    CalibrationHandProgress right{};
};

struct CalibrationSessionProfile {
    bool valid = false;
    CameraFrameSpace frameSpace{};
    Vector2 center{0.50f, 0.55f};
    Vector2 left{0.18f, 0.55f};
    Vector2 right{0.82f, 0.55f};
    Vector2 top{0.50f, 0.22f};
    Vector2 bottom{0.50f, 0.84f};
    Vector2 topLeft{0.18f, 0.22f};
    Vector2 topRight{0.82f, 0.22f};
    Vector2 bottomLeft{0.18f, 0.84f};
    Vector2 bottomRight{0.82f, 0.84f};
    f32 nearPalmSpan = 0.29f;
    f32 farPalmSpan = 0.12f;
    f32 referencePalmSpan = 0.20f;
};

struct CalibrationAxisRange {
    f32 min = 0.0f;
    f32 max = 1.0f;
};

struct CalibrationImageWarp {
    f32 leftX = 0.0f;
    f32 rightX = 1.0f;
    f32 topY = 0.0f;
    f32 bottomY = 1.0f;
};

[[nodiscard]] inline f32 clamp01(const f32 value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] inline Vector2 mirroredPoint(const CameraFrameSpace& frameSpace, const Vector2 value) noexcept {
    if (frameSpace.mirror != MirrorPolicy::Selfie) {
        return value;
    }
    return Vector2{1.0f - value.x, value.y};
}

[[nodiscard]] inline f32 remapClamped(
    const f32 value,
    const f32 inMin,
    const f32 inMax,
    const f32 outMin,
    const f32 outMax) noexcept
{
    const f32 range = inMax - inMin;
    if (std::fabs(range) <= 0.0001f) {
        return (outMin + outMax) * 0.5f;
    }
    const f32 t = clamp01((value - inMin) / range);
    return outMin + (outMax - outMin) * t;
}

[[nodiscard]] inline f32 remapUnclamped(
    const f32 value,
    const f32 inMin,
    const f32 inMax,
    const f32 outMin,
    const f32 outMax) noexcept
{
    const f32 range = inMax - inMin;
    if (std::fabs(range) <= 0.0001f) {
        return (outMin + outMax) * 0.5f;
    }
    const f32 t = (value - inMin) / range;
    return outMin + (outMax - outMin) * t;
}

[[nodiscard]] inline f32 lerpValue(const f32 a, const f32 b, const f32 t) noexcept {
    return a + (b - a) * t;
}

[[nodiscard]] inline f32 edgeSampleAt(
    const f32 start,
    const f32 middle,
    const f32 end,
    const f32 t) noexcept
{
    const f32 clamped = clamp01(t);
    if (clamped <= 0.5f) {
        return lerpValue(start, middle, clamped * 2.0f);
    }
    return lerpValue(middle, end, (clamped - 0.5f) * 2.0f);
}

[[nodiscard]] inline CalibrationWizardStep nextCalibrationStep(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return CalibrationWizardStep::Left;
    case CalibrationWizardStep::Left: return CalibrationWizardStep::Right;
    case CalibrationWizardStep::Right: return CalibrationWizardStep::Top;
    case CalibrationWizardStep::Top: return CalibrationWizardStep::Bottom;
    case CalibrationWizardStep::Bottom: return CalibrationWizardStep::Complete;
    case CalibrationWizardStep::TopLeft: return CalibrationWizardStep::TopRight;
    case CalibrationWizardStep::TopRight: return CalibrationWizardStep::BottomLeft;
    case CalibrationWizardStep::BottomLeft: return CalibrationWizardStep::BottomRight;
    case CalibrationWizardStep::BottomRight: return CalibrationWizardStep::Near;
    case CalibrationWizardStep::Near: return CalibrationWizardStep::Far;
    case CalibrationWizardStep::Far: return CalibrationWizardStep::Complete;
    case CalibrationWizardStep::Inactive:
    case CalibrationWizardStep::Complete:
        break;
    default:
        break;
    }
    return CalibrationWizardStep::Complete;
}

[[nodiscard]] inline CalibrationHandPhase nextCalibrationHandPhase(const CalibrationHandPhase phase) noexcept {
    switch (phase) {
    case CalibrationHandPhase::Left: return CalibrationHandPhase::Right;
    case CalibrationHandPhase::Right: return CalibrationHandPhase::Complete;
    case CalibrationHandPhase::Complete:
        break;
    default:
        break;
    }
    return CalibrationHandPhase::Complete;
}

[[nodiscard]] inline u8 calibrationStepCount() noexcept {
    return 5U;
}

[[nodiscard]] inline u8 calibrationStepOrdinal(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return 1U;
    case CalibrationWizardStep::Left: return 2U;
    case CalibrationWizardStep::Right: return 3U;
    case CalibrationWizardStep::Top: return 4U;
    case CalibrationWizardStep::Bottom: return 5U;
    case CalibrationWizardStep::TopLeft:
    case CalibrationWizardStep::TopRight:
    case CalibrationWizardStep::BottomLeft:
    case CalibrationWizardStep::BottomRight:
    case CalibrationWizardStep::Near:
    case CalibrationWizardStep::Far:
    case CalibrationWizardStep::Inactive:
    case CalibrationWizardStep::Complete:
        break;
    default:
        break;
    }
    return 0U;
}

[[nodiscard]] inline std::string_view calibrationHandPhaseName(const CalibrationHandPhase phase) noexcept {
    switch (phase) {
    case CalibrationHandPhase::Left: return "Left hand";
    case CalibrationHandPhase::Right: return "Right hand";
    case CalibrationHandPhase::Complete: return "Hands";
    default: return "Hand";
    }
}

[[nodiscard]] inline std::string_view calibrationPrompt(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return "Hold at the center marker";
    case CalibrationWizardStep::Left: return "Move to the left edge marker";
    case CalibrationWizardStep::Right: return "Move to the right edge marker";
    case CalibrationWizardStep::Top: return "Raise to the top marker";
    case CalibrationWizardStep::Bottom: return "Lower to the bottom marker";
    case CalibrationWizardStep::TopLeft: return "Hold at the upper-left corner";
    case CalibrationWizardStep::TopRight: return "Hold at the upper-right corner";
    case CalibrationWizardStep::BottomLeft: return "Hold at the lower-left corner";
    case CalibrationWizardStep::BottomRight: return "Hold at the lower-right corner";
    case CalibrationWizardStep::Near: return "Move closer to the camera";
    case CalibrationWizardStep::Far: return "Move farther from the camera";
    case CalibrationWizardStep::Inactive: return "Calibration idle";
    case CalibrationWizardStep::Complete: return "Calibration complete";
    default: return "Calibration";
    }
}

[[nodiscard]] inline std::string_view calibrationPrompt(
    const CalibrationHandPhase phase,
    const CalibrationWizardStep step) noexcept
{
    if (phase == CalibrationHandPhase::Left) {
        switch (step) {
        case CalibrationWizardStep::Center: return "Left hand: hold at the center marker";
        case CalibrationWizardStep::Left: return "Left hand: move to the left edge marker";
        case CalibrationWizardStep::Right: return "Left hand: move to the right edge marker";
        case CalibrationWizardStep::Top: return "Left hand: raise to the top marker";
        case CalibrationWizardStep::Bottom: return "Left hand: lower to the bottom marker";
        case CalibrationWizardStep::TopLeft: return "Left hand: hold at the upper-left corner";
        case CalibrationWizardStep::TopRight: return "Left hand: hold at the upper-right corner";
        case CalibrationWizardStep::BottomLeft: return "Left hand: hold at the lower-left corner";
        case CalibrationWizardStep::BottomRight: return "Left hand: hold at the lower-right corner";
        case CalibrationWizardStep::Near: return "Left hand: move closer to the camera";
        case CalibrationWizardStep::Far: return "Left hand: move farther from the camera";
        case CalibrationWizardStep::Inactive: return "Calibration idle";
        case CalibrationWizardStep::Complete: return "Left hand calibration complete";
        default: break;
        }
    }
    if (phase == CalibrationHandPhase::Right) {
        switch (step) {
        case CalibrationWizardStep::Center: return "Right hand: hold at the center marker";
        case CalibrationWizardStep::Left: return "Right hand: move to the left edge marker";
        case CalibrationWizardStep::Right: return "Right hand: move to the right edge marker";
        case CalibrationWizardStep::Top: return "Right hand: raise to the top marker";
        case CalibrationWizardStep::Bottom: return "Right hand: lower to the bottom marker";
        case CalibrationWizardStep::TopLeft: return "Right hand: hold at the upper-left corner";
        case CalibrationWizardStep::TopRight: return "Right hand: hold at the upper-right corner";
        case CalibrationWizardStep::BottomLeft: return "Right hand: hold at the lower-left corner";
        case CalibrationWizardStep::BottomRight: return "Right hand: hold at the lower-right corner";
        case CalibrationWizardStep::Near: return "Right hand: move closer to the camera";
        case CalibrationWizardStep::Far: return "Right hand: move farther from the camera";
        case CalibrationWizardStep::Inactive: return "Calibration idle";
        case CalibrationWizardStep::Complete: return "Right hand calibration complete";
        default: break;
        }
    }
    return calibrationPrompt(step);
}

[[nodiscard]] inline std::string_view calibrationCaptureStatusName(const CalibrationCaptureStatus status) noexcept {
    switch (status) {
    case CalibrationCaptureStatus::Missing: return "missing";
    case CalibrationCaptureStatus::OutsideTarget: return "move to marker";
    case CalibrationCaptureStatus::Unstable: return "hold still";
    case CalibrationCaptureStatus::Capturing: return "capturing";
    case CalibrationCaptureStatus::Captured: return "captured";
    default: return "waiting";
    }
}

[[nodiscard]] inline f32 calibrationRequiredHoldSeconds(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return 0.46f;
    case CalibrationWizardStep::Left:
    case CalibrationWizardStep::Right:
    case CalibrationWizardStep::Top:
    case CalibrationWizardStep::Bottom:
        return 0.38f;
    case CalibrationWizardStep::TopLeft:
    case CalibrationWizardStep::TopRight:
    case CalibrationWizardStep::BottomLeft:
    case CalibrationWizardStep::BottomRight:
    case CalibrationWizardStep::Near:
    case CalibrationWizardStep::Far:
        return 0.42f;
    case CalibrationWizardStep::Inactive:
    case CalibrationWizardStep::Complete:
        break;
    default:
        break;
    }
    return 0.45f;
}

[[nodiscard]] inline Vector2 calibrationTarget(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return Vector2{0.50f, 0.55f};
    case CalibrationWizardStep::Left: return Vector2{0.18f, 0.55f};
    case CalibrationWizardStep::Right: return Vector2{0.82f, 0.55f};
    case CalibrationWizardStep::Top: return Vector2{0.50f, 0.22f};
    case CalibrationWizardStep::Bottom: return Vector2{0.50f, 0.84f};
    case CalibrationWizardStep::TopLeft: return Vector2{0.18f, 0.22f};
    case CalibrationWizardStep::TopRight: return Vector2{0.82f, 0.22f};
    case CalibrationWizardStep::BottomLeft: return Vector2{0.18f, 0.84f};
    case CalibrationWizardStep::BottomRight: return Vector2{0.82f, 0.84f};
    case CalibrationWizardStep::Near:
    case CalibrationWizardStep::Far:
        return Vector2{0.50f, 0.55f};
    case CalibrationWizardStep::Inactive:
    case CalibrationWizardStep::Complete:
        break;
    default:
        break;
    }
    return Vector2{0.50f, 0.55f};
}

[[nodiscard]] inline f32 calibrationTargetRadius(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return 0.135f;
    case CalibrationWizardStep::Left:
    case CalibrationWizardStep::Right:
    case CalibrationWizardStep::Top:
    case CalibrationWizardStep::Bottom:
        return 0.125f;
    case CalibrationWizardStep::TopLeft:
    case CalibrationWizardStep::TopRight:
    case CalibrationWizardStep::BottomLeft:
    case CalibrationWizardStep::BottomRight:
        return 0.110f;
    case CalibrationWizardStep::Near:
    case CalibrationWizardStep::Far:
        return 0.170f;
    case CalibrationWizardStep::Inactive:
    case CalibrationWizardStep::Complete:
        break;
    default:
        break;
    }
    return 0.105f;
}

[[nodiscard]] inline CalibrationAxisRange calibrationHorizontalRange(
    const CalibrationSessionProfile& profile) noexcept
{
    f32 leftEdge = (profile.left.x + profile.topLeft.x + profile.bottomLeft.x) / 3.0f;
    f32 rightEdge = (profile.right.x + profile.topRight.x + profile.bottomRight.x) / 3.0f;
    if (rightEdge < leftEdge) {
        std::swap(leftEdge, rightEdge);
    }
    return CalibrationAxisRange{.min = leftEdge, .max = rightEdge};
}

[[nodiscard]] inline CalibrationAxisRange calibrationVerticalRange(
    const CalibrationSessionProfile& profile) noexcept
{
    f32 topEdge = (profile.top.y + profile.topLeft.y + profile.topRight.y) / 3.0f;
    f32 bottomEdge = (profile.bottom.y + profile.bottomLeft.y + profile.bottomRight.y) / 3.0f;
    if (bottomEdge < topEdge) {
        std::swap(topEdge, bottomEdge);
    }
    return CalibrationAxisRange{.min = topEdge, .max = bottomEdge};
}

[[nodiscard]] inline CalibrationImageWarp calibrationImageWarpAt(
    const CalibrationSessionProfile& profile,
    const Vector2 displayPoint) noexcept
{
    const CalibrationAxisRange xRange = calibrationHorizontalRange(profile);
    const CalibrationAxisRange yRange = calibrationVerticalRange(profile);
    const f32 xT = remapClamped(displayPoint.x, xRange.min, xRange.max, 0.0f, 1.0f);
    const f32 yT = remapClamped(displayPoint.y, yRange.min, yRange.max, 0.0f, 1.0f);

    CalibrationImageWarp warp{
        .leftX = edgeSampleAt(profile.topLeft.x, profile.left.x, profile.bottomLeft.x, yT),
        .rightX = edgeSampleAt(profile.topRight.x, profile.right.x, profile.bottomRight.x, yT),
        .topY = edgeSampleAt(profile.topLeft.y, profile.top.y, profile.topRight.y, xT),
        .bottomY = edgeSampleAt(profile.bottomLeft.y, profile.bottom.y, profile.bottomRight.y, xT),
    };
    if ((warp.rightX - warp.leftX) < 0.12f) {
        warp.leftX = xRange.min;
        warp.rightX = xRange.max;
    }
    if ((warp.bottomY - warp.topY) < 0.12f) {
        warp.topY = yRange.min;
        warp.bottomY = yRange.max;
    }
    return warp;
}

[[nodiscard]] inline StageVolume makeStageVolume(
    const PoseBounds fullBounds,
    const StageLayoutPolicy layout,
    const usize activeHands) noexcept
{
    StageVolume volume{
        .full = fullBounds,
        .left = fullBounds,
        .right = fullBounds,
    };
    if (layout == StageLayoutPolicy::Shared || layout == StageLayoutPolicy::Adaptive || activeHands < 2U) {
        return volume;
    }

    const f32 gap = 0.22f;
    const f32 centerX = (fullBounds.min.x + fullBounds.max.x) * 0.5f;
    volume.left.max.x = centerX - gap * 0.5f;
    volume.right.min.x = centerX + gap * 0.5f;
    return volume;
}

struct PoseStabilizer {
    template<usize N>
    static void apply(
        std::array<Vector3, N>& current,
        const std::array<Vector3, N>& previous,
        const f32 dt,
        const f32 response = 16.0f) noexcept
    {
        const f32 safeDt = std::max(dt, 0.0f);
        const f32 alpha = 1.0f - std::exp(-response * safeDt);
        smoothPose(current, previous, alpha);
    }
};

struct PoseVisibilityFitter {
    template<usize N>
    static void apply(std::array<Vector3, N>& points, const PoseBounds bounds) noexcept {
        fitPoseInsideBounds(points, bounds);
    }
};

struct PoseSeparationSolver {
    template<usize N>
    static void apply(
        std::array<Vector3, N>& left,
        std::array<Vector3, N>& right,
        const Vector3 leftCenter,
        const Vector3 rightCenter,
        const f32 minimumDistance) noexcept
    {
        separatePoses(left, right, leftCenter, rightCenter, minimumDistance);
    }
};

inline void sanitizeCalibrationProfile(CalibrationSessionProfile& profile) noexcept {
    if (!profile.frameSpace.valid()) {
        return;
    }

    const f32 minHorizontal = 0.20f;
    const f32 minVertical = 0.22f;
    if ((profile.right.x - profile.left.x) < minHorizontal) {
        const f32 center = profile.center.x;
        profile.left.x = clamp01(center - minHorizontal * 0.5f);
        profile.right.x = clamp01(center + minHorizontal * 0.5f);
    }
    if ((profile.bottom.y - profile.top.y) < minVertical) {
        const f32 center = profile.center.y;
        profile.top.y = clamp01(center - minVertical * 0.5f);
        profile.bottom.y = clamp01(center + minVertical * 0.5f);
    }
    profile.left.y = profile.center.y;
    profile.right.y = profile.center.y;
    profile.top.x = profile.center.x;
    profile.bottom.x = profile.center.x;

    profile.topLeft = Vector2{profile.left.x, profile.top.y};
    profile.topRight = Vector2{profile.right.x, profile.top.y};
    profile.bottomLeft = Vector2{profile.left.x, profile.bottom.y};
    profile.bottomRight = Vector2{profile.right.x, profile.bottom.y};

    const CalibrationAxisRange xRange = calibrationHorizontalRange(profile);
    if ((xRange.max - xRange.min) < minHorizontal) {
        profile.topLeft.x = profile.left.x;
        profile.bottomLeft.x = profile.left.x;
        profile.topRight.x = profile.right.x;
        profile.bottomRight.x = profile.right.x;
    }
    const CalibrationAxisRange yRange = calibrationVerticalRange(profile);
    if ((yRange.max - yRange.min) < minVertical) {
        profile.topLeft.y = profile.top.y;
        profile.topRight.y = profile.top.y;
        profile.bottomLeft.y = profile.bottom.y;
        profile.bottomRight.y = profile.bottom.y;
    }

    if (profile.nearPalmSpan <= profile.farPalmSpan + 0.02f) {
        const f32 middle = std::max(profile.referencePalmSpan, 0.12f);
        profile.nearPalmSpan = middle * 1.18f;
        profile.farPalmSpan = middle * 0.82f;
    }
    profile.referencePalmSpan = std::clamp(profile.referencePalmSpan, 0.05f, 0.42f);
    profile.nearPalmSpan = std::clamp(profile.nearPalmSpan, 0.07f, 0.50f);
    profile.farPalmSpan = std::clamp(profile.farPalmSpan, 0.04f, profile.nearPalmSpan - 0.01f);
}

} // namespace biofuel::engine::custom::procedural::pose
