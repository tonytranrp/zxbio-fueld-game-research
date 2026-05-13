#include "App.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include "engine/debug/DebugOverlayService.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/graphics/shaders/LoadingPreludeModule.hpp"
#include "engine/graphics/shaders/MenuOptionModule.hpp"
#include "engine/audio/AudioManager.hpp"
#include "engine/video/VideoManager.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/runtime/typed/Assets.hpp"
#include "engine/graphics/shaders/TypedShaderModule.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "game/screens/loading/LoadingScreen.hpp"
#include "engine/input/InputSystem.hpp"
#include "engine/animation/AnimationManager.hpp"
#include <raylib.h>

#ifdef _WIN32
#include "engine/window/DragHandler.hpp"
#endif

namespace biofuel::engine::app {

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
    auto& services = ::biofuel::engine::runtime::Runtime::services();
    auto& shaderManager = services.get<::biofuel::engine::runtime::typed::ShaderService>();
    shaderManager.init();
    ::biofuel::engine::runtime::typed::Shaders::load<::biofuel::engine::runtime::typed::shader::LoadingPrelude>(shaderManager);
    ::biofuel::engine::runtime::typed::Shaders::load<::biofuel::engine::runtime::typed::shader::MenuOption>(shaderManager);

    // Push loading screen immediately — it handles ALL remaining init
    // (window config, systems init, shader compilation, crossfade preload)
    // while showing real-time progress via the task queue.
    services.get<::biofuel::engine::runtime::typed::ScreenService>().push<::biofuel::game::screens::LoadingScreen>(
        m_config.width, m_config.height, m_config.targetFps);

    m_initialized = true;
    m_running = true;
}

void Application::shutdown() {
    if (!m_initialized) {
        return;
    }

    ::biofuel::engine::debug::MemoryTelemetry::snapshot("app.shutdown.begin");
    auto& services = ::biofuel::engine::runtime::Runtime::services();
    services.get<::biofuel::engine::runtime::typed::DebugOverlayService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::AnimationService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::ScreenService>().shutdown();
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    services.get<::biofuel::engine::runtime::typed::HandTrackingService>().shutdown();
#endif
    services.get<::biofuel::engine::runtime::typed::ModelService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::PhysicsService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::VideoService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::AudioService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::ShaderService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::FontService>().shutdown();
    services.get<::biofuel::engine::runtime::typed::EventService>().shutdown();
#ifdef _WIN32
    killWindowDragTimer();
#endif
    CloseWindow();
    ::biofuel::engine::debug::MemoryTelemetry::snapshot("app.shutdown.end");
    m_initialized = false;
    m_running = false;
}

// ------------------------------------------------------------------------------
// Main Loop
// ------------------------------------------------------------------------------

i32 Application::run() {
    init();

    f64 accumulator = 0.0;

    while (m_running && !WindowShouldClose() && !::biofuel::engine::runtime::Runtime::screen().quitRequested()) {
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
    auto& services = ::biofuel::engine::runtime::Runtime::services();
    services.get<::biofuel::engine::runtime::typed::InputService>().poll();
    services.get<::biofuel::engine::runtime::typed::ScreenService>().handleInput();
}

void Application::update(const f32 dt) {
    auto& services = ::biofuel::engine::runtime::Runtime::services();
    services.get<::biofuel::engine::runtime::typed::AnimationService>().update(dt);
    services.get<::biofuel::engine::runtime::typed::PhysicsService>().stepFixed(dt);
    services.get<::biofuel::engine::runtime::typed::ModelService>().update(dt);
    services.get<::biofuel::engine::runtime::typed::AudioService>().update();
    services.get<::biofuel::engine::runtime::typed::VideoService>().update();
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    services.get<::biofuel::engine::runtime::typed::HandTrackingService>().update(dt);
#endif
#ifdef _WIN32
    flushDragMove();
#endif
    services.get<::biofuel::engine::runtime::typed::ScreenService>().update(dt);
}

void Application::render() {
    using namespace ::biofuel::engine::graphics;

    Renderer::beginFrame(BLACK);
    ::biofuel::engine::runtime::Runtime::screen().render();

    ::biofuel::engine::runtime::Runtime::debugOverlay().render(
        ::biofuel::engine::debug::DebugOverlayContext{
            .screenWidth = Renderer::screenWidth(),
            .screenHeight = Renderer::screenHeight(),
            .frameTime = GetFrameTime(),
        });

    Renderer::endFrame();
}

#ifdef _WIN32

void Application::setupWindowDragTimer() {
    biofuel::engine::window::DragHandler::install(GetWindowHandle());
}

void Application::killWindowDragTimer() {
    biofuel::engine::window::DragHandler::uninstall();
}

void Application::flushDragMove() {
    biofuel::engine::window::DragHandler::flush();
}

#endif

} // namespace biofuel::engine::app
