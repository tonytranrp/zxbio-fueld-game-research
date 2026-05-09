#include "App.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "Data/Data.hpp"
#include "UI/ScreenManager.hpp"
#include "UI/screens/LoadingScreen.hpp"
#include "Systems/Input/InputSystem.hpp"
#include "AnimationController/AnimationManager.hpp"
#include <raylib.h>

namespace biofuel {

Application::Application(Config config)
    : m_config(std::move(config))
{
}

Application::~Application() {
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

    InitWindow(m_config.width, m_config.height, m_config.title.c_str());

    // We handle ESC ourselves — disable the default Raylib ESC→close behavior
    SetExitKey(KEY_NULL);

    SetWindowMinSize(m_config.width, m_config.height);

    if (m_config.fullscreen) {
        ToggleFullscreen();
    }

    SetTargetFPS(m_config.targetFps);

    Data::events().init();
    Data::screens().init();
    animation::AnimationManager::instance().init();
    utils::render::ShaderManager::instance().init();

    // Push loading screen immediately — it handles shader compilation
    // and other deferred init work while showing progress.
    Data::screens().push(std::make_unique<ui::screens::LoadingScreen>());

    m_initialized = true;
    m_running = true;
}

void Application::shutdown() {
    if (!m_initialized) {
        return;
    }

    Data::screens().shutdown();
    animation::AnimationManager::instance().shutdown();
    utils::render::ShaderManager::instance().shutdown();
    Data::events().shutdown();
    CloseWindow();
    m_initialized = false;
    m_running = false;
}

// ------------------------------------------------------------------------------
// Main Loop
// ------------------------------------------------------------------------------

int Application::run() {
    init();

    while (m_running && !WindowShouldClose() && !Data::screens().quitRequested()) {
        const f32 dt = GetFrameTime();
        processInput();
        update(dt);
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
    Data::screens().update(dt);
}

void Application::render() {
    using namespace utils::render;

    Renderer::beginFrame(BLACK);
    Data::screens().render();

    // Debug overlay
    const i32 screenH = Renderer::screenHeight();
    Renderer::drawText(
        TextFormat("Window: %dx%d | FPS: %d",
            Renderer::screenWidth(), screenH, GetFPS()),
        20, screenH - 30, 16, DARKGRAY
    );

    Renderer::endFrame();
}

} // namespace biofuel
