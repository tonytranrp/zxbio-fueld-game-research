#include "VideoScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "UI/ScreenFwd.hpp"
#include "Utils/video/VideoManager.hpp"
#include "Utils/render/Render.hpp"
#include <raylib.h>

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

// =============================================================================
// Preload
// =============================================================================

void VideoScreen::preloadVideo(std::string_view name, std::string_view path) {
    auto& vm = utils::video::VideoManager::instance();
    vm.init();
    if (!vm.hasVideo(name)) {
        vm.loadVideo(name, path);
    }
}

// =============================================================================
// Constructor
// =============================================================================

VideoScreen::VideoScreen(std::string_view videoName)
    : m_videoName{videoName} {}

// =============================================================================
// Lifecycle
// =============================================================================

void VideoScreen::onEnter() {
    setTransitionDuration(0.0f);
    m_inputDelay = 0.0f;
    m_inputReady = false;
    m_started = false;

    auto& vm = utils::video::VideoManager::instance();
    if (!vm.hasVideo(m_videoName)) return;

    vm.setLooping(m_videoName, m_looping);
    vm.play(m_videoName);
    m_started = vm.isPlaying(m_videoName);
}

void VideoScreen::onExit() {
    auto& vm = utils::video::VideoManager::instance();
    if (vm.isPlaying(m_videoName) || vm.isPaused(m_videoName)) {
        vm.stop(m_videoName);
    }
}

void VideoScreen::onUpdate(f32 dt) {
    if (!m_inputReady) {
        m_inputDelay += dt;
        if (m_inputDelay >= m_inputDelayDuration) {
            m_inputReady = true;
        }
    }
}

void VideoScreen::onRender() {
    using namespace utils::render;

    auto& vm = utils::video::VideoManager::instance();

    Texture2D frame = vm.getFrameTexture(m_videoName);
    if (m_started && frame.id != 0) {
        Renderer::drawFullscreenTexture(frame);
    } else {
        Renderer::drawFullscreen(FALLBACK_COLOR);
    }
}

void VideoScreen::onInput() {
    if (!m_inputReady || isTransitioning() || !m_skipOnAnyInput) return;

    if (dismissInputPressed()) {
        if (auto* sm = manager()) {
            sm->pop();
        }
    }
}

} // namespace biofuel::ui::screens

// =============================================================================
// Factory
// =============================================================================

std::unique_ptr<biofuel::ui::Screen> biofuel::ui::screens::makeVideoScreen(
    std::string_view videoName)
{
    return std::make_unique<biofuel::ui::screens::VideoScreen>(videoName);
}
