#pragma once

#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/ik/FabrikSolver.hpp"
#include "engine/custom/procedural/ik/JointLimits.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::hand {

struct FingerDebugState {
    FingerId id = FingerId::Index;
    Vector3 target{0.0f, 0.0f, 0.0f};
    ::biofuel::engine::custom::procedural::ik::IkSolveResult solve{};
};

template<typename THandTag>
class ProceduralHand final {
public:
    static constexpr usize FINGER_COUNT = static_cast<usize>(FingerId::Count);
    static constexpr usize JOINTS_PER_FINGER = 5U;
    static constexpr usize PALM_JOINT_COUNT = 2U;
    static constexpr usize JOINT_COUNT = PALM_JOINT_COUNT + FINGER_COUNT * JOINTS_PER_FINGER;
    static constexpr usize BONE_COUNT = 1U + FINGER_COUNT * JOINTS_PER_FINGER;

    ProceduralHand() {
        reset(defaultOrigin(), defaultRobotHandDimensions());
    }

    void reset(const Vector3 origin, const HandRigDimensions& dimensions) noexcept {
        m_dimensions = dimensions;
        reset(origin);
    }

    void reset(const Vector3 origin) noexcept {
        m_origin = origin;
        m_pitch = 0.0f;
        m_yaw = 0.0f;
        m_roll = 0.0f;
        m_curl = HandPose<NeutralPoseTag>::curl;
        m_spread = HandPose<NeutralPoseTag>::spread;
        buildRestPose();
        resetTargetsFromPose();
        solve(::biofuel::engine::custom::procedural::ik::IkSolveSettings{});
    }

    void setRigDimensions(const HandRigDimensions& dimensions) noexcept {
        m_dimensions = dimensions;
        buildRestPose();
        resetTargetsFromPose();
    }

    [[nodiscard]] const HandRigDimensions& rigDimensions() const noexcept {
        return m_dimensions;
    }

    void setCurl(const f32 value) noexcept {
        m_curl = std::clamp(value, 0.0f, 1.0f);
        resetTargetsFromPose();
    }

    void setSpread(const f32 value) noexcept {
        m_spread = std::clamp(value, -1.0f, 1.0f);
        resetTargetsFromPose();
    }

    [[nodiscard]] f32 curl() const noexcept { return m_curl; }
    [[nodiscard]] f32 spread() const noexcept { return m_spread; }
    [[nodiscard]] Vector3 origin() const noexcept { return m_origin; }
    [[nodiscard]] Vector3 wristRotation() const noexcept { return Vector3{m_pitch, m_yaw, m_roll}; }
    [[nodiscard]] std::string_view name() const noexcept { return HandRigSpec<THandTag>::name; }
    [[nodiscard]] static constexpr HandSide side() noexcept { return HandRigSpec<THandTag>::side; }

    void setWristPose(const Vector3 origin, const f32 pitch, const f32 yaw, const f32 roll) noexcept {
        m_origin = origin;
        m_pitch = pitch;
        m_yaw = yaw;
        m_roll = roll;
        buildRestPose();
        resetTargetsFromPose();
    }

    void setTarget(const FingerId finger, const Vector3 target) noexcept {
        fingerRuntime(finger).target = target;
    }

    [[nodiscard]] Vector3 target(const FingerId finger) const noexcept {
        return fingerRuntime(finger).target;
    }

    void moveTarget(const FingerId finger, const Vector3 delta) noexcept {
        auto& runtime = fingerRuntime(finger);
        runtime.target = Vector3Add(runtime.target, delta);
    }

    void solve(const ::biofuel::engine::custom::procedural::ik::IkSolveSettings settings) noexcept {
        m_world = m_rest;
        for (auto& finger : m_fingers) {
            solveFinger(finger, settings);
        }
    }

    [[nodiscard]] std::span<const Vector3> joints() const noexcept {
        return std::span<const Vector3>(m_world.data(), m_world.size());
    }

