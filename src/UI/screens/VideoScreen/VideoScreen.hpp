#pragma once

#include "UI/Screen.hpp"
#include "Core/Types.hpp"
#include <raylib.h>
#include <string>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// VideoScreen — Fullscreen video playback overlay.
//
// Follows the IdleScreen pattern: onEnter starts video, onRender draws the
// frame texture, any input pops the screen. Supports looping and skip-on-input.
//
// The caller must call VideoManager::loadVideo() before pushing this screen,
// or call VideoScreen::preloadVideo() for convenience.
// ------------------------------------------------------------------------------
class VideoScreen final : public Screen {
public:
    explicit VideoScreen(std::string_view videoName);

    static void preloadVideo(std::string_view name, std::string_view path);

    void onEnter() override;
    void onExit() override;
    void onUpdate(f32 dt) override;
    void onRender() override;
    void onInput() override;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "VideoScreen";
    }

    void setLooping(bool loop) noexcept { m_looping = loop; }
    void setSkipOnAnyInput(bool skip) noexcept { m_skipOnAnyInput = skip; }
    void setInputDelay(f32 delay) noexcept { m_inputDelayDuration = delay; }

private:
    std::string m_videoName;
    bool m_looping = true;
    bool m_skipOnAnyInput = true;
    f32 m_inputDelayDuration = 0.3f;
    f32 m_inputDelay = 0.0f;
    bool m_inputReady = false;
    bool m_started = false;

    static constexpr Color FALLBACK_COLOR = Color{0, 0, 0, 255};
};

} // namespace biofuel::ui::screens
