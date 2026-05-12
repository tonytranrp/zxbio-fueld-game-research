#include "VideoScreen.hpp"
#include "VideoScreenModule.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/video/VideoManager.hpp"
#include "engine/graphics/Render.hpp"
#include <raylib.h>

namespace biofuel::game::screens {

namespace {

[[nodiscard]] bool dismissInputPressed() noexcept {
    return IsKeyDown(KEY_SPACE) ||
        IsKeyDown(KEY_ENTER) ||
        IsKeyDown(KEY_ESCAPE) ||
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) ||
        IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);
}

} // namespace

} // namespace biofuel::game::screens

namespace biofuel::engine::ui::typed {

bool videoscreen::VideoFrameTag::visible(const ::biofuel::game::screens::VideoScreen& screen) noexcept {
    return screen.started();
}

Texture2D videoscreen::VideoFrameTag::frame(const ::biofuel::game::screens::VideoScreen& screen) noexcept {
    return ::biofuel::engine::runtime::Runtime::video().getFrameTexture(screen.videoName());
}

bool videoscreen::FallbackColorTag::visible(const ::biofuel::game::screens::VideoScreen& screen) noexcept {
    const Texture2D frame = ::biofuel::engine::runtime::Runtime::video().getFrameTexture(screen.videoName());
    return !screen.started() || frame.id == 0;
}

Color videoscreen::FallbackColorTag::color(const ::biofuel::game::screens::VideoScreen& screen) noexcept {
    (void)screen;
    return ::biofuel::game::screens::VideoScreen::fallbackColor();
}

} // namespace biofuel::engine::ui::typed

namespace biofuel::game::screens {

// =============================================================================
// Preload
// =============================================================================

void VideoScreen::preloadVideo(std::string_view name, std::string_view path) {
    auto& vm = ::biofuel::engine::runtime::Runtime::video();
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

    auto& vm = ::biofuel::engine::runtime::Runtime::video();
    if (!vm.hasVideo(m_videoName)) return;

    vm.setLooping(m_videoName, m_looping);
    vm.play(m_videoName);
    m_started = vm.isPlaying(m_videoName);
}

void VideoScreen::onExit() {
    auto& vm = ::biofuel::engine::runtime::Runtime::video();
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
    ::biofuel::engine::ui::typed::RenderContext context{
        .manager = manager(),
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = screenId(),
        .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
        .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
        .transitionAlpha = transitionAlpha(),
        .frameTime = GetFrameTime(),
    };
    ::biofuel::engine::ui::typed::RenderPipeline<VideoScreen>::render(*this, context);
}

void VideoScreen::onInput() {
    if (!m_inputReady || isTransitioning() || !m_skipOnAnyInput) return;

    if (dismissInputPressed()) {
        if (auto* sm = manager()) {
            sm->pop();
        }
    }
}

} // namespace biofuel::game::screens