    [[nodiscard]] std::span<const FingerDebugState> debugFingers() const noexcept {
        for (usize index = 0U; index < m_fingers.size(); ++index) {
            m_debug[index] = FingerDebugState{
                .id = m_fingers[index].id,
                .target = m_fingers[index].target,
                .solve = m_fingers[index].solve,
            };
        }
        return std::span<const FingerDebugState>(m_debug.data(), m_debug.size());
    }

    [[nodiscard]] const std::array<std::array<usize, 2>, BONE_COUNT>& bones() const noexcept {
        return m_bones;
    }

private:
    struct FingerRuntime {
        FingerId id = FingerId::Index;
        std::array<usize, JOINTS_PER_FINGER> joints{};
        Vector3 target{0.0f, 0.0f, 0.0f};
        ::biofuel::engine::custom::procedural::ik::IkSolveResult solve{};
    };

    [[nodiscard]] static Vector3 defaultOrigin() noexcept {
        return Vector3{HandRigSpec<THandTag>::mirror * 0.34f, -0.22f, 0.0f};
    }

    [[nodiscard]] static constexpr usize fingerBaseIndex(const FingerId finger) noexcept {
        return PALM_JOINT_COUNT + static_cast<usize>(finger) * JOINTS_PER_FINGER;
    }

    [[nodiscard]] FingerRuntime& fingerRuntime(const FingerId finger) noexcept {
        return m_fingers[static_cast<usize>(finger)];
    }

    [[nodiscard]] const FingerRuntime& fingerRuntime(const FingerId finger) const noexcept {
        return m_fingers[static_cast<usize>(finger)];
    }

    void solveFinger(
        FingerRuntime& finger,
        const ::biofuel::engine::custom::procedural::ik::IkSolveSettings settings) noexcept
    {
        switch (finger.id) {
        case FingerId::Thumb: solveFingerTyped<FingerId::Thumb>(finger, settings); return;
        case FingerId::Index: solveFingerTyped<FingerId::Index>(finger, settings); return;
        case FingerId::Middle: solveFingerTyped<FingerId::Middle>(finger, settings); return;
        case FingerId::Ring: solveFingerTyped<FingerId::Ring>(finger, settings); return;
        case FingerId::Pinky: solveFingerTyped<FingerId::Pinky>(finger, settings); return;
        case FingerId::Count: break;
        default: break;
        }
    }

    template<FingerId TFinger>
    void solveFingerTyped(
        FingerRuntime& finger,
        const ::biofuel::engine::custom::procedural::ik::IkSolveSettings settings) noexcept
    {
        std::array<Vector3, JOINTS_PER_FINGER> chain{};
        for (usize index = 0U; index < JOINTS_PER_FINGER; ++index) {
            chain[index] = m_world[finger.joints[index]];
        }

        finger.solve = ::biofuel::engine::custom::procedural::ik::FabrikSolver<FingerChainSpec<TFinger>>::solve(
            std::span<Vector3>(chain.data(), chain.size()),
            finger.target,
            settings);

        for (usize index = 0U; index < JOINTS_PER_FINGER; ++index) {
            m_world[finger.joints[index]] = chain[index];
        }
    }

    void buildRestPose() noexcept {
        m_rest[0] = m_origin;
        m_rest[1] = pointFromLocal(m_dimensions.palmJointOffset);

        for (u8 raw = 0U; raw < static_cast<u8>(FingerId::Count); ++raw) {
            const FingerId finger = static_cast<FingerId>(raw);
            const HandFingerDimensions& dimensions = m_dimensions.fingers[static_cast<usize>(finger)];
            configureFinger(finger, dimensions.baseOffset, dimensions.direction, dimensions.segmentLength);
        }

        m_bones = {};
        usize bone = 0U;
        m_bones[bone++] = {0U, 1U};
        for (const auto& finger : m_fingers) {
            m_bones[bone++] = {1U, finger.joints[0]};
            for (usize index = 0U; index + 1U < JOINTS_PER_FINGER; ++index) {
                m_bones[bone++] = {finger.joints[index], finger.joints[index + 1U]};
            }
        }
    }

    [[nodiscard]] Matrix wristMatrix() const noexcept {
        return MatrixRotateXYZ(Vector3{m_pitch, m_yaw, m_roll});
    }

