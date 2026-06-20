#include "LoadingScreen.hpp"
#include "LoadingScreenModule.hpp"
#include "game/screens/main_menu/MainMenuScreen.hpp"
#include "engine/app/AppLifecycle.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/shaders/LoadingPreludeModule.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/runtime/typed/AssetCatalog.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "game/screens/idle/IdleScreen.hpp"
#include <raylib.h>
#include <string>

namespace biofuel::engine::ui::typed {

namespace {

// Panel dimensions come from the screen class so they are defined once.
constexpr i32 LOADING_PANEL_WIDTH = ::biofuel::game::screens::LoadingScreen::PANEL_WIDTH;
constexpr i32 LOADING_PANEL_HEIGHT = ::biofuel::game::screens::LoadingScreen::PANEL_HEIGHT;

// Panel placement and chrome layout, measured in pixels.
constexpr i32 PANEL_VERTICAL_OFFSET = 88;   // panel top above screen vertical center
constexpr i32 TITLE_ABOVE_PANEL = 78;       // title baseline above the panel top
constexpr i32 BAR_TOP_INSET = 62;           // progress bar top below the panel top
constexpr i32 BAR_FILL_PADDING = 2;         // inset of the fill from the bar outline (per side)
constexpr i32 STATUS_BELOW_BAR = 18;        // status text baseline below the bar bottom
constexpr i32 SKIP_HINT_BELOW_BAR = 64;     // skip hint baseline below the bar bottom
constexpr i32 FOOTER_BOTTOM_INSET = 30;     // footer baseline above the screen bottom
constexpr i32 DOTS_MODULO = 4;              // animated status dots cycle through 0..3

constexpr i32 HINT_SIZE = 14;               // skip-hint font size (px)
constexpr i32 FOOTER_SIZE = 12;             // footer font size (px)

// Render-only color literals (RGBA).
constexpr Color TITLE_COLOR = {208, 220, 240, 255};
constexpr Color PANEL_FILL_COLOR = {14, 19, 28, 182};
constexpr Color PANEL_BORDER_COLOR = {72, 96, 124, 188};
constexpr Color BAR_OUTLINE_COLOR = {84, 104, 132, 255};
constexpr Color BAR_FILL_COLOR = {92, 182, 224, 255};
constexpr Color STATUS_TEXT_COLOR = {216, 224, 236, 255};
constexpr Color SKIP_HINT_COLOR = {130, 148, 172, 255};
constexpr Color FOOTER_TEXT_COLOR = {92, 104, 126, 255};

[[nodiscard]] Rectangle loadingPanelRect(const RenderContext& context) noexcept {
    return Rectangle{
        static_cast<f32>((context.screenWidth - LOADING_PANEL_WIDTH) / 2),
        static_cast<f32>(context.screenHeight / 2 - PANEL_VERTICAL_OFFSET),
        static_cast<f32>(LOADING_PANEL_WIDTH),
        static_cast<f32>(LOADING_PANEL_HEIGHT)
    };
}

} // namespace

template<>
struct RenderElementExecutor<loading::BackdropElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen& screen, RenderContext& context) {
        screen.m_backdrop.render(context.transitionAlpha);
    }
};

template<>
struct RenderElementExecutor<loading::TitleTextElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen&, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        const Rectangle panel = loadingPanelRect(context);
        static constexpr std::string_view TITLE = "FUEL FARM";
        const i32 titleW = Renderer::measureText(TITLE, ::biofuel::game::screens::LoadingScreen::TITLE_SIZE);
        Renderer::drawText(
            TITLE,
            (context.screenWidth - titleW) / 2,
            static_cast<i32>(panel.y) - TITLE_ABOVE_PANEL,
            ::biofuel::game::screens::LoadingScreen::TITLE_SIZE,
            TITLE_COLOR
        );
    }
};

template<>
struct RenderElementExecutor<loading::LoadingPanelElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen&, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        const Rectangle panel = loadingPanelRect(context);
        Renderer::drawRect(static_cast<i32>(panel.x), static_cast<i32>(panel.y), static_cast<i32>(panel.width), static_cast<i32>(panel.height), PANEL_FILL_COLOR);
        Renderer::drawRectLines(static_cast<i32>(panel.x), static_cast<i32>(panel.y), static_cast<i32>(panel.width), static_cast<i32>(panel.height), PANEL_BORDER_COLOR);
    }
};

template<>
struct RenderElementExecutor<loading::ProgressBarElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        const Rectangle panel = loadingPanelRect(context);
        const i32 barX = (context.screenWidth - ::biofuel::game::screens::LoadingScreen::BAR_WIDTH) / 2;
        const i32 barY = static_cast<i32>(panel.y) + BAR_TOP_INSET;
        Renderer::drawRectLines(barX, barY, ::biofuel::game::screens::LoadingScreen::BAR_WIDTH, ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT, BAR_OUTLINE_COLOR);

        const i32 fillW = static_cast<i32>((::biofuel::game::screens::LoadingScreen::BAR_WIDTH - 2 * BAR_FILL_PADDING) * screen.m_displayProgress);
        if (fillW > 0) {
            Renderer::drawRect(barX + BAR_FILL_PADDING, barY + BAR_FILL_PADDING, fillW, ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT - 2 * BAR_FILL_PADDING, BAR_FILL_COLOR);
        }
    }
};

