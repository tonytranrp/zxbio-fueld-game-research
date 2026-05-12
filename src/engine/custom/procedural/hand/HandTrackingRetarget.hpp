#pragma once

#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/hand/TrackedRobotHand.hpp"
#include "engine/custom/procedural/physics/TrackedPoseMapping.hpp"
#include "engine/vision/hand_tracking/HandTrackingTypes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <raylib.h>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::hand {

struct HandTrackingRetargetSettings {
    ::biofuel::engine::custom::procedural::physics::MirrorPolicy mirrorPolicy =
        ::biofuel::engine::custom::procedural::physics::MirrorPolicy::Selfie;
    ::biofuel::engine::custom::procedural::physics::StageLayoutPolicy layoutPolicy =
        ::biofuel::engine::custom::procedural::physics::StageLayoutPolicy::Shared;
    f32 targetPalmLength = 0.20f;
    f32 minimumScale = 0.75f;
    f32 maximumScale = 3.20f;
    f32 smoothingResponse = 18.0f;
    f32 minimumPalmSeparation = 0.04f;
    ::biofuel::engine::custom::procedural::physics::PoseBounds visibleBounds{
        .min = Vector3{-0.82f, -0.24f, -0.46f},
        .max = Vector3{0.82f, 0.56f, 0.46f},
    };
};

struct MappedTrackedHands {
    TrackedRobotHandPose leftPose{};
    TrackedRobotHandPose rightPose{};
    ::biofuel::engine::custom::procedural::physics::MappingState mappingState =
        ::biofuel::engine::custom::procedural::physics::MappingState::Idle;
    ::biofuel::engine::custom::procedural::physics::CalibrationWizardState calibrationState{};
    ::biofuel::engine::custom::procedural::physics::PoseBounds visibleStageBounds{};
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
        ::biofuel::engine::custom::procedural::physics::CalibrationSessionProfile profile{};
        ::biofuel::engine::custom::procedural::physics::CalibrationHandProgress progress{};
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
    using MirrorPolicy = ::biofuel::engine::custom::procedural::physics::MirrorPolicy;
    using StageLayoutPolicy = ::biofuel::engine::custom::procedural::physics::StageLayoutPolicy;
    using CalibrationWizardState = ::biofuel::engine::custom::procedural::physics::CalibrationWizardState;
    using CalibrationWizardStep = ::biofuel::engine::custom::procedural::physics::CalibrationWizardStep;
    using CalibrationHandProgress = ::biofuel::engine::custom::procedural::physics::CalibrationHandProgress;
    using CalibrationSessionProfile = ::biofuel::engine::custom::procedural::physics::CalibrationSessionProfile;
    using CameraFrameSpace = ::biofuel::engine::custom::procedural::physics::CameraFrameSpace;
    using MappingState = ::biofuel::engine::custom::procedural::physics::MappingState;
    using StageVolume = ::biofuel::engine::custom::procedural::physics::StageVolume;
    using PoseBounds = ::biofuel::engine::custom::procedural::physics::PoseBounds;

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
            .step = CalibrationWizardStep::Center,
            .holdSeconds = 0.0f,
            .requiredHoldSeconds = 1.25f,
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
        ::biofuel::engine::custom::procedural::physics::sanitizeCalibrationProfile(m_leftCalibration.profile);
        ::biofuel::engine::custom::procedural::physics::sanitizeCalibrationProfile(m_rightCalibration.profile);
        m_leftCalibration.profile.valid = true;
        m_rightCalibration.profile.valid = true;
        m_calibration = combinedCalibrationProfile();
        m_calibration.valid = true;
        m_wizard.active = false;
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
        ::biofuel::engine::vision::hand_tracking::HandTrackingLandmark result = landmark;
        if (m_frameSpace.mirror == MirrorPolicy::Selfie) {
            result.x = 1.0f - result.x;
        }
        return result;
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

        const usize activeHands = activeHandCount(frame);
        const StageVolume stageVolume =
            ::biofuel::engine::custom::procedural::physics::makeStageVolume(
                m_settings.visibleBounds,
                m_settings.layoutPolicy,
                activeHands);
        m_results.visibleStageBounds = stageVolume.full;

        for (const auto& hand : frame.hands) {
            if (!hand.valid) {
                continue;
            }
            const HandSide side = resolveSide(hand);
            const PoseBounds bounds = resolveBoundsForHand(stageVolume, side, activeHands);
            const TrackedRobotHandPose pose = buildPose(hand, side, bounds);
            if (side == HandSide::Left) {
                m_results.leftPose = pose;
            } else {
                m_results.rightPose = pose;
            }
        }

