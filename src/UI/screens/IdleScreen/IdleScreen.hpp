#pragma once

#include "UI/Screen.hpp"
#include "AnimationController/screen/ScreenBackdropController.hpp"
#include <string>
#include <string_view>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// IdleScreen - Ambient idle overlay pushed when the player is inactive.
//
// Preferred mode plays a local MP4 through VideoManager. If the video is missing
// or cannot decode, the screen falls back to the existing shader + music path.
// ------------------------------------------------------------------------------
class IdleScreen final : public Screen {
public:
    static constexpr std::string_view MUSIC_PATH =
        "assets/audio/Take me out Franz ferdinand (Loop best part Remix).mp3";
    static constexpr std::string_view VIDEO_PATH =
        "assets/video/ssstik.io_1778485755339.mp4";

    [[nodiscard]] static std::string idleVideoPath();
    static void preloadAssets();

    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] std::string_view getName() const noexcept override { return "IdleScreen"; }

    void setIdleVideo(std::string_view videoName) { m_idleVideoName = videoName; }

private:
    static constexpr f32 INPUT_DELAY = 0.3f;
    static constexpr Color BG_COLOR = {12, 12, 20, 255};

    void startFallbackBackdrop();

    animation::screen::ScreenBackdropController m_backdrop;
    std::string m_idleVideoName;
    f32 m_inputDelay = 0.0f;
    bool m_inputReady = false;
    bool m_videoMode = false;
};

} // namespace biofuel::ui::screens
