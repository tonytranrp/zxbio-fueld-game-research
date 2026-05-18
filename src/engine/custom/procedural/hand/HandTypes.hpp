#pragma once

#include "engine/core/Types.hpp"
#include <array>
#include <string_view>
#include <raylib.h>

namespace biofuel::engine::custom::procedural::hand {

enum class HandSide : u8 {
    Left,
    Right,
};

enum class FingerId : u8 {
    Thumb,
    Index,
    Middle,
    Ring,
    Pinky,
    Count,
};

struct LeftRobotHand {};
struct RightRobotHand {};
struct BiofuelRobotHands {};
struct DefaultRobotHandPreset {};
struct RobotHandStyle {};
struct NeutralPoseTag {};
struct ProceduralFingerLimitTag {};

template<typename THandTag>
struct HandRigSpec;

template<>
struct HandRigSpec<LeftRobotHand> {
    static constexpr HandSide side = HandSide::Left;
    static constexpr f32 mirror = -1.0f;
    static constexpr std::string_view name = "Left";
};

template<>
struct HandRigSpec<RightRobotHand> {
    static constexpr HandSide side = HandSide::Right;
    static constexpr f32 mirror = 1.0f;
    static constexpr std::string_view name = "Right";
};

template<FingerId TFinger>
struct FingerChainSpec {
    static constexpr FingerId id = TFinger;
    static constexpr usize jointCount = 5U;
};

struct HandFingerDimensions {
    Vector3 baseOffset{0.0f, 0.0f, 0.0f};
    Vector3 direction{0.0f, 1.0f, 0.0f};
    f32 segmentLength = 0.065f;
};

struct HandRigDimensions {
    Vector3 palmJointOffset{0.0f, 0.12f, 0.0f};
    std::array<HandFingerDimensions, static_cast<usize>(FingerId::Count)> fingers{};
};

struct HandPoseSettings {
    f32 curl = 0.18f;
    f32 spread = 0.0f;
};

template<typename TPoseTag>
struct HandPose {
    static constexpr f32 curl = 0.18f;
    static constexpr f32 spread = 0.0f;
};

struct HandTargetHandle {
    HandSide hand = HandSide::Left;
    FingerId finger = FingerId::Index;
};

[[nodiscard]] constexpr usize fingerIndex(const FingerId finger) noexcept {
    return static_cast<usize>(finger);
}

[[nodiscard]] constexpr std::string_view fingerName(const FingerId finger) noexcept {
    switch (finger) {
    case FingerId::Thumb: return "Thumb";
    case FingerId::Index: return "Index";
    case FingerId::Middle: return "Middle";
    case FingerId::Ring: return "Ring";
    case FingerId::Pinky: return "Pinky";
    case FingerId::Count: break;
    default: break;
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view handName(const HandSide hand) noexcept {
    switch (hand) {
    case HandSide::Left: return "Left";
    case HandSide::Right: return "Right";
    default: return "Unknown";
    }
}

[[nodiscard]] constexpr HandRigDimensions defaultRobotHandDimensions() noexcept {
    return HandRigDimensions{
        .palmJointOffset = Vector3{0.0f, 0.105f, 0.0f},
        .fingers = {{
            HandFingerDimensions{.baseOffset = Vector3{0.126f, 0.022f, 0.018f}, .direction = Vector3{0.78f, 0.38f, -0.04f}, .segmentLength = 0.050f},
            HandFingerDimensions{.baseOffset = Vector3{0.074f, 0.142f, -0.004f}, .direction = Vector3{0.17f, 0.985f, 0.018f}, .segmentLength = 0.058f},
            HandFingerDimensions{.baseOffset = Vector3{0.020f, 0.158f, -0.006f}, .direction = Vector3{0.035f, 0.999f, 0.010f}, .segmentLength = 0.066f},
            HandFingerDimensions{.baseOffset = Vector3{-0.038f, 0.148f, -0.004f}, .direction = Vector3{-0.105f, 0.993f, 0.018f}, .segmentLength = 0.060f},
            HandFingerDimensions{.baseOffset = Vector3{-0.092f, 0.116f, 0.000f}, .direction = Vector3{-0.235f, 0.970f, 0.035f}, .segmentLength = 0.051f},
        }},
    };
}

} // namespace biofuel::engine::custom::procedural::hand
