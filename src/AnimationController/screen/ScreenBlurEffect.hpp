#pragma once

#include "Core/Types.hpp"
#include <raylib.h>

namespace biofuel::ui {
    class Screen;
}

namespace biofuel::animation::screen {

// ------------------------------------------------------------------------------
// BlurConfig - Visual parameters for a screen blur fade effect
// ------------------------------------------------------------------------------
struct BlurConfig {
    Color tintColor = {.r = 15, .g = 15, .b = 25, .a = 0};
    u8 maxTintAlpha = 100;
    f32 fadeInDuration = 0.3f;
    f32 fadeOutDuration = 0.3f;
    f32 blurRadius = 2.0f;   // Shader blur radius in pixels
};

// ------------------------------------------------------------------------------
// ScreenBlurEffect - Self-contained background blur for popup/modal screens.
// Captures the screen behind, applies a two-pass Gaussian blur shader,
// and draws the result as a tinted backdrop.
// This utility is a "co-owner" of rendering — it manages its own visual state
// and provides a clean API for screens to drive blur animations.
// ------------------------------------------------------------------------------
class ScreenBlurEffect {
public:
    void init(i32 width, i32 height);
    void shutdown();

    void startBlurIn(const BlurConfig& config) noexcept;
    void startBlurOut(const BlurConfig& config) noexcept;
    void cancel() noexcept;

    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isBlurringIn() const noexcept;
    [[nodiscard]] bool isBlurringOut() const noexcept;
    [[nodiscard]] u8 currentTintAlpha() const noexcept;
    [[nodiscard]] f32 currentBlurRadius() const noexcept;

    void update(f32 dt);

    // Render the blurred background by capturing the previous screen's output.
    // 'prevScreen' should be the screen below the popup in the stack.
    void render(biofuel::ui::Screen* prevScreen) const;

private:
    void resetAnimation(f32 from, f32 to, f32 duration) noexcept;
    [[nodiscard]] f32 easeOutQuad(f32 t) const noexcept;

    // Recreate RenderTextures if screen size changed
    void ensureTextures(i32 width, i32 height);

    // Perform one blur pass: source texture → destination RenderTexture
    void blurPass(Shader shader, Texture2D source, RenderTexture2D dest, f32 radius) const;

    RenderTexture2D m_capture{};      // Captures the background screen
    RenderTexture2D m_pingPong{};     // Intermediate target for 2-pass blur

    BlurConfig m_config{};

    enum class State : u8 {
        Idle,
        BlurringIn,
        BlurringOut
    };

    State m_state = State::Idle;
    u8 m_tintAlpha = 0;
    f32 m_blurRadius = 0.0f;

    f32 m_elapsed = 0.0f;
    f32 m_duration = 0.0f;
    f32 m_from = 0.0f;
    f32 m_to = 0.0f;

    i32 m_cachedWidth = 0;
    i32 m_cachedHeight = 0;
};

} // namespace biofuel::animation::screen
