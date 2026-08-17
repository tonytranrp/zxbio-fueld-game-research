#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "engine/ui/typed/ScreenLifecycle.hpp"
#include "MainMenuTypes.hpp"
#include "game/presentation/idle/IdleTrigger.hpp"
#include "game/presentation/effects/ScreenBackdropController.hpp"
#include "engine/graphics/components/Camera/CameraComponent.hpp"
#include <raylib.h>
#include <array>
#include <string_view>

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// MainMenuScreen - Title screen with shader backdrop and centered menu carousel.
// ------------------------------------------------------------------------------
class MainMenuScreen final : public ::biofuel::engine::ui::Screen {
    template<typename, typename>
    friend struct ::biofuel::engine::ui::typed::RenderElementExecutor;

public:
    void onEnter() override;
    void onExit() override;
    void onResume() override;
    void onResume(::biofuel::engine::ui::typed::ResumeContext& context);
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::game::screens::screen_id::MainMenu; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "MainMenuScreen"; }

private:
    // ---- Menu items ----
    static constexpr std::array<game::presentation::widgets::MenuItem, 3> s_items = {{
        {.label = "New Game", .locked = false},
        {.label = "Continue", .locked = false},
        {.label = "Quit",     .locked = false},
    }};

    // ---- Color palette (RGBA, 0..255) ----
    static constexpr Color COLOR_BG                = {15, 15, 25, 255};    // backdrop fallback fill
    static constexpr Color COLOR_GOLD              = {200, 155, 60, 255};  // selected menu item
    static constexpr Color COLOR_GOLD_DIM          = {140, 110, 40, 255};  // dimmed gold accent
    static constexpr Color COLOR_WARM_HI           = {255, 200, 80, 255};  // selected item glow highlight
    static constexpr Color COLOR_GRAY_DIM          = {90, 90, 100, 255};   // subtitle / hints text
    static constexpr Color COLOR_GRAY_LOCKED       = {55, 55, 65, 255};    // locked item base
    static constexpr Color COLOR_GRAY_LOCKED_LABEL = {80, 80, 90, 255};    // locked item "(locked)" label
    static constexpr Color COLOR_VERSION           = {60, 60, 70, 255};    // footer version string

    // ---- Title area layout (top-left) ----
    static constexpr i32 TITLE_X                = 40;      // px from left edge
    static constexpr i32 TITLE_Y                = 30;      // px from top edge
    static constexpr i32 TITLE_FONT_SIZE        = 40;      // px
    static constexpr i32 SUBTITLE_FONT_SIZE     = 16;      // px
    static constexpr i32 HINTS_FONT_SIZE        = 13;      // px
    static constexpr i32 TITLE_SUBTITLE_GAP     = 10;      // px between title baseline and subtitle
    static constexpr i32 SUBTITLE_HINTS_GAP     = 8;       // px between subtitle and hints line
    static constexpr f32 TITLE_PULSE_SPEED      = 1.8f;    // radians/sec for the title glow sine
    static constexpr f32 TITLE_PULSE_MIN        = 225.0f;  // min channel value of the pulse (0..255)
    static constexpr f32 TITLE_PULSE_RANGE      = 30.0f;   // peak-to-trough channel swing added to the min
    static constexpr f32 BG_REVEAL_DELAY        = 0.08f;   // sec before the backdrop reveal starts
    static constexpr f32 BG_REVEAL_DURATION     = 1.35f;   // sec for the backdrop reveal to finish
    static constexpr f32 BG_TEXT_SYNC_THRESHOLD = 0.35f;   // reveal progress (0..1) at which intro text begins
    static constexpr f32 KEY_REPEAT_DELAY       = 0.12f;   // sec between held-key menu navigation steps

    // ---- Menu bar layout (bottom-middle) ----
    static constexpr i32 MENU_BAR_Y_OFFSET = 108;  // px above the screen bottom (baseline at 1280x720)
    static constexpr game::presentation::widgets::HorizontalMenuLayout MENU_LAYOUT = {
        .sideOffsetX        = 205,
        .sideOffsetY        = 16,
        .centerFontSize     = 29,
        .sideFontSize       = 22,
        .lockedLabelFontSize = 13,
        .hitboxPaddingX     = 18,
        .hitboxPaddingY     = 10,
        .underlineWidth     = 82,
        .underlineHeight    = 3,
        .underlineOffsetY   = 10,
        .accentGap          = 22,
        .accentWidth        = 18,
        .accentHeight       = 2,
        .colorSelected      = COLOR_GOLD,
        .colorSelectedGlow  = COLOR_WARM_HI,
        .colorSide          = {134, 138, 154, 236},
        .colorSideLocked    = {82, 86, 98, 224},
        .colorLockedLabel   = COLOR_GRAY_LOCKED_LABEL,
        .keyRepeatDelay     = KEY_REPEAT_DELAY,
    };

    // ---- Footer layout (bottom-right version string) ----
    static constexpr i32 FOOTER_FONT_SIZE      = 12;  // px
    static constexpr i32 FOOTER_MARGIN_X       = 10;  // px inset from the right edge
    static constexpr i32 FOOTER_BOTTOM_OFFSET  = 25;  // px above the screen bottom

    // ---- Dismiss slide distances (pixels off-screen) ----
    static constexpr i32 DISMISS_SLIDE_LEFT = 500;  // px the title/hints slide left when dismissed
    static constexpr i32 DISMISS_SLIDE_DOWN = 200;  // px the menu/footer slide down when dismissed

    // ---- Dimension shift (shader warp after dismiss) ----
    static constexpr f32 DIMENSION_SHIFT_DURATION = 3.0f;     // sec to ramp the shader warp 0 → 1
    static constexpr f32 MENU_FX_WRAP_PERIOD = 1000.0f;       // sec; wrap the FX clock to avoid float precision loss

    // ---- Camera look sequence (yaw-only, no position movement) ----
    // Phase 1: instantly look right
    // Phase 2: smooth sweep from right → left
    // Phase 3: smooth sweep from left → center
    static constexpr f32 CAMERA_YAW_RIGHT     =  0.30f;   // start looking right (~17°)
    static constexpr f32 CAMERA_YAW_LEFT      = -0.30f;   // sweep to left (~-17°)
    static constexpr f32 CAMERA_SWEEP_DURATION = 2.0f;     // right→left duration
    static constexpr f32 CAMERA_RETURN_DURATION = 1.5f;    // left→center duration

    // ---- Backdrop config helper ----
    [[nodiscard]] game::presentation::effects::ScreenBackdropConfig backdropConfig(Color fallback) const noexcept;

    // Camera animation phase tracker
    enum class CameraPhase { Idle, SweepToLeft, ReturnToCenter, Done };

    // ---- State ----
    i32 m_selected    = 0;
    i32 m_hovered     = -1;
    f32 m_cooldown    = 0.0f;
    f32 m_titlePulse  = 0.0f;
    f32 m_menuFxTime  = 0.0f;
    MenuSlideState m_menuSlide{};

    // ---- Dismiss animation ----
    UIDismissState m_dismiss{};

    // ---- Dimension shift (driven after dismiss completes) ----
    f32 m_dimensionShift = 0.0f;
    bool m_joinTransitionQueued = false;

    // ---- Camera component (managed via Component system) ----
    ::biofuel::engine::graphics::component::CameraComponent m_cameraComponent;
    CameraPhase m_cameraPhase = CameraPhase::Idle;

    // ---- Intro fade-in animation ----
    IntroPhase m_introPhase = IntroPhase::WaitingForTransition;
    TextFade m_titleFade{0.0f, 0.50f};
    TextFade m_subtitleFade{0.2f, 0.40f};
    TextFade m_hintsFade{0.2f, 0.35f};
    TextFade m_menuFade{0.2f, 0.45f};

    game::presentation::effects::ScreenBackdropController m_backdrop;

    // Idle detection → pushes IdleScreen
    game::presentation::idle::IdleTrigger m_idleTrigger{5.0f};

    // Idle transition state
    f32 m_idleTransitionDim = 0.0f;
    bool m_idleTransitionActive = false;
    bool m_revealBackdropOnResume = false;
    bool m_reportedStableMemory = false;

    void startIdleTransition();
    void updateIdleTransition(f32 dt);

    [[nodiscard]] f32 backgroundRevealProgress() const noexcept;

    // ---- Intro animation helpers ----
    void startIntro();
    void advanceIntro(f32 dt);
    void updateMenuSlide(f32 dt) noexcept;
    void selectMenuIndex(i32 newIndex) noexcept;
    [[nodiscard]] i32 inferMenuDirection(i32 oldIndex, i32 newIndex) const noexcept;

    // ---- Dismiss animation ----
    void startDismiss();         // game transition (New Game/Continue)
    void startIdleDismiss();     // idle transition — text only
    void beginDismiss(bool resetIdleTrigger); // shared body of the two above
    void updateDismiss(f32 dt) noexcept;
    [[nodiscard]] bool isDismissing() const noexcept;

    // ---- Dimension shift ----
    void updateDimensionShift(f32 dt) noexcept;
    void startCameraSequence() noexcept;
    void advanceCameraSequence() noexcept;
    void transitionToJoinIfReady();

    // ---- Methods ----
    void activateSelected();
};

} // namespace biofuel::game::screens
