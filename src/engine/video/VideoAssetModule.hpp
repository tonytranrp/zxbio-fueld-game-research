#pragma once

#include "engine/runtime/typed/AssetDeclare.hpp"

namespace biofuel::engine::runtime::typed::video {
struct IdleAmbient {};
} // namespace biofuel::engine::runtime::typed::video

namespace biofuel::engine::runtime::typed {
BIOFUEL_VIDEO_ASSET(video::IdleAmbient, "idle", "assets/video/ssstik.io_1778485755339.mp4", false);
BIOFUEL_ASSET_MODULE(VideoAssetModule, VideoAssetRegistry, video::IdleAmbient)
} // namespace biofuel::engine::runtime::typed

