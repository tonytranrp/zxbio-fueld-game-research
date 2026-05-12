#include "IdleScreen.hpp"
#include "IdleScreenModule.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/graphics/shaders/MainMenuBgModule.hpp"
#include "engine/audio/AudioManager.hpp"
#include "engine/video/VideoManager.hpp"
#include <filesystem>
#include <raylib.h>
#include <spdlog/spdlog.h>

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

bool idle::VideoFrameTag::visible(const ::biofuel::game::screens::IdleScreen& screen) noexcept {
    return screen.videoMode();
}

Texture2D idle::VideoFrameTag::frame(const ::biofuel::game::screens::IdleScreen& screen) noexcept {
    return ::biofuel::engine::runtime::Runtime::video().getFrameTexture(screen.idleVideoName());
}

bool idle::FallbackColorTag::visible(const ::biofuel::game::screens::IdleScreen& screen) noexcept {
    if (!screen.videoMode() && !screen.fallbackBackdropReady()) {
        return true;
    }

    if (!screen.videoMode()) {
        return false;
    }

    const Texture2D frame = ::biofuel::engine::runtime::Runtime::video().getFrameTexture(screen.idleVideoName());
    return frame.id == 0;
}

Color idle::FallbackColorTag::color(const ::biofuel::game::screens::IdleScreen& screen) noexcept {
    (void)screen;
    return ::biofuel::game::screens::IdleScreen::fallbackColor();
}

bool idle::FallbackBackdropTag::visible(const ::biofuel::game::screens::IdleScreen& screen) noexcept {
    return !screen.videoMode() && screen.fallbackBackdropReady();
}

void idle::FallbackBackdropTag::render(const ::biofuel::game::screens::IdleScreen& screen, RenderContext& context) {
    (void)context;
    screen.renderFallbackBackdrop();
}

} // namespace biofuel::engine::ui::typed

namespace biofuel::game::screens {

std::string IdleScreen::idleVideoPath() {
    const std::filesystem::path videoPath{VIDEO_PATH};
    if (!std::filesystem::is_regular_file(videoPath)) {
        return {};
    }

    return std::string{VIDEO_PATH};
}

void IdleScreen::onEnter() {
    ::biofuel::engine::debug::MemoryTelemetry::snapshot("idle.open.begin");
    m_inputDelay = 0.0f;
    m_inputReady = false;
    m_videoMode = false;

    if (!m_idleVideoName.empty()) {
        auto& vm = ::biofuel::engine::runtime::Runtime::video();
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
        auto& vm = ::biofuel::engine::runtime::Runtime::video();
        if (vm.isPlaying(m_idleVideoName) || vm.isPaused(m_idleVideoName)) {
            vm.stop(m_idleVideoName);
        }
        vm.unloadVideo(m_idleVideoName);
        m_videoMode = false;
        ::biofuel::engine::debug::MemoryTelemetry::snapshot("idle.close.video");
        return;
    }

    ::biofuel::engine::runtime::Runtime::audio().stopMusic();
    ::biofuel::engine::debug::MemoryTelemetry::snapshot("idle.close.fallback");
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
    ::biofuel::engine::ui::typed::RenderContext context{
        .manager = manager(),
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = screenId(),
        .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
        .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
        .transitionAlpha = transitionAlpha(),
        .frameTime = GetFrameTime(),
    };
    ::biofuel::engine::ui::typed::RenderPipeline<IdleScreen>::render(*this, context);
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
    m_backdrop.configure(game::presentation::effects::ScreenBackdropConfig{
        .shaderName = ::biofuel::engine::graphics::shader::MainMenuBgModule::NAME,
        .fallbackColor = BG_COLOR,
        .revealDelay = 0.0f,
        .revealDuration = 0.01f,
    });
    m_backdrop.reset();

    auto& audio = ::biofuel::engine::runtime::Runtime::audio();
    if (!audio.hasMusic(MUSIC_PATH)) {
        audio.loadMusic(MUSIC_PATH, MUSIC_PATH);
    }
    audio.playMusic(MUSIC_PATH);
}

} // namespace biofuel::game::screens
