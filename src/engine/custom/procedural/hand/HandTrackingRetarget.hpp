#pragma once

#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/hand/TrackedRobotHand.hpp"
#include "engine/custom/procedural/pose/TrackedPoseMapping.hpp"
#include "engine/vision/hand_tracking/HandTrackingTypes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <raylib.h>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::hand {

struct HandTrackingRetargetSettings {
    ::biofuel::engine::custom::procedural::pose::MirrorPolicy mirrorPolicy =
        ::biofuel::engine::custom::procedural::pose::MirrorPolicy::Selfie;
    ::biofuel::engine::custom::procedural::pose::StageLayoutPolicy layoutPolicy =
        ::biofuel::engine::custom::procedural::pose::StageLayoutPolicy::Adaptive;
    f32 targetPalmLength = 0.20f;
    f32 minimumScale = 0.75f;
    f32 maximumScale = 3.20f;
    f32 smoothingResponse = 18.0f;
    f32 ambiguousSmoothingResponse = 8.0f;
    f32 fastSmoothingResponse = 34.0f;
    f32 poseFastMotionDistance = 0.18f;
    f32 trackingDropoutGraceSeconds = 0.14f;
    f32 stageDepthResponse = 1.55f;
    f32 stageDepthMargin = 0.035f;
    f32 absoluteLandmarkProjection = 0.72f;
    f32 cameraThroughDepthScale = 1.18f;
    f32 maximumLocalDepth = 0.22f;
    f32 minimumPalmSeparation = 0.012f;
    f32 minimumTrackingConfidence = 0.18f;
    f32 minimumCalibrationConfidence = 0.30f;
    f32 handednessLockConfidence = 0.58f;
    f32 calibrationPositionJitter = 0.036f;
    f32 calibrationSpanJitter = 0.036f;
    f32 adaptiveCalibrationResponse = 1.35f;
    f32 adaptiveCalibrationEdgeMargin = 0.035f;
    f32 adaptiveCalibrationMinimumConfidence = 0.62f;
    ::biofuel::engine::custom::procedural::pose::PoseBounds visibleBounds{
        .min = Vector3{-0.82f, -0.24f, -0.46f},
        .max = Vector3{0.82f, 0.56f, 0.46f},
    };
};

struct MappedTrackedHands {
    TrackedRobotHandPose leftPose{};
    TrackedRobotHandPose rightPose{};
    ::biofuel::engine::custom::procedural::pose::MappingState mappingState =
        ::biofuel::engine::custom::procedural::pose::MappingState::Idle;
    ::biofuel::engine::custom::procedural::pose::CalibrationWizardState calibrationState{};
    ::biofuel::engine::custom::procedural::pose::PoseBounds visibleStageBounds{};
};

template<usize... TIndices>
struct HandLandmarkSet {
    inline static constexpr std::array<usize, sizeof...(TIndices)> indices{{TIndices...}};
    static constexpr usize count = sizeof...(TIndices);
};

using PalmLandmarks = HandLandmarkSet<0U, 5U, 9U, 13U, 17U>;

struct ImageHandMetrics {
    Vector2 palmCenter{0.0f, 0.0f};
    f32 palmLength = 0.0f;
    f32 palmSpan = 0.0f;
    f32 palmDepth = 0.0f;
};

class TrackedPoseMapper final {
    struct CalibrationSampleAccumulator {
        Vector2 palmSum{0.0f, 0.0f};
        f32 spanSum = 0.0f;
        u32 count = 0U;
        Vector2 lastPalm{0.0f, 0.0f};
        f32 lastPalmSpan = 0.0f;
        bool hasLast = false;
    };

    struct HandCalibrationSlot {
        ::biofuel::engine::custom::procedural::pose::CalibrationSessionProfile profile{};
        ::biofuel::engine::custom::procedural::pose::CalibrationHandProgress progress{};
        CalibrationSampleAccumulator sample{};
    };

    struct CalibrationHandSelection {
        const ::biofuel::engine::vision::hand_tracking::HandTrackingHand* left = nullptr;
        const ::biofuel::engine::vision::hand_tracking::HandTrackingHand* right = nullptr;
    };

    struct CalibrationCandidate {
        const ::biofuel::engine::vision::hand_tracking::HandTrackingHand* hand = nullptr;
        HandSide side = HandSide::Left;
        f32 displayX = 0.0f;
    };

public:
    using MirrorPolicy = ::biofuel::engine::custom::procedural::pose::MirrorPolicy;
    using StageLayoutPolicy = ::biofuel::engine::custom::procedural::pose::StageLayoutPolicy;
    using CalibrationWizardState = ::biofuel::engine::custom::procedural::pose::CalibrationWizardState;
    using CalibrationWizardStep = ::biofuel::engine::custom::procedural::pose::CalibrationWizardStep;
    using CalibrationHandPhase = ::biofuel::engine::custom::procedural::pose::CalibrationHandPhase;
    using CalibrationHandProgress = ::biofuel::engine::custom::procedural::pose::CalibrationHandProgress;
    using CalibrationSessionProfile = ::biofuel::engine::custom::procedural::pose::CalibrationSessionProfile;
    using CameraFrameSpace = ::biofuel::engine::custom::procedural::pose::CameraFrameSpace;
    using MappingState = ::biofuel::engine::custom::procedural::pose::MappingState;
    using StageVolume = ::biofuel::engine::custom::procedural::pose::StageVolume;
    using PoseBounds = ::biofuel::engine::custom::procedural::pose::PoseBounds;

    void resetCalibration() noexcept {
        const CameraFrameSpace frameSpace = m_frameSpace;
        m_calibration = CalibrationSessionProfile{};
        m_calibration.frameSpace = frameSpace;
        m_leftCalibration = HandCalibrationSlot{};
        m_rightCalibration = HandCalibrationSlot{};
        m_leftCalibration.profile.frameSpace = frameSpace;
        m_rightCalibration.profile.frameSpace = frameSpace;
        m_wizard = CalibrationWizardState{};
        resetCalibrationAccumulator();
    }

