#include "PausePopupScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include <raylib.h>

namespace biofuel::ui::screens {

void PausePopupScreen::onEnter() {
    m_selected = 0;
    m_cooldown = 0.0f;

    // Block rendering of screens below (MainMenuScreen won't show through)
    setRenderPassthrough(false);

    // Start with invisible state
    m_overlayAlpha = 0;
    m_panelScale = 0.0f;
    m_panelOffsetY = 30.0f;
    m_animatingIn = true;
    m_animatingOut = false;

    // Start fade-in animation using AnimationManager
    startFadeIn();
}

void PausePopupScreen::onExit() {
    // Cancel any running animations to prevent dangling pointer access
    auto& mgr = animation::AnimationManager::instance();
    mgr.cancelAll("pause_fade_in");
    mgr.cancelAll("pause_scale_in");
    mgr.cancelAll("pause_slide_in");
    mgr.cancelAll("pause_fade_out");
    mgr.cancelAll("pause_scale_out");
    mgr.cancelAll("pause_slide_out");
    mgr.prune();
}

void PausePopupScreen::onUpdate(const f32 dt) {
    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
    }

    // AnimationManager handles the animation updates - no extra work needed here.
    // m_overlayAlpha, m_panelScale, and m_panelOffsetY are driven by the
    // animation callbacks set in startFadeIn().
}

void PausePopupScreen::onRender() {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    // Dark overlay — alpha driven by animation (0→180 for fade-in)
    Renderer::drawRect(0, 0, sw, sh, {0, 0, 0, m_overlayAlpha});

    // Only render panel content if visible enough
    if (m_overlayAlpha < 10 && m_panelScale < 0.01f) {
        return;
    }

    // Panel with scale and slide offset
    const i32 panelW = static_cast<i32>(PANEL_WIDTH * m_panelScale);
    const i32 panelH = static_cast<i32>(PANEL_HEIGHT * m_panelScale);
    const i32 panelX = (sw - panelW) / 2;
    const i32 panelY = (sh - panelH) / 2 + static_cast<i32>(m_panelOffsetY);

    if (panelW <= 0 || panelH <= 0) {
        return;
    }

    // Panel background
    Renderer::drawRect(panelX, panelY, panelW, panelH, {30, 30, 40, 240});
    Renderer::drawRectLines(panelX, panelY, panelW, panelH, {80, 80, 100, 255});

    // Title — centered in panel
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

void PausePopupScreen::onInput() {
    // ESC → dismiss (starts fade-out animation)
    if (IsKeyPressed(KEY_ESCAPE) && !m_animatingOut) {
        startFadeOut();
        return;
    }

    // Block input while animating in
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

    // Mouse hit-testing
    const i32 sw = utils::render::Renderer::screenWidth();
    const i32 sh = utils::render::Renderer::screenHeight();
    const i32 panelX = (sw - PANEL_WIDTH) / 2;
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

void PausePopupScreen::startFadeIn() {
    auto& mgr = animation::AnimationManager::instance();

    // Overlay alpha: 0 → 180
    auto fadeAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_fade_in",
        0.0f, 180.0f, FADE_DURATION,
        animation::Easing::easeOutQuad
    );
    fadeAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_overlayAlpha = static_cast<u8>(a->current());
    });
    fadeAnim->onComplete([this](animation::Animation<f32>*) {
        m_overlayAlpha = 180;
        m_animatingIn = false;
    });
    fadeAnim->onCancel([this](animation::Animation<f32>*) {
        m_animatingIn = false;
    });
    mgr.add(std::move(fadeAnim));

    // Panel scale: 0.7 → 1.0 (easeOutBack for slight overshoot)
    auto scaleAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_scale_in",
        0.7f, 1.0f, SCALE_DURATION,
        animation::Easing::easeOutBack
    );
    scaleAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_panelScale = a->current();
    });
    mgr.add(std::move(scaleAnim));

    // Panel Y offset: 30 → 0 (slide up)
    auto slideAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_slide_in",
        30.0f, 0.0f, SCALE_DURATION,
        animation::Easing::easeOutCubic
    );
    slideAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_panelOffsetY = a->current();
    });
    mgr.add(std::move(slideAnim));
}

void PausePopupScreen::startFadeOut() {
    m_animatingOut = true;
    auto& mgr = animation::AnimationManager::instance();

    // Cancel any in-progress fade-in animations
    mgr.cancelAll("pause_fade_in");
    mgr.cancelAll("pause_scale_in");
    mgr.cancelAll("pause_slide_in");

    // Overlay alpha: 180 → 0
    auto fadeAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_fade_out",
        180.0f, 0.0f, FADE_DURATION,
        animation::Easing::easeInQuad
    );
    fadeAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_overlayAlpha = static_cast<u8>(a->current());
    });
    mgr.add(std::move(fadeAnim));

    // Panel scale: 1.0 → 0.7 (easeInQuad — smooth shrink, no overshoot)
    auto scaleAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_scale_out",
        1.0f, 0.7f, SCALE_DURATION,
        animation::Easing::easeInQuad
    );
    scaleAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_panelScale = a->current();
    });
    mgr.add(std::move(scaleAnim));

    // Panel Y offset: 0 → 30 (slide down, dismiss on complete)
    auto slideAnim = animation::PremadeAnimations::makeFloatLerp(
        "pause_slide_out",
        0.0f, 30.0f, SCALE_DURATION,
        animation::Easing::easeInQuad
    );
    slideAnim->onUpdate([this](animation::Animation<f32>* a) {
        m_panelOffsetY = a->current();
    });
    slideAnim->onComplete([this](animation::Animation<f32>*) {
        if (auto* sm = manager()) {
            setTransitionDuration(0.0f); // skip ScreenManager's transition overlay
            sm->pop();
            if (m_quitting) {
                sm->requestQuit();
            }
        }
    });
    mgr.add(std::move(slideAnim));
}

void PausePopupScreen::activateSelected() {
    m_quitting = (m_selected == 1); // index 1 = "Quit to Desktop"
    startFadeOut();
}

} // namespace biofuel::ui::screens