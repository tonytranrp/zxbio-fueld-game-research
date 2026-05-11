#pragma once

#include "UI/Screen.hpp"
#include "AnimationController/screen/ScreenBackdropController.hpp"
#include <string_view>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// IdleScreen — Ambient idle overlay pushed when the player is inactive.
//
// Renders a dimmed version of the main menu shader, plays atmospheric
// background music, and waits for any input to return to the game.
//
// On any key press or mouse movement, IdleScreen pops itself, revealing
// the underlying screen (which restarts its reveal animation via onResume).
// ------------------------------------------------------------------------------
class IdleScreen final : public Screen {
public:
    static constexpr std::string_view MUSIC_PATH =
        "assets/audio/Take me out Franz ferdinand (Loop best part Remix).mp3";

    static void preloadAssets();

    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] std::string_view getName() const noexcept override { return "IdleScreen"; }

private:
    static constexpr f32 INPUT_DELAY = 0.3f;
    static constexpr Color BG_COLOR = {12, 12, 20, 255};

    animation::screen::ScreenBackdropController m_backdrop;
    f32 m_inputDelay = 0.0f;
    bool m_inputReady = false;
};

} // namespace biofuel::ui::screens
