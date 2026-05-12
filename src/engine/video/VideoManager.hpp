#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace biofuel::engine::video {

// -----------------------------------------------------------------------------
// VideoManager - Singleton video playback manager.
//
// Windows builds use ffmpeg.exe as an external decoder process. FFmpeg decodes
// MP4 video to raw RGBA frames and audio to raw PCM; the game uploads those
// buffers into Raylib Texture2D / AudioStream objects. Non-Windows builds keep
// the same API and fail cleanly until a native backend is added.
// -----------------------------------------------------------------------------
class VideoManager {
public:
    struct Backend;

    [[nodiscard]] static VideoManager& instance() noexcept;

    void init();
    void shutdown() noexcept;
    void update();

    void loadVideo(std::string_view name, std::string_view path);
    void unloadVideo(std::string_view name);
    void unloadAllVideos() noexcept;

    void play(std::string_view name);
    void stop(std::string_view name) noexcept;
    void pause(std::string_view name) noexcept;
    void resume(std::string_view name) noexcept;

    [[nodiscard]] bool hasVideo(std::string_view name) const noexcept;
    [[nodiscard]] bool isPlaying(std::string_view name) const noexcept;
    [[nodiscard]] bool isPaused(std::string_view name) const noexcept;
    [[nodiscard]] bool hasEnded(std::string_view name) const noexcept;
    [[nodiscard]] bool hasError(std::string_view name) const noexcept;
    [[nodiscard]] std::string_view errorMessage(std::string_view name) const noexcept;

    [[nodiscard]] Texture2D getFrameTexture(std::string_view name) const noexcept;

    void setLooping(std::string_view name, bool loop);
    void setVolume(std::string_view name, f32 volume);
    void mute(std::string_view name) noexcept;
    void unmute(std::string_view name) noexcept;

    VideoManager(const VideoManager&) = delete;
    VideoManager& operator=(const VideoManager&) = delete;
    VideoManager(VideoManager&&) = delete;
    VideoManager& operator=(VideoManager&&) = delete;

private:
    VideoManager() = default;
    ~VideoManager() noexcept;

    struct VideoInstance {
        std::string name;
        std::string path;
        std::unique_ptr<Backend> backend;
        bool loaded = false;
        bool playing = false;
        bool paused = false;
        bool looping = false;
        bool ended = false;
        bool error = false;
        f32 volume = 1.0f;
        std::string errorMessage;
    };

    void setError(VideoInstance& inst, std::string_view message);
    [[nodiscard]] VideoInstance* findVideo(std::string_view name) noexcept;
    [[nodiscard]] const VideoInstance* findVideo(std::string_view name) const noexcept;

    std::unordered_map<std::string, VideoInstance, TransparentHash, std::equal_to<>> m_videos;
    bool m_initialized = false;
};

} // namespace biofuel::engine::video
