#include "PausePopupScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include <raylib.h>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------

f32 PausePopupScreen::panelSlideOffsetX(const i32 screenWidth) const {
    return (screenWidth + PANEL_WIDTH) / 2.0f * m_panelSlidePct;
}

// ------------------------------------------------------------------------------
// Screen lifecycle
// ------------------------------------------------------------------------------

void PausePopupScreen::onEnter() {
    m_selected = 0;
    m_cooldown = 0.0f;

    setRenderPassthrough(false);
    setTransitionDuration(0.0f);  // ScreenManager transition disabled — we animate ourselves

    m_overlayAlpha = 0;
    m_panelSlidePct = 1.0f;
    m_animatingIn = true;
    m_animatingOut = false;
    m_quitting = false;

    startSlideIn();
}

void PausePopupScreen::onExit() {
    auto& mgr = animation::AnimationManager::instance();
    mgr.cancelAll("pause_in_overlay");
    mgr.cancelAll("pause_in_slide");
    mgr.cancelAll("pause_out_overlay");
    mgr.cancelAll("pause_out_slide");
    mgr.prune();
}

void PausePopupScreen::onUpdate(const f32 dt) {
    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
    }
    // Animation values are driven by AnimationManager callbacks.
}

// ------------------------------------------------------------------------------
// Rendering
// ------------------------------------------------------------------------------

void PausePopupScreen::onRender() {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    // Dark backdrop — alpha driven by animation (0→180 for slide-in)
    Renderer::drawRect(0, 0, sw, sh, {0, 0, 0, m_overlayAlpha});

    // Skip panel while fully off-screen
    if (m_overlayAlpha < 5 && m_panelSlidePct > 0.95f) {
        return;
    }

    const i32 panelW = PANEL_WIDTH;
    const i32 panelH = PANEL_HEIGHT;
    const i32 panelX = (sw - panelW) / 2 + static_cast<i32>(panelSlideOffsetX(sw));
    const i32 panelY = (sh - panelH) / 2;

    // Panel background
    Renderer::drawRect(panelX, panelY, panelW, panelH, {30, 30, 40, 240});
    Renderer::drawRectLines(panelX, panelY, panelW, panelH, {80, 80, 100, 255});

    // Title
    constexpr std::string_view title = "PAUSED";
    const i32 titleW = MeasureText(title.data(), TITLE_SIZE);
    Renderer::drawText(
        std::string{title},
        panelX + (panelW - titleW) / 2,
        panelY + 28,
        TITLE_SIZE,
        RAYWHITE
    );

    // Separator
    const i32 sepY = panelY + 70;
    Renderer::drawRect(panelX + 40, sepY, panelW - 80, 2, {80, 80, 100, 255});

    // Menu items
    const i32 menuStartY = sepY + 28;
    utils::ui::renderVerticalMenu(
        std::span{s_items},
        m_selected,
        panelX + panelW / 2,
        menuStartY,
        MENU_LAYOUT
    );

    // Hint
    constexpr std::string_view hint = "ESC to close  |  \x1A\x1B to navigate  |  ENTER to select";
    const i32 hintW = MeasureText(hint.data(), HINT_SIZE);
    Renderer::drawText(
        std::string{hint},
        panelX + (panelW - hintW) / 2,
        panelY + panelH - 32,
        HINT_SIZE,
        {120, 120, 140, 255}
    );
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

    // Block navigation while sliding in
    if (m_animatingIn) {
        return;
    }

    // Keyboard navigation
    if (utils::ui::navigateVerticalMenu(
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
    const i32 sw = utils::render::Renderer::screenWidth();
    const i32 sh = utils::render::Renderer::screenHeight();
    const i32 panelX = (sw - PANEL_WIDTH) / 2 + static_cast<i32>(panelSlideOffsetX(sw));
    const i32 panelY = (sh - PANEL_HEIGHT) / 2;
    const i32 menuStartY = panelY + 70 + 28;

    const auto hit = utils::ui::hitTestVerticalMenu(
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
    auto& mgr = animation::AnimationManager::instance();

    // Overlay: 0 → 180
    auto overlayAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_in_overlay",
        0.0f, 180.0f, SLIDE_DURATION,
        animation::Easing::easeOutQuad
    );
    overlayAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_overlayAlpha = static_cast<u8>(a->current());
    });
    overlayAnim->onComplete([this](animation::Animation<f32>*) {
        m_overlayAlpha = 180;
        m_animatingIn = false;
    });
    overlayAnim->onCancel([this](animation::Animation<f32>*) {
        m_animatingIn = false;
    });
    mgr.add(std::move(overlayAnim));

    // Panel: slide from right (1.0 → 0.0)
    auto slideAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_in_slide",
        1.0f, 0.0f, SLIDE_DURATION,
        animation::Easing::easeOutCubic
    );
    slideAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_panelSlidePct = a->current();
    });
    mgr.add(std::move(slideAnim));
}

void PausePopupScreen::startSlideOut() {
    m_animatingOut = true;

    // Let MainMenuScreen render through while this screen slides away
    setRenderPassthrough(true);

    auto& mgr = animation::AnimationManager::instance();

    mgr.cancelAll("pause_in_overlay");
    mgr.cancelAll("pause_in_slide");

    // Overlay: 180 → 0 — easeOutQuad clears quickly then decelerates to transparent
    auto overlayAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_out_overlay",
        180.0f, 0.0f, SLIDE_DURATION,
        animation::Easing::easeOutQuad
    );
    overlayAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_overlayAlpha = static_cast<u8>(a->current());
    });
    mgr.add(std::move(overlayAnim));

    // Panel: continues leftward off-screen (0.0 → -1.0)
    auto slideAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_out_slide",
        0.0f, -1.0f, SLIDE_DURATION,
        animation::Easing::easeOutQuad
    );
    slideAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_panelSlidePct = a->current();
    });
    slideAnim->onComplete([this](animation::Animation<f32>*) {
        if (auto* sm = manager()) {
            sm->pop();
            if (m_quitting) {
                sm->requestQuit();
            }
        }
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

} // namespace biofuel::ui::screens
