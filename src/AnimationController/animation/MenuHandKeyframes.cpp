#include "ModelKeyframe.hpp"
#include <cmath>

namespace biofuel::animation::model {

namespace {

constexpr f32 DEG_TO_RAD = 3.14159265f / 180.0f;

[[nodiscard]] Quaternion eulerDegrees(const f32 pitch, const f32 yaw, const f32 roll) noexcept {
    return QuaternionFromEuler(pitch * DEG_TO_RAD, yaw * DEG_TO_RAD, roll * DEG_TO_RAD);
}

[[nodiscard]] BoneTrackBinding makeFingerCurlTrack(
    std::string boneName,
    const f32 enterA,
    const f32 enterB,
    const f32 settle) noexcept
{
    return BoneTrackBinding{
        .boneName = std::move(boneName),
        .rotation = KeyframeTrack<Quaternion>{
            Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 0.48f, .value = eulerDegrees(2.0f, 0.0f, enterA), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 1.08f, .value = eulerDegrees(-3.0f, 1.0f, enterB), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 1.58f, .value = eulerDegrees(1.0f, 0.0f, settle), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 2.24f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
        }
    };
}

[[nodiscard]] BoneTrackBinding makeThumbTrack(
    std::string boneName,
    const f32 pitchA,
    const f32 yawA,
    const f32 rollA,
    const f32 pitchB,
    const f32 yawB,
    const f32 rollB) noexcept
{
    return BoneTrackBinding{
        .boneName = std::move(boneName),
        .rotation = KeyframeTrack<Quaternion>{
            Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 0.55f, .value = eulerDegrees(pitchA, yawA, rollA), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 1.20f, .value = eulerDegrees(pitchB, yawB, rollB), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 2.24f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
        }
    };
}

} // namespace

std::vector<KeyframeClip> buildMenuHandKeyframeClips() {
    std::vector<KeyframeClip> clips;
    clips.reserve(2);

    KeyframeClip idle;
    idle.name = "idle";
    idle.durationSeconds = 2.80f;
    idle.loop = true;
    idle.rootTranslation = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{0.0f, 0.010f, 0.0f}, .easing = Easing::easeInOutSine},
        Keyframe<Vector3>{.timeSeconds = 1.40f, .value = Vector3{0.0f, -0.012f, -0.010f}, .easing = Easing::easeInOutSine},
        Keyframe<Vector3>{.timeSeconds = 2.80f, .value = Vector3{0.0f, 0.010f, 0.0f}, .easing = Easing::easeInOutSine},
    };
    idle.rootRotation = KeyframeTrack<Quaternion>{
        Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(0.0f, 0.0f, -2.0f), .easing = Easing::easeInOutSine},
        Keyframe<Quaternion>{.timeSeconds = 1.40f, .value = eulerDegrees(1.5f, 1.0f, 2.5f), .easing = Easing::easeInOutSine},
        Keyframe<Quaternion>{.timeSeconds = 2.80f, .value = eulerDegrees(0.0f, 0.0f, -2.0f), .easing = Easing::easeInOutSine},
    };
    idle.rootScale = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{1.0f, 1.0f, 1.0f}, .easing = Easing::easeInOutSine},
        Keyframe<Vector3>{.timeSeconds = 1.40f, .value = Vector3{1.01f, 1.0f, 1.01f}, .easing = Easing::easeInOutSine},
        Keyframe<Vector3>{.timeSeconds = 2.80f, .value = Vector3{1.0f, 1.0f, 1.0f}, .easing = Easing::easeInOutSine},
    };
    idle.scalarTracks = {
        ScalarTrackBinding{
            .channelName = "portal_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.22f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 1.4f, .value = 0.28f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 2.8f, .value = 0.22f, .easing = Easing::easeInOutSine},
            }
        },
        ScalarTrackBinding{
            .channelName = "aura_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.16f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 1.4f, .value = 0.20f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 2.8f, .value = 0.16f, .easing = Easing::easeInOutSine},
            }
        },
    };
    idle.boneTracks = {
        makeThumbTrack("ThumbRoot", 2.0f, 2.0f, 6.0f, -2.0f, -3.0f, -5.0f),
        makeThumbTrack("ThumbMiddle", 0.0f, 0.0f, 4.5f, 0.0f, 0.0f, -3.5f),
        makeFingerCurlTrack("IndexF_lower", 5.0f, 8.0f, 3.0f),
        makeFingerCurlTrack("IndexF_middle", 4.0f, 7.0f, 2.0f),
        makeFingerCurlTrack("IndexF_tip", 3.0f, 5.0f, 1.0f),
        makeFingerCurlTrack("MiddleF_lower", 3.5f, 6.0f, 2.0f),
        makeFingerCurlTrack("MiddleF_middle", 3.0f, 5.0f, 1.5f),
        makeFingerCurlTrack("MiddleF_tip", 2.0f, 4.0f, 1.0f),
        makeFingerCurlTrack("RingF_lower", 4.5f, 7.0f, 2.0f),
        makeFingerCurlTrack("RingF_middle", 3.0f, 5.0f, 1.0f),
        makeFingerCurlTrack("RingF_tip", 2.0f, 4.0f, 1.0f),
        makeFingerCurlTrack("PinkyF_lower", 5.0f, 8.0f, 2.5f),
        makeFingerCurlTrack("PinkyF_middle", 4.0f, 6.0f, 2.0f),
        makeFingerCurlTrack("PinkyF_tip", 3.0f, 5.0f, 1.0f),
    };
    clips.push_back(std::move(idle));

    KeyframeClip action;
    action.name = "action";
    action.durationSeconds = 2.40f;
    action.loop = false;
    action.rootTranslation = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{1.25f, 0.28f, 0.24f}, .easing = Easing::easeInOutQuad},
        Keyframe<Vector3>{.timeSeconds = 0.24f, .value = Vector3{1.22f, 0.26f, 0.22f}, .easing = Easing::easeInCubic},
        Keyframe<Vector3>{.timeSeconds = 0.82f, .value = Vector3{0.76f, 0.12f, 0.08f}, .easing = Easing::easeOutCubic},
        Keyframe<Vector3>{.timeSeconds = 1.34f, .value = Vector3{0.30f, 0.02f, -0.02f}, .easing = Easing::easeInOutCubic},
        Keyframe<Vector3>{.timeSeconds = 1.86f, .value = Vector3{0.08f, -0.03f, -0.04f}, .easing = Easing::easeOutCubic},
        Keyframe<Vector3>{.timeSeconds = 2.40f, .value = Vector3{0.0f, 0.0f, 0.0f}, .easing = Easing::easeInOutCubic},
    };
    action.rootRotation = KeyframeTrack<Quaternion>{
        Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(-10.0f, 20.0f, 26.0f), .easing = Easing::easeInOutQuad},
        Keyframe<Quaternion>{.timeSeconds = 0.36f, .value = eulerDegrees(-8.0f, 18.0f, 34.0f), .easing = Easing::easeOutCubic},
        Keyframe<Quaternion>{.timeSeconds = 0.94f, .value = eulerDegrees(7.0f, -7.0f, 10.0f), .easing = Easing::easeInOutCubic},
        Keyframe<Quaternion>{.timeSeconds = 1.42f, .value = eulerDegrees(-5.0f, 9.0f, -14.0f), .easing = Easing::easeInOutCubic},
        Keyframe<Quaternion>{.timeSeconds = 1.90f, .value = eulerDegrees(1.0f, 3.0f, 4.0f), .easing = Easing::easeOutCubic},
        Keyframe<Quaternion>{.timeSeconds = 2.40f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
    };
    action.rootScale = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{0.98f, 0.98f, 0.98f}, .easing = Easing::easeInOutQuad},
        Keyframe<Vector3>{.timeSeconds = 0.90f, .value = Vector3{1.02f, 1.01f, 1.02f}, .easing = Easing::easeOutCubic},
        Keyframe<Vector3>{.timeSeconds = 1.72f, .value = Vector3{1.01f, 1.00f, 1.01f}, .easing = Easing::easeInOutCubic},
        Keyframe<Vector3>{.timeSeconds = 2.40f, .value = Vector3{1.0f, 1.0f, 1.0f}, .easing = Easing::easeInOutCubic},
    };
    action.scalarTracks = {
        ScalarTrackBinding{
            .channelName = "portal_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.18f, .easing = Easing::easeInQuad},
                Keyframe<f32>{.timeSeconds = 0.42f, .value = 0.32f, .easing = Easing::easeOutCubic},
                Keyframe<f32>{.timeSeconds = 1.14f, .value = 0.88f, .easing = Easing::easeInOutCubic},
                Keyframe<f32>{.timeSeconds = 2.40f, .value = 0.52f, .easing = Easing::easeOutQuad},
            }
        },
        ScalarTrackBinding{
            .channelName = "aura_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.10f, .easing = Easing::easeInQuad},
                Keyframe<f32>{.timeSeconds = 0.58f, .value = 0.28f, .easing = Easing::easeOutCubic},
                Keyframe<f32>{.timeSeconds = 1.36f, .value = 0.74f, .easing = Easing::easeInOutCubic},
                Keyframe<f32>{.timeSeconds = 2.40f, .value = 0.34f, .easing = Easing::easeOutQuad},
            }
        },
        ScalarTrackBinding{
            .channelName = "awareness",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.0f, .easing = Easing::easeInQuad},
                Keyframe<f32>{.timeSeconds = 0.50f, .value = 0.35f, .easing = Easing::easeOutCubic},
                Keyframe<f32>{.timeSeconds = 1.22f, .value = 1.0f, .easing = Easing::easeInOutCubic},
                Keyframe<f32>{.timeSeconds = 2.40f, .value = 0.58f, .easing = Easing::easeOutQuad},
            }
        },
    };
    action.boneTracks = {
        makeThumbTrack("ThumbRoot", 4.0f, 10.0f, 18.0f, -3.0f, -6.0f, 8.0f),
        makeThumbTrack("ThumbMiddle", 0.0f, 2.0f, 18.0f, 0.0f, -2.0f, 8.0f),
        makeThumbTrack("ThumbTop", 0.0f, 0.0f, 16.0f, 0.0f, 0.0f, 6.0f),
        makeFingerCurlTrack("IndexRoot", 3.0f, -4.0f, 2.0f),
        makeFingerCurlTrack("IndexF_lower", 18.0f, 46.0f, 20.0f),
        makeFingerCurlTrack("IndexF_middle", 10.0f, 58.0f, 16.0f),
        makeFingerCurlTrack("IndexF_tip", 6.0f, 40.0f, 10.0f),
        makeFingerCurlTrack("MiddleRoot", 2.0f, -1.0f, 1.0f),
        makeFingerCurlTrack("MiddleF_lower", 16.0f, 54.0f, 18.0f),
        makeFingerCurlTrack("MiddleF_middle", 12.0f, 64.0f, 20.0f),
        makeFingerCurlTrack("MiddleF_tip", 8.0f, 42.0f, 12.0f),
        makeFingerCurlTrack("RingRoot", 2.0f, 2.0f, 2.0f),
        makeFingerCurlTrack("RingF_lower", 14.0f, 44.0f, 16.0f),
        makeFingerCurlTrack("RingF_middle", 10.0f, 52.0f, 16.0f),
        makeFingerCurlTrack("RingF_tip", 8.0f, 34.0f, 10.0f),
        makeFingerCurlTrack("PinkyRoot", 3.0f, 7.0f, 4.0f),
        makeFingerCurlTrack("PinkyF_lower", 12.0f, 38.0f, 16.0f),
        makeFingerCurlTrack("PinkyF_middle", 9.0f, 46.0f, 14.0f),
        makeFingerCurlTrack("PinkyF_tip", 7.0f, 30.0f, 9.0f),
    };
    clips.push_back(std::move(action));

    return clips;
}

} // namespace biofuel::animation::model
