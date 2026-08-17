#include "PausePopupScreen.hpp"
#include "PausePopupScreenModule.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/animation/AnimationManager.hpp"
#include "engine/animation/PremadeAnimations.hpp"
#include "engine/animation/Easing.hpp"
#include <entt/signal/dispatcher.hpp>
#include <raylib.h>

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------

f32 PausePopupScreen::panelSlideOffsetX(const i32 screenWidth) const noexcept {
    return (screenWidth + PANEL_WIDTH) / 2.0f * m_panelSlidePct;
}

} // namespace biofuel::game::screens

namespace biofuel::engine::ui::typed {

namespace {

// Panel dimensions come from the screen class so they are defined once.
constexpr i32 PAUSE_PANEL_WIDTH = ::biofuel::game::screens::PausePopupScreen::PANEL_WIDTH;
constexpr i32 PAUSE_PANEL_HEIGHT = ::biofuel::game::screens::PausePopupScreen::PANEL_HEIGHT;

// Panel chrome layout, measured in pixels from the panel's top-left corner.
constexpr i32 TITLE_INSET_Y = 28;        // title baseline below the panel top
constexpr i32 SEPARATOR_Y = 70;          // divider line below the panel top
constexpr i32 SEPARATOR_INSET_X = 40;    // divider horizontal inset on each side
constexpr i32 SEPARATOR_HEIGHT = 2;
constexpr i32 MENU_TOP_GAP = 28;         // menu list top below the divider
constexpr i32 HINT_BOTTOM_INSET = 32;    // hint baseline above the panel bottom

constexpr Color PANEL_FILL_COLOR = {30, 30, 40, 240};
constexpr Color PANEL_BORDER_COLOR = {80, 80, 100, 255};
constexpr Color HINT_TEXT_COLOR = {120, 120, 140, 255};

struct PausePanelGeometry {
    i32 x = 0;
    i32 y = 0;
    i32 w = PAUSE_PANEL_WIDTH;
    i32 h = PAUSE_PANEL_HEIGHT;
};

[[nodiscard]] PausePanelGeometry pausePanelGeometry(
    const f32 panelSlidePct,
    const RenderContext& context) noexcept
{
    const f32 slideOffset = (context.screenWidth + PAUSE_PANEL_WIDTH) / 2.0f * panelSlidePct;
    return PausePanelGeometry{
        .x = (context.screenWidth - PAUSE_PANEL_WIDTH) / 2 +
            static_cast<i32>(slideOffset),
        .y = (context.screenHeight - PAUSE_PANEL_HEIGHT) / 2,
        .w = PAUSE_PANEL_WIDTH,
        .h = PAUSE_PANEL_HEIGHT,
    };
}

[[nodiscard]] bool pausePanelHidden(const bool blurActive, const f32 panelSlidePct) noexcept {
    return !blurActive && panelSlidePct > 0.95f;
}

} // namespace

template<>
struct RenderElementExecutor<pausepopup::BlurCaptureElement, ::biofuel::game::screens::PausePopupScreen> {
    static void render(::biofuel::game::screens::PausePopupScreen& screen, RenderContext&) {
        struct CaptureRequest {
            ScreenManager* manager = nullptr;
            Screen* screen = nullptr;
        };

        CaptureRequest captureRequest;
        if (auto* sm = screen.manager()) {
            captureRequest.manager = sm;
            captureRequest.screen = sm->screenBelowTop();
        }

        screen.m_blurEffect.render(
            [](void* userData, ::biofuel::engine::graphics::RenderSurface& target) {
                auto* request = static_cast<CaptureRequest*>(userData);
                if (request == nullptr || request->manager == nullptr) {
                    return;
                }
                request->manager->captureScreen(request->screen, target);
            },
            &captureRequest
        );
    }
};

template<>
struct RenderElementExecutor<pausepopup::PopupPanelElement, ::biofuel::game::screens::PausePopupScreen> {
    static void render(::biofuel::game::screens::PausePopupScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        if (pausePanelHidden(screen.m_blurEffect.isActive(), screen.m_panelSlidePct)) {
            return;
        }

        const auto panel = pausePanelGeometry(screen.m_panelSlidePct, context);
        Renderer::drawRect(panel.x, panel.y, panel.w, panel.h, PANEL_FILL_COLOR);
        Renderer::drawRectLines(panel.x, panel.y, panel.w, panel.h, PANEL_BORDER_COLOR);
    }
};

template<>
struct RenderElementExecutor<pausepopup::TitleTextElement, ::biofuel::game::screens::PausePopupScreen> {
    static void render(::biofuel::game::screens::PausePopupScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        if (pausePanelHidden(screen.m_blurEffect.isActive(), screen.m_panelSlidePct)) {
            return;
        }

        const auto panel = pausePanelGeometry(screen.m_panelSlidePct, context);
        static constexpr std::string_view title = "PAUSED";
        const i32 titleW = Renderer::measureText(title, ::biofuel::game::screens::PausePopupScreen::TITLE_SIZE);
        Renderer::drawText(title, panel.x + (panel.w - titleW) / 2, panel.y + TITLE_INSET_Y, ::biofuel::game::screens::PausePopupScreen::TITLE_SIZE, RAYWHITE);
    }
};

template<>
struct RenderElementExecutor<pausepopup::SeparatorElement, ::biofuel::game::screens::PausePopupScreen> {
    static void render(::biofuel::game::screens::PausePopupScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        if (pausePanelHidden(screen.m_blurEffect.isActive(), screen.m_panelSlidePct)) {
            return;
        }

        const auto panel = pausePanelGeometry(screen.m_panelSlidePct, context);
        Renderer::drawRect(
            panel.x + SEPARATOR_INSET_X,
            panel.y + SEPARATOR_Y,
            panel.w - 2 * SEPARATOR_INSET_X,
            SEPARATOR_HEIGHT,
            PANEL_BORDER_COLOR);
    }
};

template<>
struct RenderElementExecutor<pausepopup::VerticalMenuElement, ::biofuel::game::screens::PausePopupScreen> {
    static void render(::biofuel::game::screens::PausePopupScreen& screen, RenderContext& context) {
        if (pausePanelHidden(screen.m_blurEffect.isActive(), screen.m_panelSlidePct)) {
            return;
        }

        const auto panel = pausePanelGeometry(screen.m_panelSlidePct, context);
        game::presentation::widgets::renderVerticalMenu(
            std::span{::biofuel::game::screens::PausePopupScreen::s_items},
            screen.m_selected,
            panel.x + panel.w / 2,
            panel.y + SEPARATOR_Y + MENU_TOP_GAP,
            ::biofuel::game::screens::PausePopupScreen::MENU_LAYOUT
        );
    }
};

template<>
struct RenderElementExecutor<pausepopup::HintTextElement, ::biofuel::game::screens::PausePopupScreen> {
    static void render(::biofuel::game::screens::PausePopupScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;
        if (pausePanelHidden(screen.m_blurEffect.isActive(), screen.m_panelSlidePct)) {
            return;
        }

        const auto panel = pausePanelGeometry(screen.m_panelSlidePct, context);
        static constexpr std::string_view hint = "ESC to close  |  UP / DOWN to navigate  |  ENTER to select";
        const i32 hintW = Renderer::measureText(hint, ::biofuel::game::screens::PausePopupScreen::HINT_SIZE);
        Renderer::drawText(hint, panel.x + (panel.w - hintW) / 2, panel.y + panel.h - HINT_BOTTOM_INSET, ::biofuel::game::screens::PausePopupScreen::HINT_SIZE, HINT_TEXT_COLOR);
    }
};

} // namespace biofuel::engine::ui::typed

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// Screen lifecycle
// ------------------------------------------------------------------------------

