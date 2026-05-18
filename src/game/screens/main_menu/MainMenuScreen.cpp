#include "MainMenuScreen.hpp"
#include "MainMenuScreenModule.hpp"
#include "game/screens/join/JoinScreen.hpp"
#include "game/screens/idle/IdleScreen.hpp"
#include "game/presentation/hands/CalibrationFlowState.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/shaders/MainMenuBgModule.hpp"
#include "engine/audio/AudioManager.hpp"
#include "engine/input/InputServiceModule.hpp"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <memory>

namespace biofuel::game::screens {

namespace {

using ::biofuel::game::presentation::hands::CalibrationFlowState;
using ::biofuel::game::presentation::hands::CalibrationRoute;

} // namespace

} // namespace biofuel::game::screens

namespace biofuel::engine::ui::typed {

template<>
struct RenderElementExecutor<mainmenu::BackdropElement, ::biofuel::game::screens::MainMenuScreen> {
    static void render(::biofuel::game::screens::MainMenuScreen& screen, RenderContext& context) {
        screen.m_backdrop.setFloat(
            ::biofuel::engine::graphics::shader::MainMenuBgModule::UNIFORM_UDIMENSION_SHIFT,
            screen.m_dimensionShift
        );
        screen.m_backdrop.setFloat("uIdleDim", screen.m_idleTransitionDim);
        screen.m_cameraComponent.apply(screen.m_backdrop.shader());
        screen.m_backdrop.render(context.transitionAlpha);
    }
};

template<>
struct RenderElementExecutor<mainmenu::TitleBlockElement, ::biofuel::game::screens::MainMenuScreen> {
    static void render(::biofuel::game::screens::MainMenuScreen& screen, RenderContext&) {
        using namespace ::biofuel::engine::graphics;

        const f32 titleDismiss = screen.m_dismiss.progress(::biofuel::game::screens::UIDismissState::ELEM_TITLE);
        if (titleDismiss >= 1.0f) {
            return;
        }

        const i32 titleSlideX = -static_cast<i32>(static_cast<f32>(::biofuel::game::screens::MainMenuScreen::DISMISS_SLIDE_LEFT) * titleDismiss);
        const f32 titleFadeMul = 1.0f - titleDismiss;

        if (screen.m_introPhase >= ::biofuel::game::screens::IntroPhase::TitleFade) {
            const f32 pulse = (std::sin(screen.m_titlePulse * ::biofuel::game::screens::MainMenuScreen::TITLE_PULSE_SPEED) * 0.5f + 0.5f)
                * ::biofuel::game::screens::MainMenuScreen::TITLE_PULSE_RANGE + ::biofuel::game::screens::MainMenuScreen::TITLE_PULSE_MIN;
            const u8 fadeAlpha = static_cast<u8>(screen.m_titleFade.alpha() * 255.0f * titleFadeMul);
            const Color titleColor = {
                static_cast<u8>(pulse),
                static_cast<u8>(pulse * 0.85f),
                static_cast<u8>(pulse * 0.5f),
                fadeAlpha
            };

            static constexpr std::string_view titleStr = "FUEL FARM";
            Renderer::drawText(titleStr, ::biofuel::game::screens::MainMenuScreen::TITLE_X + titleSlideX, ::biofuel::game::screens::MainMenuScreen::TITLE_Y, ::biofuel::game::screens::MainMenuScreen::TITLE_FONT_SIZE, titleColor);
        }

        if (screen.m_introPhase >= ::biofuel::game::screens::IntroPhase::SubtitleFade) {
            const u8 alpha = static_cast<u8>(screen.m_subtitleFade.alpha() * 255.0f * titleFadeMul);
            const Color subColor = {
                ::biofuel::game::screens::MainMenuScreen::COLOR_GRAY_DIM.r,
                ::biofuel::game::screens::MainMenuScreen::COLOR_GRAY_DIM.g,
                ::biofuel::game::screens::MainMenuScreen::COLOR_GRAY_DIM.b,
                alpha
            };
            static constexpr std::string_view subtitleStr = "2D Pixel-Art Biofuel Management Sim";
            Renderer::drawText(
                subtitleStr,
                ::biofuel::game::screens::MainMenuScreen::TITLE_X + titleSlideX,
                ::biofuel::game::screens::MainMenuScreen::TITLE_Y + ::biofuel::game::screens::MainMenuScreen::TITLE_FONT_SIZE + ::biofuel::game::screens::MainMenuScreen::TITLE_SUBTITLE_GAP,
                ::biofuel::game::screens::MainMenuScreen::SUBTITLE_FONT_SIZE,
                subColor
            );
        }
    }
};

template<>
struct RenderElementExecutor<mainmenu::HintTextElement, ::biofuel::game::screens::MainMenuScreen> {
    static void render(::biofuel::game::screens::MainMenuScreen& screen, RenderContext&) {
        using namespace ::biofuel::engine::graphics;

        const f32 hintsDismiss = screen.m_dismiss.progress(::biofuel::game::screens::UIDismissState::ELEM_HINTS);
        if (screen.m_introPhase < ::biofuel::game::screens::IntroPhase::HintsFade || hintsDismiss >= 1.0f) {
            return;
        }

        const i32 hintsSlideX = -static_cast<i32>(static_cast<f32>(::biofuel::game::screens::MainMenuScreen::DISMISS_SLIDE_LEFT) * hintsDismiss);
        const f32 hintsFadeMul = 1.0f - hintsDismiss;
        const u8 alpha = static_cast<u8>(screen.m_hintsFade.alpha() * 255.0f * hintsFadeMul);
        const Color hintColor = {
            ::biofuel::game::screens::MainMenuScreen::COLOR_GRAY_DIM.r,
            ::biofuel::game::screens::MainMenuScreen::COLOR_GRAY_DIM.g,
            ::biofuel::game::screens::MainMenuScreen::COLOR_GRAY_DIM.b,
            alpha
        };
        static constexpr std::string_view hintsStr = "ESC Pause  |  LEFT / RIGHT Navigate  |  ENTER Select";
        const i32 subtitleY = ::biofuel::game::screens::MainMenuScreen::TITLE_Y + ::biofuel::game::screens::MainMenuScreen::TITLE_FONT_SIZE + ::biofuel::game::screens::MainMenuScreen::TITLE_SUBTITLE_GAP;
        const i32 hintsY = subtitleY + ::biofuel::game::screens::MainMenuScreen::SUBTITLE_FONT_SIZE + ::biofuel::game::screens::MainMenuScreen::SUBTITLE_HINTS_GAP;
        Renderer::drawText(hintsStr, ::biofuel::game::screens::MainMenuScreen::TITLE_X + hintsSlideX, hintsY, ::biofuel::game::screens::MainMenuScreen::HINTS_FONT_SIZE, hintColor);
    }
};

template<>
struct RenderElementExecutor<mainmenu::HorizontalMenuElement, ::biofuel::game::screens::MainMenuScreen> {
    static void render(::biofuel::game::screens::MainMenuScreen& screen, RenderContext& context) {
        const f32 menuDismiss = screen.m_dismiss.progress(::biofuel::game::screens::UIDismissState::ELEM_MENU);
        if (screen.m_introPhase < ::biofuel::game::screens::IntroPhase::MenuFade || menuDismiss >= 1.0f) {
            return;
        }

        const i32 menuSlideY = static_cast<i32>(static_cast<f32>(::biofuel::game::screens::MainMenuScreen::DISMISS_SLIDE_DOWN) * menuDismiss);
        const f32 menuFadeMul = 1.0f - menuDismiss;

        auto layout = ::biofuel::game::screens::MainMenuScreen::MENU_LAYOUT;
        const u8 menuAlpha = static_cast<u8>(screen.m_menuFade.alpha() * 255.0f * menuFadeMul);
        layout.colorSelected.a = menuAlpha;
        layout.colorSelectedGlow.a = menuAlpha;
        layout.colorSide.a = menuAlpha;
        layout.colorSideLocked.a = menuAlpha;
        layout.colorLockedLabel.a = menuAlpha;

        game::presentation::widgets::renderHorizontalCarousel(
            std::span{::biofuel::game::screens::MainMenuScreen::s_items},
            screen.m_selected,
            screen.m_hovered,
            context.screenWidth / 2,
            context.screenHeight - ::biofuel::game::screens::MainMenuScreen::MENU_BAR_Y_OFFSET + menuSlideY,
            layout,
            screen.m_menuSlide.motion(),
            screen.m_menuFxTime
        );
    }
};

template<>
struct RenderElementExecutor<mainmenu::FooterTextElement, ::biofuel::game::screens::MainMenuScreen> {
    static void render(::biofuel::game::screens::MainMenuScreen& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;

        const f32 footerDismiss = screen.m_dismiss.progress(::biofuel::game::screens::UIDismissState::ELEM_FOOTER);
        if (footerDismiss >= 1.0f) {
            return;
        }

        const i32 footerSlideY = static_cast<i32>(static_cast<f32>(::biofuel::game::screens::MainMenuScreen::DISMISS_SLIDE_DOWN) * footerDismiss);
        const f32 footerFadeMul = 1.0f - footerDismiss;
        static constexpr std::string_view versionStr = "v0.1.0 | Raylib 5.5 | C++20";
        const i32 versionX = context.screenWidth - ::biofuel::game::screens::MainMenuScreen::FOOTER_MARGIN_X;
        const i32 versionY = context.screenHeight - ::biofuel::game::screens::MainMenuScreen::FOOTER_BOTTOM_OFFSET + footerSlideY;
        const Color footerColor = {
            ::biofuel::game::screens::MainMenuScreen::COLOR_VERSION.r,
            ::biofuel::game::screens::MainMenuScreen::COLOR_VERSION.g,
            ::biofuel::game::screens::MainMenuScreen::COLOR_VERSION.b,
            static_cast<u8>(static_cast<f32>(::biofuel::game::screens::MainMenuScreen::COLOR_VERSION.a) * footerFadeMul)
        };
        Renderer::drawText(
            versionStr,
            versionX - Renderer::measureText(versionStr, ::biofuel::game::screens::MainMenuScreen::FOOTER_FONT_SIZE),
            versionY,
            ::biofuel::game::screens::MainMenuScreen::FOOTER_FONT_SIZE,
            footerColor
        );
    }
};

} // namespace biofuel::engine::ui::typed

