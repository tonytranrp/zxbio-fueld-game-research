#include "ScreenBlurEffect.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/TransientResourceCache.hpp"
#include "engine/graphics/shaders/BlurCompositeModule.hpp"
#include "engine/graphics/shaders/BlurHModule.hpp"
#include "engine/graphics/shaders/BlurVModule.hpp"
#include "engine/graphics/shaders/TypedShaderModule.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace biofuel::game::presentation::effects {

namespace {

using BlurHShader = ::biofuel::engine::runtime::typed::shader::BlurH;
using BlurVShader = ::biofuel::engine::runtime::typed::shader::BlurV;
using BlurCompositeShader = ::biofuel::engine::runtime::typed::shader::BlurComposite;
namespace BlurHUniforms = ::biofuel::engine::runtime::typed::shader::blur_h;
namespace BlurVUniforms = ::biofuel::engine::runtime::typed::shader::blur_v;
namespace BlurCompositeUniforms = ::biofuel::engine::runtime::typed::shader::blur_composite;

} // namespace

void ScreenBlurEffect::init(const i32 width, const i32 height) {
    ensureTextures(width, height);
}

void ScreenBlurEffect::init(const i32 width, const i32 height, const BlurConfig& config) {
    m_config = config;
    ensureTextures(width, height);
}

void ScreenBlurEffect::shutdown() {
    auto& cache = ::biofuel::engine::graphics::TransientResourceCache::instance();
    cache.releaseSurface(CAPTURE_CACHE_KEY, CACHE_TTL_SECONDS);
    cache.releaseSurface(BLUR_A_CACHE_KEY, CACHE_TTL_SECONDS);
    cache.releaseSurface(BLUR_B_CACHE_KEY, CACHE_TTL_SECONDS);
    m_captureSurface = nullptr;
    m_blurSurfaceA = nullptr;
    m_blurSurfaceB = nullptr;
    invalidateCache();
    m_cachedWidth = 0;
    m_cachedHeight = 0;
    m_cachedCaptureScale = 0.0f;
    m_cachedTexelLocH = -1;
    m_cachedRadiusLocH = -1;
    m_cachedTexelLocV = -1;
    m_cachedRadiusLocV = -1;
    m_cachedDesaturationLoc = -1;
    m_cachedVignetteLoc = -1;
    m_cachedDimLoc = -1;
}

void ScreenBlurEffect::startBlurIn(const BlurConfig& config) noexcept {
    m_config = config;
    invalidateCache();
    resetAnimation(0.0f, static_cast<f32>(config.maxTintAlpha), config.fadeInDuration);
    m_state = State::BlurringIn;
    m_blurRadius = 0.0f;
    m_fromBlurRadius = 0.0f;
    m_toBlurRadius = config.blurRadius;
}

void ScreenBlurEffect::startBlurOut(const BlurConfig& config) noexcept {
    m_config = config;
    resetAnimation(static_cast<f32>(m_tintAlpha), 0.0f, config.fadeOutDuration);
    m_state = State::BlurringOut;
    m_fromBlurRadius = m_blurRadius;
    m_toBlurRadius = 0.0f;
}

void ScreenBlurEffect::cancel() noexcept {
    m_state = State::Idle;
    m_tintAlpha = 0;
    m_blurRadius = 0.0f;
    m_fromBlurRadius = 0.0f;
    m_toBlurRadius = 0.0f;
    m_elapsed = 0.0f;
    invalidateCache();
    // Reset cached uniform locations so they're re-resolved next use (B053)
    m_cachedTexelLocH = -1;
    m_cachedRadiusLocH = -1;
    m_cachedTexelLocV = -1;
    m_cachedRadiusLocV = -1;
    m_cachedDesaturationLoc = -1;
    m_cachedVignetteLoc = -1;
    m_cachedDimLoc = -1;
}

bool ScreenBlurEffect::isActive() const noexcept {
    return m_state != State::Idle;
}

bool ScreenBlurEffect::isBlurringIn() const noexcept {
    return m_state == State::BlurringIn;
}

