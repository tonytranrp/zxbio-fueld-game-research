#pragma once

#include "engine/custom/procedural/physics/ProceduralPosePhysics.hpp"
#include "engine/core/Types.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <raylib.h>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::physics {

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
    Near,
    Far,
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
    f32 requiredHoldSeconds = 1.25f;
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
    CalibrationWizardStep step = CalibrationWizardStep::Inactive;
    f32 holdSeconds = 0.0f;
    f32 requiredHoldSeconds = 1.25f;
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
    f32 nearPalmSpan = 0.29f;
    f32 farPalmSpan = 0.12f;
    f32 referencePalmSpan = 0.20f;
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

[[nodiscard]] inline CalibrationWizardStep nextCalibrationStep(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return CalibrationWizardStep::Left;
    case CalibrationWizardStep::Left: return CalibrationWizardStep::Right;
    case CalibrationWizardStep::Right: return CalibrationWizardStep::Top;
    case CalibrationWizardStep::Top: return CalibrationWizardStep::Bottom;
    case CalibrationWizardStep::Bottom: return CalibrationWizardStep::Near;
    case CalibrationWizardStep::Near: return CalibrationWizardStep::Far;
    case CalibrationWizardStep::Far: return CalibrationWizardStep::Complete;
    case CalibrationWizardStep::Inactive:
    case CalibrationWizardStep::Complete:
        break;
    }
    return CalibrationWizardStep::Complete;
}

[[nodiscard]] inline std::string_view calibrationPrompt(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return "Hold both hands in the center";
    case CalibrationWizardStep::Left: return "Move both hands to the left edge";
    case CalibrationWizardStep::Right: return "Move both hands to the right edge";
    case CalibrationWizardStep::Top: return "Raise both hands to the top";
    case CalibrationWizardStep::Bottom: return "Lower both hands to the bottom";
    case CalibrationWizardStep::Near: return "Move both hands closer to the camera";
    case CalibrationWizardStep::Far: return "Move both hands farther from the camera";
    case CalibrationWizardStep::Inactive: return "Calibration idle";
    case CalibrationWizardStep::Complete: return "Calibration complete";
    }
    return "Calibration";
}

[[nodiscard]] inline std::string_view calibrationCaptureStatusName(const CalibrationCaptureStatus status) noexcept {
    switch (status) {
    case CalibrationCaptureStatus::Missing: return "missing";
    case CalibrationCaptureStatus::OutsideTarget: return "move to marker";
    case CalibrationCaptureStatus::Unstable: return "hold still";
    case CalibrationCaptureStatus::Capturing: return "capturing";
    case CalibrationCaptureStatus::Captured: return "captured";
    }
    return "waiting";
}

[[nodiscard]] inline Vector2 calibrationTarget(const CalibrationWizardStep step) noexcept {
    switch (step) {
    case CalibrationWizardStep::Center: return Vector2{0.50f, 0.55f};
    case CalibrationWizardStep::Left: return Vector2{0.18f, 0.55f};
    case CalibrationWizardStep::Right: return Vector2{0.82f, 0.55f};
    case CalibrationWizardStep::Top: return Vector2{0.50f, 0.22f};
    case CalibrationWizardStep::Bottom: return Vector2{0.50f, 0.84f};
    case CalibrationWizardStep::Near:
    case CalibrationWizardStep::Far:
        return Vector2{0.50f, 0.55f};
    case CalibrationWizardStep::Inactive:
    case CalibrationWizardStep::Complete:
        break;
    }
    return Vector2{0.50f, 0.55f};
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
    if (layout == StageLayoutPolicy::Shared || activeHands < 2U) {
        return volume;
    }

    const f32 gap = layout == StageLayoutPolicy::Adaptive ? 0.14f : 0.22f;
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

    if (profile.nearPalmSpan <= profile.farPalmSpan + 0.02f) {
        const f32 middle = std::max(profile.referencePalmSpan, 0.12f);
        profile.nearPalmSpan = middle * 1.28f;
        profile.farPalmSpan = middle * 0.72f;
    }
    profile.referencePalmSpan = std::clamp(profile.referencePalmSpan, 0.05f, 0.42f);
    profile.nearPalmSpan = std::clamp(profile.nearPalmSpan, 0.07f, 0.50f);
    profile.farPalmSpan = std::clamp(profile.farPalmSpan, 0.04f, profile.nearPalmSpan - 0.01f);
}

} // namespace biofuel::engine::custom::procedural::physics
