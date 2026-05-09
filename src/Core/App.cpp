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
    animation::AnimationManager::instance().shutdown();
    utils::render::ShaderManager::instance().shutdown();
    Data::fonts().shutdown();
    Data::events().shutdown();
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
    Data::screens().update(dt);
}

void Application::render() {
    using namespace utils::render;

    Renderer::beginFrame(BLACK);
    Data::screens().render();

    // Debug overlay
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

    Renderer::endFrame();
}

} // namespace biofuel
