#pragma once

#include "engine/core/Types.hpp"

namespace biofuel::engine::video {

struct VideoBufferPolicyData {
    size_t maxVideoFrames = 4;
    size_t maxAudioChunks = 16;
    size_t minVideoPrefillFrames = 2;
    size_t minAudioPrefillChunks = 3;
};

constexpr VideoBufferPolicyData kIdleBufferPolicy{};

} // namespace biofuel::engine::video
