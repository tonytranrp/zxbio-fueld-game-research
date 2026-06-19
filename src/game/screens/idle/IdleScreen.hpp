#pragma once

#include "game/screens/GameScreenIds.hpp"
#include "engine/ui/Screen.hpp"
#include "game/presentation/effects/ScreenBackdropController.hpp"
#include <string>
#include <string_view>

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// IdleScreen - Ambient idle overlay pushed when the player is inactive.
//
// Preferred mode plays a local MP4 through VideoManager. If the video is missing
// or cannot decode, the screen falls back to the existing shader + music path.
// ------------------------------------------------------------------------------
class IdleScreen final : public ::biofuel::engine::ui::Screen {
    template<typename, typename>
    friend struct ::biofuel::engine::ui::typed::RenderElementExecutor;

public:
    static constexpr std::string_view MUSIC_PATH =
        "assets/audio/Take me out Franz ferdinand (Loop best part Remix).mp3";
    static constexpr std::string_view VIDEO_PATH =
        "assets/video/ssstik.io_1778485755339.mp4";

    [[nodiscard]] static std::string idleVideoPath();

    explicit IdleScreen(std::string_view videoName = {}) : m_idleVideoName(videoName) {}

    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] ::biofuel::engine::ui::typed::ScreenId screenId() const noexcept override { return ::biofuel::game::screens::screen_id::Idle; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "IdleScreen"; }

    void setIdleVideo(std::string_view videoName) { m_idleVideoName = videoName; }
    [[nodiscard]] bool videoMode() const noexcept { return m_videoMode; }
    [[nodiscard]] std::string_view idleVideoName() const noexcept { return m_idleVideoName; }
    [[nodiscard]] bool fallbackBackdropReady() const noexcept { return m_backdrop.shader().id != 0; }
    [[nodiscard]] static constexpr Color fallbackColor() noexcept { return BG_COLOR; }
    void renderFallbackBackdrop() const { m_backdrop.render(1.0f); }

private:
    static constexpr f32 INPUT_DELAY = 0.3f;
    static constexpr Color BG_COLOR = {12, 12, 20, 255};

    void startFallbackBackdrop();

    game::presentation::effects::ScreenBackdropController m_backdrop;
    std::string m_idleVideoName;
    f32 m_inputDelay = 0.0f;
    bool m_inputReady = false;
    bool m_videoMode = false;
};

} // namespace biofuel::game::screens
