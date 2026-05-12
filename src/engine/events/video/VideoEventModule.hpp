#pragma once

#include "engine/events/video/VideoEvents.hpp"
#include "engine/runtime/typed/EventDeclare.hpp"

namespace biofuel::engine::runtime::typed::video {
BIOFUEL_EVENT_TAG(Started, ::biofuel::engine::events::video::VideoStartedEvent);
BIOFUEL_EVENT_TAG(Completed, ::biofuel::engine::events::video::VideoCompletedEvent);
BIOFUEL_EVENT_TAG(Error, ::biofuel::engine::events::video::VideoErrorEvent);
} // namespace biofuel::engine::runtime::typed::video

namespace biofuel::engine::runtime::typed {
BIOFUEL_EVENT_SPEC(video::Started, "video.started");
BIOFUEL_EVENT_SPEC(video::Completed, "video.completed");
BIOFUEL_EVENT_SPEC(video::Error, "video.error");
BIOFUEL_EVENT_MODULE(VideoEventModule, VideoEvents, video::Started, video::Completed, video::Error)
} // namespace biofuel::engine::runtime::typed

