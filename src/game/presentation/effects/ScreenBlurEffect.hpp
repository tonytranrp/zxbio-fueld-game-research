#pragma once

#include "engine/core/Types.hpp"
#include "engine/graphics/RenderSurface.hpp"
#include <raylib.h>
#include <memory>
#include <string_view>

namespace biofuel::engine::ui {
    class Screen;
}

namespace biofuel::game::presentation::effects {

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
    using CaptureCallback = void(*)(void* userData, ::biofuel::engine::graphics::RenderSurface& target);

    void init(i32 width, i32 height);
    void init(i32 width, i32 height, const BlurConfig& config);
    void shutdown();

    void startBlurIn(const BlurConfig& config) noexcept;
    void startBlurOut(const BlurConfig& config) noexcept;
    void cancel() noexcept;

    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isBlurringIn() const noexcept;
    [[nodiscard]] bool isBlurringOut() const noexcept;
    [[nodiscard]] u8 currentTintAlpha() const noexcept;
    [[nodiscard]] f32 currentBlurRadius() const noexcept;

    void warmCache(CaptureCallback capturePrevious, void* userData);
    void update(f32 dt);
    void render(CaptureCallback capturePrevious, void* userData);

private:
    enum class State : u8 {
        Idle,
        BlurringIn,
        BlurringOut
    };

    void resetAnimation(f32 from, f32 to, f32 duration) noexcept;
    [[nodiscard]] f32 smoothStep(f32 t) const noexcept;
    void ensureTextures(i32 width, i32 height);
    void invalidateCache() noexcept;
    void rebuildBlurCache(CaptureCallback capturePrevious, void* userData);
    void blurPass(Shader shader, Texture2D source, RenderTexture2D dest, f32 radius, bool horizontal);
    [[nodiscard]] ::biofuel::engine::graphics::RenderSurface& captureSurface() noexcept { return *m_captureSurface; }
    [[nodiscard]] ::biofuel::engine::graphics::RenderSurface& blurSurfaceA() noexcept { return *m_blurSurfaceA; }
    [[nodiscard]] ::biofuel::engine::graphics::RenderSurface& blurSurfaceB() noexcept { return *m_blurSurfaceB; }

    inline static constexpr f32 CACHE_TTL_SECONDS = 2.0f;
    inline static constexpr std::string_view CAPTURE_CACHE_KEY = "pause.blur.capture";
    inline static constexpr std::string_view BLUR_A_CACHE_KEY = "pause.blur.a";
    inline static constexpr std::string_view BLUR_B_CACHE_KEY = "pause.blur.b";

    std::shared_ptr<::biofuel::engine::graphics::RenderSurface> m_captureSurface;
    std::shared_ptr<::biofuel::engine::graphics::RenderSurface> m_blurSurfaceA;
    std::shared_ptr<::biofuel::engine::graphics::RenderSurface> m_blurSurfaceB;

    BlurConfig m_config{};
    State m_state = State::Idle;
    u8 m_tintAlpha = 0;
    f32 m_blurRadius = 0.0f;
    f32 m_elapsed = 0.0f;
    f32 m_duration = 0.0f;
    f32 m_from = 0.0f;
    f32 m_to = 0.0f;
    f32 m_fromBlurRadius = 0.0f;
    f32 m_toBlurRadius = 0.0f;

    i32 m_cachedWidth = 0;
    i32 m_cachedHeight = 0;
    f32 m_cachedCaptureScale = 0.0f;
    bool m_blurCacheValid = false;

    i32 m_cachedTexelLocH = -1;
    i32 m_cachedRadiusLocH = -1;
    i32 m_cachedTexelLocV = -1;
    i32 m_cachedRadiusLocV = -1;
    i32 m_cachedDesaturationLoc = -1;
    i32 m_cachedVignetteLoc = -1;
    i32 m_cachedDimLoc = -1;
};

} // namespace biofuel::game::presentation::effects
