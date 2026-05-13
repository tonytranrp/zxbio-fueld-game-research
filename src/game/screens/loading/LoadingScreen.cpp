#include "LoadingScreen.hpp"
#include "LoadingScreenModule.hpp"
#include "game/screens/main_menu/MainMenuScreen.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include "engine/graphics/shaders/BlurCompositeModule.hpp"
#include "engine/graphics/shaders/BlurHModule.hpp"
#include "engine/graphics/shaders/BlurVModule.hpp"
#include "engine/graphics/shaders/CrossfadeModule.hpp"
#include "engine/graphics/shaders/LoadingPreludeModule.hpp"
#include "engine/graphics/shaders/MainMenuBgModule.hpp"
#include "engine/graphics/shaders/MenuOptionModule.hpp"
#include "engine/audio/AudioManager.hpp"
#include "engine/video/VideoManager.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/runtime/typed/AssetCatalog.hpp"
#include "engine/runtime/typed/Assets.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/graphics/shaders/TypedShaderModule.hpp"
#include "engine/animation/AnimationManager.hpp"
#include "game/screens/idle/IdleScreen.hpp"
#if defined(BIOFUEL_ENABLE_DEV_SCREENS) && defined(BIOFUEL_DEV_STARTUP_HAND_LAB)
#include "game/screens/dev_hand_lab/DevHandLabScreen.hpp"
#endif
#include <raylib.h>
#include <string>

namespace biofuel::game::screens {

namespace {

template<typename TShader>
void ensureShaderLoaded(::biofuel::engine::graphics::ShaderManager& shaderManager) {
    if (::biofuel::engine::runtime::typed::Shaders::loaded<TShader>(shaderManager)) {
        return;
    }

    ::biofuel::engine::runtime::typed::Shaders::load<TShader>(shaderManager);
}

} // namespace

} // namespace biofuel::game::screens

namespace biofuel::engine::ui::typed {

namespace {

[[nodiscard]] Rectangle loadingPanelRect(const RenderContext& context) noexcept {
    return Rectangle{
        static_cast<f32>((context.screenWidth - 520) / 2),
        static_cast<f32>(context.screenHeight / 2 - 88),
        520.0f,
        164.0f
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
        const Color titleColor = {208, 220, 240, 255};
        const i32 titleW = Renderer::measureText(TITLE, ::biofuel::game::screens::LoadingScreen::TITLE_SIZE);
        Renderer::drawText(
            TITLE,
            (context.screenWidth - titleW) / 2,
            static_cast<i32>(panel.y) - 78,
            ::biofuel::game::screens::LoadingScreen::TITLE_SIZE,
            titleColor
        );
    }
};

template<>
struct RenderElementExecutor<loading::LoadingPanelElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen&, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        const Rectangle panel = loadingPanelRect(context);
        Renderer::drawRect(static_cast<i32>(panel.x), static_cast<i32>(panel.y), static_cast<i32>(panel.width), static_cast<i32>(panel.height), {14, 19, 28, 182});
        Renderer::drawRectLines(static_cast<i32>(panel.x), static_cast<i32>(panel.y), static_cast<i32>(panel.width), static_cast<i32>(panel.height), {72, 96, 124, 188});
    }
};

template<>
struct RenderElementExecutor<loading::ProgressBarElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        const Rectangle panel = loadingPanelRect(context);
        const i32 barX = (context.screenWidth - ::biofuel::game::screens::LoadingScreen::BAR_WIDTH) / 2;
        const i32 barY = static_cast<i32>(panel.y) + 62;
        Renderer::drawRectLines(barX, barY, ::biofuel::game::screens::LoadingScreen::BAR_WIDTH, ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT, {84, 104, 132, 255});

        const i32 fillW = static_cast<i32>((::biofuel::game::screens::LoadingScreen::BAR_WIDTH - 4) * screen.m_displayProgress);
        if (fillW > 0) {
            Renderer::drawRect(barX + 2, barY + 2, fillW, ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT - 4, {92, 182, 224, 255});
        }
    }
};

template<>
struct RenderElementExecutor<loading::StatusTextElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        const Rectangle panel = loadingPanelRect(context);
        const i32 barY = static_cast<i32>(panel.y) + 62;
        const bool fullyDone = screen.m_tasksDone && screen.m_displayProgress >= 1.0f;
        std::string status = "Ready.";
        if (!fullyDone) {
            const i32 dotCount = static_cast<i32>(screen.m_elapsed / ::biofuel::game::screens::LoadingScreen::DOTS_INTERVAL) % 4;
            status = screen.m_tasks.currentName() + std::string(dotCount, '.');
        }

        const i32 statusW = Renderer::measureText(status, ::biofuel::game::screens::LoadingScreen::STATUS_SIZE);
        Renderer::drawText(status, (context.screenWidth - statusW) / 2, barY + ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT + 18, ::biofuel::game::screens::LoadingScreen::STATUS_SIZE, {216, 224, 236, 255});
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
        const i32 barY = static_cast<i32>(panel.y) + 62;
        static constexpr std::string_view SKIP_HINT = "Press any key to continue...";
        constexpr i32 HINT_SIZE = 14;
        const i32 hintW = Renderer::measureText(SKIP_HINT, HINT_SIZE);
        Renderer::drawText(
            SKIP_HINT,
            (context.screenWidth - hintW) / 2,
            barY + ::biofuel::game::screens::LoadingScreen::BAR_HEIGHT + 64,
            HINT_SIZE,
            {130, 148, 172, 255}
        );
    }
};

