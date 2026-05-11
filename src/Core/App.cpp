#include "App.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "Utils/render/Shader/LoadingPreludeModule.hpp"
#include "Utils/render/Shader/MenuOptionModule.hpp"
#include "Utils/audio/AudioManager.hpp"
#include "Data/Data.hpp"
#include "UI/ScreenManager.hpp"
#include "UI/screens/LoadingScreen/LoadingScreen.hpp"
#include "Systems/Input/InputSystem.hpp"
#include "AnimationController/AnimationManager.hpp"
#include <raylib.h>

#ifdef _WIN32
#include "Systems/Window/DragHandler.hpp"
#endif

namespace biofuel {

Application::Application(Config config)
    : m_config(std::move(config))
{
}

Application::~Application() noexcept {
    if (m_initialized) {
        shutdown();
    }
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void Application::init() {
    if (m_initialized) {
        return;
    }

    if (m_config.resizable) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    }
    if (m_config.fullscreen) {
        SetConfigFlags(FLAG_FULLSCREEN_MODE);
    }

    InitWindow(m_config.width, m_config.height, m_config.title.c_str());
#ifdef _WIN32
    setupWindowDragTimer();
#endif
    auto& shaderManager = utils::render::ShaderManager::instance();
    shaderManager.init();
    shaderManager.loadFromMemory(
        utils::render::shader::LoadingPreludeModule::NAME.data(),
        utils::render::shader::LoadingPreludeModule::VERTEX_SOURCE,
        utils::render::shader::LoadingPreludeModule::FRAGMENT_SOURCE.data()
    );
    shaderManager.loadFromMemory(
        utils::render::shader::MenuOptionModule::NAME.data(),
        utils::render::shader::MenuOptionModule::VERTEX_SOURCE,
        utils::render::shader::MenuOptionModule::FRAGMENT_SOURCE.data()
    );

    // Push loading screen immediately — it handles ALL remaining init
    // (window config, systems init, shader compilation, crossfade preload)
    // while showing real-time progress via the task queue.
    Data::screens().push(std::make_unique<ui::screens::LoadingScreen>(
        m_config.width, m_config.height, m_config.targetFps));

    m_initialized = true;
    m_running = true;
}

void Application::shutdown() {
    if (!m_initialized) {
        return;
    }

    Data::screens().shutdown();
    Data::models().shutdown();
    animation::AnimationManager::instance().shutdown();
    utils::audio::AudioManager::instance().shutdown();
    utils::render::ShaderManager::instance().shutdown();
    Data::fonts().shutdown();
    Data::events().shutdown();
#ifdef _WIN32
    killWindowDragTimer();
#endif
    CloseWindow();
    m_initialized = false;
    m_running = false;
}

// ------------------------------------------------------------------------------
// Main Loop
// ------------------------------------------------------------------------------

i32 Application::run() {
    init();

    f64 accumulator = 0.0;

    while (m_running && !WindowShouldClose() && !Data::screens().quitRequested()) {
        const f64 dt = static_cast<f64>(GetFrameTime());
        accumulator += dt;

        // Cap to prevent spiral-of-death (max 5 frames behind)
        if (accumulator > FIXED_DT * 5.0) {
            accumulator = FIXED_DT * 5.0;
        }

        processInput();

        while (accumulator >= FIXED_DT) {
            update(static_cast<f32>(FIXED_DT));
            accumulator -= FIXED_DT;
        }

        render();
    }

    shutdown();
    return 0;
}

// ------------------------------------------------------------------------------
// Per-Frame Methods
// ------------------------------------------------------------------------------

void Application::processInput() {
    systems::input::InputSystem::poll();
    Data::screens().handleInput();
}

void Application::update(const f32 dt) {
    animation::AnimationManager::instance().update(dt);
    Data::models().update(dt);
    utils::audio::AudioManager::instance().update();
#ifdef _WIN32
    flushDragMove();
#endif
    Data::screens().update(dt);
}

void Application::render() {
    using namespace utils::render;

    Renderer::beginFrame(BLACK);
    Data::screens().render();

    // Debug overlay (hidden in release builds)
#ifndef NDEBUG
    const i32 screenH = Renderer::screenHeight();
    const i32 overlayX = 14;
    const i32 overlayY = screenH - 34;
    const i32 overlayW = 182;
    const i32 overlayH = 22;
    Renderer::drawRect(overlayX, overlayY - 2, overlayW, overlayH, {10, 12, 18, 118});
    Renderer::drawText(
        TextFormat("Window: %dx%d | FPS: %d",
            Renderer::screenWidth(), screenH, GetFPS()),
        overlayX + 6, overlayY + 1, 14, {108, 112, 126, 255}
    );
#endif

    Renderer::endFrame();
}

#ifdef _WIN32

void Application::setupWindowDragTimer() {
    biofuel::systems::window::DragHandler::install(GetWindowHandle());
}

void Application::killWindowDragTimer() {
    biofuel::systems::window::DragHandler::uninstall();
}

void Application::flushDragMove() {
    biofuel::systems::window::DragHandler::flush();
}

#endif

} // namespace biofuel
