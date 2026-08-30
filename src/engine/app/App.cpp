#include "App.hpp"
#include "AppLifecycle.hpp"
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
#include "engine/bevy/BevyRenderService.hpp"
#endif
#ifdef BIOFUEL_WITH_WORLD_BRIDGE
#include "engine/world/WorldBridge.hpp"
#endif
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
        .vsync = m_config.vsync,
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

void Application::runWorldSessionAndReturn() {
    auto& screens = ::biofuel::engine::runtime::Runtime::screen();
    const i32 saveSlot = screens.worldSessionSaveSlot();
    screens.clearWorldSessionRequest();

    // Exactly the same teardown a real quit does (services, Win32 drag
    // timer, CloseWindow()) -- reusing shutdown() rather than duplicating
    // it, since the two really are the same sequence. Confirmed safe to
    // reopen a fresh raylib window afterward by a dedicated isolated check
    // (tests/engine/WorldRaylibHandoffCheck.cpp) before this was wired in
    // here: raylib's second InitWindow() resets its internal resource IDs
    // exactly like a true fresh start, not reused stale state.
    shutdown();

#ifdef BIOFUEL_WITH_WORLD_BRIDGE
    // Return value intentionally not yet branched on -- every outcome
    // (ReturnedToMenu, VulkanUnavailable, InternalError) currently just
    // goes back to a freshly-booted menu below. Routing the failure cases
    // to a real in-menu error message is a later phase's scope (see the
    // migration plan).
    (void)::biofuel::engine::world::runWorldSession(
        ::biofuel::engine::world::WorldSessionInput{.saveSlot = saveSlot});
#endif

    // Fresh window + LoadingScreen -> MainMenu, identical to a cold start --
    // deliberately not trying to preserve any of the prior run's engine
    // state, since the new raylib window is a genuinely new GL context and
    // every typed-registry service that caches raylib resource IDs
    // (ShaderManager, ModelSystem, ...) needs real re-initialization, not
    // naive reuse.
    init();
}

// ------------------------------------------------------------------------------
// Main Loop
// ------------------------------------------------------------------------------

namespace {

// Precomputed accumulator ceiling: FIXED_DT (1/60) × max catchup frames.
// Avoids a multiply on every frame in the hot path.
constexpr f64 kAccumulatorCap = Application::kFixedDt * Application::kMaxFrameCatchupMultiplier;

// Clamp the accumulator to prevent spiral-of-death.
// BIOFUEL_FORCE_INLINE — single branch, called exactly once per frame in the
// innermost game loop; the call overhead matters here.
BIOFUEL_FORCE_INLINE void clampAccumulator(f64& accumulator) noexcept {
    if (accumulator > kAccumulatorCap) {
        accumulator = kAccumulatorCap;
    }
}

} // namespace

i32 Application::run() {
    init();

    // Outer loop: normally runs its body's while-loop exactly once, all the
    // way to a real quit. A World-session request instead falls out of the
    // inner loop early, runs runWorldSessionAndReturn() (which blocks for
    // the whole gameplay session and then re-inits a fresh window +
    // LoadingScreen, see that method's own doc), and the outer loop starts
    // the inner one again -- repeatable for as many New Game/Continue ->
    // menu round trips as the player makes in one process lifetime.
    for (;;) {
        f64 accumulator = 0.0;
        auto& screens = ::biofuel::engine::runtime::Runtime::screen();

        while (m_running && !WindowShouldClose() && !screens.quitRequested() && !screens.worldSessionRequested()) {
            accumulator += static_cast<f64>(GetFrameTime());
            clampAccumulator(accumulator);

            processInput();

            while (accumulator >= Application::kFixedDt) {
                update(static_cast<f32>(Application::kFixedDt));
                accumulator -= Application::kFixedDt;
            }

            render();
        }

        if (!screens.worldSessionRequested()) {
            break; // real quit, or the OS/user closed the window directly
        }

        runWorldSessionAndReturn();
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
#ifdef BIOFUEL_WITH_BEVY_BRIDGE
    // Same reasoning as VideoService above: the embedded Bevy scene keeps
    // rendering under a frozen top screen. Unlike VideoService (deliberately
    // wall-clock-paced), it gets the same fixed dt as PhysicsService.stepFixed()
    // above, so its motion stays in lockstep with the host's fixed timestep.
    services.get<::biofuel::engine::runtime::typed::BevyRendererService>().update(dt);
#endif
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