    void resetTracking() noexcept {
        m_results.leftPose.valid = false;
        m_results.rightPose.valid = false;
        m_results.mappingState = m_wizard.active ? MappingState::Calibrating : MappingState::Idle;
        m_leftMissingSeconds = 0.0f;
        m_rightMissingSeconds = 0.0f;
    }

    void setSettings(const HandTrackingRetargetSettings settings) noexcept {
        m_settings = settings;
    }

    [[nodiscard]] const HandTrackingRetargetSettings& settings() const noexcept {
        return m_settings;
    }

    [[nodiscard]] const CalibrationSessionProfile& calibrationProfile() const noexcept {
        return m_calibration;
    }

    [[nodiscard]] const CalibrationSessionProfile& calibrationProfile(const HandSide side) const noexcept {
        return side == HandSide::Left ? m_leftCalibration.profile : m_rightCalibration.profile;
    }

    [[nodiscard]] const CalibrationWizardState& calibrationState() const noexcept {
        return m_wizard;
    }

    [[nodiscard]] bool calibrationValid() const noexcept {
        return m_calibration.valid && m_leftCalibration.profile.valid && m_rightCalibration.profile.valid;
    }

    [[nodiscard]] const MappedTrackedHands& mappedHands() const noexcept {
        return m_results;
    }

    [[nodiscard]] const TrackedRobotHandPose& leftPose() const noexcept {
        return m_results.leftPose;
    }

    [[nodiscard]] const TrackedRobotHandPose& rightPose() const noexcept {
        return m_results.rightPose;
    }

    void beginSession(
        const u16 cameraWidth,
        const u16 cameraHeight,
        const MirrorPolicy mirrorPolicy,
        const StageLayoutPolicy layoutPolicy) noexcept
    {
        const bool changed = !m_frameSpace.valid()
            || m_frameSpace.width != cameraWidth
            || m_frameSpace.height != cameraHeight
            || m_frameSpace.mirror != mirrorPolicy;

        m_frameSpace = CameraFrameSpace{
            .width = cameraWidth,
            .height = cameraHeight,
            .mirror = mirrorPolicy,
        };
        m_settings.mirrorPolicy = mirrorPolicy;
        m_settings.layoutPolicy = layoutPolicy;
        m_calibration.frameSpace = m_frameSpace;
        if (!m_leftCalibration.profile.frameSpace.valid() || changed) {
            m_leftCalibration.profile.frameSpace = m_frameSpace;
        }
        if (!m_rightCalibration.profile.frameSpace.valid() || changed) {
            m_rightCalibration.profile.frameSpace = m_frameSpace;
        }

        if (!changed || !m_sessionStarted) {
            m_sessionStarted = true;
            return;
        }

        resetCalibration();
        startCalibration();
        m_wizard.cameraChanged = true;
        m_sessionStarted = true;
    }

    void startCalibration() noexcept {
        const CameraFrameSpace frameSpace = m_frameSpace;
        m_calibration = CalibrationSessionProfile{};
        m_calibration.frameSpace = frameSpace;
        m_leftCalibration = HandCalibrationSlot{};
        m_rightCalibration = HandCalibrationSlot{};
        m_leftCalibration.profile.frameSpace = frameSpace;
        m_rightCalibration.profile.frameSpace = frameSpace;
        m_wizard = CalibrationWizardState{
            .active = true,
            .cameraChanged = false,
            .activeHand = CalibrationHandPhase::Left,
            .step = CalibrationWizardStep::Center,
            .holdSeconds = 0.0f,
            .requiredHoldSeconds =
                ::biofuel::engine::custom::procedural::pose::calibrationRequiredHoldSeconds(
                    CalibrationWizardStep::Center),
            .targetAcquired = false,
            .targetError = 1.0f,
        };
        resetCalibrationAccumulator();
        resetTracking();
    }

    void cancelCalibration() noexcept {
        m_wizard = CalibrationWizardState{};
    }

    void finishCalibration() noexcept {
        ::biofuel::engine::custom::procedural::pose::sanitizeCalibrationProfile(m_leftCalibration.profile);
        ::biofuel::engine::custom::procedural::pose::sanitizeCalibrationProfile(m_rightCalibration.profile);
        m_leftCalibration.profile.valid = true;
        m_rightCalibration.profile.valid = true;
        m_calibration = combinedCalibrationProfile();
        ::biofuel::engine::custom::procedural::pose::sanitizeCalibrationProfile(m_calibration);
        m_calibration.valid = true;
        m_wizard.active = false;
        m_wizard.activeHand = CalibrationHandPhase::Complete;
        m_wizard.step = CalibrationWizardStep::Complete;
        m_wizard.holdSeconds = m_wizard.requiredHoldSeconds;
        m_wizard.targetAcquired = true;
        m_leftCalibration.progress.sampleCaptured = true;
        m_rightCalibration.progress.sampleCaptured = true;
        syncWizardProgress();
        m_results.mappingState = MappingState::Ready;
    }

    [[nodiscard]] ::biofuel::engine::vision::hand_tracking::HandTrackingLandmark displayLandmark(
        const ::biofuel::engine::vision::hand_tracking::HandTrackingLandmark landmark) const noexcept
    {
        auto normalized = landmark.toNormalizedCameraCoord();
        if (m_frameSpace.mirror == MirrorPolicy::Selfie) {
            normalized.x = 1.0f - normalized.x;
        }
        return ::biofuel::engine::vision::hand_tracking::HandTrackingLandmark::fromNormalizedCameraCoord(
            normalized);
    }