void PausePopupScreen::onEnter() {
    ::biofuel::engine::debug::MemoryTelemetry::snapshot("pause.open.begin");
    m_selected = 0;
    m_cooldown = 0.0f;

    // Disable ScreenManager transition — we handle all animation ourselves
    setTransitionDuration(0.0f);

    m_panelSlidePct = 1.0f;
    m_animatingIn = true;
    m_animatingOut = false;
    m_quitting = false;

    // Initialize blur effect with current screen size
    const i32 sw = ::biofuel::engine::graphics::Renderer::screenWidth();
    const i32 sh = ::biofuel::engine::graphics::Renderer::screenHeight();
    m_blurEffect.init(sw, sh, BLUR_CONFIG);
    m_blurEffect.startBlurIn(BLUR_CONFIG);
    if (auto* sm = manager()) {
        struct CaptureRequest {
            ::biofuel::engine::ui::ScreenManager* manager = nullptr;
            ::biofuel::engine::ui::Screen* screen = nullptr;
        };

        CaptureRequest request{
            .manager = sm,
            .screen = sm->currentScreen(),
        };
        m_blurEffect.warmCache(
            [](void* userData, ::biofuel::engine::graphics::RenderSurface& target) {
                auto* capture = static_cast<CaptureRequest*>(userData);
                if (capture == nullptr || capture->manager == nullptr) {
                    return;
                }
                capture->manager->captureScreen(capture->screen, target);
            },
            &request);
    }

    startSlideIn();
}

void PausePopupScreen::onExit() {
    // Cancel any in-flight slide animations: their callbacks capture this, and
    // the global AnimationManager would otherwise fire them on a destroyed
    // screen if it is popped while a slide is still running.
    auto& mgr = ::biofuel::engine::runtime::Runtime::animation();
    mgr.cancelAll("pause_in_slide");
    mgr.cancelAll("pause_out_slide");

    m_blurEffect.shutdown();
    ::biofuel::engine::debug::MemoryTelemetry::snapshot("pause.close.exit");
}