template<>
struct RenderElementExecutor<loading::FooterTextElement, ::biofuel::game::screens::LoadingScreen> {
    static void render(::biofuel::game::screens::LoadingScreen&, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        static constexpr std::string_view FOOTER = "v0.1.0  |  C++20  |  Raylib 5.5";
        constexpr i32 FOOTER_SIZE = 12;
        const i32 footerW = Renderer::measureText(FOOTER, FOOTER_SIZE);
        Renderer::drawText(FOOTER, (context.screenWidth - footerW) / 2, context.screenHeight - 30, FOOTER_SIZE, {92, 104, 126, 255});
    }
};

} // namespace biofuel::engine::ui::typed

namespace biofuel::game::screens {

LoadingScreen::LoadingScreen(i32 width, i32 height, i32 targetFps)
    : m_appWidth(width), m_appHeight(height)
    , m_appTargetFps(targetFps) {}

void LoadingScreen::buildTasks() {
    m_tasks.clear();
    m_tasks.reserve(16U + ::biofuel::engine::runtime::typed::AssetCatalog<
        ::biofuel::engine::runtime::typed::EngineStartupCatalog>::Assets::size);

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
        ::biofuel::engine::runtime::Runtime::events().init();
    }});
    m_tasks.add({"Initializing screen stack...", 0.5f, []() {
        ::biofuel::engine::runtime::Runtime::screen().init();
    }});
    m_tasks.add({"Initializing animation system...", 0.5f, []() {
        ::biofuel::engine::runtime::Runtime::animation().init();
    }});
    m_tasks.add({"Initializing physics engine...", 0.5f, []() {
        ::biofuel::engine::runtime::Runtime::physics().init();
    }});
    m_tasks.add({"Initializing shader system...", 0.3f, []() {
        ::biofuel::engine::runtime::Runtime::shader().init();
    }});
    m_tasks.add({"Initializing model system...", 0.4f, []() {
        ::biofuel::engine::runtime::Runtime::model().init();
    }});
    m_tasks.add({"Initializing audio device...", 0.5f, []() {
        ::biofuel::engine::runtime::Runtime::audio().init();
    }});
    m_tasks.add({"Initializing video system...", 0.4f, []() {
        ::biofuel::engine::runtime::Runtime::video().init();
    }});

    auto& shaderManager = ::biofuel::engine::runtime::Runtime::shader();
    m_tasks.add({"Compiling blur horizontal shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::BlurH>(shaderManager);
    }});
    m_tasks.add({"Compiling blur vertical shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::BlurV>(shaderManager);
    }});
    m_tasks.add({"Compiling blur composite shader...", 1.2f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::BlurComposite>(shaderManager);
    }});
    m_tasks.add({"Compiling crossfade shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::Crossfade>(shaderManager);
    }});
    m_tasks.add({"Compiling loading prelude shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::LoadingPrelude>(shaderManager);
    }});
    m_tasks.add({"Compiling menu option shader...", 1.3f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::MenuOption>(shaderManager);
    }});
    m_tasks.add({"Compiling background shader...", 2.0f, [&shaderManager]() {
        ensureShaderLoaded<::biofuel::engine::runtime::typed::shader::MainMenuBg>(shaderManager);
    }});

    auto& modelService = ::biofuel::engine::runtime::Runtime::model();
    for (const auto& modelSpec : modelService.registry()) {
        if (!modelSpec.preloadOnStartup) {
            continue;
        }

        std::string taskName = "Loading model asset: ";
        taskName += modelSpec.debugName;
        if (!modelSpec.shaderName.empty()) {
            taskName += " (model + shader)";
        }

        m_tasks.add({std::move(taskName), 1.6f, [assetId = modelSpec.id]() {
            (void)::biofuel::engine::runtime::Runtime::model().preload(assetId);
        }});
    }

    m_tasks.add({"Caching transition shader...", 1.0f, []() {
        ::biofuel::engine::runtime::Runtime::screen().preloadCrossfadeShader();
    }});
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
#if defined(BIOFUEL_ENABLE_DEV_SCREENS) && defined(BIOFUEL_DEV_STARTUP_HAND_LAB)
        sm->queueReplace<DevHandLabScreen>();
#elif defined(BIOFUEL_DEV_STARTUP_IDLE_VIDEO)
        sm->queueReplace<IdleScreen>(IdleScreen::idleVideoPath());
#else
        sm->queueReplace<MainMenuScreen>();
#endif
    }
}

} // namespace biofuel::game::screens
