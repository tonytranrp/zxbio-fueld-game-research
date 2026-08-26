#include "ScreenBackdropController.hpp"
#include "engine/animation/Easing.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/runtime/typed/Assets.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/shaders/TypedShaderModule.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include <algorithm>
#include <array>

namespace biofuel::game::presentation::effects {

namespace {

using BgShader = ::biofuel::engine::runtime::typed::shader::ProceduralBackdrop;
namespace BgUniforms = ::biofuel::engine::runtime::typed::shader::procedural_backdrop;

[[nodiscard]] Shader backdropShader(std::string_view name, ::biofuel::engine::graphics::ShaderManager& shaderManager) noexcept {
    if (name == ::biofuel::engine::runtime::typed::ShaderAsset<::biofuel::engine::runtime::typed::shader::ProceduralBackdrop>::Name) {
        return ::biofuel::engine::runtime::typed::Shaders::get<::biofuel::engine::runtime::typed::shader::ProceduralBackdrop>(shaderManager);
    }
    if (name == ::biofuel::engine::runtime::typed::ShaderAsset<::biofuel::engine::runtime::typed::shader::LoadingPrelude>::Name) {
        return ::biofuel::engine::runtime::typed::Shaders::get<::biofuel::engine::runtime::typed::shader::LoadingPrelude>(shaderManager);
    }
    return shaderManager.tryGet(name);
}

} // namespace

void ScreenBackdropController::configure(const ScreenBackdropConfig& config) noexcept {
    m_config = config;
    m_shader = {};
    m_resolutionLoc = -1;
    m_timeLoc = -1;
    m_brightnessLoc = -1;
    m_revealLoc = -1;
    m_shaderReady = false;
    m_uniformCache.clear();
}

void ScreenBackdropController::reset() noexcept {
    m_time = 0.0f;
    m_timeOrigin = GetTime();
    m_revealElapsed = 0.0f;
}

void ScreenBackdropController::restartReveal() noexcept {
    m_revealElapsed = 0.0f;
}

void ScreenBackdropController::update(const f32 dt) noexcept {
    m_time += dt;
    m_revealElapsed += dt;
}

void ScreenBackdropController::render(const f32 transitionAlpha) const {
    using namespace ::biofuel::engine::graphics;

    ensureShader();

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    if (!m_shaderReady) {
        Renderer::drawFullscreen(m_config.fallbackColor);
        return;
    }

    // Shade at a reduced internal resolution, then upscale — the raymarch
    // cost scales with pixel count, so this is the single biggest lever on
    // hardware with no dedicated GPU to fall back on.
    const i32 rw = std::max(1, static_cast<i32>(static_cast<f32>(sw) * kInternalRenderScale));
    const i32 rh = std::max(1, static_cast<i32>(static_cast<f32>(sh) * kInternalRenderScale));
    m_surface.ensureSize(rw, rh);
    const bool useSurface = m_surface.valid();
    if (useSurface) {
        SetTextureFilter(m_surface.texture(), TEXTURE_FILTER_BILINEAR);
    }

    const std::array<f32, 3> resolution{
        static_cast<f32>(useSurface ? rw : sw),
        static_cast<f32>(useSurface ? rh : sh),
        1.0f
    };

    const f32 crossfadeProgress = ::biofuel::engine::animation::Easing::easeOutCubic(transitionAlpha);
    const f32 reveal = revealProgress();
    const f32 brightnessMix = std::clamp(
        (crossfadeProgress * m_config.transitionWeight) + (reveal * m_config.revealWeight),
        0.0f,
        1.0f
    );
    const f32 brightness = std::clamp(
        m_config.brightnessFloor +
        (m_config.brightnessCeiling - m_config.brightnessFloor) * ::biofuel::engine::animation::Easing::easeOutCubic(brightnessMix),
        0.0f,
        1.0f
    );

    const f32 time = shaderTime();
    ::biofuel::engine::runtime::typed::Shaders::set<BgShader, BgUniforms::IResolution>(m_shader, m_resolutionLoc, resolution.data());
    ::biofuel::engine::runtime::typed::Shaders::set<BgShader, BgUniforms::ITime>(m_shader, m_timeLoc, &time);
    ::biofuel::engine::runtime::typed::Shaders::set<BgShader, BgUniforms::Brightness>(m_shader, m_brightnessLoc, &brightness);
    ::biofuel::engine::runtime::typed::Shaders::set<BgShader, BgUniforms::RevealProgress>(m_shader, m_revealLoc, &reveal);

    if (useSurface) {
        {
            ScopedTextureMode textureScope(m_surface.target());
            ScopedShaderMode shaderScope(m_shader);
            Renderer::drawRect(0, 0, rw, rh, WHITE);
        }
        Renderer::drawRenderTexture(m_surface.texture(), 0, 0, sw, sh, WHITE);
    } else {
        // GPU allocation failed — fall back to direct native-resolution draw.
        ScopedShaderMode shaderScope(m_shader);
        Renderer::drawFullscreen(WHITE);
    }
}

f32 ScreenBackdropController::revealProgress() const noexcept {
    const f32 delayed = std::max(0.0f, m_revealElapsed - m_config.revealDelay);
    const f32 normalized = (m_config.revealDuration > 0.0f)
        ? std::clamp(delayed / m_config.revealDuration, 0.0f, 1.0f)
        : 1.0f;
    return ::biofuel::engine::animation::Easing::easeOutCubic(normalized);
}

void ScreenBackdropController::setFloat(std::string_view uniformName, const f32 value) const {
    ensureShader();
    if (!m_shaderReady) {
        return;
    }
    // Transparent find — no allocation on cache hit
    i32 loc = -1;
    auto it = m_uniformCache.find(uniformName);
    if (it != m_uniformCache.end()) {
        loc = it->second;
    } else {
        loc = ::biofuel::engine::graphics::ShaderManager::getLocation(m_shader, uniformName);
        m_uniformCache.emplace(std::string{uniformName}, loc);
    }
    ::biofuel::engine::graphics::ShaderManager::setValue(m_shader, loc, &value, SHADER_UNIFORM_FLOAT);
}

Shader ScreenBackdropController::shader() const noexcept {
    ensureShader();
    return m_shader;
}

bool ScreenBackdropController::ready() const noexcept {
    ensureShader();
    return m_shaderReady;
}

f32 ScreenBackdropController::shaderTime() const noexcept {
    const f64 elapsed = GetTime() - m_timeOrigin;
    if (elapsed >= 0.0) {
        return static_cast<f32>(elapsed);
    }
    return m_time;
}

void ScreenBackdropController::ensureShader() const {
    if (m_shaderReady) {
        return;
    }

    auto& shaderManager = ::biofuel::engine::runtime::Runtime::shader();
    m_shader = backdropShader(m_config.shaderName, shaderManager);
    if (m_shader.id == 0) {
        m_shaderReady = false;
        return;
    }

    m_resolutionLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BgShader, BgUniforms::IResolution>(m_shader);
    m_timeLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BgShader, BgUniforms::ITime>(m_shader);
    m_brightnessLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BgShader, BgUniforms::Brightness>(m_shader);
    m_revealLoc = ::biofuel::engine::runtime::typed::Shaders::loc<BgShader, BgUniforms::RevealProgress>(m_shader);
    m_shaderReady = true;
}

} // namespace biofuel::game::presentation::effects
