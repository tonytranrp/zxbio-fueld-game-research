#pragma once

#include "engine/core/Types.hpp"
#include "Easing.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace biofuel::engine::animation::model {

template<typename T>
struct KeyframeValueSampler;

template<>
struct KeyframeValueSampler<f32> {
    [[nodiscard]] static f32 interpolate(const f32 a, const f32 b, const f32 t) noexcept {
        return a + (b - a) * t;
    }
};

template<>
struct KeyframeValueSampler<Vector3> {
    [[nodiscard]] static Vector3 interpolate(const Vector3& a, const Vector3& b, const f32 t) noexcept {
        return Vector3{
            KeyframeValueSampler<f32>::interpolate(a.x, b.x, t),
            KeyframeValueSampler<f32>::interpolate(a.y, b.y, t),
            KeyframeValueSampler<f32>::interpolate(a.z, b.z, t),
        };
    }
};

template<>
struct KeyframeValueSampler<Quaternion> {
    [[nodiscard]] static Quaternion interpolate(const Quaternion& a, const Quaternion& b, const f32 t) noexcept {
        return QuaternionNormalize(QuaternionSlerp(a, b, t));
    }
};

template<typename T>
struct Keyframe {
    f32 timeSeconds = 0.0f;
    T value{};
    Easing::Fn easing = Easing::linear;
};

template<typename T>
class KeyframeTrack final {
public:
    KeyframeTrack() = default;
    KeyframeTrack(std::initializer_list<Keyframe<T>> keys)
        : m_keys(keys) {}

    [[nodiscard]] bool empty() const noexcept { return m_keys.empty(); }
    [[nodiscard]] std::span<const Keyframe<T>> keys() const noexcept { return m_keys; }

    [[nodiscard]] T sample(
        const f32 timeSeconds,
        const f32 durationSeconds,
        const bool loop,
        const T& fallback) const noexcept
    {
        if (m_keys.empty()) {
            return fallback;
        }

        if (m_keys.size() == 1) {
            return m_keys.front().value;
        }

        f32 localTime = timeSeconds;
        if (loop && durationSeconds > 0.0f) {
            localTime = std::fmod(std::max(timeSeconds, 0.0f), durationSeconds);
            if (localTime < 0.0f) {
                localTime += durationSeconds;
            }
        }

        if (localTime <= m_keys.front().timeSeconds) {
            return m_keys.front().value;
        }

        for (size_t index = 0; index + 1 < m_keys.size(); ++index) {
            const auto& current = m_keys[index];
            const auto& next = m_keys[index + 1];
            if (localTime > next.timeSeconds) {
                continue;
            }

            const f32 span = std::max(next.timeSeconds - current.timeSeconds, 0.0001f);
            const f32 rawT = std::clamp((localTime - current.timeSeconds) / span, 0.0f, 1.0f);
            const f32 eased = current.easing ? current.easing(rawT) : rawT;
            return KeyframeValueSampler<T>::interpolate(current.value, next.value, eased);
        }

        return m_keys.back().value;
    }

private:
    std::vector<Keyframe<T>> m_keys;
};

struct BoneTrackBinding {
    std::string boneName;
    KeyframeTrack<Vector3> translation;
    KeyframeTrack<Quaternion> rotation;
    KeyframeTrack<Vector3> scale;
};

struct ScalarTrackBinding {
    std::string channelName;
    KeyframeTrack<f32> track;
};

struct KeyframeClip {
    std::string name;
    f32 durationSeconds = 0.0f;
    bool loop = false;
    KeyframeTrack<Vector3> rootTranslation;
    KeyframeTrack<Quaternion> rootRotation;
    KeyframeTrack<Vector3> rootScale;
    std::vector<BoneTrackBinding> boneTracks;
    std::vector<ScalarTrackBinding> scalarTracks;
};

struct ModelRigBinding {
    std::vector<std::string> boneNames;
    std::unordered_map<std::string, i32, TransparentHash, std::equal_to<>> boneIndices;

    [[nodiscard]] i32 findBoneIndex(std::string_view boneName) const noexcept;
    [[nodiscard]] bool empty() const noexcept { return boneNames.empty(); }
};

struct ModelKeyframeState {
    std::string currentClipName;
    std::string previousClipName;
    f32 clipProgress = 0.0f;
    f32 transitionProgress = 1.0f;
    Vector3 rootTranslation{0.0f, 0.0f, 0.0f};
    Quaternion rootRotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 rootScale{1.0f, 1.0f, 1.0f};
    std::unordered_map<std::string, f32, TransparentHash, std::equal_to<>> scalars;
};

class ModelKeyframePlayer final {
public:
    void configure(ModelRigBinding rig, std::vector<KeyframeClip> clips) noexcept;
    void reset() noexcept;
    void syncState(std::string_view clipName, f32 clipProgress, f32 transitionProgress) noexcept;
    void apply(std::span<const Transform> bindPose, std::span<Transform> outPose) noexcept;

    [[nodiscard]] bool empty() const noexcept { return m_clips.empty(); }
    [[nodiscard]] const ModelKeyframeState& state() const noexcept { return m_state; }
    [[nodiscard]] f32 scalar(std::string_view channelName, f32 fallback = 0.0f) const noexcept;

private:
    struct ResolvedBoneTrack {
        i32 boneIndex = -1;
        KeyframeTrack<Vector3> translation;
        KeyframeTrack<Quaternion> rotation;
        KeyframeTrack<Vector3> scale;
    };

    struct ResolvedClip {
        std::string name;
        f32 durationSeconds = 0.0f;
        bool loop = false;
        KeyframeTrack<Vector3> rootTranslation;
        KeyframeTrack<Quaternion> rootRotation;
        KeyframeTrack<Vector3> rootScale;
        std::vector<ResolvedBoneTrack> boneTracks;
        std::vector<ScalarTrackBinding> scalarTracks;
    };

    struct BoneChannelSample {
        bool hasTranslation = false;
        bool hasRotation = false;
        bool hasScale = false;
        Vector3 translation{0.0f, 0.0f, 0.0f};
        Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vector3 scale{1.0f, 1.0f, 1.0f};
    };

    struct PoseSample {
        Vector3 rootTranslation{0.0f, 0.0f, 0.0f};
        Quaternion rootRotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vector3 rootScale{1.0f, 1.0f, 1.0f};
        std::unordered_map<std::string, f32> scalars;
    };

    [[nodiscard]] const ResolvedClip* findClip(std::string_view clipName) const noexcept;
    void sampleClip(
        const ResolvedClip& clip,
        f32 clipProgress,
        std::span<BoneChannelSample> outSamples,
        PoseSample& outPose) noexcept;

    ModelRigBinding m_rig;
    std::vector<ResolvedClip> m_clips;
    std::vector<BoneChannelSample> m_currentSamples;
    std::vector<BoneChannelSample> m_previousSamples;
    PoseSample m_currentPose;
    PoseSample m_previousPose;
    ModelKeyframeState m_state;
};

} // namespace biofuel::engine::animation::model
