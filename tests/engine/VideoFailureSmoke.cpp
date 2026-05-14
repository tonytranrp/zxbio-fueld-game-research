#include <memory>
#include <string>
#include <unordered_map>

#include <raylib.h>

#define private public
#include "engine/video/VideoManager.hpp"
#undef private

namespace {

int require(const bool condition, const int code) {
    return condition ? 0 : code;
}

} // namespace

int main() {
    auto& videos = biofuel::engine::video::VideoManager::instance();
    videos.shutdown();
    videos.init();

    constexpr std::string_view name = "missing-video";
    videos.loadVideo(name, "assets/video/this-file-should-not-exist-for-video-failure-smoke.mp4");

    if (const int code = require(!videos.hasVideo(name), 1); code != 0) {
        return code;
    }
    if (const int code = require(videos.hasError(name), 2); code != 0) {
        return code;
    }
    if (const int code = require(!videos.errorMessage(name).empty(), 3); code != 0) {
        return code;
    }

    const auto it = videos.m_videos.find(name);
    if (const int code = require(it != videos.m_videos.end(), 4); code != 0) {
        return code;
    }
    if (const int code = require(!it->second.loaded, 5); code != 0) {
        return code;
    }
    if (const int code = require(it->second.error, 6); code != 0) {
        return code;
    }
    if (const int code = require(it->second.backend == nullptr, 7); code != 0) {
        return code;
    }

    videos.shutdown();
    return 0;
}
