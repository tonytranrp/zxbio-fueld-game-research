#include "engine/custom/procedural/hand/HandPoseService.hpp"

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
#include "engine/runtime/Runtime.hpp"
#endif

#include <algorithm>

namespace biofuel::engine::custom::procedural::hand {

void HandPoseService::shutdown() noexcept {
    cancelCalibration();
    resetTracking();
    m_mapped = {};
    m_featureEnabled = false;
    m_secondsSinceMappedFrame = 999.0f;
    m_lastFrameSequence = 0U;
}

void HandPoseService::update(const f32 dt) noexcept {
    const f32 safeDt = std::min(std::max(dt, 0.0f), 0.05f);
    m_secondsSinceMappedFrame += safeDt;

#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    auto& tracking = ::biofuel::engine::runtime::Runtime::handTracking();
    m_featureEnabled = tracking.featureEnabled();
    if (!m_featureEnabled) {
        m_mapped.leftPose.valid = false;
        m_mapped.rightPose.valid = false;
        m_mapped.mappingState = calibrationActive() ? MappingState::Calibrating : MappingState::Idle;
        return;
    }

    const auto frame = tracking.latestFrame();
    if (!frame) {
        m_mapped.leftPose.valid = false;
        m_mapped.rightPose.valid = false;
        m_mapped.mappingState = calibrationActive() ? MappingState::Calibrating : MappingState::Idle;
        return;
    }

    if (frame->cameraWidth > 0U && frame->cameraHeight > 0U) {
        beginSession(frame->cameraWidth, frame->cameraHeight);
    }

    m_mapped = m_retargeter.map(*frame, safeDt);
    m_lastFrameSequence = frame->sequence;
    m_secondsSinceMappedFrame = 0.0f;
#else
    (void)safeDt;
    m_featureEnabled = false;
    m_mapped.leftPose.valid = false;
    m_mapped.rightPose.valid = false;
    m_mapped.mappingState = MappingState::Idle;
#endif
}

void HandPoseService::beginSession(
    const u16 cameraWidth,
    const u16 cameraHeight,
    const MirrorPolicy mirror,
    const StageLayoutPolicy layout) noexcept
{
    if (cameraWidth == 0U || cameraHeight == 0U) {
        return;
    }
    m_retargeter.beginSession(cameraWidth, cameraHeight, mirror, layout);
    m_mapped = m_retargeter.mappedHands();
}

void HandPoseService::startCalibration() noexcept {
    m_retargeter.resetCalibration();
    m_retargeter.resetTracking();
    m_retargeter.startCalibration();
    m_mapped = m_retargeter.mappedHands();
}

void HandPoseService::resetCalibration() noexcept {
    m_retargeter.resetCalibration();
    m_mapped = m_retargeter.mappedHands();
}

void HandPoseService::resetTracking() noexcept {
    m_retargeter.resetTracking();
    m_mapped = m_retargeter.mappedHands();
}

void HandPoseService::cancelCalibration() noexcept {
    m_retargeter.cancelCalibration();
    m_mapped = m_retargeter.mappedHands();
}

} // namespace biofuel::engine::custom::procedural::hand
