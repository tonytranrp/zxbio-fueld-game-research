#include "ScreenBlurEffect.hpp"
#include "UI/Screen.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/Shader/BlurCompositeModule.hpp"
#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"
#include "Utils/render/ShaderManager.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace biofuel::animation::screen {

void ScreenBlurEffect::init(const i32 width, const i32 height) {
    ensureTextures(width, height);
}

void ScreenBlurEffect::shutdown() {
    m_captureSurface.release();
    m_blurSurfaceA.release();
    m_blurSurfaceB.release();
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
    resetAnimation(0.0f, static_cast<f32>(config.maxTintAlpha), config.fadeInDuration);
    m_state = State::BlurringIn;
    m_blurRadius = 0.0f;
}

void ScreenBlurEffect::startBlurOut(const BlurConfig& config) noexcept {
    m_config = config;
    resetAnimation(static_cast<f32>(m_tintAlpha), 0.0f, config.fadeOutDuration);
    m_state = State::BlurringOut;
}

void ScreenBlurEffect::cancel() noexcept {
    m_state = State::Idle;
    m_tintAlpha = 0;
    m_blurRadius = 0.0f;
    m_elapsed = 0.0f;
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

void ScreenBlurEffect::update(const f32 dt) {
    const i32 sw = utils::render::Renderer::screenWidth();
    const i32 sh = utils::render::Renderer::screenHeight();
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
    const f32 eased = easeOutQuad(t);
    const f32 tintValue = std::clamp(m_from + (m_to - m_from) * eased, 0.0f, 255.0f);
    m_tintAlpha = static_cast<u8>(tintValue);

    if (m_state == State::BlurringIn) {
        m_blurRadius = m_config.blurRadius * eased;
    } else {
        m_blurRadius = m_config.blurRadius * (1.0f - eased);
    }
}

void ScreenBlurEffect::render(biofuel::ui::Screen* prevScreen) {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();
    ensureTextures(sw, sh);

    if (prevScreen == nullptr) {
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

    {
        ScopedTextureMode textureScope(m_captureSurface.target());
        ClearBackground(BLANK);
        prevScreen->onRender();
    }

    auto& shaderManager = ShaderManager::instance();
    const bool canBlur = m_blurRadius > 0.1f &&
        shaderManager.has(shader::BlurHModule::NAME.data()) &&
        shaderManager.has(shader::BlurVModule::NAME.data()) &&
        m_blurSurfaceA.valid() &&
        m_blurSurfaceB.valid();

    if (canBlur) {
        {
            ScopedTextureMode textureScope(m_blurSurfaceA.target());
            ClearBackground(BLANK);
            Renderer::drawRenderTexture(
                m_captureSurface.texture(),
                0,
                0,
                m_blurSurfaceA.width(),
                m_blurSurfaceA.height(),
                WHITE
            );
        }

        Shader blurH = shaderManager.get(shader::BlurHModule::NAME.data());
        Shader blurV = shaderManager.get(shader::BlurVModule::NAME.data());
        Texture2D source = m_blurSurfaceA.texture();
        const i32 blurPasses = std::clamp(m_config.blurPassCount, 1, 4);

        for (i32 i = 0; i < blurPasses; ++i) {
            blurPass(blurH, source, m_blurSurfaceB.target(), m_blurRadius, true);
            blurPass(blurV, m_blurSurfaceB.texture(), m_blurSurfaceA.target(), m_blurRadius, false);
            source = m_blurSurfaceA.texture();
        }

        if (shaderManager.has(shader::BlurCompositeModule::NAME.data())) {
            Shader composite = shaderManager.get(shader::BlurCompositeModule::NAME.data());
            if (m_cachedDesaturationLoc < 0) {
                m_cachedDesaturationLoc = ShaderManager::getLocation(composite, shader::BlurCompositeModule::UNIFORM_DESATURATION.data());
                m_cachedVignetteLoc = ShaderManager::getLocation(composite, shader::BlurCompositeModule::UNIFORM_VIGNETTE_STRENGTH.data());
                m_cachedDimLoc = ShaderManager::getLocation(composite, shader::BlurCompositeModule::UNIFORM_DIM_STRENGTH.data());
            }

            ShaderManager::setValue(composite, m_cachedDesaturationLoc, &m_config.desaturation, SHADER_UNIFORM_FLOAT);
            ShaderManager::setValue(composite, m_cachedVignetteLoc, &m_config.vignetteStrength, SHADER_UNIFORM_FLOAT);
            ShaderManager::setValue(composite, m_cachedDimLoc, &m_config.dimStrength, SHADER_UNIFORM_FLOAT);

            ScopedShaderMode shaderScope(composite);
            Renderer::drawRenderTexture(source, 0, 0, sw, sh, WHITE);
        } else {
            Renderer::drawRenderTexture(source, 0, 0, sw, sh, WHITE);
        }
    } else {
        Renderer::drawRenderTexture(m_captureSurface.texture(), 0, 0, sw, sh, WHITE);
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

void ScreenBlurEffect::ensureTextures(const i32 width, const i32 height) {
    const f32 captureScale = std::clamp(m_config.captureScale, 0.2f, 1.0f);
    const i32 scaledWidth = std::max(1, static_cast<i32>(width * captureScale));
    const i32 scaledHeight = std::max(1, static_cast<i32>(height * captureScale));

    if (width == m_cachedWidth &&
        height == m_cachedHeight &&
        captureScale - m_cachedCaptureScale < 0.001f &&
        captureScale - m_cachedCaptureScale > -0.001f &&
        m_captureSurface.valid() &&
        m_blurSurfaceA.valid() &&
        m_blurSurfaceB.valid()) {
        return;
    }

    m_captureSurface.ensureSize(width, height);
    m_blurSurfaceA.ensureSize(scaledWidth, scaledHeight);
    m_blurSurfaceB.ensureSize(scaledWidth, scaledHeight);
    m_cachedWidth = width;
    m_cachedHeight = height;
    m_cachedCaptureScale = captureScale;

    spdlog::debug(
        "ScreenBlurEffect: resized capture {}x{} and blur {}x{}",
        width, height, scaledWidth, scaledHeight
    );
}

void ScreenBlurEffect::blurPass(
    Shader shader,
    Texture2D source,
    RenderTexture2D dest,
    const f32 radius,
    const bool horizontal)
{
    using namespace utils::render;

    i32& texelLoc = horizontal ? m_cachedTexelLocH : m_cachedTexelLocV;
    i32& radiusLoc = horizontal ? m_cachedRadiusLocH : m_cachedRadiusLocV;
    const std::string_view texelName = horizontal
        ? shader::BlurHModule::UNIFORM_TEXEL_SIZE
        : shader::BlurVModule::UNIFORM_TEXEL_SIZE;
    const std::string_view radiusName = horizontal
        ? shader::BlurHModule::UNIFORM_BLUR_RADIUS
        : shader::BlurVModule::UNIFORM_BLUR_RADIUS;

    if (texelLoc < 0) {
        texelLoc = ShaderManager::getLocation(shader, texelName);
        radiusLoc = ShaderManager::getLocation(shader, radiusName);
    }

    ScopedTextureMode textureScope(dest);
    ClearBackground(BLANK);

    Vector2 texelSize = {
        1.0f / static_cast<f32>(std::max(source.width, 1)),
        1.0f / static_cast<f32>(std::max(source.height, 1))
    };
    ShaderManager::setValue(shader, texelLoc, &texelSize, SHADER_UNIFORM_VEC2);
    ShaderManager::setValue(shader, radiusLoc, &radius, SHADER_UNIFORM_FLOAT);

    ScopedShaderMode shaderScope(shader);
    Renderer::drawRenderTexture(source, 0, 0, dest.texture.width, dest.texture.height, WHITE);
}

void ScreenBlurEffect::resetAnimation(const f32 from, const f32 to, const f32 duration) noexcept {
    m_from = from;
    m_to = to;
    m_duration = duration;
    m_elapsed = 0.0f;
}

f32 ScreenBlurEffect::easeOutQuad(const f32 t) const noexcept {
    return t * (2.0f - t);
}

} // namespace biofuel::animation::screen
