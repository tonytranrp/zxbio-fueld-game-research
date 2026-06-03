#pragma once

#include "VideoManager.hpp"
#include <memory>
#include <string>
#include <string_view>

namespace biofuel::engine::video {

// Result of a single backend update() pump.
struct VideoUpdate {
    bool completed = false;
    bool error = false;
    std::string errorMessage;
};

// Abstract decode/playback backend behind VideoManager. The FFmpeg subprocess
// implementation lives in VideoFfmpegBackend.cpp; the manager facade in
// VideoManager.cpp only talks to this interface.
struct VideoManager::Backend {
    virtual ~Backend() = default;
    virtual bool load(std::string_view path, std::string& error) = 0;
    virtual void unload() noexcept = 0;
    virtual bool play(bool looping, f32 volume, std::string& error) = 0;
    virtual void stop() noexcept = 0;
    virtual void pause() noexcept = 0;
    virtual void resume() noexcept = 0;
    virtual VideoUpdate update() = 0;
    virtual void setLooping(bool looping) noexcept = 0;
    virtual void setVolume(f32 volume) noexcept = 0;
    [[nodiscard]] virtual Texture2D texture() const noexcept = 0;
};

// Factory for the platform default backend (FFmpeg subprocess).
[[nodiscard]] std::unique_ptr<VideoManager::Backend> makeBackend();

} // namespace biofuel::engine::video
