#include "ModelKeyframe.hpp"
#include <algorithm>
#include <cmath>

namespace biofuel::engine::animation::model {

namespace {

[[nodiscard]] constexpr Quaternion IDENTITY_QUATERNION() noexcept {
    return Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
}

[[nodiscard]] constexpr Vector3 ONE_VECTOR() noexcept {
    return Vector3{1.0f, 1.0f, 1.0f};
}

[[nodiscard]] constexpr Vector3 ZERO_VECTOR() noexcept {
    return Vector3{0.0f, 0.0f, 0.0f};
}

} // namespace

i32 ModelRigBinding::findBoneIndex(const std::string_view boneName) const noexcept {
    const auto it = boneIndices.find(std::string{boneName});
    if (it == boneIndices.end()) {
        return -1;
    }
    return it->second;
}

void ModelKeyframePlayer::configure(ModelRigBinding rig, std::vector<KeyframeClip> clips) noexcept {
    m_rig = std::move(rig);
    m_clips.clear();
    m_clips.reserve(clips.size());

    for (auto& clip : clips) {
        ResolvedClip resolved;
        resolved.name = std::move(clip.name);
        resolved.durationSeconds = std::max(clip.durationSeconds, 0.0f);
        resolved.loop = clip.loop;
        resolved.rootTranslation = std::move(clip.rootTranslation);
        resolved.rootRotation = std::move(clip.rootRotation);
        resolved.rootScale = std::move(clip.rootScale);
        resolved.scalarTracks = std::move(clip.scalarTracks);

        resolved.boneTracks.reserve(clip.boneTracks.size());
        for (auto& boneTrack : clip.boneTracks) {
            const i32 boneIndex = m_rig.findBoneIndex(boneTrack.boneName);
            if (boneIndex < 0) {
                continue;
            }

            resolved.boneTracks.push_back(ResolvedBoneTrack{
                .boneIndex = boneIndex,
                .translation = std::move(boneTrack.translation),
                .rotation = std::move(boneTrack.rotation),
                .scale = std::move(boneTrack.scale),
            });
        }

        m_clips.push_back(std::move(resolved));
    }

    m_currentSamples.assign(m_rig.boneNames.size(), BoneChannelSample{});
    m_previousSamples.assign(m_rig.boneNames.size(), BoneChannelSample{});
    reset();
}

void ModelKeyframePlayer::reset() noexcept {
    m_state.currentClipName.clear();
    m_state.previousClipName.clear();
    m_state.clipProgress = 0.0f;
    m_state.transitionProgress = 1.0f;
    m_state.rootTranslation = ZERO_VECTOR();
    m_state.rootRotation = IDENTITY_QUATERNION();
    m_state.rootScale = ONE_VECTOR();
    m_state.scalars.clear();
    std::fill(m_currentSamples.begin(), m_currentSamples.end(), BoneChannelSample{});
    std::fill(m_previousSamples.begin(), m_previousSamples.end(), BoneChannelSample{});
    m_currentPose = {};
    m_previousPose = {};
}

void ModelKeyframePlayer::syncState(
    const std::string_view clipName,
    const f32 clipProgress,
    const f32 transitionProgress) noexcept
{
    if (clipName.empty() || findClip(clipName) == nullptr) {
        m_state.currentClipName.clear();
        m_state.previousClipName.clear();
        m_state.clipProgress = 0.0f;
        m_state.transitionProgress = 1.0f;
        return;
    }

    if (m_state.currentClipName != clipName) {
        m_state.previousClipName = m_state.currentClipName;
        m_state.currentClipName = std::string{clipName};
    } else if (transitionProgress >= 1.0f) {
        m_state.previousClipName.clear();
    }

    m_state.clipProgress = std::clamp(clipProgress, 0.0f, 1.0f);
    m_state.transitionProgress = std::clamp(transitionProgress, 0.0f, 1.0f);
}

void ModelKeyframePlayer::apply(const std::span<const Transform> bindPose, const std::span<Transform> outPose) noexcept {
    if (bindPose.empty() || outPose.empty() || bindPose.size() != outPose.size()) {
        return;
    }

    std::copy(bindPose.begin(), bindPose.end(), outPose.begin());
    std::fill(m_currentSamples.begin(), m_currentSamples.end(), BoneChannelSample{});
    std::fill(m_previousSamples.begin(), m_previousSamples.end(), BoneChannelSample{});
    m_state.scalars.clear();
    m_state.rootTranslation = ZERO_VECTOR();
    m_state.rootRotation = IDENTITY_QUATERNION();
    m_state.rootScale = ONE_VECTOR();

    const ResolvedClip* currentClip = findClip(m_state.currentClipName);
    if (currentClip == nullptr) {
        return;
    }

    sampleClip(*currentClip, m_state.clipProgress, m_currentSamples, m_currentPose);

    const ResolvedClip* previousClip = nullptr;
    const bool blendFromPrevious = !m_state.previousClipName.empty() && (m_state.transitionProgress < 1.0f);
    if (blendFromPrevious) {
        previousClip = findClip(m_state.previousClipName);
        if (previousClip != nullptr) {
            sampleClip(*previousClip, 1.0f, m_previousSamples, m_previousPose);
        }
    }

    const f32 blend = (previousClip != nullptr) ? m_state.transitionProgress : 1.0f;
    m_state.rootTranslation = KeyframeValueSampler<Vector3>::interpolate(
        (previousClip != nullptr) ? m_previousPose.rootTranslation : ZERO_VECTOR(),
        m_currentPose.rootTranslation,
        blend
    );
    m_state.rootRotation = KeyframeValueSampler<Quaternion>::interpolate(
        (previousClip != nullptr) ? m_previousPose.rootRotation : IDENTITY_QUATERNION(),
        m_currentPose.rootRotation,
        blend
    );
    m_state.rootScale = KeyframeValueSampler<Vector3>::interpolate(
        (previousClip != nullptr) ? m_previousPose.rootScale : ONE_VECTOR(),
        m_currentPose.rootScale,
        blend
    );

    for (size_t boneIndex = 0; boneIndex < outPose.size(); ++boneIndex) {
        const auto& base = bindPose[boneIndex];
        const auto& current = m_currentSamples[boneIndex];
        const auto& previous = m_previousSamples[boneIndex];

        const Vector3 translationA = previous.hasTranslation ? previous.translation : ZERO_VECTOR();
        const Vector3 translationB = current.hasTranslation ? current.translation : ZERO_VECTOR();
        const Quaternion rotationA = previous.hasRotation ? previous.rotation : IDENTITY_QUATERNION();
        const Quaternion rotationB = current.hasRotation ? current.rotation : IDENTITY_QUATERNION();
        const Vector3 scaleA = previous.hasScale ? previous.scale : ONE_VECTOR();
        const Vector3 scaleB = current.hasScale ? current.scale : ONE_VECTOR();

        const Vector3 translation = KeyframeValueSampler<Vector3>::interpolate(translationA, translationB, blend);
        const Quaternion rotation = KeyframeValueSampler<Quaternion>::interpolate(rotationA, rotationB, blend);
        const Vector3 scale = KeyframeValueSampler<Vector3>::interpolate(scaleA, scaleB, blend);

        outPose[boneIndex].translation = Vector3Add(base.translation, translation);
        outPose[boneIndex].rotation = QuaternionNormalize(QuaternionMultiply(base.rotation, rotation));
        outPose[boneIndex].scale = Vector3{
            base.scale.x * scale.x,
            base.scale.y * scale.y,
            base.scale.z * scale.z,
        };
    }

    const auto accumulateScalars =
        [blend, this](const std::unordered_map<std::string, f32>& source, const bool isCurrent) {
            for (const auto& [name, value] : source) {
                const f32 currentValue = m_state.scalars.contains(name) ? m_state.scalars[name] : 0.0f;
                if (isCurrent) {
                    m_state.scalars[name] = currentValue + value * blend;
                } else {
                    m_state.scalars[name] = currentValue + value * (1.0f - blend);
                }
            }
        };

    if (previousClip != nullptr) {
        accumulateScalars(m_previousPose.scalars, false);
    }
    accumulateScalars(m_currentPose.scalars, true);
}

f32 ModelKeyframePlayer::scalar(const std::string_view channelName, const f32 fallback) const noexcept {
    const auto it = m_state.scalars.find(std::string{channelName});
    if (it == m_state.scalars.end()) {
        return fallback;
    }
    return it->second;
}

const ModelKeyframePlayer::ResolvedClip* ModelKeyframePlayer::findClip(const std::string_view clipName) const noexcept {
    const auto it = std::find_if(
        m_clips.begin(),
        m_clips.end(),
        [clipName](const ResolvedClip& clip) { return clip.name == clipName; });
    if (it == m_clips.end()) {
        return nullptr;
    }
    return &(*it);
}

void ModelKeyframePlayer::sampleClip(
    const ResolvedClip& clip,
    const f32 clipProgress,
    const std::span<BoneChannelSample> outSamples,
    PoseSample& outPose) noexcept
{
    outPose.rootTranslation = clip.rootTranslation.sample(
        clip.durationSeconds * std::clamp(clipProgress, 0.0f, 1.0f),
        clip.durationSeconds,
        clip.loop,
        ZERO_VECTOR());
    outPose.rootRotation = clip.rootRotation.sample(
        clip.durationSeconds * std::clamp(clipProgress, 0.0f, 1.0f),
        clip.durationSeconds,
        clip.loop,
        IDENTITY_QUATERNION());
    outPose.rootScale = clip.rootScale.sample(
        clip.durationSeconds * std::clamp(clipProgress, 0.0f, 1.0f),
        clip.durationSeconds,
        clip.loop,
        ONE_VECTOR());
    outPose.scalars.clear();

    const f32 sampleTime = clip.durationSeconds * std::clamp(clipProgress, 0.0f, 1.0f);
    for (const auto& scalarTrack : clip.scalarTracks) {
        outPose.scalars[scalarTrack.channelName] = scalarTrack.track.sample(
            sampleTime,
            clip.durationSeconds,
            clip.loop,
            0.0f);
    }

    for (const auto& track : clip.boneTracks) {
        if (track.boneIndex < 0 || static_cast<size_t>(track.boneIndex) >= outSamples.size()) {
            continue;
        }

        auto& sample = outSamples[track.boneIndex];
        if (!track.translation.empty()) {
            sample.hasTranslation = true;
            sample.translation = track.translation.sample(sampleTime, clip.durationSeconds, clip.loop, ZERO_VECTOR());
        }
        if (!track.rotation.empty()) {
            sample.hasRotation = true;
            sample.rotation = track.rotation.sample(sampleTime, clip.durationSeconds, clip.loop, IDENTITY_QUATERNION());
        }
        if (!track.scale.empty()) {
            sample.hasScale = true;
            sample.scale = track.scale.sample(sampleTime, clip.durationSeconds, clip.loop, ONE_VECTOR());
        }
    }
}

} // namespace biofuel::engine::animation::model