    [[nodiscard]] MappedTrackedHands map(
        const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame,
        const f32 dt) noexcept
    {
        if (frame.cameraWidth > 0U && frame.cameraHeight > 0U) {
            beginSession(frame.cameraWidth, frame.cameraHeight, m_settings.mirrorPolicy, m_settings.layoutPolicy);
        }

        const TrackedRobotHandPose previousLeft = m_results.leftPose;
        const TrackedRobotHandPose previousRight = m_results.rightPose;
        m_results.leftPose.valid = false;
        m_results.rightPose.valid = false;
        m_results.visibleStageBounds = m_settings.visibleBounds;
        m_results.calibrationState = m_wizard;

        if (!frame.valid) {
            m_results.mappingState = m_wizard.active ? MappingState::Calibrating : MappingState::Idle;
            return m_results;
        }

        if (m_wizard.active) {
            updateCalibration(frame, dt);
            m_results.mappingState = MappingState::Calibrating;
            m_results.calibrationState = m_wizard;
            return m_results;
        }

        if (!m_calibration.valid) {
            m_results.mappingState = MappingState::Idle;
            return m_results;
        }

        const CalibrationHandSelection selection = trackedHands(frame);
        const f32 safeDt = std::min(std::max(dt, 0.0f), 0.05f);
        const bool retainLeft = canRetainPose(selection.left, previousLeft, m_leftMissingSeconds, safeDt);
        const bool retainRight = canRetainPose(selection.right, previousRight, m_rightMissingSeconds, safeDt);
        const usize activeHands = selectedHandCount(selection) + (retainLeft ? 1U : 0U) + (retainRight ? 1U : 0U);
        const StageVolume stageVolume =
            ::biofuel::engine::custom::procedural::pose::makeStageVolume(
                m_settings.visibleBounds,
                m_settings.layoutPolicy,
                activeHands);
        m_results.visibleStageBounds = stageVolume.full;

        if (selection.left != nullptr) {
            refineCalibrationProfile(HandSide::Left, *selection.left, safeDt);
            const PoseBounds bounds = resolveBoundsForHand(stageVolume, HandSide::Left, activeHands);
            m_results.leftPose = buildPose(*selection.left, HandSide::Left, bounds);
            m_leftMissingSeconds = 0.0f;
        } else {
            retainMissingPose(m_results.leftPose, previousLeft, m_leftMissingSeconds, safeDt);
        }
        if (selection.right != nullptr) {
            refineCalibrationProfile(HandSide::Right, *selection.right, safeDt);
            const PoseBounds bounds = resolveBoundsForHand(stageVolume, HandSide::Right, activeHands);
            m_results.rightPose = buildPose(*selection.right, HandSide::Right, bounds);
            m_rightMissingSeconds = 0.0f;
        } else {
            retainMissingPose(m_results.rightPose, previousRight, m_rightMissingSeconds, safeDt);
        }

        if (m_results.leftPose.valid && previousLeft.valid) {
            smoothPoseToward(m_results.leftPose, previousLeft, safeDt);
        }
        if (m_results.rightPose.valid && previousRight.valid) {
            smoothPoseToward(m_results.rightPose, previousRight, safeDt);
        }

        separateHands();
        fitVisible(m_results.leftPose, resolveBoundsForHand(stageVolume, HandSide::Left, activeHands));
        fitVisible(m_results.rightPose, resolveBoundsForHand(stageVolume, HandSide::Right, activeHands));
        m_results.mappingState = MappingState::Ready;
        m_results.calibrationState = m_wizard;
        return m_results;
    }

private:
    using HandTrackingHand = ::biofuel::engine::vision::hand_tracking::HandTrackingHand;
    using HandTrackingHandedness = ::biofuel::engine::vision::hand_tracking::HandTrackingHandedness;
    using HandTrackingLandmark = ::biofuel::engine::vision::hand_tracking::HandTrackingLandmark;

    [[nodiscard]] static Vector2 add2(const Vector2 a, const Vector2 b) noexcept {
        return Vector2{a.x + b.x, a.y + b.y};
    }

    [[nodiscard]] static Vector2 scale2(const Vector2 value, const f32 scale) noexcept {
        return Vector2{value.x * scale, value.y * scale};
    }

