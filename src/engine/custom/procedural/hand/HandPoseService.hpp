#pragma once

#include "engine/core/Types.hpp"
#include "engine/custom/procedural/hand/HandTrackingRetarget.hpp"

namespace biofuel::engine::custom::procedural::hand {

// Persistent user-facing hand pose coordinator.
//
// HandTrackingService owns camera/worker IPC and preview frames. This service
// owns retargeting/calibration state so a calibration performed by one screen
// can be consumed by later screens without copying tool-screen session-local
// retargeter.
class HandPoseService final {
public:
    using MappingState = ::biofuel::engine::custom::procedural::pose::MappingState;
    using MirrorPolicy = ::biofuel::engine::custom::procedural::pose::MirrorPolicy;
    using StageLayoutPolicy = ::biofuel::engine::custom::procedural::pose::StageLayoutPolicy;
    using CalibrationWizardState = ::biofuel::engine::custom::procedural::pose::CalibrationWizardState;

    void init() noexcept {}
    void shutdown() noexcept;
    void update(f32 dt) noexcept;

    void beginSession(u16 cameraWidth, u16 cameraHeight,
        MirrorPolicy mirror = MirrorPolicy::Selfie,
        StageLayoutPolicy layout = StageLayoutPolicy::Adaptive) noexcept;
    void startCalibration() noexcept;
    void resetCalibration() noexcept;
    void resetTracking() noexcept;
    void cancelCalibration() noexcept;

    [[nodiscard]] bool featureEnabled() const noexcept { return m_featureEnabled; }
    [[nodiscard]] bool calibrationActive() const noexcept { return m_retargeter.calibrationState().active; }
    [[nodiscard]] bool calibrationValid() const noexcept { return m_retargeter.calibrationValid(); }
    [[nodiscard]] MappingState mappingState() const noexcept { return m_mapped.mappingState; }
    [[nodiscard]] const CalibrationWizardState& calibrationState() const noexcept { return m_retargeter.calibrationState(); }
    [[nodiscard]] const MappedTrackedHands& mapped() const noexcept { return m_mapped; }
    [[nodiscard]] const TrackedRobotHandPose& leftPose() const noexcept { return m_mapped.leftPose; }
    [[nodiscard]] const TrackedRobotHandPose& rightPose() const noexcept { return m_mapped.rightPose; }
    [[nodiscard]] f32 secondsSinceMappedFrame() const noexcept { return m_secondsSinceMappedFrame; }
    [[nodiscard]] u64 lastFrameSequence() const noexcept { return m_lastFrameSequence; }

private:
    HandTrackingRetargeter m_retargeter{};
    MappedTrackedHands m_mapped{};
    bool m_featureEnabled = false;
    f32 m_secondsSinceMappedFrame = 999.0f;
    u64 m_lastFrameSequence = 0U;
    u64 m_lastFrameTimestampMs = 0U;
};

} // namespace biofuel::engine::custom::procedural::hand
