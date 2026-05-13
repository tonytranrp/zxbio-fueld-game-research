#include "engine/custom/procedural/hand/HandTrackingRetarget.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

namespace {

using ::biofuel::f32;
using ::biofuel::usize;
using ::biofuel::engine::custom::procedural::hand::HandSide;
using ::biofuel::engine::custom::procedural::hand::HandTrackingRetargeter;
using ::biofuel::engine::custom::procedural::hand::TrackedRobotHandPose;
using ::biofuel::engine::custom::procedural::pose::MirrorPolicy;
using ::biofuel::engine::custom::procedural::pose::StageLayoutPolicy;
using ::biofuel::engine::custom::procedural::pose::CalibrationWizardStep;
using ::biofuel::engine::vision::hand_tracking::HandTrackingFrame;
using ::biofuel::engine::vision::hand_tracking::HandTrackingGesture;
using ::biofuel::engine::vision::hand_tracking::HandTrackingHand;
using ::biofuel::engine::vision::hand_tracking::HandTrackingHandedness;
using ::biofuel::engine::vision::hand_tracking::HandTrackingLandmark;

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

HandTrackingLandmark landmark(const f32 x, const f32 y, const f32 z = 0.0f) noexcept {
    return HandTrackingLandmark{.x = x, .y = y, .z = z};
}

void setFinger(
    HandTrackingHand& hand,
    const std::array<usize, 4U> indices,
    const f32 baseX,
    const f32 baseY,
    const f32 tipX,
    const f32 tipY) noexcept
{
    for (usize offset = 0U; offset < indices.size(); ++offset) {
        const f32 t = static_cast<f32>(offset) / static_cast<f32>(indices.size() - 1U);
        hand.imageLandmarks[indices[offset]] =
            landmark(baseX + (tipX - baseX) * t, baseY + (tipY - baseY) * t, -0.015f * t);
    }
}

HandTrackingHand makeOpenHand(
    const f32 centerX,
    const f32 centerY,
    const HandTrackingHandedness handedness,
    const f32 scale = 1.0f) noexcept
{
    HandTrackingHand hand{};
    hand.valid = true;
    hand.handedness = handedness;
    hand.handednessScore = 0.96f;
    hand.gesture = HandTrackingGesture::OpenPalm;
    hand.gestureScore = 0.92f;

    for (HandTrackingLandmark& point : hand.imageLandmarks) {
        point = landmark(centerX, centerY);
    }

    hand.imageLandmarks[0] = landmark(centerX, centerY + 0.145f * scale, 0.018f);
    setFinger(hand, std::array<usize, 4U>{{1U, 2U, 3U, 4U}}, centerX - 0.060f * scale, centerY + 0.070f * scale, centerX - 0.165f * scale, centerY - 0.075f * scale);
    setFinger(hand, std::array<usize, 4U>{{5U, 6U, 7U, 8U}}, centerX - 0.070f * scale, centerY - 0.010f * scale, centerX - 0.125f * scale, centerY - 0.250f * scale);
    setFinger(hand, std::array<usize, 4U>{{9U, 10U, 11U, 12U}}, centerX, centerY - 0.036f * scale, centerX + 0.002f * scale, centerY - 0.285f * scale);
    setFinger(hand, std::array<usize, 4U>{{13U, 14U, 15U, 16U}}, centerX + 0.066f * scale, centerY - 0.010f * scale, centerX + 0.105f * scale, centerY - 0.235f * scale);
    setFinger(hand, std::array<usize, 4U>{{17U, 18U, 19U, 20U}}, centerX + 0.125f * scale, centerY + 0.014f * scale, centerX + 0.180f * scale, centerY - 0.180f * scale);

    hand.worldLandmarks = hand.imageLandmarks;
    return hand;
}

HandTrackingFrame makeFrame(const HandTrackingHand& first) noexcept {
    HandTrackingFrame frame{};
    frame.valid = true;
    frame.cameraWidth = 640U;
    frame.cameraHeight = 480U;
    frame.handCount = 1U;
    frame.hands[0] = first;
    return frame;
}

HandTrackingFrame makeFrame(const HandTrackingHand& first, const HandTrackingHand& second) noexcept {
    HandTrackingFrame frame = makeFrame(first);
    frame.handCount = 2U;
    frame.hands[1] = second;
    return frame;
}

HandTrackingRetargeter calibratedMapper() noexcept {
    HandTrackingRetargeter mapper;
    mapper.beginSession(640U, 480U, MirrorPolicy::Selfie, StageLayoutPolicy::Adaptive);
    mapper.finishCalibration();
    return mapper;
}

HandTrackingRetargeter guidedMapperWithSideOffsets(const f32 leftDisplayOffset, const f32 rightDisplayOffset) noexcept {
    HandTrackingRetargeter mapper;
    mapper.beginSession(640U, 480U, MirrorPolicy::Selfie, StageLayoutPolicy::Adaptive);
    mapper.startCalibration();

    for (usize frame = 0U; frame < 360U && mapper.calibrationState().active; ++frame) {
        const auto state = mapper.calibrationState();
        const Vector2 target = ::biofuel::engine::custom::procedural::pose::calibrationTarget(state.step);
        const bool rightPhase =
            state.activeHand == ::biofuel::engine::custom::procedural::pose::CalibrationHandPhase::Right;
        const f32 displayOffset = rightPhase ? rightDisplayOffset : leftDisplayOffset;
        const f32 rawX = 1.0f - std::clamp(target.x + displayOffset, 0.05f, 0.95f);
        const HandTrackingHandedness handedness =
            rightPhase ? HandTrackingHandedness::Right : HandTrackingHandedness::Left;
        const HandTrackingHand hand = makeOpenHand(rawX, target.y, handedness);
        (void)mapper.map(makeFrame(hand), 1.0f / 30.0f);
    }

    return mapper;
}

void setDisplayLandmark(HandTrackingHand& hand, const usize index, const f32 x, const f32 y, const f32 z = 0.0f) noexcept {
    hand.imageLandmarks[index] = landmark(1.0f - x, y, z);
    hand.worldLandmarks[index] = hand.imageLandmarks[index];
}

Vector3 palmCenter(const TrackedRobotHandPose& pose) noexcept {
    constexpr std::array<usize, 5U> indices{{0U, 5U, 9U, 13U, 17U}};
    Vector3 center{0.0f, 0.0f, 0.0f};
    for (const usize index : indices) {
        center = Vector3Add(center, pose.landmarks[index]);
    }
    return Vector3Scale(center, 1.0f / static_cast<f32>(indices.size()));
}

f32 fingerSpread(const TrackedRobotHandPose& pose) noexcept {
    return Vector3Distance(pose.landmarks[8], pose.landmarks[20]);
}

} // namespace