bool ScreenBlurEffect::isBlurringOut() const noexcept {
    return m_state == State::BlurringOut;
}

u8 ScreenBlurEffect::currentTintAlpha() const noexcept {
    return m_tintAlpha;
}

f32 ScreenBlurEffect::currentBlurRadius() const noexcept {
    return m_blurRadius;
}

void ScreenBlurEffect::warmCache(CaptureCallback capturePrevious, void* userData) {
    if (capturePrevious == nullptr) {
        return;
    }

    const i32 sw = ::biofuel::engine::graphics::Renderer::screenWidth();
    const i32 sh = ::biofuel::engine::graphics::Renderer::screenHeight();
    ensureTextures(sw, sh);

    if (!m_blurCacheValid) {
        rebuildBlurCache(capturePrevious, userData);
    }
}

void ScreenBlurEffect::update(const f32 dt) {
    const i32 sw = ::biofuel::engine::graphics::Renderer::screenWidth();
    const i32 sh = ::biofuel::engine::graphics::Renderer::screenHeight();
    ensureTextures(sw, sh);

    if (m_state == State::Idle) {
        return;
    }

    m_elapsed += dt;
    if (m_elapsed >= m_duration) {
        m_elapsed = m_duration;
        m_tintAlpha = static_cast<u8>(m_to);
        m_blurRadius = (m_state == State::BlurringIn) ? m_config.blurRadius : 0.0f;
        m_state = State::Idle;
        return;
    }

    const f32 t = (m_duration > 0.0f) ? (m_elapsed / m_duration) : 1.0f;
    const f32 eased = smoothStep(t);
    const f32 tintValue = std::clamp(m_from + (m_to - m_from) * eased, 0.0f, 255.0f);
    m_tintAlpha = static_cast<u8>(tintValue);
    m_blurRadius = std::max(0.0f, m_fromBlurRadius + (m_toBlurRadius - m_fromBlurRadius) * eased);
}

void ScreenBlurEffect::render(CaptureCallback capturePrevious, void* userData) {
    using namespace ::biofuel::engine::graphics;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();
    ensureTextures(sw, sh);

    if (capturePrevious == nullptr) {
        if (m_tintAlpha > 0) {
            Renderer::drawRect(0, 0, sw, sh, Color{
                m_config.tintColor.r,
                m_config.tintColor.g,
                m_config.tintColor.b,
                m_tintAlpha
            });
        }
        return;
    }

    if (!m_blurCacheValid) {
        rebuildBlurCache(capturePrevious, userData);
    }

    if (!m_blurCacheValid) {
        if (m_tintAlpha > 0) {
            Renderer::drawRect(0, 0, sw, sh, Color{
                m_config.tintColor.r,
                m_config.tintColor.g,
                m_config.tintColor.b,
                m_tintAlpha
            });
        }
        return;
    }

    const f32 blurRatio = (m_config.blurRadius > 0.0f)
        ? std::clamp(m_blurRadius / m_config.blurRadius, 0.0f, 1.0f)
        : 0.0f;
    const u8 blurAlpha = static_cast<u8>(std::clamp(blurRatio * 255.0f, 0.0f, 255.0f));
    if (blurAlpha > 0) {
        Renderer::drawRenderTexture(blurSurfaceA().texture(), 0, 0, sw, sh, Color{255, 255, 255, blurAlpha});
    }

    if (m_tintAlpha > 0) {
        Renderer::drawRect(0, 0, sw, sh, Color{
            m_config.tintColor.r,
            m_config.tintColor.g,
            m_config.tintColor.b,
            m_tintAlpha
        });
    }
}

