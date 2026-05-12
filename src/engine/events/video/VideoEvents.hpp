#pragma once

#include <string_view>

namespace biofuel::engine::events::video {

// ------------------------------------------------------------------------------
// Video playback events — fired by VideoManager during its update() cycle.
// Screens subscribe via entt to react to completions or errors.
// ------------------------------------------------------------------------------

// Fired when video playback begins (via VideoManager::play())
struct VideoStartedEvent {
    std::string_view videoName;
};

// Fired when a video reaches end-of-file (including looping enabled — fires
// after each loop iteration). Note: hasEnded() will be true when this fires.
struct VideoCompletedEvent {
    std::string_view videoName;
};

// Fired when the video backend reports an irrecoverable error (missing decoder,
// missing file, corrupt file, etc.)
// The video will stop playing and hasError() will return true.
struct VideoErrorEvent {
    std::string_view videoName;
    std::string_view errorMessage;
};

} // namespace biofuel::engine::events::video
