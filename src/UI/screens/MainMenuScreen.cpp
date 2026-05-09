#include "MainMenuScreen.hpp"
#include "PausePopupScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/Shader/MainMenuBgModule.hpp"
#include <raylib.h>
#include <cmath>
#include <memory>

namespace biofuel::ui::screens {

void MainMenuScreen::onEnter() {
    m_selected = 0;
    m_cooldown = 0.0f;
    m_titlePulse = 0.0f;
    m_menuSlide = {};

    m_introPhase = IntroPhase::WaitingForTransition;
    m_titleFade.elapsed = 0.0f;
    m_subtitleFade.elapsed = 0.0f;
    m_hintsFade.elapsed = 0.0f;
    m_menuFade.elapsed = 0.0f;

    m_backdrop.configure(animation::screen::ScreenBackdropConfig{
        .shaderName = utils::render::shader::MainMenuBgModule::NAME,
        .fallbackColor = COLOR_BG,
        .revealDelay = BG_REVEAL_DELAY,
        .revealDuration = BG_REVEAL_DURATION,
        .brightnessFloor = 0.0f,
        .brightnessCeiling = 1.0f,
        .transitionWeight = 0.45f,
        .revealWeight = 0.55f,
    });
    m_backdrop.reset();

#ifdef BIOFUEL_DEV_STARTUP_PAUSE_POPUP
    if (auto* sm = manager()) {
        sm->queuePush(std::make_unique<PausePopupScreen>());
    }
#endif
}

void MainMenuScreen::onExit() {}

void MainMenuScreen::onUpdate(const f32 dt) {
    m_backdrop.update(dt);
    updateMenuSlide(dt);

    if (m_introPhase == IntroPhase::WaitingForTransition) {
        if (!isTransitioning() && backgroundRevealProgress() >= BG_TEXT_SYNC_THRESHOLD) {
            startIntro();
        }
        return;
    }

    if (m_introPhase != IntroPhase::Done) {
        advanceIntro(dt);
    }

    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
    }

    if (m_introPhase >= IntroPhase::TitleFade) {
        m_titlePulse += dt;
    }
}

void MainMenuScreen::startIntro() {
    m_introPhase = IntroPhase::TitleFade;
    m_titleFade.elapsed = 0.001f;
}

void MainMenuScreen::advanceIntro(const f32 dt) {
    if (m_introPhase >= IntroPhase::TitleFade) {
        m_titleFade.elapsed += dt;
    }

    if (m_introPhase == IntroPhase::TitleFade && m_titleFade.elapsed >= m_subtitleFade.delay) {
        m_introPhase = IntroPhase::SubtitleFade;
        m_subtitleFade.elapsed = 0.001f;
    }

    if (m_introPhase >= IntroPhase::SubtitleFade) {
        m_subtitleFade.elapsed += dt;
    }

    if (m_introPhase == IntroPhase::SubtitleFade && m_subtitleFade.elapsed >= m_hintsFade.delay) {
        m_introPhase = IntroPhase::HintsFade;
        m_hintsFade.elapsed = 0.001f;
    }

    if (m_introPhase >= IntroPhase::HintsFade) {
        m_hintsFade.elapsed += dt;
    }

    if (m_introPhase == IntroPhase::HintsFade && m_hintsFade.elapsed >= m_menuFade.delay) {
        m_introPhase = IntroPhase::MenuFade;
        m_menuFade.elapsed = 0.001f;
    }

    if (m_introPhase >= IntroPhase::MenuFade) {
        m_menuFade.elapsed += dt;
    }

    if (m_introPhase == IntroPhase::MenuFade && m_menuFade.alpha() >= 1.0f) {
        m_introPhase = IntroPhase::Done;
    }
}

