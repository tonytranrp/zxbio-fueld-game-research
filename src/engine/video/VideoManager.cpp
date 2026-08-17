#include "VideoManager.hpp"
#include "VideoBackend.hpp"
#include "engine/runtime/typed/Events.hpp"
#include <algorithm>
#include <utility>
#include <spdlog/spdlog.h>

namespace biofuel::engine::video {


VideoManager& VideoManager::instance() noexcept {
    static VideoManager mgr;
    return mgr;
}

VideoManager::~VideoManager() noexcept {
    shutdown();
}

void VideoManager::init() {
    initGlobalAudioSettings();
    m_initialized = true;
}

void VideoManager::shutdown() noexcept {
    unloadAllVideos();
    m_initialized = false;
}

void VideoManager::update() {
    if (!m_initialized) {
        return;
    }

    for (auto& [_, inst] : m_videos) {
        if (!inst.loaded || inst.error || !inst.backend) {
            continue;
        }

        const VideoUpdate result = inst.backend->update();
        if (result.error) {
            setError(inst, result.errorMessage);
            continue;
        }
        if (result.completed) {
            ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Completed>({
                .videoName = inst.name
            });
            if (!inst.looping) {
                inst.playing = false;
                inst.ended = true;
            }
        }
    }
}

void VideoManager::loadVideo(std::string_view name, std::string_view path) {
    if (!m_initialized) {
        init();
    }

    const std::string key{name};
    unloadVideo(key);

    VideoInstance inst;
    inst.name = key;
    inst.path = std::string{path};
    inst.backend = makeBackend();

    std::string error;
    if (!inst.backend || !inst.backend->load(inst.path, error)) {
        inst.loaded = false;
        inst.error = true;
        inst.errorMessage = error.empty() ? "Video backend failed to load the file" : std::move(error);
        if (inst.backend) {
            inst.backend->unload();
            inst.backend.reset();
        }
        spdlog::error("VideoManager: failed to load '{}': {}", inst.path, inst.errorMessage);
        m_videos.emplace(key, std::move(inst));
        ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Error>({
            .videoName = m_videos.at(key).name,
            .errorMessage = m_videos.at(key).errorMessage
        });
        return;
    }

    inst.loaded = true;
    spdlog::info("VideoManager: loaded '{}'", inst.path);
    m_videos.emplace(key, std::move(inst));
}

void VideoManager::unloadVideo(std::string_view name) {
    const auto it = m_videos.find(name);
    if (it == m_videos.end()) {
        return;
    }
    if (it->second.backend) {
        it->second.backend->unload();
    }
    m_videos.erase(it);
}

void VideoManager::unloadAllVideos() noexcept {
    for (auto& [_, inst] : m_videos) {
        if (inst.backend) {
            inst.backend->unload();
        }
    }
    m_videos.clear();
}

void VideoManager::play(std::string_view name) {
    auto* inst = findVideo(name);
    if (!inst || !inst->loaded || inst->error || !inst->backend) {
        return;
    }

    std::string error;
    if (!inst->backend->play(inst->looping, inst->volume, error)) {
        setError(*inst, error);
        return;
    }

    inst->playing = true;
    inst->paused = false;
    inst->ended = false;
    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Started>({
        .videoName = inst->name
    });
}

void VideoManager::stop(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (!inst || !inst->backend) {
        return;
    }
    inst->backend->stop();
    inst->playing = false;
    inst->paused = false;
}

void VideoManager::pause(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (!inst || !inst->backend || !inst->playing) {
        return;
    }
    inst->backend->pause();
    inst->paused = true;
}

void VideoManager::resume(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (!inst || !inst->backend || !inst->playing) {
        return;
    }
    inst->backend->resume();
    inst->paused = false;
}

bool VideoManager::hasVideo(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->loaded && !inst->error;
}

bool VideoManager::isPlaying(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->playing && !inst->paused;
}

bool VideoManager::isPaused(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->paused;
}

bool VideoManager::hasEnded(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->ended;
}

bool VideoManager::hasError(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst && inst->error;
}

std::string_view VideoManager::errorMessage(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    return inst ? std::string_view{inst->errorMessage} : std::string_view{};
}

Texture2D VideoManager::getFrameTexture(std::string_view name) const noexcept {
    const auto* inst = findVideo(name);
    if (!inst || !inst->loaded || inst->error || !inst->backend) {
        return {};
    }
    return inst->backend->texture();
}

void VideoManager::setLooping(std::string_view name, const bool loop) {
    auto* inst = findVideo(name);
    if (!inst) {
        return;
    }
    inst->looping = loop;
    if (inst->backend) {
        inst->backend->setLooping(loop);
    }
}

void VideoManager::setVolume(std::string_view name, const f32 volume) {
    auto* inst = findVideo(name);
    if (!inst) {
        return;
    }
    inst->volume = std::clamp(volume, 0.0f, 1.0f);
    if (inst->backend) {
        inst->backend->setVolume(inst->volume);
    }
}

void VideoManager::mute(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (inst && inst->backend) {
        inst->volume = 0.0f;
        inst->backend->setVolume(0.0f);
    }
}

void VideoManager::unmute(std::string_view name) noexcept {
    auto* inst = findVideo(name);
    if (inst && inst->backend) {
        inst->volume = 1.0f;
        inst->backend->setVolume(1.0f);
    }
}

void VideoManager::setError(VideoInstance& inst, std::string_view message) {
    inst.playing = false;
    inst.paused = false;
    inst.error = true;
    inst.errorMessage = message.empty() ? "Unknown video error" : std::string{message};
    if (inst.backend) {
        inst.backend->stop();
    }
    spdlog::error("VideoManager: '{}': {}", inst.name, inst.errorMessage);
    ::biofuel::engine::runtime::typed::Events::publish<::biofuel::engine::runtime::typed::video::Error>({
        .videoName = inst.name,
        .errorMessage = inst.errorMessage
    });
}

VideoManager::VideoInstance* VideoManager::findVideo(std::string_view name) noexcept {
    const auto it = m_videos.find(name);
    return it == m_videos.end() ? nullptr : &it->second;
}

const VideoManager::VideoInstance* VideoManager::findVideo(std::string_view name) const noexcept {
    const auto it = m_videos.find(name);
    return it == m_videos.end() ? nullptr : &it->second;
}

} // namespace biofuel::engine::video