void ScreenBlurEffect::rebuildBlurCache(CaptureCallback capturePrevious, void* userData) {
    using namespace ::biofuel::engine::graphics;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    capturePrevious(userData, captureSurface());

    auto& shaderManager = ::biofuel::engine::runtime::Runtime::shader();
    const bool canBlur = ::biofuel::engine::runtime::typed::Shaders::loaded<::biofuel::engine::runtime::typed::shader::BlurH>(shaderManager) &&
        ::biofuel::engine::runtime::typed::Shaders::loaded<::biofuel::engine::runtime::typed::shader::BlurV>(shaderManager) &&
        blurSurfaceA().valid() &&
        blurSurfaceB().valid();

    if (canBlur) {
        {
            ScopedTextureMode textureScope(blurSurfaceA().target());
            ClearBackground(BLANK);
            Renderer::drawRenderTexture(
                captureSurface().texture(),
                0,
                0,
                blurSurfaceA().width(),
                blurSurfaceA().height(),
                WHITE
            );
        }

        Shader blurH = ::biofuel::engine::runtime::typed::Shaders::get<::biofuel::engine::runtime::typed::shader::BlurH>(shaderManager);
        Shader blurV = ::biofuel::engine::runtime::typed::Shaders::get<::biofuel::engine::runtime::typed::shader::BlurV>(shaderManager);
        Texture2D source = blurSurfaceA().texture();
        const i32 blurPasses = std::clamp(m_config.blurPassCount, 1, 4);
        const f32 cacheRadius = m_config.blurRadius;

        for (i32 i = 0; i < blurPasses; ++i) {
            blurPass(blurH, source, blurSurfaceB().target(), cacheRadius, true);
            blurPass(blurV, blurSurfaceB().texture(), blurSurfaceA().target(), cacheRadius, false);
            source = blurSurfaceA().texture();
        }

        if (::biofuel::engine::runtime::typed::Shaders::loaded<::biofuel::engine::runtime::typed::shader::BlurComposite>(shaderManager)) {
            Shader composite = ::biofuel::engine::runtime::typed::Shaders::get<::biofuel::engine::runtime::typed::shader::BlurComposite>(shaderManager);
            if (m_cachedDesaturationLoc < 0) {
                m_cachedDesaturationLoc =
                    ::biofuel::engine::runtime::typed::Shaders::loc<BlurCompositeShader, BlurCompositeUniforms::Desaturation>(composite);
                m_cachedVignetteLoc =
                    ::biofuel::engine::runtime::typed::Shaders::loc<BlurCompositeShader, BlurCompositeUniforms::VignetteStrength>(composite);
                m_cachedDimLoc =
                    ::biofuel::engine::runtime::typed::Shaders::loc<BlurCompositeShader, BlurCompositeUniforms::DimStrength>(composite);
            }

            ::biofuel::engine::runtime::typed::Shaders::set<BlurCompositeShader, BlurCompositeUniforms::Desaturation>(
                composite, m_cachedDesaturationLoc, &m_config.desaturation);
            ::biofuel::engine::runtime::typed::Shaders::set<BlurCompositeShader, BlurCompositeUniforms::VignetteStrength>(
                composite, m_cachedVignetteLoc, &m_config.vignetteStrength);
            ::biofuel::engine::runtime::typed::Shaders::set<BlurCompositeShader, BlurCompositeUniforms::DimStrength>(
                composite, m_cachedDimLoc, &m_config.dimStrength);

            {
                ScopedTextureMode textureScope(blurSurfaceB().target());
                ClearBackground(BLANK);
                ScopedShaderMode shaderScope(composite);
                Renderer::drawRenderTexture(source, 0, 0, blurSurfaceB().width(), blurSurfaceB().height(), WHITE);
            }
            {
                ScopedTextureMode textureScope(blurSurfaceA().target());
                ClearBackground(BLANK);
                Renderer::drawRenderTexture(blurSurfaceB().texture(), 0, 0, blurSurfaceA().width(), blurSurfaceA().height(), WHITE);
            }
        } else {
            if (source.id != blurSurfaceA().texture().id) {
                ScopedTextureMode textureScope(blurSurfaceA().target());
                ClearBackground(BLANK);
                Renderer::drawRenderTexture(source, 0, 0, sw, sh, WHITE);
            }
        }
    } else {
        ScopedTextureMode textureScope(blurSurfaceA().target());
        ClearBackground(BLANK);
        Renderer::drawRenderTexture(captureSurface().texture(), 0, 0, sw, sh, WHITE);
    }

    m_blurCacheValid = true;
}