    [[nodiscard]] static f32 distance2(const Vector2 a, const Vector2 b) noexcept {
        const f32 dx = a.x - b.x;
        const f32 dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    [[nodiscard]] static Vector2 imagePoint(const HandTrackingLandmark landmark) noexcept {
        return Vector2{landmark.x, landmark.y};
    }

    [[nodiscard]] Vector2 displayPoint(const HandTrackingLandmark landmark) const noexcept {
        const HandTrackingLandmark mirrored = displayLandmark(landmark);
        return Vector2{mirrored.x, mirrored.y};
    }

    template<typename TLandmarkSet>
    [[nodiscard]] Vector2 displayCentroid(const HandTrackingHand& hand) const noexcept {
        Vector2 sum{0.0f, 0.0f};
        for (const usize index : TLandmarkSet::indices) {
            sum = add2(sum, displayPoint(hand.imageLandmarks[index]));
        }
        return scale2(sum, 1.0f / static_cast<f32>(TLandmarkSet::count));
    }

    [[nodiscard]] Vector2 imagePalmCenter(const HandTrackingHand& hand) const noexcept {
        return displayCentroid<PalmLandmarks>(hand);
    }

    [[nodiscard]] static f32 imagePalmSpan(const HandTrackingHand& hand) noexcept {
        const f32 wristToMiddle = distance2(imagePoint(hand.imageLandmarks[0]), imagePoint(hand.imageLandmarks[9]));
        const f32 indexToPinky = distance2(imagePoint(hand.imageLandmarks[5]), imagePoint(hand.imageLandmarks[17]));
        return std::max(std::max(wristToMiddle, indexToPinky * 0.82f), 0.025f);
    }

    [[nodiscard]] static f32 imagePalmLength(const HandTrackingHand& hand) noexcept {
        return std::max(distance2(imagePoint(hand.imageLandmarks[0]), imagePoint(hand.imageLandmarks[9])), 0.025f);
    }

    [[nodiscard]] static f32 imagePalmDepth(const HandTrackingHand& hand) noexcept {
        f32 sum = 0.0f;
        for (const usize index : PalmLandmarks::indices) {
            sum += hand.imageLandmarks[index].z;
        }
        return sum / static_cast<f32>(PalmLandmarks::count);
    }

    [[nodiscard]] ImageHandMetrics imageMetrics(const HandTrackingHand& hand) const noexcept {
        return ImageHandMetrics{
            .palmCenter = imagePalmCenter(hand),
            .palmLength = imagePalmLength(hand),
            .palmSpan = imagePalmSpan(hand),
            .palmDepth = imagePalmDepth(hand),
        };
    }

    [[nodiscard]] static Vector3 posePalmCenter(const TrackedRobotHandPose& pose) noexcept {
        constexpr std::array<usize, 5U> palmIndices{{0U, 5U, 9U, 13U, 17U}};
        return ::biofuel::engine::custom::procedural::pose::poseWeightedCenter(
            pose.landmarks,
            palmIndices);
    }

    void updateCalibration(
        const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame,
        const f32 dt) noexcept
    {
        const CalibrationHandSelection selection = calibrationHands(frame);
        HandCalibrationSlot& activeSlot = activeCalibrationSlot();
        const HandTrackingHand* activeHand = calibrationHandForPhase(selection, m_wizard.activeHand);
        updateCalibrationSlot(activeSlot, activeHand, dt);
        syncWizardProgress();

        if (!activeSlot.progress.sampleCaptured) {
            return;
        }

        const CalibrationWizardStep next =
            ::biofuel::engine::custom::procedural::pose::nextCalibrationStep(m_wizard.step);
        if (next != CalibrationWizardStep::Complete) {
            m_wizard.step = next;
            m_wizard.requiredHoldSeconds =
                ::biofuel::engine::custom::procedural::pose::calibrationRequiredHoldSeconds(m_wizard.step);
            resetCalibrationStepAccumulator(activeSlot);
            syncWizardProgress();
            return;
        }

        activeSlot.profile.valid = true;
        ::biofuel::engine::custom::procedural::pose::sanitizeCalibrationProfile(activeSlot.profile);
        const CalibrationHandPhase nextPhase =
            ::biofuel::engine::custom::procedural::pose::nextCalibrationHandPhase(m_wizard.activeHand);
        if (nextPhase == CalibrationHandPhase::Complete) {
            finishCalibration();
            return;
        }

        m_wizard.activeHand = nextPhase;
        m_wizard.step = CalibrationWizardStep::Center;
        m_wizard.requiredHoldSeconds =
            ::biofuel::engine::custom::procedural::pose::calibrationRequiredHoldSeconds(m_wizard.step);
        resetCalibrationStepAccumulator(activeCalibrationSlot());
        syncWizardProgress();
    }

    [[nodiscard]] HandCalibrationSlot& activeCalibrationSlot() noexcept {
        return m_wizard.activeHand == CalibrationHandPhase::Right
            ? m_rightCalibration
            : m_leftCalibration;
    }

    [[nodiscard]] const HandCalibrationSlot& activeCalibrationSlot() const noexcept {
        return m_wizard.activeHand == CalibrationHandPhase::Right
            ? m_rightCalibration
            : m_leftCalibration;
    }

    [[nodiscard]] static const HandTrackingHand* calibrationHandForPhase(
        const CalibrationHandSelection selection,
        const CalibrationHandPhase phase) noexcept
    {
        if (phase == CalibrationHandPhase::Right) {
            return selection.right;
        }
        if (phase == CalibrationHandPhase::Left) {
            return selection.left;
        }
        return nullptr;
    }

    [[nodiscard]] bool validCalibrationHand(const HandTrackingHand& hand) const noexcept {
        if (!validTrackedHand(hand) || hand.handednessScore < m_settings.minimumCalibrationConfidence) {
            return false;
        }

        const f32 palmSpan = imageMetrics(hand).palmSpan;
        if (palmSpan < 0.035f || palmSpan > 0.68f) {
            return false;
        }

        for (const usize index : PalmLandmarks::indices) {
            const HandTrackingLandmark landmark = hand.imageLandmarks[index];
            if (landmark.x < -0.18f || landmark.x > 1.18f || landmark.y < -0.18f || landmark.y > 1.18f) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool validTrackedHand(const HandTrackingHand& hand) const noexcept {
        if (!hand.valid || hand.handednessScore < m_settings.minimumTrackingConfidence) {
            return false;
        }
        for (const usize index : PalmLandmarks::indices) {
            const HandTrackingLandmark landmark = hand.imageLandmarks[index];
            if (!std::isfinite(landmark.x) || !std::isfinite(landmark.y) || !std::isfinite(landmark.z)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] f32 calibrationTargetError(
        const CalibrationWizardStep step,
        const CalibrationSessionProfile& profile,
        const Vector2 palm,
        const f32 palmSpan) const noexcept
    {
        const Vector2 target = ::biofuel::engine::custom::procedural::pose::calibrationTarget(step);
        const f32 positionRadius =
            ::biofuel::engine::custom::procedural::pose::calibrationTargetRadius(step);
        f32 error = distance2(palm, target) / positionRadius;

        if (step == CalibrationWizardStep::Near) {
            const f32 requiredSpan = std::max(profile.referencePalmSpan * 1.18f, profile.referencePalmSpan + 0.030f);
            if (palmSpan < requiredSpan) {
                error = std::max(error, 1.0f + (requiredSpan - palmSpan) / std::max(requiredSpan, 0.01f));
            }
        } else if (step == CalibrationWizardStep::Far) {
            const f32 requiredSpan = std::min(profile.referencePalmSpan * 0.84f, profile.nearPalmSpan * 0.78f);
            if (palmSpan > requiredSpan) {
                error = std::max(error, 1.0f + (palmSpan - requiredSpan) / std::max(requiredSpan, 0.01f));
            }
        }

        return error;
    }

    [[nodiscard]] CalibrationHandSelection calibrationHands(
        const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame) const noexcept
    {
        std::array<CalibrationCandidate, 2U> candidates{};
        usize count = 0U;
        for (const auto& hand : frame.hands) {
            if (!validCalibrationHand(hand) || count >= candidates.size()) {
                continue;
            }
            candidates[count] = CalibrationCandidate{
                .hand = &hand,
                .side = resolveSide(hand),
                .displayX = imagePalmCenter(hand).x,
            };
            ++count;
        }
        return selectHandCandidates(candidates, count);
    }

    [[nodiscard]] CalibrationHandSelection trackedHands(
        const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame) const noexcept
    {
        std::array<CalibrationCandidate, 2U> candidates{};
        usize count = 0U;
        for (const auto& hand : frame.hands) {
            if (!validTrackedHand(hand) || count >= candidates.size()) {
                continue;
            }
            candidates[count] = CalibrationCandidate{
                .hand = &hand,
                .side = resolveSide(hand),
                .displayX = imagePalmCenter(hand).x,
            };
            ++count;
        }
        return selectHandCandidates(candidates, count);
    }

    [[nodiscard]] static usize selectedHandCount(const CalibrationHandSelection selection) noexcept {
        return (selection.left != nullptr ? 1U : 0U) + (selection.right != nullptr ? 1U : 0U);
    }

    [[nodiscard]] bool canRetainPose(
        const HandTrackingHand* observedHand,
        const TrackedRobotHandPose& previousPose,
        const f32 missingSeconds,
        const f32 dt) const noexcept
    {
        return observedHand == nullptr
            && previousPose.valid
            && missingSeconds + dt <= m_settings.trackingDropoutGraceSeconds;
    }

    void retainMissingPose(
        TrackedRobotHandPose& pose,
        const TrackedRobotHandPose& previousPose,
        f32& missingSeconds,
        const f32 dt) const noexcept
    {
        missingSeconds = std::min(m_settings.trackingDropoutGraceSeconds + dt, missingSeconds + dt);
        if (!previousPose.valid || missingSeconds > m_settings.trackingDropoutGraceSeconds) {
            return;
        }

        pose = previousPose;
        const f32 remaining =
            m_settings.trackingDropoutGraceSeconds <= 0.0f
                ? 0.0f
                : 1.0f - missingSeconds / m_settings.trackingDropoutGraceSeconds;
        pose.confidence *= std::clamp(remaining, 0.0f, 1.0f);
    }

    [[nodiscard]] static CalibrationHandSelection selectHandCandidates(
        const std::array<CalibrationCandidate, 2U>& candidates,
        const usize count) noexcept
    {
        CalibrationHandSelection selection{};
        if (count == 0U) {
            return selection;
        }
        if (count == 1U) {
            if (candidates[0].side == HandSide::Left) {
                selection.left = candidates[0].hand;
            } else {
                selection.right = candidates[0].hand;
            }
            return selection;
        }

        if (candidates[0].side != candidates[1].side) {
            selection.left = candidates[0].side == HandSide::Left ? candidates[0].hand : candidates[1].hand;
            selection.right = candidates[0].side == HandSide::Right ? candidates[0].hand : candidates[1].hand;
            return selection;
        }

        const CalibrationCandidate& screenLeft = candidates[0].displayX <= candidates[1].displayX
            ? candidates[0]
            : candidates[1];
        const CalibrationCandidate& screenRight = candidates[0].displayX <= candidates[1].displayX
            ? candidates[1]
            : candidates[0];
        selection.left = screenLeft.hand;
        selection.right = screenRight.hand;
        return selection;
    }

    void updateCalibrationSlot(
        HandCalibrationSlot& slot,
        const HandTrackingHand* hand,
        const f32 dt) noexcept
    {
        slot.progress.requiredHoldSeconds = m_wizard.requiredHoldSeconds;
        if (slot.progress.sampleCaptured) {
            slot.progress.detected = hand != nullptr;
            slot.progress.targetAcquired = true;
            slot.progress.holdSeconds = m_wizard.requiredHoldSeconds;
            slot.progress.targetError = 0.0f;
            slot.progress.status =
                ::biofuel::engine::custom::procedural::pose::CalibrationCaptureStatus::Captured;
            return;
        }

        if (hand == nullptr) {
            resetSlotAccumulator(slot);
            slot.progress.detected = false;
            slot.progress.targetAcquired = false;
            slot.progress.targetError = 1.0f;
            slot.progress.status =
                ::biofuel::engine::custom::procedural::pose::CalibrationCaptureStatus::Missing;
            return;
        }

        slot.progress.detected = true;
        const ImageHandMetrics metrics = imageMetrics(*hand);
        const f32 targetError = calibrationTargetError(m_wizard.step, slot.profile, metrics.palmCenter, metrics.palmSpan);
        const bool stable = calibrationSampleStable(slot, metrics.palmCenter, metrics.palmSpan);
        slot.progress.targetError = targetError;

        if (targetError > 1.0f) {
            resetSlotAccumulator(slot);
            slot.progress.targetAcquired = false;
            slot.progress.status =
                ::biofuel::engine::custom::procedural::pose::CalibrationCaptureStatus::OutsideTarget;
            return;
        }
        if (!stable) {
            resetSlotAccumulator(slot);
            slot.progress.targetAcquired = false;
            slot.progress.status =
                ::biofuel::engine::custom::procedural::pose::CalibrationCaptureStatus::Unstable;
            return;
        }

        slot.progress.targetAcquired = true;
        slot.progress.status =
            ::biofuel::engine::custom::procedural::pose::CalibrationCaptureStatus::Capturing;
        const f32 safeDt = std::min(std::max(dt, 0.0f), 0.05f);
        slot.progress.holdSeconds = std::min(m_wizard.requiredHoldSeconds, slot.progress.holdSeconds + safeDt);
        accumulateCalibrationSample(slot, metrics.palmCenter, metrics.palmSpan);

        if (slot.progress.holdSeconds < m_wizard.requiredHoldSeconds) {
            return;
        }

        const Vector2 capturedPalm = slot.sample.count > 0U
            ? scale2(slot.sample.palmSum, 1.0f / static_cast<f32>(slot.sample.count))
            : metrics.palmCenter;
        const f32 capturedSpan = slot.sample.count > 0U
            ? slot.sample.spanSum / static_cast<f32>(slot.sample.count)
            : metrics.palmSpan;
        captureCalibrationSample(slot, capturedPalm, capturedSpan);
        slot.progress.sampleCaptured = true;
        slot.progress.targetAcquired = true;
        slot.progress.status =
            ::biofuel::engine::custom::procedural::pose::CalibrationCaptureStatus::Captured;
        slot.progress.holdSeconds = m_wizard.requiredHoldSeconds;
    }

    [[nodiscard]] bool calibrationSampleStable(
        HandCalibrationSlot& slot,
        const Vector2 palm,
        const f32 palmSpan) noexcept
    {
        const bool stable = !slot.sample.hasLast
            || (distance2(palm, slot.sample.lastPalm) <= m_settings.calibrationPositionJitter
                && std::fabs(palmSpan - slot.sample.lastPalmSpan) <= m_settings.calibrationSpanJitter);
        slot.sample.lastPalm = palm;
        slot.sample.lastPalmSpan = palmSpan;
        slot.sample.hasLast = true;
        return stable;
    }

    void accumulateCalibrationSample(HandCalibrationSlot& slot, const Vector2 palm, const f32 palmSpan) noexcept {
        slot.sample.palmSum = add2(slot.sample.palmSum, palm);
        slot.sample.spanSum += palmSpan;
        ++slot.sample.count;
    }

    void resetCalibrationAccumulator() noexcept {
        m_wizard.holdSeconds = 0.0f;
        resetCalibrationStepAccumulator(m_leftCalibration);
        resetCalibrationStepAccumulator(m_rightCalibration);
    }

    void resetCalibrationStepAccumulator(HandCalibrationSlot& slot) noexcept {
        resetSlotAccumulator(slot);
        slot.progress = CalibrationHandProgress{};
        slot.progress.requiredHoldSeconds = m_wizard.requiredHoldSeconds;
    }

    static void resetSlotAccumulator(HandCalibrationSlot& slot) noexcept {
        slot.progress.holdSeconds = 0.0f;
        slot.sample = CalibrationSampleAccumulator{};
    }

    void captureCalibrationSample(HandCalibrationSlot& slot, const Vector2 palm, const f32 palmSpan) noexcept {
        switch (m_wizard.step) {
        case CalibrationWizardStep::Center:
            slot.profile.center = palm;
            slot.profile.referencePalmSpan = palmSpan;
            slot.profile.nearPalmSpan = std::max(palmSpan * 1.18f, palmSpan + 0.026f);
            slot.profile.farPalmSpan = std::max(palmSpan * 0.82f, 0.040f);
            break;
        case CalibrationWizardStep::Left:
            slot.profile.left = palm;
            break;
        case CalibrationWizardStep::Right:
            slot.profile.right = palm;
            break;
        case CalibrationWizardStep::Top:
            slot.profile.top = palm;
            break;
        case CalibrationWizardStep::Bottom:
            slot.profile.bottom = palm;
            break;
        case CalibrationWizardStep::TopLeft:
            slot.profile.topLeft = palm;
            break;
        case CalibrationWizardStep::TopRight:
            slot.profile.topRight = palm;
            break;
        case CalibrationWizardStep::BottomLeft:
            slot.profile.bottomLeft = palm;
            break;
        case CalibrationWizardStep::BottomRight:
            slot.profile.bottomRight = palm;
            break;
        case CalibrationWizardStep::Near:
            slot.profile.nearPalmSpan = palmSpan;
            break;
        case CalibrationWizardStep::Far:
            slot.profile.farPalmSpan = palmSpan;
            break;
        case CalibrationWizardStep::Inactive:
        case CalibrationWizardStep::Complete:
            break;
        default:
            break;
        }
    }

    void refineCalibrationProfile(
        const HandSide side,
        const HandTrackingHand& hand,
        const f32 dt) noexcept
    {
        if (hand.handednessScore < m_settings.adaptiveCalibrationMinimumConfidence) {
            return;
        }

        HandCalibrationSlot& slot = side == HandSide::Left ? m_leftCalibration : m_rightCalibration;
        if (!slot.profile.valid || !slot.profile.frameSpace.valid()) {
            return;
        }

        const ImageHandMetrics metrics = imageMetrics(hand);
        if (metrics.palmCenter.x < -0.08f || metrics.palmCenter.x > 1.08f
            || metrics.palmCenter.y < -0.08f || metrics.palmCenter.y > 1.08f) {
            return;
        }

        const f32 alpha = 1.0f - std::exp(-std::max(m_settings.adaptiveCalibrationResponse, 0.0f) * std::max(dt, 0.0f));
        const f32 margin = std::max(m_settings.adaptiveCalibrationEdgeMargin, 0.0f);
        bool changed = false;

        auto expandMin = [alpha, &changed](f32& edge, const f32 observed) noexcept {
            const f32 target = std::min(edge, observed);
            if (target < edge) {
                edge = edge + (target - edge) * alpha;
                changed = true;
            }
        };
        auto expandMax = [alpha, &changed](f32& edge, const f32 observed) noexcept {
            const f32 target = std::max(edge, observed);
            if (target > edge) {
                edge = edge + (target - edge) * alpha;
                changed = true;
            }
        };

        if (metrics.palmCenter.x <= slot.profile.left.x + margin) {
            expandMin(
                slot.profile.left.x,
                ::biofuel::engine::custom::procedural::pose::clamp01(metrics.palmCenter.x - margin * 0.35f));
        }
        if (metrics.palmCenter.x >= slot.profile.right.x - margin) {
            expandMax(
                slot.profile.right.x,
                ::biofuel::engine::custom::procedural::pose::clamp01(metrics.palmCenter.x + margin * 0.35f));
        }
        if (metrics.palmCenter.y <= slot.profile.top.y + margin) {
            expandMin(
                slot.profile.top.y,
                ::biofuel::engine::custom::procedural::pose::clamp01(metrics.palmCenter.y - margin * 0.35f));
        }
        if (metrics.palmCenter.y >= slot.profile.bottom.y - margin) {
            expandMax(
                slot.profile.bottom.y,
                ::biofuel::engine::custom::procedural::pose::clamp01(metrics.palmCenter.y + margin * 0.35f));
        }

        if (metrics.palmSpan >= slot.profile.nearPalmSpan * 0.94f) {
            expandMax(slot.profile.nearPalmSpan, std::min(metrics.palmSpan, 0.50f));
        }
        if (metrics.palmSpan <= slot.profile.farPalmSpan * 1.06f) {
            expandMin(slot.profile.farPalmSpan, std::max(metrics.palmSpan, 0.04f));
        }

        if (!changed) {
            return;
        }

        ::biofuel::engine::custom::procedural::pose::sanitizeCalibrationProfile(slot.profile);
        if (m_leftCalibration.profile.valid && m_rightCalibration.profile.valid) {
            m_calibration = combinedCalibrationProfile();
            ::biofuel::engine::custom::procedural::pose::sanitizeCalibrationProfile(m_calibration);
            m_calibration.valid = true;
        }
    }

    void syncWizardProgress() noexcept {
        m_wizard.left = m_leftCalibration.progress;
        m_wizard.right = m_rightCalibration.progress;
        const CalibrationHandProgress& active = activeCalibrationSlot().progress;
        m_wizard.requiredHoldSeconds = active.requiredHoldSeconds;
        m_wizard.holdSeconds = active.holdSeconds;
        m_wizard.targetAcquired = active.targetAcquired;
        m_wizard.targetError = active.targetError;
    }

    [[nodiscard]] CalibrationSessionProfile combinedCalibrationProfile() const noexcept {
        const auto average2 = [](const Vector2 a, const Vector2 b) noexcept {
            return Vector2{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
        };
        const CalibrationSessionProfile& left = m_leftCalibration.profile;
        const CalibrationSessionProfile& right = m_rightCalibration.profile;
        return CalibrationSessionProfile{
            .valid = left.valid && right.valid,
            .frameSpace = m_frameSpace,
            .center = average2(left.center, right.center),
            .left = average2(left.left, right.left),
            .right = average2(left.right, right.right),
            .top = average2(left.top, right.top),
            .bottom = average2(left.bottom, right.bottom),
            .topLeft = average2(left.topLeft, right.topLeft),
            .topRight = average2(left.topRight, right.topRight),
            .bottomLeft = average2(left.bottomLeft, right.bottomLeft),
            .bottomRight = average2(left.bottomRight, right.bottomRight),
            .nearPalmSpan = (left.nearPalmSpan + right.nearPalmSpan) * 0.5f,
            .farPalmSpan = (left.farPalmSpan + right.farPalmSpan) * 0.5f,
            .referencePalmSpan = (left.referencePalmSpan + right.referencePalmSpan) * 0.5f,
        };
    }

    [[nodiscard]] const CalibrationSessionProfile& calibrationProfileForSide(const HandSide side) const noexcept {
        const CalibrationSessionProfile& profile =
            side == HandSide::Left ? m_leftCalibration.profile : m_rightCalibration.profile;
        return profile.frameSpace.valid() ? profile : m_calibration;
    }

    void smoothPoseToward(
        TrackedRobotHandPose& pose,
        const TrackedRobotHandPose& previousPose,
        const f32 dt) const noexcept
    {
        if (!pose.valid || !previousPose.valid) {
            return;
        }

        const f32 motion = Vector3Distance(posePalmCenter(pose), posePalmCenter(previousPose));
        const f32 motionT = std::clamp(
            motion / std::max(m_settings.poseFastMotionDistance, 0.001f),
            0.0f,
            1.0f);
        f32 response = m_settings.ambiguousSmoothingResponse
            + (m_settings.fastSmoothingResponse - m_settings.ambiguousSmoothingResponse) * motionT;
        if (pose.confidence >= m_settings.handednessLockConfidence) {
            response = std::max(response, m_settings.smoothingResponse);
        }

        ::biofuel::engine::custom::procedural::pose::PoseStabilizer::apply(
            pose.landmarks,
            previousPose.landmarks,
            dt,
            response);
    }

    [[nodiscard]] TrackedRobotHandPose buildPose(
        const HandTrackingHand& hand,
        const HandSide side,
        const PoseBounds bounds) const noexcept
    {
        const CalibrationSessionProfile& sideProfile = calibrationProfileForSide(side);
        const CalibrationSessionProfile& spatialProfile = spatialCalibrationProfile();
        const ImageHandMetrics metrics = imageMetrics(hand);
        const f32 stageDepth = mapStageDepth(sideProfile, bounds, metrics);

        TrackedRobotHandPose pose{
            .valid = true,
            .side = side,
            .confidence = hand.handednessScore,
            .landmarks = {},
        };
        for (usize index = 0U; index < pose.landmarks.size(); ++index) {
            pose.landmarks[index] =
                mapImageLandmark(spatialProfile, sideProfile, bounds, stageDepth, metrics, hand.imageLandmarks[index]);
        }
        fitVisible(pose, bounds);
        return pose;
    }

    [[nodiscard]] const CalibrationSessionProfile& spatialCalibrationProfile() const noexcept {
        return m_calibration.valid ? m_calibration : m_leftCalibration.profile;
    }

    [[nodiscard]] f32 mapStageDepth(
        const CalibrationSessionProfile& profile,
        const PoseBounds bounds,
        const ImageHandMetrics metrics) const noexcept
    {
        const f32 depthMin = bounds.min.z + std::max(m_settings.stageDepthMargin, 0.0f);
        const f32 depthMax = bounds.max.z - std::max(m_settings.stageDepthMargin, 0.0f);
        const f32 rawT = ::biofuel::engine::custom::procedural::pose::remapClamped(
            metrics.palmSpan,
            profile.farPalmSpan,
            profile.nearPalmSpan,
            0.0f,
            1.0f);
        const f32 expandedT = std::clamp(
            0.5f + (rawT - 0.5f) * std::max(m_settings.stageDepthResponse, 0.01f),
            0.0f,
            1.0f);
        return depthMin + (depthMax - depthMin) * expandedT;
    }

    [[nodiscard]] f32 localLandmarkScale(
        const CalibrationSessionProfile& profile,
        const ImageHandMetrics metrics) const noexcept
    {
        const f32 rawScale = m_settings.targetPalmLength / std::max(metrics.palmLength, 0.001f);
        const f32 referenceScale = m_settings.targetPalmLength / std::max(profile.referencePalmSpan, 0.001f);
        const f32 minimumFactor = std::max(m_settings.minimumScale, 0.01f);
        const f32 maximumFactor = std::max(m_settings.maximumScale, minimumFactor);
        const f32 minimumScale = referenceScale * minimumFactor;
        const f32 maximumScale = referenceScale * maximumFactor;
        return std::clamp(rawScale, minimumScale, maximumScale);
    }

    [[nodiscard]] Vector3 mapPalmAnchor(
        const CalibrationSessionProfile& profile,
        const PoseBounds bounds,
        const f32 stageDepth,
        const Vector2 palmCenter) const noexcept
    {
        const auto warp =
            ::biofuel::engine::custom::procedural::pose::calibrationImageWarpAt(profile, palmCenter);
        const f32 x = ::biofuel::engine::custom::procedural::pose::remapUnclamped(
            palmCenter.x,
            warp.leftX,
            warp.rightX,
            bounds.min.x + 0.04f,
            bounds.max.x - 0.04f);
        const f32 y = ::biofuel::engine::custom::procedural::pose::remapUnclamped(
            palmCenter.y,
            warp.topY,
            warp.bottomY,
            bounds.max.y - 0.04f,
            bounds.min.y + 0.04f);
        return Vector3{x, y, stageDepth};
    }

    [[nodiscard]] Vector3 mapImageLandmark(
        const CalibrationSessionProfile& spatialProfile,
        const CalibrationSessionProfile& scaleProfile,
        const PoseBounds bounds,
        const f32 stageDepth,
        const ImageHandMetrics metrics,
        const HandTrackingLandmark value) const noexcept
    {
        const HandTrackingLandmark displayValue = displayLandmark(value);
        const Vector3 anchor = mapPalmAnchor(spatialProfile, bounds, stageDepth, metrics.palmCenter);
        const f32 localScale = localLandmarkScale(scaleProfile, metrics);
        const f32 aspectScale = m_frameSpace.valid() ? m_frameSpace.aspectRatio() : 1.0f;
        const f32 xOffset = (displayValue.x - metrics.palmCenter.x) * aspectScale * localScale;
        const f32 yOffset = -(displayValue.y - metrics.palmCenter.y) * localScale;
        const f32 maximumDepth = std::max(m_settings.maximumLocalDepth, 0.01f);
        const f32 zOffset = std::clamp(
            (displayValue.z - metrics.palmDepth) * localScale * m_settings.cameraThroughDepthScale,
            -maximumDepth,
            maximumDepth);
        const Vector3 local{anchor.x + xOffset, anchor.y + yOffset, anchor.z + zOffset};
        const Vector3 projected = mapPalmAnchor(
            spatialProfile,
            bounds,
            stageDepth,
            Vector2{displayValue.x, displayValue.y});
        const f32 projection = std::clamp(m_settings.absoluteLandmarkProjection, 0.0f, 1.0f);
        return Vector3{
            local.x + (projected.x - local.x) * projection,
            local.y + (projected.y - local.y) * projection,
            local.z,
        };
    }

    [[nodiscard]] HandSide resolveSide(const HandTrackingHand& hand) const noexcept {
        if (hand.handednessScore >= m_settings.handednessLockConfidence
            && hand.handedness == HandTrackingHandedness::Left) {
            return HandSide::Left;
        }
        if (hand.handednessScore >= m_settings.handednessLockConfidence
            && hand.handedness == HandTrackingHandedness::Right) {
            return HandSide::Right;
        }
        return imagePalmCenter(hand).x >= 0.5f ? HandSide::Right : HandSide::Left;
    }

    [[nodiscard]] static PoseBounds resolveBoundsForHand(
        const StageVolume& volume,
        const HandSide side,
        const usize activeHands) noexcept
    {
        if (activeHands < 2U) {
            return volume.full;
        }
        return side == HandSide::Left ? volume.left : volume.right;
    }

    void separateHands() noexcept {
        if (!m_results.leftPose.valid || !m_results.rightPose.valid) {
            return;
        }
        ::biofuel::engine::custom::procedural::pose::PoseSeparationSolver::apply(
            m_results.leftPose.landmarks,
            m_results.rightPose.landmarks,
            posePalmCenter(m_results.leftPose),
            posePalmCenter(m_results.rightPose),
            m_settings.minimumPalmSeparation);
    }

    static void fitVisible(TrackedRobotHandPose& pose, const PoseBounds bounds) noexcept {
        if (!pose.valid) {
            return;
        }
        ::biofuel::engine::custom::procedural::pose::PoseVisibilityFitter::apply(
            pose.landmarks,
            bounds);
    }

    HandTrackingRetargetSettings m_settings{};
    CameraFrameSpace m_frameSpace{};
    CalibrationSessionProfile m_calibration{};
    HandCalibrationSlot m_leftCalibration{};
    HandCalibrationSlot m_rightCalibration{};
    CalibrationWizardState m_wizard{};
    MappedTrackedHands m_results{};
    bool m_sessionStarted = false;
    f32 m_leftMissingSeconds = 0.0f;
    f32 m_rightMissingSeconds = 0.0f;
};

using HandTrackingRetargeter = TrackedPoseMapper;

} // namespace biofuel::engine::custom::procedural::hand
