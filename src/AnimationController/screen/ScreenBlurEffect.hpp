#pragma once

#include "Core/Types.hpp"
#include "Utils/render/RenderSurface.hpp"
#include <raylib.h>

namespace biofuel::ui {
    class Screen;
}

namespace biofuel::animation::screen {

struct BlurConfig {
    Color tintColor = {.r = 15, .g = 15, .b = 25, .a = 0};
    u8 maxTintAlpha = 100;
    f32 fadeInDuration = 0.3f;
    f32 fadeOutDuration = 0.3f;
    f32 blurRadius = 2.0f;
    f32 captureScale = 0.5f;
    f32 desaturation = 0.15f;
    f32 vignetteStrength = 0.18f;
    f32 dimStrength = 0.12f;
    i32 blurPassCount = 2;
};

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
    void render(biofuel::ui::Screen* prevScreen);

private:
    enum class State : u8 {
        Idle,
        BlurringIn,
        BlurringOut
    };

    void resetAnimation(f32 from, f32 to, f32 duration) noexcept;
    [[nodiscard]] f32 easeOutQuad(f32 t) const noexcept;
    void ensureTextures(i32 width, i32 height);
    void blurPass(Shader shader, Texture2D source, RenderTexture2D dest, f32 radius, bool horizontal);

    utils::render::RenderSurface m_captureSurface;
    utils::render::RenderSurface m_blurSurfaceA;
    utils::render::RenderSurface m_blurSurfaceB;

    BlurConfig m_config{};
    State m_state = State::Idle;
    u8 m_tintAlpha = 0;
    f32 m_blurRadius = 0.0f;
    f32 m_elapsed = 0.0f;
    f32 m_duration = 0.0f;
    f32 m_from = 0.0f;
    f32 m_to = 0.0f;

    i32 m_cachedWidth = 0;
    i32 m_cachedHeight = 0;
    f32 m_cachedCaptureScale = 0.0f;

    i32 m_cachedTexelLocH = -1;
    i32 m_cachedRadiusLocH = -1;
    i32 m_cachedTexelLocV = -1;
    i32 m_cachedRadiusLocV = -1;
    i32 m_cachedDesaturationLoc = -1;
    i32 m_cachedVignetteLoc = -1;
    i32 m_cachedDimLoc = -1;
};

} // namespace biofuel::animation::screen