void ScreenBlurEffect::ensureTextures(const i32 width, const i32 height) {
    const f32 captureScale = std::clamp(m_config.captureScale, 0.2f, 1.0f);
    const i32 scaledWidth = std::max(1, static_cast<i32>(width * captureScale));
    const i32 scaledHeight = std::max(1, static_cast<i32>(height * captureScale));

    if (width == m_cachedWidth &&
        height == m_cachedHeight &&
        captureScale - m_cachedCaptureScale < 0.001f &&
        captureScale - m_cachedCaptureScale > -0.001f &&
        m_captureSurface != nullptr &&
        m_blurSurfaceA != nullptr &&
        m_blurSurfaceB != nullptr &&
        m_captureSurface->valid() &&
        m_blurSurfaceA->valid() &&
        m_blurSurfaceB->valid()) {
        return;
    }

    auto& cache = ::biofuel::engine::graphics::TransientResourceCache::instance();
    m_captureSurface = cache.acquireSurface(CAPTURE_CACHE_KEY, width, height);
    m_blurSurfaceA = cache.acquireSurface(BLUR_A_CACHE_KEY, scaledWidth, scaledHeight);
    m_blurSurfaceB = cache.acquireSurface(BLUR_B_CACHE_KEY, scaledWidth, scaledHeight);
    invalidateCache();
    m_cachedWidth = width;
    m_cachedHeight = height;
    m_cachedCaptureScale = captureScale;

    spdlog::debug(
        "ScreenBlurEffect: resized capture {}x{} and blur {}x{}",
        width, height, scaledWidth, scaledHeight
    );
}

void ScreenBlurEffect::invalidateCache() noexcept {
    m_blurCacheValid = false;
}

void ScreenBlurEffect::blurPass(
    Shader shader,
    Texture2D source,
    RenderTexture2D dest,
    const f32 radius,
    const bool horizontal)
{
    using namespace ::biofuel::engine::graphics;

    i32& texelLoc = horizontal ? m_cachedTexelLocH : m_cachedTexelLocV;
    i32& radiusLoc = horizontal ? m_cachedRadiusLocH : m_cachedRadiusLocV;
    if (texelLoc < 0) {
        if (horizontal) {
            texelLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BlurHShader, BlurHUniforms::TexelSize>(shader);
            radiusLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BlurHShader, BlurHUniforms::Radius>(shader);
        } else {
            texelLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BlurVShader, BlurVUniforms::TexelSize>(shader);
            radiusLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BlurVShader, BlurVUniforms::Radius>(shader);
        }
    }

    ScopedTextureMode textureScope(dest);
    ClearBackground(BLANK);

    Vector2 texelSize = {
        1.0f / static_cast<f32>(std::max(source.width, 1)),
        1.0f / static_cast<f32>(std::max(source.height, 1))
    };
    if (horizontal) {
        ::biofuel::engine::runtime::typed::Shaders::set<BlurHShader, BlurHUniforms::TexelSize>(shader, texelLoc, &texelSize);
        ::biofuel::engine::runtime::typed::Shaders::set<BlurHShader, BlurHUniforms::Radius>(shader, radiusLoc, &radius);
    } else {
        ::biofuel::engine::runtime::typed::Shaders::set<BlurVShader, BlurVUniforms::TexelSize>(shader, texelLoc, &texelSize);
        ::biofuel::engine::runtime::typed::Shaders::set<BlurVShader, BlurVUniforms::Radius>(shader, radiusLoc, &radius);
    }

    ScopedShaderMode shaderScope(shader);
    Renderer::drawRenderTexture(source, 0, 0, dest.texture.width, dest.texture.height, WHITE);
}

void ScreenBlurEffect::resetAnimation(const f32 from, const f32 to, const f32 duration) noexcept {
    m_from = from;
    m_to = to;
    m_duration = duration;
    m_elapsed = 0.0f;
}

f32 ScreenBlurEffect::smoothStep(const f32 t) const noexcept {
    const f32 clamped = std::clamp(t, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

} // namespace biofuel::game::presentation::effects
