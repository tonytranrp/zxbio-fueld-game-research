#include "LoadingScreen.hpp"
#include "MainMenu/MainMenuScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "UI/ScreenFwd.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "Utils/render/Shader/BlurCompositeModule.hpp"
#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"
#include "Utils/render/Shader/CrossfadeModule.hpp"
#include "Utils/render/Shader/LoadingPreludeModule.hpp"
#include "Utils/render/Shader/MainMenuBgModule.hpp"
#include "Utils/render/Shader/MenuOptionModule.hpp"
#include "Utils/audio/AudioManager.hpp"
#include "Utils/video/VideoManager.hpp"
#include "Data/Data.hpp"
#include "AnimationController/AnimationManager.hpp"
#include "IdleScreen/IdleScreen.hpp"
#include <raylib.h>
#include <string>

namespace biofuel::ui::screens {

namespace {

void ensureShaderLoaded(
    utils::render::ShaderManager& shaderManager,
    const std::string_view name,
    const char* vertexSource,
    const std::string_view fragmentSource)
{
    if (shaderManager.has(name)) {
        return;
    }

    shaderManager.loadFromMemory(name, vertexSource, fragmentSource.data());
}

} // namespace

LoadingScreen::LoadingScreen(i32 width, i32 height, i32 targetFps)
    : m_appWidth(width), m_appHeight(height)
    , m_appTargetFps(targetFps) {}

void LoadingScreen::buildTasks() {
    m_tasks.clear();

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
    m_tasks.add({"Initializing model system...", 0.4f, []() {
        Data::models().init();
    }});
    m_tasks.add({"Initializing audio device...", 0.5f, []() {
        biofuel::utils::audio::AudioManager::instance().init();
    }});
    m_tasks.add({"Initializing video system...", 0.4f, []() {
        biofuel::utils::video::VideoManager::instance().init();
    }});

    using namespace utils::render::shader;
    auto& shaderManager = utils::render::ShaderManager::instance();
    m_tasks.add({"Compiling blur horizontal shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded(shaderManager, BlurHModule::NAME,
            BlurHModule::VERTEX_SOURCE, BlurHModule::FRAGMENT_SOURCE);
    }});
    m_tasks.add({"Compiling blur vertical shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded(shaderManager, BlurVModule::NAME,
            BlurVModule::VERTEX_SOURCE, BlurVModule::FRAGMENT_SOURCE);
    }});
    m_tasks.add({"Compiling blur composite shader...", 1.2f, [&shaderManager]() {
        ensureShaderLoaded(shaderManager, BlurCompositeModule::NAME,
            BlurCompositeModule::VERTEX_SOURCE, BlurCompositeModule::FRAGMENT_SOURCE);
    }});
    m_tasks.add({"Compiling crossfade shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded(shaderManager, CrossfadeModule::NAME,
            CrossfadeModule::VERTEX_SOURCE, CrossfadeModule::FRAGMENT_SOURCE);
    }});
    m_tasks.add({"Compiling loading prelude shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded(shaderManager, LoadingPreludeModule::NAME,
            LoadingPreludeModule::VERTEX_SOURCE, LoadingPreludeModule::FRAGMENT_SOURCE);
    }});
    m_tasks.add({"Compiling menu option shader...", 1.3f, [&shaderManager]() {
        ensureShaderLoaded(shaderManager, MenuOptionModule::NAME,
            MenuOptionModule::VERTEX_SOURCE, MenuOptionModule::FRAGMENT_SOURCE);
    }});
    m_tasks.add({"Compiling background shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded(shaderManager, MainMenuBgModule::NAME,
            MainMenuBgModule::VERTEX_SOURCE, MainMenuBgModule::FRAGMENT_SOURCE);
    }});

    for (const auto& modelSpec : Data::models().registry()) {
        if (!modelSpec.preloadOnStartup) {
            continue;
        }

        std::string taskName = "Loading model asset: ";
        taskName += modelSpec.debugName;
        if (!modelSpec.shaderName.empty()) {
            taskName += " (model + shader)";
        }

        m_tasks.add({std::move(taskName), 1.6f, [assetId = modelSpec.id]() {
            (void)Data::models().preload(assetId);
        }});
    }

    m_tasks.add({"Caching transition shader...", 1.0f, []() {
        Data::screens().preloadCrossfadeShader();
    }});
    m_tasks.add({"Allocating render buffers...", 1.5f, []() {
        Data::screens().preloadTransitionTextures();
    }});
}

void LoadingScreen::onEnter() {
    m_elapsed = 0.0f;
    m_displayProgress = 0.0f;
    m_actualProgress = 0.0f;
    m_tasksDone = false;
    m_allowSkip = false;
    m_transitioned = false;

    m_backdrop.configure(animation::screen::ScreenBackdropConfig{
        .shaderName = utils::render::shader::LoadingPreludeModule::NAME,
        .fallbackColor = Color{12, 14, 20, 255},
        .revealDelay = 1.8f,          // doors stay closed for 1.8s while loading
        .revealDuration = 1.5f,       // door-opening animation takes 1.5s
        .brightnessFloor = 0.65f,     // panels visible almost immediately
        .brightnessCeiling = 1.0f,
        .transitionWeight = 0.85f,    // brightness comes from transition-in (fast)
        .revealWeight = 0.15f,        // door opening barely affects brightness
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
        // Use state-polling (IsKeyDown/IsMouseButtonDown) instead of
        // queue-draining (GetKeyPressed) — InputSystem::poll() already
        // consumes the key queue before onInput() runs (B005).
        if (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ENTER) ||
            IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
            IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            m_transitioned = true;
            transitionToNext();
        }
    }
}

void LoadingScreen::transitionToNext() {
    if (auto* sm = manager()) {
#ifdef BIOFUEL_DEV_STARTUP_IDLE_VIDEO
        auto idle = std::make_unique<IdleScreen>();
        idle->setIdleVideo(IdleScreen::idleVideoPath());
        sm->queueReplace(std::move(idle));
#else
        sm->queueReplace(ui::screens::makeMainMenu());
#endif
    }
}

} // namespace biofuel::ui::screens

// ------------------------------------------------------------------------------
// Factory
// ------------------------------------------------------------------------------

std::unique_ptr<biofuel::ui::Screen> biofuel::ui::screens::makeLoading(i32 w, i32 h, i32 fps) {
    return std::make_unique<LoadingScreen>(w, h, fps);
}
