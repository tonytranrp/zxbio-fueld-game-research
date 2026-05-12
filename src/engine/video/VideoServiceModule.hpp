#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/video/VideoManager.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(VideoService);
BIOFUEL_RUNTIME_SERVICE(VideoService, "service.video", ::biofuel::engine::video::VideoManager,
    ::biofuel::engine::video::VideoManager::instance());
BIOFUEL_SERVICE_MODULE(VideoServiceModule, VideoService)
} // namespace biofuel::engine::runtime::typed

