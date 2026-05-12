#pragma once

#include "engine/core/Types.hpp"
#include "engine/video/VideoAssetModule.hpp"

namespace biofuel::engine::video {

struct VideoBufferPolicyData {
    size_t maxVideoFrames = 4;
    size_t maxAudioChunks = 16;
    size_t minVideoPrefillFrames = 2;
    size_t minAudioPrefillChunks = 3;
};

template<typename TVideo>
struct VideoBufferPolicy {
    static constexpr VideoBufferPolicyData value{};
};

template<>
struct VideoBufferPolicy<::biofuel::engine::runtime::typed::video::IdleAmbient> {
    static constexpr VideoBufferPolicyData value{
        .maxVideoFrames = 4,
        .maxAudioChunks = 16,
        .minVideoPrefillFrames = 2,
        .minAudioPrefillChunks = 3,
    };
};

} // namespace biofuel::engine::video
