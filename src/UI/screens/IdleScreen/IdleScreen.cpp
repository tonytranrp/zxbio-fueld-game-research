#include "IdleScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "UI/ScreenFwd.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/Shader/MainMenuBgModule.hpp"
#include "Utils/audio/AudioManager.hpp"
#include "Utils/video/VideoManager.hpp"
#include <filesystem>
#include <raylib.h>
#include <spdlog/spdlog.h>

namespace biofuel::ui::screens {

namespace {

[[nodiscard]] bool dismissInputPressed() noexcept {
    return IsKeyDown(KEY_SPACE) ||
        IsKeyDown(KEY_ENTER) ||
        IsKeyDown(KEY_ESCAPE) ||
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) ||
        IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);
}

} // namespace

std::string IdleScreen::idleVideoPath() {
    const std::filesystem::path videoPath{VIDEO_PATH};
    if (!std::filesystem::is_regular_file(videoPath)) {
        return {};
    }

    return std::string{VIDEO_PATH};
}

void IdleScreen::preloadAssets() {
    auto& audio = utils::audio::AudioManager::instance();
    if (!audio.hasMusic(MUSIC_PATH)) {
        audio.loadMusic(MUSIC_PATH, MUSIC_PATH);
    }

    const std::string videoPath = idleVideoPath();
    if (videoPath.empty()) {
        spdlog::warn("IdleScreen: no local MP4 found under assets/video");
        return;
    }

    auto& vm = utils::video::VideoManager::instance();
    vm.init();
    if (!vm.hasVideo(videoPath) && !vm.hasError(videoPath)) {
        vm.loadVideo(videoPath, videoPath);
        if (vm.hasVideo(videoPath)) {
            vm.setLooping(videoPath, true);
            vm.setVolume(videoPath, 1.0f);
        }
    }
}

void IdleScreen::onEnter() {
    m_inputDelay = 0.0f;
    m_inputReady = false;
    m_videoMode = false;

    if (!m_idleVideoName.empty()) {
        auto& vm = utils::video::VideoManager::instance();
        if (!vm.hasVideo(m_idleVideoName) && !vm.hasError(m_idleVideoName)) {
            vm.loadVideo(m_idleVideoName, m_idleVideoName);
        }

        if (vm.hasVideo(m_idleVideoName)) {
            vm.setLooping(m_idleVideoName, true);
            vm.setVolume(m_idleVideoName, 1.0f);
            vm.play(m_idleVideoName);
            m_videoMode = vm.isPlaying(m_idleVideoName);
            if (m_videoMode) {
                return;
            }
        }

        spdlog::warn("IdleScreen: video unavailable, using shader fallback");
    }

    startFallbackBackdrop();
}

void IdleScreen::onExit() {
    if (m_videoMode && !m_idleVideoName.empty()) {
        auto& vm = utils::video::VideoManager::instance();
        if (vm.isPlaying(m_idleVideoName) || vm.isPaused(m_idleVideoName)) {
            vm.stop(m_idleVideoName);
        }
        m_videoMode = false;
        return;
    }

    utils::audio::AudioManager::instance().stopMusic();
}

void IdleScreen::onUpdate(const f32 dt) {
    if (!m_videoMode) {
        m_backdrop.update(dt);
        m_backdrop.setFloat("uIdleDim", 1.0f);
    }

    if (!m_inputReady && m_inputDelay < INPUT_DELAY) {
        m_inputDelay += dt;
        if (m_inputDelay >= INPUT_DELAY) {
            m_inputReady = true;
        }
    }
}

void IdleScreen::onRender() {
    using namespace utils::render;

    if (m_videoMode) {
        const Texture2D frame =
            utils::video::VideoManager::instance().getFrameTexture(m_idleVideoName);
        if (frame.id != 0) {
            Renderer::drawFullscreenTexture(frame);
        } else {
            Renderer::drawFullscreen(BG_COLOR);
        }
        return;
    }

    if (!m_backdrop.shader().id) {
        Renderer::drawFullscreen(BG_COLOR);
        return;
    }

    m_backdrop.render(1.0f);
}

void IdleScreen::onInput() {
    if (!m_inputReady || isTransitioning()) {
        return;
    }

    if (dismissInputPressed() && manager()) {
        manager()->pop();
    }
}

void IdleScreen::startFallbackBackdrop() {
    m_backdrop.configure(animation::screen::ScreenBackdropConfig{
        .shaderName = utils::render::shader::MainMenuBgModule::NAME,
        .fallbackColor = BG_COLOR,
        .revealDelay = 0.0f,
        .revealDuration = 0.01f,
    });
    m_backdrop.reset();

    auto& audio = utils::audio::AudioManager::instance();
    if (!audio.hasMusic(MUSIC_PATH)) {
        audio.loadMusic(MUSIC_PATH, MUSIC_PATH);
    }
    audio.playMusic(MUSIC_PATH);
}

} // namespace biofuel::ui::screens

std::unique_ptr<biofuel::ui::Screen> biofuel::ui::screens::makeIdle() {
    return std::make_unique<biofuel::ui::screens::IdleScreen>();
}
