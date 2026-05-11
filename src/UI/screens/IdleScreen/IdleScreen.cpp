#include "IdleScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "UI/ScreenFwd.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/Shader/MainMenuBgModule.hpp"
#include "Utils/audio/AudioManager.hpp"
#include <raylib.h>
#include <algorithm>

namespace biofuel::ui::screens {

void IdleScreen::preloadAssets() {
    auto& audio = utils::audio::AudioManager::instance();
    if (!audio.hasMusic(MUSIC_PATH)) {
        audio.loadMusic(MUSIC_PATH, MUSIC_PATH);
    }
}

void IdleScreen::onEnter() {
    m_inputDelay = 0.0f;
    m_inputReady = false;

    // Use the same main menu shader, fully dimmed → hits early-out = zero GPU cost.
    // MainMenuScreen already dimmed it to 1.0 before pushing; we set it to 1.0
    // immediately — no fade-in, that would cause a double-brightening flicker.
    m_backdrop.configure(animation::screen::ScreenBackdropConfig{
        .shaderName = utils::render::shader::MainMenuBgModule::NAME,
        .fallbackColor = BG_COLOR,
        .revealDelay = 0.0f,
        .revealDuration = 0.01f,
    });
    m_backdrop.reset();

    // Start ambient music
    auto& audio = utils::audio::AudioManager::instance();
    if (!audio.hasMusic(MUSIC_PATH)) {
        audio.loadMusic(MUSIC_PATH, MUSIC_PATH);
    }
    audio.playMusic(MUSIC_PATH);
}

void IdleScreen::onExit() {
    utils::audio::AudioManager::instance().stopMusic();
}

void IdleScreen::onUpdate(f32 dt) {
    m_backdrop.update(dt);

    // Brief input delay to prevent accidental instant dismissal on wake
    if (!m_inputReady && m_inputDelay < INPUT_DELAY) {
        m_inputDelay += dt;
        if (m_inputDelay >= INPUT_DELAY) {
            m_inputReady = true;
        }
    }

    // Keep shader fully dimmed at all times — MainMenuScreen already dimmed it
    m_backdrop.setFloat("uIdleDim", 1.0f);
}

void IdleScreen::onRender() {
    using namespace utils::render;

    if (!m_backdrop.shader().id) {
        Renderer::drawFullscreen(BG_COLOR);
        return;
    }

    m_backdrop.render(1.0f);
}

void IdleScreen::onInput() {
    if (!m_inputReady) return;
    if (isTransitioning()) return;

    const Vector2 mouseDelta = GetMouseDelta();
    const bool anyInput = (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f || GetKeyPressed() != 0);

    if (anyInput && manager()) {
        manager()->pop();
    }
}

} // namespace biofuel::ui::screens

std::unique_ptr<biofuel::ui::Screen> biofuel::ui::screens::makeIdle() {
    return std::make_unique<biofuel::ui::screens::IdleScreen>();
}
