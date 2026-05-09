#include "LoadingScreen.hpp"
#include "MainMenuScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "Utils/render/Shader/BlurCompositeModule.hpp"
#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"
#include "Utils/render/Shader/CrossfadeModule.hpp"
#include "Utils/render/Shader/LoadingPreludeModule.hpp"
#include "Utils/render/Shader/MainMenuBgModule.hpp"
#include "Data/Data.hpp"
#include "AnimationController/AnimationManager.hpp"
#include <raylib.h>

namespace biofuel::ui::screens {

LoadingScreen::LoadingScreen(i32 width, i32 height, i32 targetFps)
    : m_appWidth(width), m_appHeight(height)
    , m_appTargetFps(targetFps) {}

void LoadingScreen::buildTasks() {
    m_tasks.add({"Configuring input...", 0.3f, []() {
        SetExitKey(KEY_NULL);
    }});
    m_tasks.add({"Setting window constraints...", 0.3f, [this]() {
        SetWindowMinSize(m_appWidth, m_appHeight);
    }});
    m_tasks.add({"Setting target framerate...", 0.3f, [this]() {
        SetTargetFPS(m_appTargetFps);
    }});

    m_tasks.add({"Initializing event bus...", 0.5f, []() {
        Data::events().init();
    }});
    m_tasks.add({"Initializing screen stack...", 0.5f, []() {
        Data::screens().init();
    }});
    m_tasks.add({"Initializing animation system...", 0.5f, []() {
        animation::AnimationManager::instance().init();
    }});
    m_tasks.add({"Initializing shader system...", 0.3f, []() {
        utils::render::ShaderManager::instance().init();
    }});

    using namespace utils::render::shader;
    auto& shaderManager = utils::render::ShaderManager::instance();
    m_tasks.add({"Compiling blur horizontal shader...", 2.0f, [&shaderManager]() {
        shaderManager.loadFromMemory(BlurHModule::NAME.data(),
            BlurHModule::VERTEX_SOURCE, BlurHModule::FRAGMENT_SOURCE.data());
    }});
    m_tasks.add({"Compiling blur vertical shader...", 2.0f, [&shaderManager]() {
        shaderManager.loadFromMemory(BlurVModule::NAME.data(),
            BlurVModule::VERTEX_SOURCE, BlurVModule::FRAGMENT_SOURCE.data());
    }});
    m_tasks.add({"Compiling blur composite shader...", 1.2f, [&shaderManager]() {
        shaderManager.loadFromMemory(BlurCompositeModule::NAME.data(),
            BlurCompositeModule::VERTEX_SOURCE, BlurCompositeModule::FRAGMENT_SOURCE.data());
    }});
    m_tasks.add({"Compiling crossfade shader...", 2.0f, [&shaderManager]() {
        shaderManager.loadFromMemory(CrossfadeModule::NAME.data(),
            CrossfadeModule::VERTEX_SOURCE, CrossfadeModule::FRAGMENT_SOURCE.data());
    }});
    m_tasks.add({"Compiling loading prelude shader...", 2.0f, [&shaderManager]() {
        shaderManager.loadFromMemory(LoadingPreludeModule::NAME.data(),
            LoadingPreludeModule::VERTEX_SOURCE, LoadingPreludeModule::FRAGMENT_SOURCE.data());
    }});
    m_tasks.add({"Compiling background shader...", 2.0f, [&shaderManager]() {
        shaderManager.loadFromMemory(MainMenuBgModule::NAME.data(),
            MainMenuBgModule::VERTEX_SOURCE, MainMenuBgModule::FRAGMENT_SOURCE.data());
    }});

    m_tasks.add({"Caching transition shader...", 1.0f, []() {
        Data::screens().preloadCrossfadeShader();
    }});
    m_tasks.add({"Allocating render buffers...", 1.5f, []() {
        Data::screens().preloadTransitionTextures();
    }});
}

void LoadingScreen::onEnter() {
    m_backdrop.configure(animation::screen::ScreenBackdropConfig{
        .shaderName = utils::render::shader::LoadingPreludeModule::NAME,
        .fallbackColor = Color{12, 14, 20, 255},
        .revealDelay = 0.0f,
        .revealDuration = 1.8f,
        .brightnessFloor = 0.18f,
        .brightnessCeiling = 0.96f,
        .transitionWeight = 0.25f,
        .revealWeight = 0.75f,
    });
    m_backdrop.reset();

    buildTasks();

    if (m_tasks.totalTasks() == 0) {
        m_tasksDone = true;
        m_actualProgress = 1.0f;
        m_allowSkip = true;
    }
}

void LoadingScreen::onUpdate(const f32 dt) {
    m_elapsed += dt;
    m_backdrop.update(dt);

    if (!m_tasks.isDone()) {
        m_tasks.processNext();
        m_actualProgress = m_tasks.progress();

        if (m_tasks.isDone()) {
            m_tasksDone = true;
            m_actualProgress = 1.0f;
            m_allowSkip = true;
        }
    }

    if (m_displayProgress < m_actualProgress) {
        m_displayProgress += (m_actualProgress - m_displayProgress) * PROGRESS_LERP_SPEED * dt;
        if (m_displayProgress > 0.995f && m_actualProgress >= 1.0f) {
            m_displayProgress = 1.0f;
        }
    }

    if (m_tasksDone && m_displayProgress >= 1.0f && m_elapsed >= MIN_DISPLAY_SECONDS && !m_transitioned) {
        m_transitioned = true;
        transitionToNext();
    }
}

void LoadingScreen::onRender() {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    m_backdrop.render(transitionAlpha());

    const i32 panelWidth = 520;
    const i32 panelHeight = 164;
    const i32 panelX = (sw - panelWidth) / 2;
    const i32 panelY = sh / 2 - 88;

    Renderer::drawRect(panelX, panelY, panelWidth, panelHeight, {14, 19, 28, 182});
    Renderer::drawRectLines(panelX, panelY, panelWidth, panelHeight, {72, 96, 124, 188});

    static constexpr std::string_view TITLE = "FUEL FARM";
    const Color titleColor = {208, 220, 240, 255};
    const i32 titleW = Renderer::measureText(TITLE, TITLE_SIZE);
    Renderer::drawText(TITLE, (sw - titleW) / 2, panelY - 78, TITLE_SIZE, titleColor);

    const i32 barX = (sw - BAR_WIDTH) / 2;
    const i32 barY = panelY + 62;
    Renderer::drawRectLines(barX, barY, BAR_WIDTH, BAR_HEIGHT, {84, 104, 132, 255});

    const i32 fillW = static_cast<i32>((BAR_WIDTH - 4) * m_displayProgress);
    if (fillW > 0) {
        Renderer::drawRect(barX + 2, barY + 2, fillW, BAR_HEIGHT - 4, {92, 182, 224, 255});
    }

    const bool fullyDone = m_tasksDone && m_displayProgress >= 1.0f;
    std::string status = "Ready.";
    if (!fullyDone) {
        const i32 dotCount = static_cast<i32>(m_elapsed / DOTS_INTERVAL) % 4;
        status = m_tasks.currentName() + std::string(dotCount, '.');
    }

    const i32 statusW = Renderer::measureText(status, STATUS_SIZE);
    Renderer::drawText(status, (sw - statusW) / 2, barY + BAR_HEIGHT + 18, STATUS_SIZE, {216, 224, 236, 255});

    if (m_allowSkip && m_elapsed < MIN_DISPLAY_SECONDS) {
        static constexpr std::string_view SKIP_HINT = "Press any key to continue...";
        constexpr i32 HINT_SIZE = 14;
        const i32 hintW = Renderer::measureText(SKIP_HINT, HINT_SIZE);
        Renderer::drawText(
            SKIP_HINT,
            (sw - hintW) / 2,
            barY + BAR_HEIGHT + 64,
            HINT_SIZE,
            {130, 148, 172, 255}
        );
    }

    static constexpr std::string_view FOOTER = "v0.1.0  |  C++20  |  Raylib 5.5";
    constexpr i32 FOOTER_SIZE = 12;
    const i32 footerW = Renderer::measureText(FOOTER, FOOTER_SIZE);
    Renderer::drawText(FOOTER, (sw - footerW) / 2, sh - 30, FOOTER_SIZE, {92, 104, 126, 255});
}

void LoadingScreen::onInput() {
    if (m_allowSkip && !m_transitioned) {
        if (GetKeyPressed() != 0 ||
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            m_transitioned = true;
            transitionToNext();
        }
    }
}

void LoadingScreen::transitionToNext() {
    if (auto* sm = manager()) {
        sm->queueReplace(std::make_unique<MainMenuScreen>());
    }
}

} // namespace biofuel::ui::screens
