#include "ScreenBlurEffect.hpp"
#include "UI/Screen.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::animation::screen {

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void ScreenBlurEffect::init(const i32 width, const i32 height) {
    ensureTextures(width, height);
}

void ScreenBlurEffect::shutdown() {
    if (m_capture.id > 0) {
        UnloadRenderTexture(m_capture);
        m_capture = RenderTexture2D{};
    }
    if (m_pingPong.id > 0) {
        UnloadRenderTexture(m_pingPong);
        m_pingPong = RenderTexture2D{};
    }
    m_cachedWidth = 0;
    m_cachedHeight = 0;
}

// ------------------------------------------------------------------------------
// State transitions
// ------------------------------------------------------------------------------

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
}

// ------------------------------------------------------------------------------
// Queries
// ------------------------------------------------------------------------------

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

// ------------------------------------------------------------------------------
// Per-frame update
// ------------------------------------------------------------------------------

void ScreenBlurEffect::update(const f32 dt) {
    // Ensure textures match current screen size
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
    } else {
        const f32 t = (m_duration > 0.0f) ? (m_elapsed / m_duration) : 1.0f;
        const f32 eased = easeOutQuad(t);

        f32 value = m_from + (m_to - m_from) * eased;
        if (value < 0.0f) value = 0.0f;
        if (value > 255.0f) value = 255.0f;
        m_tintAlpha = static_cast<u8>(value);

        if (m_state == State::BlurringIn) {
            m_blurRadius = m_config.blurRadius * eased;
        } else {
            m_blurRadius = m_config.blurRadius * (1.0f - eased);
        }
    }
}

// ------------------------------------------------------------------------------
// Rendering
// ------------------------------------------------------------------------------

void ScreenBlurEffect::render(biofuel::ui::Screen* prevScreen) const {
    if (prevScreen == nullptr) {
        // No screen below — draw solid tint if any alpha, otherwise nothing
        if (m_tintAlpha > 0) {
            const i32 sw = utils::render::Renderer::screenWidth();
            const i32 sh = utils::render::Renderer::screenHeight();
            utils::render::Renderer::drawRect(0, 0, sw, sh,
                Color{m_config.tintColor.r, m_config.tintColor.g, m_config.tintColor.b, m_tintAlpha});
        }
        return;
    }

    // 1. Capture the previous screen into m_capture
    BeginTextureMode(m_capture);
        ClearBackground(BLANK);
        prevScreen->onRender();
    EndTextureMode();

    // 2. Two-pass Gaussian blur (only if blur radius > 0)
    auto& shaderMgr = utils::render::ShaderManager::instance();

    if (m_blurRadius > 0.1f && shaderMgr.has(utils::render::shader::BlurHModule::NAME.data()) && shaderMgr.has(utils::render::shader::BlurVModule::NAME.data())) {
        Shader blurH = shaderMgr.get(utils::render::shader::BlurHModule::NAME.data());
        Shader blurV = shaderMgr.get(utils::render::shader::BlurVModule::NAME.data());

        // Horizontal pass: m_capture → m_pingPong
        blurPass(blurH, m_capture.texture, m_pingPong, m_blurRadius);

        // Vertical pass: m_pingPong → screen
        const i32 sw = utils::render::Renderer::screenWidth();
        const i32 sh = utils::render::Renderer::screenHeight();

        BeginShaderMode(blurV);
            // Set uniforms
            i32 locTexel = utils::render::ShaderManager::getLocation(blurV, utils::render::shader::BlurVModule::UNIFORM_TEXEL_SIZE.data());
            i32 locRadius = utils::render::ShaderManager::getLocation(blurV, utils::render::shader::BlurVModule::UNIFORM_BLUR_RADIUS.data());
            Vector2 texelSize = {1.0f / static_cast<f32>(sw), 1.0f / static_cast<f32>(sh)};
            utils::render::ShaderManager::setValue(blurV, locTexel, &texelSize, SHADER_UNIFORM_VEC2);
            utils::render::ShaderManager::setValue(blurV, locRadius, &m_blurRadius, SHADER_UNIFORM_FLOAT);

            // Draw with Y-flip (OpenGL texture origin is bottom-left)
            DrawTextureRec(
                m_pingPong.texture,
                Rectangle{0.0f, 0.0f, static_cast<f32>(m_pingPong.texture.width), static_cast<f32>(-m_pingPong.texture.height)},
                Vector2{0.0f, 0.0f},
                WHITE
            );
        EndShaderMode();
    } else {
        // No blur — just draw the captured screen as-is
        DrawTextureRec(
            m_capture.texture,
            Rectangle{0.0f, 0.0f, static_cast<f32>(m_capture.texture.width), static_cast<f32>(-m_capture.texture.height)},
            Vector2{0.0f, 0.0f},
            WHITE
        );
    }

    // 3. Draw tinted overlay on top of the blurred background
    if (m_tintAlpha > 0) {
        const i32 sw = utils::render::Renderer::screenWidth();
        const i32 sh = utils::render::Renderer::screenHeight();
        utils::render::Renderer::drawRect(0, 0, sw, sh,
            Color{m_config.tintColor.r, m_config.tintColor.g, m_config.tintColor.b, m_tintAlpha});
    }
}

// ------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------

void ScreenBlurEffect::ensureTextures(const i32 width, const i32 height) {
    if (width == m_cachedWidth && height == m_cachedHeight && m_capture.id > 0) {
        return;
    }

    if (m_capture.id > 0) {
        UnloadRenderTexture(m_capture);
    }
    if (m_pingPong.id > 0) {
        UnloadRenderTexture(m_pingPong);
    }

    m_capture = LoadRenderTexture(width, height);
    m_pingPong = LoadRenderTexture(width, height);
    m_cachedWidth = width;
    m_cachedHeight = height;

    spdlog::debug("ScreenBlurEffect: resized capture textures to {}x{}", width, height);
}

void ScreenBlurEffect::blurPass(Shader shader, Texture2D source, RenderTexture2D dest, f32 radius) const {
    const i32 sw = utils::render::Renderer::screenWidth();
    const i32 sh = utils::render::Renderer::screenHeight();

    BeginTextureMode(dest);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            i32 locTexel = utils::render::ShaderManager::getLocation(shader, utils::render::shader::BlurHModule::UNIFORM_TEXEL_SIZE.data());
            i32 locRadius = utils::render::ShaderManager::getLocation(shader, utils::render::shader::BlurHModule::UNIFORM_BLUR_RADIUS.data());
            Vector2 texelSize = {1.0f / static_cast<f32>(sw), 1.0f / static_cast<f32>(sh)};
            utils::render::ShaderManager::setValue(shader, locTexel, &texelSize, SHADER_UNIFORM_VEC2);
            utils::render::ShaderManager::setValue(shader, locRadius, &radius, SHADER_UNIFORM_FLOAT);

            DrawTextureRec(
                source,
                Rectangle{0.0f, 0.0f, static_cast<f32>(source.width), static_cast<f32>(-source.height)},
                Vector2{0.0f, 0.0f},
                WHITE
            );
        EndShaderMode();
    EndTextureMode();
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