        if (m_results.leftPose.valid && previousLeft.valid) {
            ::biofuel::engine::custom::procedural::physics::PoseStabilizer::apply(
                m_results.leftPose.landmarks,
                previousLeft.landmarks,
                dt,
                m_settings.smoothingResponse);
        }
        if (m_results.rightPose.valid && previousRight.valid) {
            ::biofuel::engine::custom::procedural::physics::PoseStabilizer::apply(
                m_results.rightPose.landmarks,
                previousRight.landmarks,
                dt,
                m_settings.smoothingResponse);
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

    [[nodiscard]] static Vector3 vector3(const HandTrackingLandmark landmark) noexcept {
        return Vector3{landmark.x, landmark.y, landmark.z};
    }

    [[nodiscard]] Vector2 displayPoint(const HandTrackingLandmark landmark) const noexcept {
        const HandTrackingLandmark mirrored = displayLandmark(landmark);
        return Vector2{mirrored.x, mirrored.y};
    }

    [[nodiscard]] Vector2 imagePalmCenter(const HandTrackingHand& hand) const noexcept {
        return scale2(
            add2(
                add2(
                    add2(
                        add2(displayPoint(hand.imageLandmarks[0]), displayPoint(hand.imageLandmarks[5])),
                        displayPoint(hand.imageLandmarks[9])),
                    displayPoint(hand.imageLandmarks[13])),
                displayPoint(hand.imageLandmarks[17])),
            0.2f);
    }

    [[nodiscard]] static f32 imagePalmSpan(const HandTrackingHand& hand) noexcept {
        const f32 wristToMiddle = distance2(imagePoint(hand.imageLandmarks[0]), imagePoint(hand.imageLandmarks[9]));
        const f32 indexToPinky = distance2(imagePoint(hand.imageLandmarks[5]), imagePoint(hand.imageLandmarks[17]));
        return std::max(std::max(wristToMiddle, indexToPinky * 0.82f), 0.025f);
    }

    [[nodiscard]] static f32 imagePalmDepth(const HandTrackingHand& hand) noexcept {
        return (hand.imageLandmarks[0].z
            + hand.imageLandmarks[5].z
            + hand.imageLandmarks[9].z
            + hand.imageLandmarks[13].z
            + hand.imageLandmarks[17].z) * 0.2f;
    }

    [[nodiscard]] static Vector3 worldPalmCenter(const HandTrackingHand& hand) noexcept {
        return Vector3Scale(
            Vector3Add(
                Vector3Add(
                    Vector3Add(
                        Vector3Add(vector3(hand.worldLandmarks[0]), vector3(hand.worldLandmarks[5])),
                        vector3(hand.worldLandmarks[9])),
                    vector3(hand.worldLandmarks[13])),
                vector3(hand.worldLandmarks[17])),
            0.2f);
    }

    [[nodiscard]] static Vector3 posePalmCenter(const TrackedRobotHandPose& pose) noexcept {
        constexpr std::array<usize, 5U> palmIndices{{0U, 5U, 9U, 13U, 17U}};
        return ::biofuel::engine::custom::procedural::physics::poseWeightedCenter(
            pose.landmarks,
            palmIndices);
    }

    void updateCalibration(
        const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame,
        const f32 dt) noexcept
    {
        const CalibrationHandSelection selection = calibrationHands(frame);
        updateCalibrationSlot(m_leftCalibration, selection.left, dt);
        updateCalibrationSlot(m_rightCalibration, selection.right, dt);
        syncWizardProgress();

        if (!m_leftCalibration.progress.sampleCaptured || !m_rightCalibration.progress.sampleCaptured) {
            return;
        }

        const CalibrationWizardStep next =
            ::biofuel::engine::custom::procedural::physics::nextCalibrationStep(m_wizard.step);
        if (next == CalibrationWizardStep::Complete) {
            finishCalibration();
            return;
        }

        m_wizard.step = next;
        resetCalibrationAccumulator();
        syncWizardProgress();
    }

    [[nodiscard]] static bool validCalibrationHand(const HandTrackingHand& hand) noexcept {
        if (!hand.valid || hand.handednessScore < 0.30f) {
            return false;
        }

        const f32 palmSpan = imagePalmSpan(hand);
        if (palmSpan < 0.035f || palmSpan > 0.68f) {
            return false;
        }

        constexpr std::array<usize, 5U> requiredLandmarks{{0U, 5U, 9U, 13U, 17U}};
        for (const usize index : requiredLandmarks) {
            const HandTrackingLandmark landmark = hand.imageLandmarks[index];
            if (landmark.x < -0.18f || landmark.x > 1.18f || landmark.y < -0.18f || landmark.y > 1.18f) {
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
        const Vector2 target = ::biofuel::engine::custom::procedural::physics::calibrationTarget(step);
        const f32 positionRadius =
            (step == CalibrationWizardStep::Near || step == CalibrationWizardStep::Far) ? 0.20f : 0.115f;
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
                ::biofuel::engine::custom::procedural::physics::CalibrationCaptureStatus::Captured;
            return;
        }

        if (hand == nullptr) {
            resetSlotAccumulator(slot);
            slot.progress.detected = false;
            slot.progress.targetAcquired = false;
            slot.progress.targetError = 1.0f;
            slot.progress.status =
                ::biofuel::engine::custom::procedural::physics::CalibrationCaptureStatus::Missing;
            return;
        }

        slot.progress.detected = true;
        const Vector2 palm = imagePalmCenter(*hand);
        const f32 palmSpan = imagePalmSpan(*hand);
        const f32 targetError = calibrationTargetError(m_wizard.step, slot.profile, palm, palmSpan);
        const bool stable = calibrationSampleStable(slot, palm, palmSpan);
        slot.progress.targetError = targetError;

        if (targetError > 1.0f) {
            resetSlotAccumulator(slot);
            slot.progress.targetAcquired = false;
            slot.progress.status =
                ::biofuel::engine::custom::procedural::physics::CalibrationCaptureStatus::OutsideTarget;
            return;
        }
        if (!stable) {
            resetSlotAccumulator(slot);
            slot.progress.targetAcquired = false;
            slot.progress.status =
                ::biofuel::engine::custom::procedural::physics::CalibrationCaptureStatus::Unstable;
            return;
        }

        slot.progress.targetAcquired = true;
        slot.progress.status =
            ::biofuel::engine::custom::procedural::physics::CalibrationCaptureStatus::Capturing;
        const f32 safeDt = std::min(std::max(dt, 0.0f), 0.05f);
        slot.progress.holdSeconds = std::min(m_wizard.requiredHoldSeconds, slot.progress.holdSeconds + safeDt);
        accumulateCalibrationSample(slot, palm, palmSpan);

        if (slot.progress.holdSeconds < m_wizard.requiredHoldSeconds) {
            return;
        }

        const Vector2 capturedPalm = slot.sample.count > 0U
            ? scale2(slot.sample.palmSum, 1.0f / static_cast<f32>(slot.sample.count))
            : palm;
        const f32 capturedSpan = slot.sample.count > 0U
            ? slot.sample.spanSum / static_cast<f32>(slot.sample.count)
            : palmSpan;
        captureCalibrationSample(slot, capturedPalm, capturedSpan);
        slot.progress.sampleCaptured = true;
        slot.progress.targetAcquired = true;
        slot.progress.status =
            ::biofuel::engine::custom::procedural::physics::CalibrationCaptureStatus::Captured;
        slot.progress.holdSeconds = m_wizard.requiredHoldSeconds;
    }

    [[nodiscard]] bool calibrationSampleStable(
        HandCalibrationSlot& slot,
        const Vector2 palm,
        const f32 palmSpan) noexcept
    {
        const bool stable = !slot.sample.hasLast
            || (distance2(palm, slot.sample.lastPalm) <= 0.030f
                && std::fabs(palmSpan - slot.sample.lastPalmSpan) <= 0.035f);
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
        resetSlotAccumulator(m_leftCalibration);
        resetSlotAccumulator(m_rightCalibration);
        m_leftCalibration.progress = CalibrationHandProgress{};
        m_rightCalibration.progress = CalibrationHandProgress{};
        m_leftCalibration.progress.requiredHoldSeconds = m_wizard.requiredHoldSeconds;
        m_rightCalibration.progress.requiredHoldSeconds = m_wizard.requiredHoldSeconds;
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
        case CalibrationWizardStep::Near:
            slot.profile.nearPalmSpan = palmSpan;
            break;
        case CalibrationWizardStep::Far:
            slot.profile.farPalmSpan = palmSpan;
            break;
        case CalibrationWizardStep::Inactive:
        case CalibrationWizardStep::Complete:
            break;
        }
    }

    void syncWizardProgress() noexcept {
        m_wizard.left = m_leftCalibration.progress;
        m_wizard.right = m_rightCalibration.progress;
        m_wizard.requiredHoldSeconds = std::max(
            m_leftCalibration.progress.requiredHoldSeconds,
            m_rightCalibration.progress.requiredHoldSeconds);
        m_wizard.holdSeconds = std::min(
            m_leftCalibration.progress.holdSeconds,
            m_rightCalibration.progress.holdSeconds);
        m_wizard.targetAcquired =
            m_leftCalibration.progress.targetAcquired && m_rightCalibration.progress.targetAcquired;
        m_wizard.targetError = std::max(
            m_leftCalibration.progress.targetError,
            m_rightCalibration.progress.targetError);
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

    [[nodiscard]] TrackedRobotHandPose buildPose(
        const HandTrackingHand& hand,
        const HandSide side,
        const PoseBounds bounds) const noexcept
    {
        const CalibrationSessionProfile& profile = calibrationProfileForSide(side);
        const f32 palmSpan = imagePalmSpan(hand);
        const f32 palmDepth = imagePalmDepth(hand);
        const f32 stageDepth = ::biofuel::engine::custom::procedural::physics::remapClamped(
            palmSpan,
            profile.farPalmSpan,
            profile.nearPalmSpan,
            bounds.min.z + 0.08f,
            bounds.max.z - 0.08f);

        TrackedRobotHandPose pose{
            .valid = true,
            .side = side,
            .confidence = hand.handednessScore,
            .landmarks = {},
        };
        for (usize index = 0U; index < pose.landmarks.size(); ++index) {
            pose.landmarks[index] = mapImageLandmark(profile, bounds, stageDepth, palmDepth, hand.imageLandmarks[index]);
        }
        fitVisible(pose, bounds);
        return pose;
    }

    [[nodiscard]] Vector3 mapWorldLandmark(
        const Vector3 origin,
        const Vector3 worldPalm,
        const Vector3 value,
        const f32 scale) const noexcept
    {
        Vector3 local = Vector3Subtract(value, worldPalm);
        if (m_settings.mirrorPolicy == MirrorPolicy::Selfie) {
            local.x = -local.x;
        }
        return Vector3Add(origin, Vector3{
            local.x * scale,
            -local.y * scale,
            -local.z * scale,
        });
    }

    [[nodiscard]] Vector3 mapImageLandmark(
        const CalibrationSessionProfile& profile,
        const PoseBounds bounds,
        const f32 stageDepth,
        const f32 palmDepth,
        const HandTrackingLandmark value) const noexcept
    {
        const HandTrackingLandmark displayValue = displayLandmark(value);
        const f32 x = ::biofuel::engine::custom::procedural::physics::remapUnclamped(
            displayValue.x,
            profile.left.x,
            profile.right.x,
            bounds.min.x + 0.04f,
            bounds.max.x - 0.04f);
        const f32 y = ::biofuel::engine::custom::procedural::physics::remapUnclamped(
            displayValue.y,
            profile.top.y,
            profile.bottom.y,
            bounds.max.y - 0.04f,
            bounds.min.y + 0.04f);
        const f32 zOffset = std::clamp((palmDepth - value.z) * 0.72f, -0.15f, 0.15f);
        return Vector3{x, y, stageDepth + zOffset};
    }

    [[nodiscard]] HandSide resolveSide(const HandTrackingHand& hand) const noexcept {
        if (hand.handedness == HandTrackingHandedness::Left) {
            return HandSide::Left;
        }
        if (hand.handedness == HandTrackingHandedness::Right) {
            return HandSide::Right;
        }
        return imagePalmCenter(hand).x >= 0.5f ? HandSide::Right : HandSide::Left;
    }

    [[nodiscard]] static usize activeHandCount(
        const ::biofuel::engine::vision::hand_tracking::HandTrackingFrame& frame) noexcept
    {
        usize count = 0U;
        for (const auto& hand : frame.hands) {
            if (hand.valid) {
                ++count;
            }
        }
        return count;
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
        ::biofuel::engine::custom::procedural::physics::PoseSeparationSolver::apply(
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
        ::biofuel::engine::custom::procedural::physics::PoseVisibilityFitter::apply(
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
};

using HandTrackingRetargeter = TrackedPoseMapper;

} // namespace biofuel::engine::custom::procedural::hand
