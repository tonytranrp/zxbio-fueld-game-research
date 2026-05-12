#pragma once

#include "engine/custom/procedural/hand/HandTypes.hpp"
#include <array>
#include <concepts>
#include <cmath>
#include <string_view>
#include <raylib.h>

namespace biofuel::engine::custom::procedural::animation {

using ::biofuel::engine::custom::procedural::hand::FingerId;
using ::biofuel::engine::custom::procedural::hand::HandSide;

struct IdleFlexClip {};
struct WaveClip {};
struct FistGrabClip {};
struct PinchClip {};
struct PointClip {};
struct PeaceSignClip {};
struct MirrorDemoClip {};
struct PlantClip {};
struct HarvestClip {};
struct UpgradePopClip {};
struct TechUnlockClip {};
struct MenuSelectClip {};
struct Swap2D3DClip {};

struct HandAnimationPose {
    f32 curl = 0.18f;
    f32 spread = 0.0f;
    Vector3 wristOffset{0.0f, 0.0f, 0.0f};
    std::array<Vector3, static_cast<usize>(FingerId::Count)> targetOffsets{};
};

template<typename TClip>
struct HandAnimationClip;

template<typename TClip>
concept HandAnimationClipType = requires {
    { HandAnimationClip<TClip>::name } -> std::convertible_to<std::string_view>;
    { HandAnimationClip<TClip>::duration } -> std::convertible_to<f32>;
};

[[nodiscard]] inline f32 wave01(const f32 phase) noexcept {
    return std::sin(phase * 6.28318530718f) * 0.5f + 0.5f;
}

[[nodiscard]] inline f32 pulseSigned(const f32 phase) noexcept {
    return std::sin(phase * 6.28318530718f);
}

[[nodiscard]] inline f32 ease01(const f32 value) noexcept {
    const f32 t = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    return t * t * (3.0f - 2.0f * t);
}

inline void setFingerOffset(HandAnimationPose& pose, const FingerId finger, const Vector3 offset) noexcept {
    pose.targetOffsets[static_cast<usize>(finger)] = offset;
}

#define BIOFUEL_HAND_CLIP(CLIP, NAME, DURATION, CURL_EXPR, SPREAD_EXPR, ...) \
    template<> struct HandAnimationClip<CLIP> { \
        static constexpr std::string_view name = NAME; \
        static constexpr f32 duration = DURATION; \
        [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept { \
            static_cast<void>(phase); \
            static_cast<void>(side); \
            HandAnimationPose pose{}; \
            pose.curl = (CURL_EXPR); \
            pose.spread = (SPREAD_EXPR); \
            pose.wristOffset = (__VA_ARGS__); \
            return pose; \
        } \
    }

BIOFUEL_HAND_CLIP(IdleFlexClip, "Idle Flex", 2.4f, 0.10f + ease01(wave01(phase)) * 0.12f, pulseSigned(phase) * 0.10f, Vector3{0.0f, wave01(phase) * 0.010f, 0.0f});
BIOFUEL_HAND_CLIP(WaveClip, "Wave", 1.8f, 0.08f + ease01(wave01(phase)) * 0.12f, pulseSigned(phase) * 0.30f, Vector3{pulseSigned(phase) * 0.035f, wave01(phase) * 0.026f, 0.0f});
BIOFUEL_HAND_CLIP(MirrorDemoClip, "Mirror Demo", 2.2f, 0.15f + wave01(phase) * 0.34f, pulseSigned(phase) * 0.64f, Vector3{pulseSigned(phase) * 0.032f, 0.0f, 0.0f});
BIOFUEL_HAND_CLIP(UpgradePopClip, "Upgrade Pop", 1.25f, 0.10f + wave01(phase) * 0.18f, 0.35f, Vector3{0.0f, wave01(phase) * 0.10f, pulseSigned(phase) * 0.035f});
BIOFUEL_HAND_CLIP(TechUnlockClip, "Tech Unlock", 2.0f, 0.18f, pulseSigned(phase) * 0.75f, Vector3{0.0f, wave01(phase) * 0.055f, 0.02f});
BIOFUEL_HAND_CLIP(Swap2D3DClip, "2D -> 3D Swap", 1.9f, wave01(phase) * 0.42f, pulseSigned(phase) * 0.38f, Vector3{0.0f, wave01(phase) * 0.08f, pulseSigned(phase) * 0.08f});

#undef BIOFUEL_HAND_CLIP

template<>
struct HandAnimationClip<FistGrabClip> {
    static constexpr std::string_view name = "Fist / Grab";
    static constexpr f32 duration = 1.35f;

    [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept {
        static_cast<void>(side);
        const f32 close = ease01(wave01(phase));
        HandAnimationPose pose{};
        pose.curl = close * 0.92f;
        pose.spread = 0.0f;
        pose.wristOffset = Vector3{0.0f, -close * 0.018f, -close * 0.040f};
        setFingerOffset(pose, FingerId::Thumb, Vector3{0.020f, -0.040f, -0.060f});
        setFingerOffset(pose, FingerId::Index, Vector3{0.006f, -0.060f, -0.072f});
        setFingerOffset(pose, FingerId::Middle, Vector3{0.000f, -0.066f, -0.076f});
        setFingerOffset(pose, FingerId::Ring, Vector3{-0.006f, -0.060f, -0.072f});
        setFingerOffset(pose, FingerId::Pinky, Vector3{-0.012f, -0.048f, -0.064f});
        return pose;
    }
};

template<>
struct HandAnimationClip<PinchClip> {
    static constexpr std::string_view name = "Pinch";
    static constexpr f32 duration = 1.6f;

    [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept {
        const f32 sign = side == HandSide::Left ? 1.0f : -1.0f;
        const f32 pinch = ease01(wave01(phase));
        HandAnimationPose pose{};
        pose.curl = 0.26f + pinch * 0.22f;
        pose.spread = -0.14f;
        pose.wristOffset = Vector3{0.0f, 0.0f, -pinch * 0.032f};
        setFingerOffset(pose, FingerId::Thumb, Vector3{sign * 0.030f, 0.016f, -0.048f});
        setFingerOffset(pose, FingerId::Index, Vector3{-sign * 0.022f, -0.020f, -0.050f});
        setFingerOffset(pose, FingerId::Middle, Vector3{0.000f, -0.030f, -0.030f});
        setFingerOffset(pose, FingerId::Ring, Vector3{0.000f, -0.038f, -0.030f});
        setFingerOffset(pose, FingerId::Pinky, Vector3{0.000f, -0.040f, -0.026f});
        return pose;
    }
};

template<>
struct HandAnimationClip<PointClip> {
    static constexpr std::string_view name = "Point";
    static constexpr f32 duration = 1.8f;

    [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept {
        static_cast<void>(phase);
        const f32 sign = side == HandSide::Left ? 1.0f : -1.0f;
        HandAnimationPose pose{};
        pose.curl = 0.64f;
        pose.spread = -0.18f;
        pose.wristOffset = Vector3{0.0f, 0.016f, -0.014f};
        setFingerOffset(pose, FingerId::Index, Vector3{sign * 0.016f, 0.126f, -0.028f});
        setFingerOffset(pose, FingerId::Thumb, Vector3{sign * 0.018f, -0.014f, -0.030f});
        return pose;
    }
};

template<>
struct HandAnimationClip<PeaceSignClip> {
    static constexpr std::string_view name = "Peace Sign";
    static constexpr f32 duration = 2.0f;

    [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept {
        const f32 sign = side == HandSide::Left ? 1.0f : -1.0f;
        const f32 lift = 0.018f + wave01(phase) * 0.010f;
        HandAnimationPose pose{};
        pose.curl = 0.44f;
        pose.spread = 0.42f;
        pose.wristOffset = Vector3{0.0f, lift, 0.0f};
        setFingerOffset(pose, FingerId::Index, Vector3{-sign * 0.012f, 0.088f, -0.020f});
        setFingerOffset(pose, FingerId::Middle, Vector3{sign * 0.012f, 0.100f, -0.020f});
        setFingerOffset(pose, FingerId::Ring, Vector3{-sign * 0.012f, -0.030f, -0.040f});
        setFingerOffset(pose, FingerId::Pinky, Vector3{-sign * 0.016f, -0.034f, -0.038f});
        return pose;
    }
};

template<>
struct HandAnimationClip<PlantClip> {
    static constexpr std::string_view name = "Plant";
    static constexpr f32 duration = 1.7f;

    [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept {
        static_cast<void>(side);
        const f32 press = ease01(wave01(phase));
        HandAnimationPose pose{};
        pose.curl = 0.44f + press * 0.28f;
        pose.spread = -0.18f;
        pose.wristOffset = Vector3{0.0f, -press * 0.060f, -0.036f};
        setFingerOffset(pose, FingerId::Index, Vector3{0.0f, -0.044f, -0.060f});
        setFingerOffset(pose, FingerId::Middle, Vector3{0.0f, -0.052f, -0.062f});
        return pose;
    }
};

template<>
struct HandAnimationClip<HarvestClip> {
    static constexpr std::string_view name = "Harvest";
    static constexpr f32 duration = 1.45f;

    [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept {
        const f32 sign = side == HandSide::Left ? 1.0f : -1.0f;
        const f32 swing = pulseSigned(phase);
        HandAnimationPose pose{};
        pose.curl = 0.70f;
        pose.spread = swing * 0.24f;
        pose.wristOffset = Vector3{sign * swing * 0.055f, -0.028f, -0.045f};
        setFingerOffset(pose, FingerId::Thumb, Vector3{sign * 0.016f, -0.030f, -0.054f});
        return pose;
    }
};

template<>
struct HandAnimationClip<MenuSelectClip> {
    static constexpr std::string_view name = "Menu Select";
    static constexpr f32 duration = 1.1f;

    [[nodiscard]] static HandAnimationPose sample(const f32 phase, const HandSide side) noexcept {
        const f32 sign = side == HandSide::Left ? 1.0f : -1.0f;
        const f32 press = ease01(wave01(phase));
        HandAnimationPose pose{};
        pose.curl = 0.38f + press * 0.24f;
        pose.spread = -0.08f;
        pose.wristOffset = Vector3{0.0f, 0.0f, -press * 0.070f};
        setFingerOffset(pose, FingerId::Index, Vector3{sign * 0.006f, 0.018f, -0.070f});
        return pose;
    }
};

template<typename... TClips>
struct HandAnimationSet {
    static constexpr usize count = sizeof...(TClips);

    [[nodiscard]] static constexpr std::array<std::string_view, count> names() noexcept {
        return {HandAnimationClip<TClips>::name...};
    }

    [[nodiscard]] static constexpr std::array<f32, count> durations() noexcept {
        return {HandAnimationClip<TClips>::duration...};
    }

    [[nodiscard]] static HandAnimationPose sample(const usize index, const f32 phase, const HandSide side) noexcept {
        using SampleFn = HandAnimationPose (*)(f32, HandSide) noexcept;
        static constexpr std::array<SampleFn, count> functions{&HandAnimationClip<TClips>::sample...};
        if (index >= functions.size()) {
            return {};
        }
        return functions[index](phase, side);
    }
};

using DefaultHandAnimationSet = HandAnimationSet<
    IdleFlexClip,
    WaveClip,
    FistGrabClip,
    PinchClip,
    PointClip,
    PeaceSignClip,
    MirrorDemoClip,
    PlantClip,
    HarvestClip,
    UpgradePopClip,
    TechUnlockClip,
    MenuSelectClip,
    Swap2D3DClip>;

template<typename TAnimationSet>
class HandDemoController final {
public:
    void reset() noexcept {
        m_clip = 0U;
        m_time = 0.0f;
        m_speed = 1.0f;
        m_playing = false;
        m_loop = true;
        m_mirror = true;
    }

    void update(const f32 dt) noexcept {
        if (!m_playing) {
            return;
        }

        const f32 duration = currentDuration();
        m_time += dt * m_speed;
        if (m_loop && duration > 0.001f) {
            while (m_time >= duration) {
                m_time -= duration;
            }
        } else if (m_time > duration) {
            m_time = duration;
            m_playing = false;
        }
    }

    void selectNext() noexcept { setClip((m_clip + 1U) % TAnimationSet::count); }
    void selectPrevious() noexcept { setClip(m_clip == 0U ? TAnimationSet::count - 1U : m_clip - 1U); }

    void setClip(const usize clip) noexcept {
        if (clip >= TAnimationSet::count) {
            return;
        }
        m_clip = clip;
        m_time = 0.0f;
    }

    void setScrub(const f32 value) noexcept {
        const f32 clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        m_time = currentDuration() * clamped;
    }

    void setSpeed(const f32 speed) noexcept {
        m_speed = speed < 0.1f ? 0.1f : (speed > 3.0f ? 3.0f : speed);
    }

    void togglePlaying() noexcept { m_playing = !m_playing; }
    void setPlaying(const bool playing) noexcept { m_playing = playing; }
    void toggleLoop() noexcept { m_loop = !m_loop; }
    void toggleMirror() noexcept { m_mirror = !m_mirror; }

    [[nodiscard]] HandAnimationPose sample(const HandSide side) const noexcept {
        return TAnimationSet::sample(m_clip, phase(), side);
    }

    [[nodiscard]] std::string_view currentName() const noexcept { return TAnimationSet::names()[m_clip]; }
    [[nodiscard]] f32 currentDuration() const noexcept { return TAnimationSet::durations()[m_clip]; }
    [[nodiscard]] f32 phase() const noexcept {
        const f32 duration = currentDuration();
        return duration > 0.001f ? m_time / duration : 0.0f;
    }
    [[nodiscard]] usize clip() const noexcept { return m_clip; }
    [[nodiscard]] f32 speed() const noexcept { return m_speed; }
    [[nodiscard]] bool playing() const noexcept { return m_playing; }
    [[nodiscard]] bool loop() const noexcept { return m_loop; }
    [[nodiscard]] bool mirror() const noexcept { return m_mirror; }

private:
    usize m_clip = 0U;
    f32 m_time = 0.0f;
    f32 m_speed = 1.0f;
    bool m_playing = true;
    bool m_loop = true;
    bool m_mirror = true;
};

} // namespace biofuel::engine::custom::procedural::animation
