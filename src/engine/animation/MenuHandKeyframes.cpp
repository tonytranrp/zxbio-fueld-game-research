#include "ModelKeyframe.hpp"
#include <array>

namespace biofuel::engine::animation::model {

namespace {

constexpr f32 DEG_TO_RAD = 3.14159265f / 180.0f;

[[nodiscard]] Quaternion eulerDegrees(const f32 pitch, const f32 yaw, const f32 roll) noexcept {
    return QuaternionFromEuler(pitch * DEG_TO_RAD, yaw * DEG_TO_RAD, roll * DEG_TO_RAD);
}

[[nodiscard]] std::string bone(const std::string_view base, const std::string_view side) {
    return std::string{base} + "." + std::string{side};
}

[[nodiscard]] BoneTrackBinding makeFingerCurlTrack(
    const std::string_view side,
    const std::string_view base,
    const f32 curlA,
    const f32 curlB,
    const f32 settle,
    const f32 splay) noexcept
{
    return BoneTrackBinding{
        .boneName = bone(base, side),
        .rotation = KeyframeTrack<Quaternion>{
            Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutSine},
            Keyframe<Quaternion>{.timeSeconds = 0.34f, .value = eulerDegrees(-4.0f, splay, curlA * 0.22f), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 0.92f, .value = eulerDegrees(3.0f, -splay * 0.5f, curlA), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 1.42f, .value = eulerDegrees(-2.0f, splay * 0.35f, curlB), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 2.02f, .value = eulerDegrees(1.0f, 0.0f, settle), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 2.52f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
        }
    };
}

[[nodiscard]] BoneTrackBinding makeThumbTrack(
    const std::string_view side,
    const std::string_view base,
    const std::array<f32, 3> a,
    const std::array<f32, 3> b,
    const std::array<f32, 3> settle) noexcept
{
    return BoneTrackBinding{
        .boneName = bone(base, side),
        .rotation = KeyframeTrack<Quaternion>{
            Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutSine},
            Keyframe<Quaternion>{.timeSeconds = 0.42f, .value = eulerDegrees(a[0], a[1], a[2]), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 1.28f, .value = eulerDegrees(b[0], b[1], b[2]), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 2.06f, .value = eulerDegrees(settle[0], settle[1], settle[2]), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 2.52f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
        }
    };
}

[[nodiscard]] BoneTrackBinding makeArmTrack(
    const std::string_view side,
    const std::string_view base,
    const std::array<f32, 3> start,
    const std::array<f32, 3> peak,
    const std::array<f32, 3> settle) noexcept
{
    return BoneTrackBinding{
        .boneName = bone(base, side),
        .rotation = KeyframeTrack<Quaternion>{
            Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(start[0], start[1], start[2]), .easing = Easing::easeInOutQuad},
            Keyframe<Quaternion>{.timeSeconds = 0.38f, .value = eulerDegrees(start[0] * 0.8f, start[1] * 0.9f, start[2] * 0.9f), .easing = Easing::easeInCubic},
            Keyframe<Quaternion>{.timeSeconds = 1.02f, .value = eulerDegrees(peak[0], peak[1], peak[2]), .easing = Easing::easeOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 1.68f, .value = eulerDegrees(settle[0], settle[1], settle[2]), .easing = Easing::easeInOutCubic},
            Keyframe<Quaternion>{.timeSeconds = 2.52f, .value = eulerDegrees(0.0f, 0.0f, 0.0f), .easing = Easing::easeOutQuad},
        }
    };
}

void appendIdleFingerTracks(std::vector<BoneTrackBinding>& tracks, const std::string_view side, const f32 sideSign) {
    tracks.push_back(makeThumbTrack(side, "finger_thumb1", {3.0f, 2.0f * sideSign, 6.0f * sideSign}, {-1.5f, -2.0f * sideSign, -4.0f * sideSign}, {0.0f, 0.0f, 0.0f}));
    tracks.push_back(makeThumbTrack(side, "finger_thumb2", {0.0f, 0.0f, 5.0f * sideSign}, {0.0f, 0.0f, -3.0f * sideSign}, {0.0f, 0.0f, 0.0f}));
    tracks.push_back(makeThumbTrack(side, "finger_thumb3", {0.0f, 0.0f, 4.0f * sideSign}, {0.0f, 0.0f, -2.0f * sideSign}, {0.0f, 0.0f, 0.0f}));

    tracks.push_back(makeFingerCurlTrack(side, "finger_index1", 10.0f, 6.0f, 2.0f, -5.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_index2", 14.0f, 10.0f, 4.0f, -3.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_index3", 12.0f, 8.0f, 3.0f, -2.0f * sideSign));

    tracks.push_back(makeFingerCurlTrack(side, "finger_middle1", 9.0f, 7.0f, 2.5f, -1.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_middle2", 12.0f, 9.0f, 3.5f, 0.0f));
    tracks.push_back(makeFingerCurlTrack(side, "finger_middle3", 10.0f, 8.0f, 3.0f, 0.0f));

    tracks.push_back(makeFingerCurlTrack(side, "finger_ring1", 11.0f, 8.0f, 3.0f, 2.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_ring2", 15.0f, 11.0f, 4.0f, 3.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_ring3", 12.0f, 9.0f, 3.0f, 2.0f * sideSign));

    tracks.push_back(makeFingerCurlTrack(side, "finger_pinky1", 13.0f, 9.0f, 3.0f, 6.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_pinky2", 16.0f, 12.0f, 4.0f, 5.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_pinky3", 13.0f, 10.0f, 3.5f, 4.0f * sideSign));
}

void appendActionArmTracks(std::vector<BoneTrackBinding>& tracks, const std::string_view side, const f32 sideSign) {
    tracks.push_back(makeArmTrack(side, "shoulder", {-6.0f, 0.0f, 28.0f * sideSign}, {-18.0f, -18.0f * sideSign, 20.0f * sideSign}, {-8.0f, -8.0f * sideSign, 8.0f * sideSign}));
    tracks.push_back(makeArmTrack(side, "bicep", {12.0f, 0.0f, 18.0f * sideSign}, {-26.0f, -10.0f * sideSign, -12.0f * sideSign}, {-10.0f, -4.0f * sideSign, -2.0f * sideSign}));
    tracks.push_back(makeArmTrack(side, "forearm", {28.0f, 8.0f * sideSign, -12.0f * sideSign}, {62.0f, 14.0f * sideSign, 12.0f * sideSign}, {34.0f, 4.0f * sideSign, 6.0f * sideSign}));
    tracks.push_back(makeArmTrack(side, "wrist", {-4.0f, 12.0f * sideSign, 18.0f * sideSign}, {28.0f, -8.0f * sideSign, -8.0f * sideSign}, {8.0f, -3.0f * sideSign, -2.0f * sideSign}));

    tracks.push_back(makeThumbTrack(side, "finger_thumb1", {6.0f, 10.0f * sideSign, 16.0f * sideSign}, {-3.0f, -6.0f * sideSign, 8.0f * sideSign}, {1.0f, 0.0f, 2.0f * sideSign}));
    tracks.push_back(makeThumbTrack(side, "finger_thumb2", {0.0f, 0.0f, 18.0f * sideSign}, {0.0f, 0.0f, 10.0f * sideSign}, {0.0f, 0.0f, 4.0f * sideSign}));
    tracks.push_back(makeThumbTrack(side, "finger_thumb3", {0.0f, 0.0f, 14.0f * sideSign}, {0.0f, 0.0f, 8.0f * sideSign}, {0.0f, 0.0f, 3.0f * sideSign}));

    tracks.push_back(makeFingerCurlTrack(side, "finger_index1", 18.0f, 8.0f, 4.0f, -10.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_index2", 48.0f, 16.0f, 10.0f, -6.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_index3", 40.0f, 14.0f, 8.0f, -4.0f * sideSign));

    tracks.push_back(makeFingerCurlTrack(side, "finger_middle1", 16.0f, 10.0f, 5.0f, -3.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_middle2", 56.0f, 22.0f, 12.0f, -1.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_middle3", 44.0f, 18.0f, 9.0f, 0.0f));

    tracks.push_back(makeFingerCurlTrack(side, "finger_ring1", 18.0f, 12.0f, 6.0f, 4.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_ring2", 52.0f, 22.0f, 12.0f, 5.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_ring3", 42.0f, 18.0f, 9.0f, 4.0f * sideSign));

    tracks.push_back(makeFingerCurlTrack(side, "finger_pinky1", 20.0f, 14.0f, 7.0f, 10.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_pinky2", 48.0f, 20.0f, 12.0f, 9.0f * sideSign));
    tracks.push_back(makeFingerCurlTrack(side, "finger_pinky3", 38.0f, 16.0f, 8.0f, 8.0f * sideSign));
}

} // namespace

std::vector<KeyframeClip> buildMenuHandKeyframeClips() {
    std::vector<KeyframeClip> clips;
    clips.reserve(2);

    KeyframeClip idle;
    idle.name = "idle";
    idle.durationSeconds = 3.00f;
    idle.loop = true;
    idle.rootTranslation = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{0.0f, 0.012f, 0.006f}, .easing = Easing::easeInOutSine},
        Keyframe<Vector3>{.timeSeconds = 1.50f, .value = Vector3{0.0f, -0.010f, -0.008f}, .easing = Easing::easeInOutSine},
        Keyframe<Vector3>{.timeSeconds = 3.00f, .value = Vector3{0.0f, 0.012f, 0.006f}, .easing = Easing::easeInOutSine},
    };
    idle.rootRotation = KeyframeTrack<Quaternion>{
        Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(0.0f, 0.0f, -1.0f), .easing = Easing::easeInOutSine},
        Keyframe<Quaternion>{.timeSeconds = 1.50f, .value = eulerDegrees(1.0f, 1.0f, 1.0f), .easing = Easing::easeInOutSine},
        Keyframe<Quaternion>{.timeSeconds = 3.00f, .value = eulerDegrees(0.0f, 0.0f, -1.0f), .easing = Easing::easeInOutSine},
    };
    idle.rootScale = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{1.0f, 1.0f, 1.0f}, .easing = Easing::easeInOutSine},
        Keyframe<Vector3>{.timeSeconds = 3.00f, .value = Vector3{1.0f, 1.0f, 1.0f}, .easing = Easing::easeInOutSine},
    };
    idle.scalarTracks = {
        ScalarTrackBinding{
            .channelName = "portal_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.18f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 1.5f, .value = 0.26f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 3.0f, .value = 0.18f, .easing = Easing::easeInOutSine},
            }
        },
        ScalarTrackBinding{
            .channelName = "aura_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.12f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 1.5f, .value = 0.18f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 3.0f, .value = 0.12f, .easing = Easing::easeInOutSine},
            }
        },
        ScalarTrackBinding{
            .channelName = "awareness",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.22f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 1.5f, .value = 0.32f, .easing = Easing::easeInOutSine},
                Keyframe<f32>{.timeSeconds = 3.0f, .value = 0.22f, .easing = Easing::easeInOutSine},
            }
        },
    };

    appendIdleFingerTracks(idle.boneTracks, "r", 1.0f);
    appendIdleFingerTracks(idle.boneTracks, "l", -1.0f);
    idle.boneTracks.push_back(makeArmTrack("r", "wrist", {-2.0f, 5.0f, 4.0f}, {2.0f, -4.0f, -6.0f}, {0.0f, -2.0f, -2.0f}));
    idle.boneTracks.push_back(makeArmTrack("l", "wrist", {-2.0f, -5.0f, -4.0f}, {2.0f, 4.0f, 6.0f}, {0.0f, 2.0f, 2.0f}));
    clips.push_back(std::move(idle));

    KeyframeClip action;
    action.name = "action";
    action.durationSeconds = 2.52f;
    action.loop = false;
    action.rootTranslation = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{0.0f, -0.06f, 0.16f}, .easing = Easing::easeInQuad},
        Keyframe<Vector3>{.timeSeconds = 0.34f, .value = Vector3{0.0f, -0.02f, 0.13f}, .easing = Easing::easeInQuad},
        Keyframe<Vector3>{.timeSeconds = 0.94f, .value = Vector3{0.0f, 0.12f, 0.08f}, .easing = Easing::easeOutCubic},
        Keyframe<Vector3>{.timeSeconds = 1.60f, .value = Vector3{0.0f, 0.18f, 0.03f}, .easing = Easing::easeInOutCubic},
        Keyframe<Vector3>{.timeSeconds = 2.52f, .value = Vector3{0.0f, 0.06f, 0.01f}, .easing = Easing::easeOutQuad},
    };
    action.rootRotation = KeyframeTrack<Quaternion>{
        Keyframe<Quaternion>{.timeSeconds = 0.00f, .value = eulerDegrees(-16.0f, 0.0f, 0.0f), .easing = Easing::easeInOutQuad},
        Keyframe<Quaternion>{.timeSeconds = 1.04f, .value = eulerDegrees(8.0f, 0.0f, 0.0f), .easing = Easing::easeOutCubic},
        Keyframe<Quaternion>{.timeSeconds = 2.52f, .value = eulerDegrees(2.0f, 0.0f, 0.0f), .easing = Easing::easeInOutCubic},
    };
    action.rootScale = KeyframeTrack<Vector3>{
        Keyframe<Vector3>{.timeSeconds = 0.00f, .value = Vector3{1.0f, 1.0f, 1.0f}, .easing = Easing::easeInOutQuad},
        Keyframe<Vector3>{.timeSeconds = 2.52f, .value = Vector3{1.0f, 1.0f, 1.0f}, .easing = Easing::easeInOutQuad},
    };
    action.scalarTracks = {
        ScalarTrackBinding{
            .channelName = "portal_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.18f, .easing = Easing::easeInQuad},
                Keyframe<f32>{.timeSeconds = 0.62f, .value = 0.34f, .easing = Easing::easeOutCubic},
                Keyframe<f32>{.timeSeconds = 1.34f, .value = 0.84f, .easing = Easing::easeInOutCubic},
                Keyframe<f32>{.timeSeconds = 2.52f, .value = 0.46f, .easing = Easing::easeOutQuad},
            }
        },
        ScalarTrackBinding{
            .channelName = "aura_bias",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.12f, .easing = Easing::easeInQuad},
                Keyframe<f32>{.timeSeconds = 0.74f, .value = 0.32f, .easing = Easing::easeOutCubic},
                Keyframe<f32>{.timeSeconds = 1.42f, .value = 0.72f, .easing = Easing::easeInOutCubic},
                Keyframe<f32>{.timeSeconds = 2.52f, .value = 0.28f, .easing = Easing::easeOutQuad},
            }
        },
        ScalarTrackBinding{
            .channelName = "awareness",
            .track = KeyframeTrack<f32>{
                Keyframe<f32>{.timeSeconds = 0.0f, .value = 0.00f, .easing = Easing::easeInQuad},
                Keyframe<f32>{.timeSeconds = 0.72f, .value = 0.48f, .easing = Easing::easeOutCubic},
                Keyframe<f32>{.timeSeconds = 1.38f, .value = 1.00f, .easing = Easing::easeInOutCubic},
                Keyframe<f32>{.timeSeconds = 2.52f, .value = 0.60f, .easing = Easing::easeOutQuad},
            }
        },
    };

    appendActionArmTracks(action.boneTracks, "r", 1.0f);
    appendActionArmTracks(action.boneTracks, "l", -1.0f);
    clips.push_back(std::move(action));

    return clips;
}

} // namespace biofuel::engine::animation::model
