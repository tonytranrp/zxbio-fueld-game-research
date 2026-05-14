#include "App.hpp"
#include "AppLifecycle.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/debug/DebugOverlayService.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/ui/ScreenManager.hpp"
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

    AppLifecycle::openWindow(WindowLifecycleConfig{
        .title = m_config.title,
        .width = m_config.width,
        .height = m_config.height,
        .fullscreen = m_config.fullscreen,
        .resizable = m_config.resizable,
    });
#ifdef _WIN32
    setupWindowDragTimer();
#endif
    AppLifecycle::prepareLoadingPrelude();

    if (m_config.startup) {
        m_config.startup(m_config.width, m_config.height, m_config.targetFps);
    }

    m_initialized = true;
    m_running = true;
}

void Application::shutdown() {
    if (!m_initialized) {
        return;
    }

    ::biofuel::engine::debug::MemoryTelemetry::snapshot("app.shutdown.begin");
    AppLifecycle::shutdownCoreServices();
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
    if (m_config.globalInput) {
        m_config.globalInput();
    }
    services.get<::biofuel::engine::runtime::typed::ScreenService>().handleInput();
}

void Application::update(const f32 dt) {
    auto& services = ::biofuel::engine::runtime::Runtime::services();
    auto& screens = services.get<::biofuel::engine::runtime::typed::ScreenService>();
    const bool freezeUnderlying = screens.blocksUnderlyingUpdates();

    services.get<::biofuel::engine::runtime::typed::AnimationService>().update(dt);
    if (!freezeUnderlying) {
        services.get<::biofuel::engine::runtime::typed::PhysicsService>().stepFixed(dt);
        services.get<::biofuel::engine::runtime::typed::ModelService>().update(dt);
    }
    services.get<::biofuel::engine::runtime::typed::AudioService>().update();
    // Video overlays need per-frame pumping for decoded frames and Raylib audio
    // streams even when the top screen freezes gameplay updates below it.
    services.get<::biofuel::engine::runtime::typed::VideoService>().update();
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    // Calibration overlays freeze gameplay below them, but camera frames still
    // need to pump while the top screen owns the calibration UI. Camera work
    // only runs after explicit consent/start in HandTrackingService.
    services.get<::biofuel::engine::runtime::typed::HandTrackingService>().update(dt);
#endif
    services.get<::biofuel::engine::runtime::typed::HandPoseService>().update(dt);
#ifdef _WIN32
    flushDragMove();
#endif
    screens.update(dt);
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