void MainMenuScreen::onRender() {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    m_backdrop.render(transitionAlpha());

    if (m_introPhase >= IntroPhase::TitleFade) {
        const f32 pulse = (std::sin(m_titlePulse * TITLE_PULSE_SPEED) * 0.5f + 0.5f)
                          * TITLE_PULSE_RANGE + TITLE_PULSE_MIN;
        const u8 fadeAlpha = static_cast<u8>(m_titleFade.alpha() * 255.0f);
        const Color titleColor = {
            static_cast<u8>(pulse),
            static_cast<u8>(pulse * 0.85f),
            static_cast<u8>(pulse * 0.5f),
            fadeAlpha
        };

        static constexpr std::string_view titleStr = "FUEL FARM";
        Renderer::drawText(titleStr, TITLE_X, TITLE_Y, TITLE_FONT_SIZE, titleColor);
    }

    if (m_introPhase >= IntroPhase::SubtitleFade) {
        const u8 alpha = static_cast<u8>(m_subtitleFade.alpha() * 255.0f);
        const Color subColor = {COLOR_GRAY_DIM.r, COLOR_GRAY_DIM.g, COLOR_GRAY_DIM.b, alpha};
        static constexpr std::string_view subtitleStr = "2D Pixel-Art Biofuel Management Sim";
        Renderer::drawText(
            subtitleStr,
            TITLE_X,
            TITLE_Y + TITLE_FONT_SIZE + TITLE_SUBTITLE_GAP,
            SUBTITLE_FONT_SIZE,
            subColor
        );
    }

    if (m_introPhase >= IntroPhase::HintsFade) {
        const u8 alpha = static_cast<u8>(m_hintsFade.alpha() * 255.0f);
        const Color hintColor = {COLOR_GRAY_DIM.r, COLOR_GRAY_DIM.g, COLOR_GRAY_DIM.b, alpha};
        static constexpr std::string_view hintsStr = "ESC Pause  |  LEFT / RIGHT Navigate  |  ENTER Select";
        const i32 subtitleY = TITLE_Y + TITLE_FONT_SIZE + TITLE_SUBTITLE_GAP;
        const i32 hintsY = subtitleY + SUBTITLE_FONT_SIZE + SUBTITLE_HINTS_GAP;
        Renderer::drawText(hintsStr, TITLE_X, hintsY, HINTS_FONT_SIZE, hintColor);
    }

    if (m_introPhase >= IntroPhase::MenuFade) {
        auto layout = MENU_LAYOUT;
        const u8 menuAlpha = static_cast<u8>(m_menuFade.alpha() * 255.0f);
        layout.colorSelected.a = menuAlpha;
        layout.colorSelectedGlow.a = menuAlpha;
        layout.colorSide.a = menuAlpha;
        layout.colorSideLocked.a = menuAlpha;
        layout.colorLockedLabel.a = menuAlpha;

        utils::ui::renderHorizontalCarousel(
            std::span{s_items},
            m_selected,
            sw / 2,
            sh - MENU_BAR_Y_OFFSET,
            layout,
            m_menuSlide.motion()
        );
    }

    static constexpr std::string_view versionStr = "v0.1.0 | Raylib 5.5 | C++20";
    const i32 versionX = sw - FOOTER_MARGIN_X;
    const i32 versionY = sh - FOOTER_BOTTOM_OFFSET;
    Renderer::drawText(
        versionStr,
        versionX - Renderer::measureText(versionStr, FOOTER_FONT_SIZE),
        versionY,
        FOOTER_FONT_SIZE,
        COLOR_VERSION
    );
}

void MainMenuScreen::onInput() {
    if (m_introPhase != IntroPhase::Done) {
        return;
    }

    using namespace utils::render;

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (auto* sm = manager()) {
            sm->push(std::make_unique<PausePopupScreen>());
        }
        return;
    }

    i32 navigatedSelection = m_selected;
    if (utils::ui::navigateHorizontalMenu(
            navigatedSelection,
            static_cast<i32>(s_items.size()),
            m_cooldown,
            std::span{s_items},
            MENU_LAYOUT)) {
        activateSelected();
        return;
    }
    if (navigatedSelection != m_selected) {
        selectMenuIndex(navigatedSelection);
    }

    const auto hit = utils::ui::hitTestHorizontalCarousel(
        std::span{s_items},
        m_selected,
        Renderer::screenWidth() / 2,
        Renderer::screenHeight() - MENU_BAR_Y_OFFSET,
        MENU_LAYOUT,
        m_menuSlide.motion()
    );
    if (hit.hoveredIndex >= 0) {
        const bool hoveredSelected = hit.hoveredIndex == m_selected;
        if (!hoveredSelected) {
            selectMenuIndex(hit.hoveredIndex);
        }
        if (hit.clicked) {
            if (hoveredSelected) {
                activateSelected();
            }
        }
    }
}

void MainMenuScreen::activateSelected() {
    switch (m_selected) {
    case 0:
    case 1:
        break;
    case 2:
        if (auto* sm = manager()) {
            sm->requestQuit();
        }
        break;
    default:
        break;
    }
}

bool MainMenuScreen::isLocked(const i32 index) const {
    return s_items[index].locked;
}

f32 MainMenuScreen::backgroundRevealProgress() const noexcept {
    return m_backdrop.revealProgress();
}

void MainMenuScreen::updateMenuSlide(const f32 dt) noexcept {
    if (!m_menuSlide.active()) {
        m_menuSlide.direction = 0;
        m_menuSlide.elapsed = m_menuSlide.duration;
        return;
    }

    m_menuSlide.elapsed += dt;
    if (m_menuSlide.elapsed >= m_menuSlide.duration) {
        m_menuSlide.elapsed = m_menuSlide.duration;
        m_menuSlide.direction = 0;
    }
}

void MainMenuScreen::selectMenuIndex(const i32 newIndex) noexcept {
    if (newIndex == m_selected) {
        return;
    }

    const i32 oldIndex = m_selected;
    m_selected = newIndex;
    m_menuSlide.direction = inferMenuDirection(oldIndex, newIndex);
    m_menuSlide.elapsed = 0.0f;
}

i32 MainMenuScreen::inferMenuDirection(const i32 oldIndex, const i32 newIndex) const noexcept {
    const i32 itemCount = static_cast<i32>(s_items.size());
    if (itemCount <= 1 || oldIndex == newIndex) {
        return 0;
    }

    const i32 wrappedRight = (oldIndex + 1) % itemCount;
    const i32 wrappedLeft = (oldIndex - 1 + itemCount) % itemCount;
    if (newIndex == wrappedRight) {
        return 1;
    }
    if (newIndex == wrappedLeft) {
        return -1;
    }

    return (newIndex > oldIndex) ? 1 : -1;
}

} // namespace biofuel::ui::screens