    [[nodiscard]] Vector3 pointFromLocal(const Vector3 local) const noexcept {
        return Vector3Add(m_origin, Vector3Transform(local, wristMatrix()));
    }

    [[nodiscard]] Vector3 directionFromLocal(const Vector3 local) const noexcept {
        return Vector3Transform(local, wristMatrix());
    }

    [[nodiscard]] static Vector3 safeNormalize(const Vector3 value, const Vector3 fallback) noexcept {
        const f32 length = Vector3Length(value);
        if (length <= 0.0001f) {
            return fallback;
        }
        return Vector3Scale(value, 1.0f / length);
    }

    void configureFinger(
        const FingerId finger,
        const Vector3 baseOffset,
        const Vector3 direction,
        const f32 segmentLength) noexcept
    {
        auto& runtime = fingerRuntime(finger);
        runtime.id = finger;
        const usize base = fingerBaseIndex(finger);
        for (usize index = 0U; index < JOINTS_PER_FINGER; ++index) {
            runtime.joints[index] = base + index;
        }

        constexpr f32 mirror = HandRigSpec<THandTag>::mirror;
        const Vector3 mirroredBase{baseOffset.x * mirror, baseOffset.y, baseOffset.z};
        const Vector3 mirroredDirection{direction.x * mirror, direction.y, direction.z};
        const Vector3 start = pointFromLocal(mirroredBase);
        const Vector3 dir = safeNormalize(directionFromLocal(mirroredDirection), Vector3{0.0f, 1.0f, 0.0f});
        for (usize index = 0U; index < JOINTS_PER_FINGER; ++index) {
            const f32 distance = static_cast<f32>(index) * segmentLength;
            m_rest[base + index] = Vector3Add(start, Vector3Scale(dir, distance));
        }
    }

    void resetTargetsFromPose() noexcept {
        constexpr f32 mirror = HandRigSpec<THandTag>::mirror;
        for (auto& finger : m_fingers) {
            const usize tipIndex = finger.joints.back();
            const usize baseIndex = finger.joints.front();
            const f32 ordinal = static_cast<f32>(static_cast<u8>(finger.id)) - 2.0f;
            const f32 curlDepth = -0.19f * m_curl;
            const f32 curlDrop = -0.09f * m_curl;
            const f32 spreadX = mirror * ordinal * 0.035f * m_spread;
            const Vector3 desired = Vector3Add(m_rest[tipIndex], Vector3{spreadX, curlDrop, curlDepth});
            const Vector3 fromBase = Vector3Subtract(desired, m_rest[baseIndex]);
            const f32 reach = std::max(Vector3Length(fromBase), 0.0001f);
            const Vector3 clamped = ::biofuel::engine::custom::procedural::ik::JointLimit<ProceduralFingerLimitTag>::clampDirection(
                Vector3Scale(fromBase, 1.0f / reach),
                -m_curl * 0.08f,
                spreadX);
            finger.target = Vector3Add(m_rest[baseIndex], Vector3Scale(clamped, reach));
        }
    }

    Vector3 m_origin{0.0f, 0.0f, 0.0f};
    f32 m_pitch = 0.0f;
    f32 m_yaw = 0.0f;
    f32 m_roll = 0.0f;
    f32 m_curl = 0.0f;
    f32 m_spread = 0.0f;
    HandRigDimensions m_dimensions = defaultRobotHandDimensions();
    std::array<Vector3, JOINT_COUNT> m_rest{};
    std::array<Vector3, JOINT_COUNT> m_world{};
    std::array<FingerRuntime, FINGER_COUNT> m_fingers{{
        FingerRuntime{.id = FingerId::Thumb},
        FingerRuntime{.id = FingerId::Index},
        FingerRuntime{.id = FingerId::Middle},
        FingerRuntime{.id = FingerId::Ring},
        FingerRuntime{.id = FingerId::Pinky},
    }};
    std::array<std::array<usize, 2>, BONE_COUNT> m_bones{};
    mutable std::array<FingerDebugState, FINGER_COUNT> m_debug{};
};

} // namespace biofuel::engine::custom::procedural::hand
