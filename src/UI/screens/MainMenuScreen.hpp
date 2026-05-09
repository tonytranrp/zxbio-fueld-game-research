#pragma once

#include "UI/Screen.hpp"
#include "Utils/ui/MenuHelper.hpp"
#include "AnimationController/animation/Easing.hpp"
#include "AnimationController/screen/ScreenBackdropController.hpp"
#include <raylib.h>
#include <array>
#include <string_view>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// MainMenuScreen - Title screen with shader backdrop and centered menu carousel.
// ------------------------------------------------------------------------------

// Intro animation phase — controls text fade-in sequence
enum class IntroPhase {
    WaitingForTransition,   // crossfade in progress, bg shader only
    TitleFade,              // title fading in
    SubtitleFade,           // subtitle fading in
    HintsFade,              // controls hint fading in
    MenuFade,               // menu bar fading in
    Done,                   // all elements visible, input active
};

// Fade-in state for a single text element
struct TextFade {
    f32 delay = 0.0f;       // seconds after previous element starts
    f32 duration = 0.5f;    // how long this fade takes
    f32 elapsed = 0.0f;     // time since this fade started

    [[nodiscard]] f32 alpha() const noexcept {
        if (elapsed <= 0.0f) return 0.0f;
        const f32 t = (elapsed >= duration) ? 1.0f : elapsed / duration;
        return animation::Easing::easeOutCubic(t);
    }
};

class MainMenuScreen final : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

private:
    struct MenuSlideState {
        i32 direction = 0;
        f32 elapsed = 0.0f;
        f32 duration = 0.22f;

        [[nodiscard]] bool active() const noexcept {
            return direction != 0 && elapsed < duration;
        }

        [[nodiscard]] f32 progress() const noexcept {
            if (direction == 0 || duration <= 0.0f) {
                return 1.0f;
            }
            return elapsed >= duration ? 1.0f : elapsed / duration;
        }

        [[nodiscard]] utils::ui::HorizontalMenuMotion motion() const noexcept {
            const f32 t = animation::Easing::easeOutCubic(progress());
            return utils::ui::HorizontalMenuMotion{
                .slotShift = static_cast<f32>(direction) * (1.0f - t)
            };
        }
    };

    // ---- Menu items ----
    static constexpr std::array<utils::ui::MenuItem, 3> s_items = {{
        {.label = "New Game", .locked = false},
        {.label = "Continue", .locked = false},
        {.label = "Quit",     .locked = false},
    }};

    // ---- Color palette ----
    static constexpr Color COLOR_BG              = {15, 15, 25, 255};   // Dark background fallback
    static constexpr Color COLOR_GOLD             = {200, 155, 60, 255}; // Gold accent (selected)
    static constexpr Color COLOR_GOLD_DIM         = {140, 110, 40, 255}; // Dimmer gold for arrows
    static constexpr Color COLOR_WARM_HI          = {255, 200, 80, 255}; // Warm highlight (pulse peak)
    static constexpr Color COLOR_GRAY_DIM         = {90, 90, 100, 255};  // Unselected items
    static constexpr Color COLOR_GRAY_LOCKED       = {55, 55, 65, 255};  // Locked item text
    static constexpr Color COLOR_GRAY_LOCKED_LABEL = {80, 80, 90, 255};  // Locked "(locked)" label
    static constexpr Color COLOR_VERSION           = {60, 60, 70, 255};  // Version/footer text

    // ---- Title area layout (top-left) ----
    static constexpr i32 TITLE_X                = 40;
    static constexpr i32 TITLE_Y                = 30;
    static constexpr i32 TITLE_FONT_SIZE        = 40;
    static constexpr i32 SUBTITLE_FONT_SIZE     = 16;
    static constexpr i32 HINTS_FONT_SIZE        = 13;
    static constexpr i32 TITLE_SUBTITLE_GAP     = 10;  // Pixels between title baseline and subtitle
    static constexpr i32 SUBTITLE_HINTS_GAP     = 8;   // Pixels between subtitle baseline and hints
    static constexpr f32 TITLE_PULSE_SPEED      = 1.8f;
    static constexpr f32 TITLE_PULSE_MIN        = 225.0f;
    static constexpr f32 TITLE_PULSE_RANGE      = 30.0f;
    static constexpr f32 BG_REVEAL_DELAY        = 0.08f;
    static constexpr f32 BG_REVEAL_DURATION     = 1.35f;
    static constexpr f32 BG_TEXT_SYNC_THRESHOLD = 0.35f;
    static constexpr f32 KEY_REPEAT_DELAY       = 0.12f;

    // ---- Menu bar layout (bottom-middle) ----
    static constexpr i32 MENU_BAR_Y_OFFSET      = 108;
    static constexpr utils::ui::HorizontalMenuLayout MENU_LAYOUT = {
        .sideOffsetX = 205,
        .sideOffsetY = 16,
        .centerFontSize = 29,
        .sideFontSize = 22,
        .lockedLabelFontSize = 13,
        .hitboxPaddingX = 18,
        .hitboxPaddingY = 10,
        .underlineWidth = 82,
        .underlineHeight = 3,
        .underlineOffsetY = 10,
        .accentGap = 22,
        .accentWidth = 18,
        .accentHeight = 2,
        .colorSelected = COLOR_GOLD,
        .colorSelectedGlow = COLOR_WARM_HI,
        .colorSide = {134, 138, 154, 236},
        .colorSideLocked = {82, 86, 98, 224},
        .colorLockedLabel = COLOR_GRAY_LOCKED_LABEL,
        .keyRepeatDelay = KEY_REPEAT_DELAY,
    };

    // ---- Footer layout ----
    static constexpr i32 FOOTER_FONT_SIZE        = 12;
    static constexpr i32 FOOTER_MARGIN_X        = 10;
    static constexpr i32 FOOTER_BOTTOM_OFFSET   = 25;
    // ---- State ----
    i32 m_selected  = 0;
    i32 m_hovered = -1;
    f32 m_cooldown  = 0.0f;
    f32 m_titlePulse = 0.0f;
    f32 m_menuFxTime = 0.0f;
    MenuSlideState m_menuSlide{};

    // ---- Intro fade-in animation ----
    IntroPhase m_introPhase = IntroPhase::WaitingForTransition;
    TextFade m_titleFade{0.0f, 0.50f};
    TextFade m_subtitleFade{0.2f, 0.40f};
    TextFade m_hintsFade{0.2f, 0.35f};
    TextFade m_menuFade{0.2f, 0.45f};

    animation::screen::ScreenBackdropController m_backdrop;

    [[nodiscard]] f32 backgroundRevealProgress() const noexcept;

    // ---- Intro animation helpers ----
    void startIntro();
    void advanceIntro(f32 dt);
    void updateMenuSlide(f32 dt) noexcept;
    void selectMenuIndex(i32 newIndex) noexcept;
    [[nodiscard]] i32 inferMenuDirection(i32 oldIndex, i32 newIndex) const noexcept;

    // ---- Methods ----
    void activateSelected();
    [[nodiscard]] bool isLocked(i32 index) const;
};

} // namespace biofuel::ui::screens