template<>
struct RenderElementExecutor<loading::StatusTextElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        const Rectangle panel = loadingPanelRect(context);
        const i32 barY = static_cast<i32>(panel.y) + BAR_TOP_INSET;
        const bool fullyDone = screen.m_tasksDone && screen.m_displayProgress >= 1.0f;
        std::string status = "Ready.";
        if (screen.m_tasks.isFailed()) {
            status = screen.m_tasks.failureMessage();
        } else if (!fullyDone) {
            const i32 dotCount = static_cast<i32>(screen.m_elapsed / ::biofuel::game::screens::LoadingScreen::DOTS_INTERVAL) % DOTS_MODULO;
            status = screen.m_tasks.currentName() + std::string(dotCount, '.');
        }

        const i32 statusW = Renderer::measureText(status, ::biofuel::game::screens::LoadingScreen::STATUS_SIZE);
        Renderer::drawText(status, (context.screenWidth - statusW) / 2, barY + ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT + STATUS_BELOW_BAR, ::biofuel::game::screens::LoadingScreen::STATUS_SIZE, STATUS_TEXT_COLOR);
    }
};

template<>
struct RenderElementExecutor<loading::SkipHintElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        if (!screen.m_allowSkip || screen.m_elapsed >= ::biofuel::game::screens::LoadingScreen::MIN_DISPLAY_SECONDS) {
            return;
        }

        const Rectangle panel = loadingPanelRect(context);
        const i32 barY = static_cast<i32>(panel.y) + BAR_TOP_INSET;
        static constexpr std::string_view SKIP_HINT = "Press any key to continue...";
        const i32 hintW = Renderer::measureText(SKIP_HINT, HINT_SIZE);
        Renderer::drawText(
            SKIP_HINT,
            (context.screenWidth - hintW) / 2,
            barY + ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT + SKIP_HINT_BELOW_BAR,
            HINT_SIZE,
            SKIP_HINT_COLOR
        );
    }
};

template<>
struct RenderElementExecutor<loading::FooterTextElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen&, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        static constexpr std::string_view FOOTER = "v0.1.0  |  C++20  |  Raylib 5.5";
        const i32 footerW = Renderer::measureText(FOOTER, FOOTER_SIZE);
        Renderer::drawText(FOOTER, (context.screenWidth - footerW) / 2, context.screenHeight - FOOTER_BOTTOM_INSET, FOOTER_SIZE, FOOTER_TEXT_COLOR);
    }
};

} // namespace biofuel::engine::ui::typed

namespace biofuel::game::screens {

LoadingScreen::LoadingScreen(i32 width, i32 height, i32 targetFps)
    : m_appWidth(width), m_appHeight(height)
    , m_appTargetFps(targetFps) {}

void LoadingScreen::buildTasks() {
    m_tasks.clear(::biofuel::engine::runtime::Runtime::tasks());
    m_tasks.reserve(16U + ::biofuel::engine::runtime::typed::AssetCatalog<
        ::biofuel::engine::runtime::typed::EngineStartupCatalog>::Assets::size);
    ::biofuel::engine::app::AppLifecycle::addStartupTasks(
        m_tasks,
        ::biofuel::engine::app::StartupLifecycleConfig{
            .width = m_appWidth,
            .height = m_appHeight,
            .targetFps = m_appTargetFps,
        });
}

void LoadingScreen::onEnter() {
    m_elapsed = 0.0f;
    m_displayProgress = 0.0f;
    m_actualProgress = 0.0f;
    m_tasksDone = false;
    m_allowSkip = false;
    m_transitioned = false;
    m_reportedStartupMemory = false;

    m_backdrop.configure(game::presentation::effects::ScreenBackdropConfig{
        .shaderName = ::biofuel::engine::graphics::shader::LoadingPreludeModule::NAME,
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
        m_tasks.processNext(&::biofuel::engine::runtime::Runtime::tasks());
        m_actualProgress = m_tasks.progress();

        if (m_tasks.isFailed()) {
            m_tasksDone = false;
            m_allowSkip = false;
            return;
        }

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
        if (!m_reportedStartupMemory) {
            m_reportedStartupMemory = true;
            ::biofuel::engine::debug::MemoryTelemetry::snapshot("startup.complete");
        }
        m_transitioned = true;
        transitionToNext();
    }
}

void LoadingScreen::onRender() {
    ::biofuel::engine::ui::typed::RenderContext context{
        .manager = manager(),
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = screenId(),
        .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
        .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
        .transitionAlpha = transitionAlpha(),
        .frameTime = GetFrameTime(),
    };
    ::biofuel::engine::ui::typed::RenderPipeline<LoadingScreen>::render(*this, context);
}

void LoadingScreen::onInput() {
    if (m_allowSkip && !m_transitioned) {
        // Use state-polling (IsKeyDown/IsMouseButtonDown) instead of
        // queue-draining (GetKeyPressed) â€” InputSystem::poll() already
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
#if defined(BIOFUEL_DEV_STARTUP_IDLE_VIDEO)
        sm->queueReplace<IdleScreen>(IdleScreen::idleVideoPath());
#else
        sm->queueReplace<MainMenuScreen>();
#endif
    }
}

} // namespace biofuel::game::screens
