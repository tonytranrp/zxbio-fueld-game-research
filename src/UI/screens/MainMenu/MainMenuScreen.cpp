#include "MainMenuScreen.hpp"
#include "PausePopupScreen/PausePopupScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/Shader/MainMenuBgModule.hpp"
#include <raylib.h>
#include <cmath>
#include <memory>

namespace biofuel::ui::screens {

void MainMenuScreen::onEnter() {
    m_selected = 0;
    m_hovered = -1;
    m_cooldown = 0.0f;
    m_titlePulse = 0.0f;
    m_menuFxTime = 0.0f;
    m_menuSlide = {};
    m_dismiss = {};
    m_dimensionShift = 0.0f;
    m_cameraComponent.reset();
    m_cameraPhase = CameraPhase::Idle;

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
    m_transitionHands.load();
    m_transitionHands.reset();

#ifdef BIOFUEL_DEV_STARTUP_PAUSE_POPUP
    if (auto* sm = manager()) {
        sm->queuePush(std::make_unique<PausePopupScreen>());
    }
#endif
}

void MainMenuScreen::onExit() {
    m_transitionHands.unload();
}

void MainMenuScreen::onUpdate(const f32 dt) {
    m_backdrop.update(dt);
    updateMenuSlide(dt);
    updateDismiss(dt);
    updateDimensionShift(dt);
    m_cameraComponent.update(dt);
    m_transitionHands.update(dt, m_dimensionShift, m_cameraComponent.controller().current());
    m_menuFxTime += dt;
    // Prevent precision loss after extended play — wrap at ~16 min
    if (m_menuFxTime > 1000.0f) {
        m_menuFxTime = std::fmod(m_menuFxTime, 1000.0f);
    }

    if (m_introPhase == IntroPhase::WaitingForTransition) {
        if (!isTransitioning() && backgroundRevealProgress() >= BG_TEXT_SYNC_THRESHOLD) {
            startIntro();
        }
        return;
    }

    if (m_introPhase != IntroPhase::Done) {
        advanceIntro(dt);
    }

#ifdef BIOFUEL_DEV_STARTUP_MENU_TRANSITION
    if (!isDismissing()
        && !isTransitioning()
        && backgroundRevealProgress() >= BG_TEXT_SYNC_THRESHOLD
        && m_introPhase >= IntroPhase::MenuFade)
    {
        startDismiss();
    }
#endif

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

    // Pass dimension shift to shader before render
    m_backdrop.setFloat(
        utils::render::shader::MainMenuBgModule::UNIFORM_UDIMENSION_SHIFT,
        m_dimensionShift
    );

    // Apply camera component uniforms to the shader directly
    m_cameraComponent.apply(m_backdrop.shader());

    m_backdrop.render(transitionAlpha());
    m_transitionHands.render();

    // ---- Compute dismiss offsets per element ----
    const f32 titleDismiss  = m_dismiss.progress(UIDismissState::ELEM_TITLE);
    const f32 hintsDismiss  = m_dismiss.progress(UIDismissState::ELEM_HINTS);
    const f32 menuDismiss   = m_dismiss.progress(UIDismissState::ELEM_MENU);
    const f32 footerDismiss = m_dismiss.progress(UIDismissState::ELEM_FOOTER);

    // Title block slides LEFT; menu/footer slide DOWN
    const i32 titleSlideX  = -static_cast<i32>(static_cast<f32>(DISMISS_SLIDE_LEFT) * titleDismiss);
    const i32 hintsSlideX  = -static_cast<i32>(static_cast<f32>(DISMISS_SLIDE_LEFT) * hintsDismiss);
    const i32 menuSlideY   =  static_cast<i32>(static_cast<f32>(DISMISS_SLIDE_DOWN) * menuDismiss);
    const i32 footerSlideY =  static_cast<i32>(static_cast<f32>(DISMISS_SLIDE_DOWN) * footerDismiss);

    // Fade multiplier: 1.0 at rest, 0.0 when fully dismissed
    const f32 titleFadeMul  = 1.0f - titleDismiss;
    const f32 hintsFadeMul  = 1.0f - hintsDismiss;
    const f32 menuFadeMul   = 1.0f - menuDismiss;
    const f32 footerFadeMul = 1.0f - footerDismiss;

    if (m_introPhase >= IntroPhase::TitleFade && titleDismiss < 1.0f) {
        const f32 pulse = (std::sin(m_titlePulse * TITLE_PULSE_SPEED) * 0.5f + 0.5f)
                          * TITLE_PULSE_RANGE + TITLE_PULSE_MIN;
        const u8 fadeAlpha = static_cast<u8>(m_titleFade.alpha() * 255.0f * titleFadeMul);
        const Color titleColor = {
            static_cast<u8>(pulse),
            static_cast<u8>(pulse * 0.85f),
            static_cast<u8>(pulse * 0.5f),
            fadeAlpha
        };

        static constexpr std::string_view titleStr = "FUEL FARM";
        Renderer::drawText(titleStr, TITLE_X + titleSlideX, TITLE_Y, TITLE_FONT_SIZE, titleColor);
    }

    if (m_introPhase >= IntroPhase::SubtitleFade && titleDismiss < 1.0f) {
        const u8 alpha = static_cast<u8>(m_subtitleFade.alpha() * 255.0f * titleFadeMul);
        const Color subColor = {COLOR_GRAY_DIM.r, COLOR_GRAY_DIM.g, COLOR_GRAY_DIM.b, alpha};
        static constexpr std::string_view subtitleStr = "2D Pixel-Art Biofuel Management Sim";
        Renderer::drawText(
            subtitleStr,
            TITLE_X + titleSlideX,
            TITLE_Y + TITLE_FONT_SIZE + TITLE_SUBTITLE_GAP,
            SUBTITLE_FONT_SIZE,
            subColor
        );
    }

    if (m_introPhase >= IntroPhase::HintsFade && hintsDismiss < 1.0f) {
        const u8 alpha = static_cast<u8>(m_hintsFade.alpha() * 255.0f * hintsFadeMul);
        const Color hintColor = {COLOR_GRAY_DIM.r, COLOR_GRAY_DIM.g, COLOR_GRAY_DIM.b, alpha};
        static constexpr std::string_view hintsStr = "ESC Pause  |  LEFT / RIGHT Navigate  |  ENTER Select";
        const i32 subtitleY = TITLE_Y + TITLE_FONT_SIZE + TITLE_SUBTITLE_GAP;
        const i32 hintsY = subtitleY + SUBTITLE_FONT_SIZE + SUBTITLE_HINTS_GAP;
        Renderer::drawText(hintsStr, TITLE_X + hintsSlideX, hintsY, HINTS_FONT_SIZE, hintColor);
    }

    if (m_introPhase >= IntroPhase::MenuFade && menuDismiss < 1.0f) {
        auto layout = MENU_LAYOUT;
        const u8 menuAlpha = static_cast<u8>(m_menuFade.alpha() * 255.0f * menuFadeMul);
        layout.colorSelected.a = menuAlpha;
        layout.colorSelectedGlow.a = menuAlpha;
        layout.colorSide.a = menuAlpha;
        layout.colorSideLocked.a = menuAlpha;
        layout.colorLockedLabel.a = menuAlpha;

        utils::ui::renderHorizontalCarousel(
            std::span{s_items},
            m_selected,
            m_hovered,
            sw / 2,
            sh - MENU_BAR_Y_OFFSET + menuSlideY,
            layout,
            m_menuSlide.motion(),
            m_menuFxTime
        );
    }

    if (footerDismiss < 1.0f) {
        static constexpr std::string_view versionStr = "v0.1.0 | Raylib 5.5 | C++20";
        const i32 versionX = sw - FOOTER_MARGIN_X;
        const i32 versionY = sh - FOOTER_BOTTOM_OFFSET + footerSlideY;
        const Color footerColor = {
            COLOR_VERSION.r, COLOR_VERSION.g, COLOR_VERSION.b,
            static_cast<u8>(static_cast<f32>(COLOR_VERSION.a) * footerFadeMul)
        };
        Renderer::drawText(
            versionStr,
            versionX - Renderer::measureText(versionStr, FOOTER_FONT_SIZE),
            versionY,
            FOOTER_FONT_SIZE,
            footerColor
        );
    }
}

void MainMenuScreen::onInput() {
    if (isDismissing()) {
        return;
    }

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
    m_hovered = hit.hoveredIndex;
    if (hit.hoveredIndex >= 0) {
        if (hit.clicked) {
            if (hit.hoveredIndex == m_selected) {
                activateSelected();
            } else {
                selectMenuIndex(hit.hoveredIndex);
            }
        }
    }
}

void MainMenuScreen::activateSelected() {
    switch (m_selected) {
    case 0: // New Game
    case 1: // Continue
        startDismiss();
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

// ------------------------------------------------------------------------------
// Dismiss Animation
// ------------------------------------------------------------------------------

void MainMenuScreen::startDismiss() {
    if (m_dismiss.active) {
        return;
    }
    m_dismiss.active = true;
    m_dismiss.elapsed = 0.0f;
    m_transitionHands.start();
}

void MainMenuScreen::updateDismiss(const f32 dt) noexcept {
    if (!m_dismiss.active) {
        return;
    }
    m_dismiss.elapsed += dt;
}

bool MainMenuScreen::isDismissing() const noexcept {
    return m_dismiss.active;
}

// ------------------------------------------------------------------------------
// Dimension Shift
// ------------------------------------------------------------------------------

void MainMenuScreen::updateDimensionShift(const f32 dt) noexcept {
    if (!m_dismiss.isDone()) {
        return;  // only start after dismiss fully completes
    }
    if (m_dimensionShift >= 1.0f) {
        return;  // already at maximum
    }
    m_dimensionShift = std::min(1.0f, m_dimensionShift + dt / DIMENSION_SHIFT_DURATION);

    // Start camera sequence once when dimension shift begins
    if (m_cameraPhase == CameraPhase::Idle) {
        startCameraSequence();
    }

    // Advance to next phase when current animation completes
    advanceCameraSequence();
}

void MainMenuScreen::startCameraSequence() noexcept {
    // Phase 1: instantly snap to looking right (very brief, ~0.1s)
    m_cameraComponent.controller().reset();
    m_cameraComponent.controller().setTarget(
        utils::render::component::ShaderCameraState{.yaw = CAMERA_YAW_RIGHT},
        0.1f,
        animation::Easing::easeOutCubic
    );
    m_cameraPhase = CameraPhase::SweepToLeft;
}

void MainMenuScreen::advanceCameraSequence() noexcept {
    if (!m_cameraComponent.controller().isComplete()) {
        return;  // current phase still animating
    }

    switch (m_cameraPhase) {
    case CameraPhase::SweepToLeft:
        // Phase 2: smooth sweep from right → left
        m_cameraComponent.controller().setTarget(
            utils::render::component::ShaderCameraState{.yaw = CAMERA_YAW_LEFT},
            CAMERA_SWEEP_DURATION,
            animation::Easing::easeInOutCubic
        );
        m_cameraPhase = CameraPhase::ReturnToCenter;
        break;

    case CameraPhase::ReturnToCenter:
        // Phase 3: smooth sweep from left → center
        m_cameraComponent.controller().setTarget(
            utils::render::component::ShaderCameraState{.yaw = 0.0f},
            CAMERA_RETURN_DURATION,
            animation::Easing::easeInOutQuad
        );
        m_cameraPhase = CameraPhase::Done;
        break;

    case CameraPhase::Idle:
    case CameraPhase::Done:
        break;
    }
}

} // namespace biofuel::ui::screens