void PausePopupScreen::onUpdate(const f32 dt) {
    if (m_wantsPop) {
        m_wantsPop = false;
        if (auto* sm = manager()) {
            sm->queuePop();
            if (m_quitting) {
                sm->requestQuit();
            }
        }
        return;
    }

    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
    }
    m_blurEffect.update(dt);
}

// ------------------------------------------------------------------------------
// Rendering
// ------------------------------------------------------------------------------

void PausePopupScreen::onRender() {
    ::biofuel::engine::ui::typed::RenderContext context{
        .manager = manager(),
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = screenId(),
        .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
        .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
        .transitionAlpha = transitionAlpha(),
        .frameTime = GetFrameTime(),
    };
    ::biofuel::engine::ui::typed::RenderPipeline<PausePopupScreen>::render(*this, context);
}

// ------------------------------------------------------------------------------
// Input
// ------------------------------------------------------------------------------

void PausePopupScreen::onInput() {
    // ESC dismisses via slide-out
    if (IsKeyPressed(KEY_ESCAPE) && !m_animatingOut) {
        startSlideOut();
        return;
    }

    if (m_animatingOut) {
        return;
    }

    // Block navigation while sliding in
    if (m_animatingIn) {
        return;
    }

    // Keyboard navigation
    if (game::presentation::widgets::navigateVerticalMenu(
            m_selected,
            static_cast<i32>(s_items.size()),
            m_cooldown,
            0.0f,
            std::span{s_items},
            MENU_LAYOUT)) {
        activateSelected();
        return;
    }

    // Mouse hit-testing — account for current slide position
    const i32 sw = ::biofuel::engine::graphics::Renderer::screenWidth();
    const i32 sh = ::biofuel::engine::graphics::Renderer::screenHeight();
    const i32 panelX = (sw - PANEL_WIDTH) / 2 + static_cast<i32>(panelSlideOffsetX(sw));
    const i32 panelY = (sh - PANEL_HEIGHT) / 2;
    const i32 menuStartY = panelY + 70 + 28;

    const auto hit = game::presentation::widgets::hitTestVerticalMenu(
        std::span{s_items},
        panelX + PANEL_WIDTH / 2,
        menuStartY,
        MENU_LAYOUT
    );

    if (hit.hoveredIndex >= 0) {
        m_selected = hit.hoveredIndex;
        if (hit.clicked) {
            activateSelected();
        }
    }
}

// ------------------------------------------------------------------------------
// Animation setup
// ------------------------------------------------------------------------------

void PausePopupScreen::startSlideIn() {
    auto& mgr = ::biofuel::engine::runtime::Runtime::animation();

    // Panel: slide from right (1.0 → 0.0)
    auto slideAnim = ::biofuel::engine::animation::PremadeAnimations::makeFloatLerp(
        "pause_in_slide",
        1.0f, 0.0f, SLIDE_DURATION,
        ::biofuel::engine::animation::Easing::easeOutCubic
    );
    slideAnim->onUpdate([this](::biofuel::engine::animation::Animation<f32>* a) {
        m_panelSlidePct = a->current();
    });
    slideAnim->onComplete([this](::biofuel::engine::animation::Animation<f32>*) {
        m_animatingIn = false;
    });
    slideAnim->onCancel([this](::biofuel::engine::animation::Animation<f32>*) {
        m_animatingIn = false;
    });
    mgr.add(std::move(slideAnim));
}

void PausePopupScreen::startSlideOut() {
    if (m_animatingOut) {
        return;
    }

    m_animatingOut = true;
    m_animatingIn = false;

    // Start blur fade-out — synchronized with panel slide
    m_blurEffect.startBlurOut(BLUR_CONFIG);

    auto& mgr = ::biofuel::engine::runtime::Runtime::animation();
    mgr.cancelAll("pause_in_slide");

    // Panel continues leftward from its current point, avoiding a snap if
    // ESC is pressed while the popup is still sliding in.
    auto slideAnim = ::biofuel::engine::animation::PremadeAnimations::makeFloatLerp(
        "pause_out_slide",
        m_panelSlidePct, -1.0f, SLIDE_DURATION,
        ::biofuel::engine::animation::Easing::easeOutQuad
    );
    slideAnim->onUpdate([this](::biofuel::engine::animation::Animation<f32>* a) {
        m_panelSlidePct = a->current();
    });
    slideAnim->onComplete([this](::biofuel::engine::animation::Animation<f32>*) {
        m_wantsPop = true;
    });
    mgr.add(std::move(slideAnim));
}

// ------------------------------------------------------------------------------
// Actions
// ------------------------------------------------------------------------------

void PausePopupScreen::activateSelected() {
    m_quitting = (m_selected == 1);
    startSlideOut();
}

} // namespace biofuel::game::screens