namespace biofuel::game::screens {

void MainMenuScreen::onEnter() {
    m_selected = 0;
    m_hovered = -1;
    m_cooldown = 0.0f;
    m_titlePulse = 0.0f;
    m_menuFxTime = 0.0f;
    m_menuSlide = {};
    m_dismiss = {};
    m_dimensionShift = 0.0f;
    m_joinTransitionQueued = false;
    m_cameraComponent.reset();
    m_cameraPhase = CameraPhase::Idle;

    m_introPhase = IntroPhase::WaitingForTransition;
    m_titleFade.elapsed = 0.0f;
    m_subtitleFade.elapsed = 0.0f;
    m_hintsFade.elapsed = 0.0f;
    m_menuFade.elapsed = 0.0f;

    m_backdrop.configure(backdropConfig(COLOR_BG));
    m_backdrop.reset();

    // ---- Idle detection ----
    m_idleTrigger.reset();
    m_idleTrigger.setCallback([this] { startIdleTransition(); });
    m_idleTransitionDim = 0.0f;
    m_idleTransitionActive = false;
    m_revealBackdropOnResume = false;
    m_reportedStableMemory = false;
    m_handCalibrationReady = ::biofuel::engine::runtime::Runtime::handPose().calibrationValid();
    m_handOverlay.onEnter();
    CalibrationFlowState::instance().clear();

}

void MainMenuScreen::onExit() {
    m_handOverlay.onExit();
    ::biofuel::engine::runtime::Runtime::audio().stopMusic();
}

void MainMenuScreen::onUpdate(const f32 dt) {
    m_backdrop.update(dt);
    updateMenuSlide(dt);
    updateDismiss(dt);
    updateDimensionShift(dt);
    m_cameraComponent.update(dt);
    m_menuFxTime += dt;
    // Prevent precision loss after extended play — wrap at ~16 min
    if (m_menuFxTime > MENU_FX_WRAP_PERIOD) {
        m_menuFxTime = std::fmod(m_menuFxTime, MENU_FX_WRAP_PERIOD);
    }

    if (m_introPhase == IntroPhase::WaitingForTransition) {
        if (!isTransitioning() && backgroundRevealProgress() >= BG_TEXT_SYNC_THRESHOLD) {
            startIntro();
        }
        return;
    }

    if (m_introPhase != IntroPhase::Done) {
        advanceIntro(dt);
        if (m_introPhase == IntroPhase::Done && !m_reportedStableMemory) {
            m_reportedStableMemory = true;
            ::biofuel::engine::debug::MemoryTelemetry::snapshot("main_menu.stable");
        }
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

    ensureHandTrackingForModelOverlay();
    m_handOverlay.update(dt);

    // ---- Idle detection (only when screen is fully interactive) ----
    m_idleTrigger.update(dt, !isDismissing() && m_introPhase == IntroPhase::Done && !m_idleTransitionActive);

    // Idle → IdleScreen transition
    updateIdleTransition(dt);
    transitionToJoinIfReady();
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
    ::biofuel::engine::ui::typed::RenderContext context{
        .manager = manager(),
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = screenId(),
        .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
        .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
        .transitionAlpha = transitionAlpha(),
        .frameTime = GetFrameTime(),
    };
    ::biofuel::engine::ui::typed::RenderPipeline<MainMenuScreen>::render(*this, context);
    m_handOverlay.render();
}

void MainMenuScreen::onInput() {
    // Reset idle timer on any mouse movement or key press
    {
        const Vector2 mouseDelta = GetMouseDelta();
        const bool keyboardActivity =
            ::biofuel::engine::runtime::Runtime::services()
                .get<::biofuel::engine::runtime::typed::InputService>()
                .keyPressedThisPoll();
        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f || keyboardActivity) {
            m_idleTrigger.onInput();
        }
    }

    if (isDismissing()) {
        return;
    }

    if (m_introPhase != IntroPhase::Done) {
        return;
    }

    using namespace ::biofuel::engine::graphics;

    i32 navigatedSelection = m_selected;
    if (game::presentation::widgets::navigateHorizontalMenu(
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

    const auto hit = game::presentation::widgets::hitTestHorizontalCarousel(
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

    // Reset idle so starting a game doesn't trigger an idle transition
    m_idleTrigger.onInput();
}

void MainMenuScreen::startIdleDismiss() {
    if (m_dismiss.active) {
        return;
    }
    m_dismiss.active = true;
    m_dismiss.elapsed = 0.0f;
    // No hands — idle is a quiet text-only transition
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
        return;
    }
    if (m_idleTransitionActive) {
        return;  // idle transitions don't warp the shader
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

void MainMenuScreen::transitionToJoinIfReady() {
    if (m_joinTransitionQueued || m_idleTransitionActive || isTransitioning()) {
        return;
    }
    if (!m_dismiss.isDone() || m_dimensionShift < 1.0f) {
        return;
    }

    if (auto* sm = manager()) {
        m_joinTransitionQueued = true;
        sm->queueReplace<JoinScreen>();
    }
}

void MainMenuScreen::startCameraSequence() noexcept {
    // Phase 1: instantly snap to looking right (very brief, ~0.1s)
    m_cameraComponent.controller().reset();
    m_cameraComponent.controller().setTarget(
        ::biofuel::engine::graphics::component::ShaderCameraState{.yaw = CAMERA_YAW_RIGHT},
        0.1f,
        ::biofuel::engine::animation::Easing::easeOutCubic
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
            ::biofuel::engine::graphics::component::ShaderCameraState{.yaw = CAMERA_YAW_LEFT},
            CAMERA_SWEEP_DURATION,
            ::biofuel::engine::animation::Easing::easeInOutCubic
        );
        m_cameraPhase = CameraPhase::ReturnToCenter;
        break;

    case CameraPhase::ReturnToCenter:
        // Phase 3: smooth sweep from left → center
        m_cameraComponent.controller().setTarget(
            ::biofuel::engine::graphics::component::ShaderCameraState{.yaw = 0.0f},
            CAMERA_RETURN_DURATION,
            ::biofuel::engine::animation::Easing::easeInOutQuad
        );
        m_cameraPhase = CameraPhase::Done;
        break;

    case CameraPhase::Idle:
    case CameraPhase::Done:
        break;
    }
}

// ------------------------------------------------------------------------------
// Idle Transition
// ------------------------------------------------------------------------------

void MainMenuScreen::startIdleTransition() {
    if (m_idleTransitionActive) return;
    startIdleDismiss();
    m_idleTransitionActive = true;
}

void MainMenuScreen::updateIdleTransition(f32 dt) {
    if (!m_idleTransitionActive) return;

    // Fade shader to black during dismiss
    if (m_idleTransitionDim < 1.0f) {
        static constexpr f32 DIM_SPEED = 1.0f / 0.6f;
        m_idleTransitionDim = std::min(1.0f, m_idleTransitionDim + dt * DIM_SPEED);
    }

    // Once text has slid off and shader is fully dimmed, push IdleScreen
    if (m_dismiss.isDone() && m_idleTransitionDim >= 1.0f) {
        m_idleTransitionActive = false;
        m_revealBackdropOnResume = true;
        if (auto* sm = manager()) {
            sm->queuePush<IdleScreen>(IdleScreen::idleVideoPath());
        }
    }
}

// ------------------------------------------------------------------------------
// onResume — Called when IdleScreen is popped
// ------------------------------------------------------------------------------

void MainMenuScreen::onResume() {
    if (!m_revealBackdropOnResume) {
        m_idleTrigger.onInput();
        return;
    }

    m_revealBackdropOnResume = false;

    // Reset dismiss state so text slides back
    m_dismiss.active = false;
    m_dismiss.elapsed = 0.0f;
    m_idleTransitionDim = 0.0f;
    m_idleTransitionActive = false;

    // Undo any dimension shift / camera leak from the idle transition gap
    m_dimensionShift = 0.0f;
    m_cameraComponent.reset();
    m_cameraPhase = CameraPhase::Idle;

    // Fade shader back in with circular reveal
    m_backdrop.restartReveal();

    // Reset idle timer so it doesn't fire immediately on return
    m_idleTrigger.onInput();
}

void MainMenuScreen::onResume(::biofuel::engine::ui::typed::ResumeContext& context) {
    if (context.poppedScreenId == ::biofuel::engine::ui::typed::ScreenId::Calibration) {
        auto& flow = CalibrationFlowState::instance();
        const bool calibrationCompleted = flow.completed() && flow.route() == CalibrationRoute::Join;
        flow.clear();

        m_handCalibrationReady = calibrationCompleted;
        restoreMainMenuAfterCalibration();
        return;
    }

    if (context.poppedScreenId != ::biofuel::engine::ui::typed::ScreenId::Idle) {
        m_revealBackdropOnResume = false;
        m_idleTrigger.onInput();
        return;
    }

    m_revealBackdropOnResume = true;
    onResume();
}

void MainMenuScreen::restoreMainMenuAfterCalibration() noexcept {
    // Calibration is a pre-flight overlay. After it closes, return to the
    // animated menu and wait for the player to press New Game/Continue again.
    m_joinTransitionQueued = false;
    m_dismiss.active = false;
    m_dismiss.elapsed = 0.0f;
    m_dimensionShift = 0.0f;
    m_cameraComponent.reset();
    m_cameraPhase = CameraPhase::Idle;
    m_idleTransitionActive = false;
    m_idleTransitionDim = 0.0f;
    m_revealBackdropOnResume = false;
    m_introPhase = IntroPhase::Done;
    m_backdrop.restartReveal();
    m_handOverlay.onEnter();
    m_idleTrigger.onInput();
}

void MainMenuScreen::ensureHandTrackingForModelOverlay() {
    if (!m_handCalibrationReady) {
        return;
    }
    game::presentation::hands::ensureModelOnlyHandTracking();
}

// ------------------------------------------------------------------------------
// Backdrop Config Helper
// ------------------------------------------------------------------------------

game::presentation::effects::ScreenBackdropConfig MainMenuScreen::backdropConfig(Color fallback) const noexcept {
    return game::presentation::effects::ScreenBackdropConfig{
        .shaderName = ::biofuel::engine::graphics::shader::MainMenuBgModule::NAME,
        .fallbackColor = fallback,
        .revealDelay = BG_REVEAL_DELAY,
        .revealDuration = BG_REVEAL_DURATION,
        .brightnessFloor = 0.0f,
        .brightnessCeiling = 1.0f,
        .transitionWeight = 0.45f,
        .revealWeight = 0.55f,
    };
}

} // namespace biofuel::game::screens