int main() {
    const HandTrackingHand left = makeOpenHand(0.56f, 0.55f, HandTrackingHandedness::Left);
    const HandTrackingHand right = makeOpenHand(0.44f, 0.55f, HandTrackingHandedness::Right);
    const HandTrackingHand nearLeft = makeOpenHand(0.56f, 0.55f, HandTrackingHandedness::Left, 1.18f);
    const HandTrackingHand farLeft = makeOpenHand(0.56f, 0.55f, HandTrackingHandedness::Left, 0.82f);

    HandTrackingRetargeter oneHandMapper = calibratedMapper();
    const auto oneHand = oneHandMapper.map(makeFrame(left), 1.0f / 60.0f);
    HandTrackingRetargeter nearMapper = calibratedMapper();
    const auto nearHand = nearMapper.map(makeFrame(nearLeft), 1.0f / 60.0f);
    HandTrackingRetargeter farMapper = calibratedMapper();
    const auto farHand = farMapper.map(makeFrame(farLeft), 1.0f / 60.0f);

    HandTrackingRetargeter twoHandMapper = calibratedMapper();
    const auto twoHands = twoHandMapper.map(makeFrame(left, right), 1.0f / 60.0f);

    HandTrackingHand heartLeft = makeOpenHand(0.515f, 0.55f, HandTrackingHandedness::Left);
    HandTrackingHand heartRight = makeOpenHand(0.485f, 0.55f, HandTrackingHandedness::Right);
    setDisplayLandmark(heartLeft, 8U, 0.500f, 0.330f, -0.020f);
    setDisplayLandmark(heartRight, 8U, 0.500f, 0.330f, -0.020f);
    setDisplayLandmark(heartLeft, 4U, 0.485f, 0.390f, -0.016f);
    setDisplayLandmark(heartRight, 4U, 0.515f, 0.390f, -0.016f);
    HandTrackingRetargeter offsetCalibratedMapper = guidedMapperWithSideOffsets(-0.030f, 0.030f);
    const auto heartHands = offsetCalibratedMapper.map(makeFrame(heartLeft, heartRight), 1.0f / 60.0f);

    bool ok = true;
    ok = check(oneHand.leftPose.valid, "single-hand left pose was not mapped") && ok;
    ok = check(nearHand.leftPose.valid, "near-distance left pose was not mapped") && ok;
    ok = check(farHand.leftPose.valid, "far-distance left pose was not mapped") && ok;
    ok = check(twoHands.leftPose.valid, "two-hand left pose was not mapped") && ok;
    ok = check(twoHands.rightPose.valid, "two-hand right pose was not mapped") && ok;
    ok = check(heartHands.leftPose.valid, "heart left pose was not mapped") && ok;
    ok = check(heartHands.rightPose.valid, "heart right pose was not mapped") && ok;

    const f32 singleSpread = fingerSpread(oneHand.leftPose);
    const f32 pairedSpread = fingerSpread(twoHands.leftPose);
    ok = check(pairedSpread >= singleSpread * 0.92f, "adaptive two-hand mapping compressed finger spread") && ok;
    ok = check(pairedSpread <= singleSpread * 1.08f, "adaptive two-hand mapping over-expanded finger spread") && ok;
    ok = check(
        oneHand.leftPose.landmarks[8].z < oneHand.leftPose.landmarks[5].z,
        "camera-through depth did not preserve fingertip side") && ok;
    ok = check(
        palmCenter(nearHand.leftPose).z > palmCenter(farHand.leftPose).z + 0.26f,
        "palm distance did not drive forward/back stage depth") && ok;

    const f32 palmDistance = Vector3Distance(palmCenter(twoHands.leftPose), palmCenter(twoHands.rightPose));
    ok = check(palmDistance < 0.45f, "adaptive two-hand mapping forced close hands too far apart") && ok;
    const f32 heartTipDistance = Vector3Distance(heartHands.leftPose.landmarks[8], heartHands.rightPose.landmarks[8]);
    ok = check(heartTipDistance < 0.050f, "two-hand close landmark contact was not preserved") && ok;
    ok = check(twoHands.leftPose.side == HandSide::Left, "left pose side changed") && ok;
    ok = check(twoHands.rightPose.side == HandSide::Right, "right pose side changed") && ok;
    ok = check(
        ::biofuel::engine::custom::procedural::pose::calibrationStepCount() == 5U,
        "quick calibration step count changed") && ok;
    ok = check(
        ::biofuel::engine::custom::procedural::pose::nextCalibrationStep(CalibrationWizardStep::Bottom)
            == CalibrationWizardStep::Complete,
        "quick calibration did not finish after bottom marker") && ok;
    ok = check(
        ::biofuel::engine::custom::procedural::pose::calibrationRequiredHoldSeconds(CalibrationWizardStep::Left) < 0.5f,
        "quick calibration hold time regressed") && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
