#include "ScreenBackdropController.hpp"
#include "AnimationController/animation/Easing.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include <algorithm>

namespace biofuel::animation::screen {

namespace {

constexpr std::string_view UNIFORM_IRESOLUTION = "iResolution";
constexpr std::string_view UNIFORM_ITIME = "iTime";
constexpr std::string_view UNIFORM_UBRIGHTNESS = "uBrightness";
constexpr std::string_view UNIFORM_UREVEAL_PROGRESS = "uRevealProgress";

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
    m_revealElapsed = 0.0f;
}

void ScreenBackdropController::update(const f32 dt) noexcept {
    m_time += dt;
    m_revealElapsed += dt;
}

void ScreenBackdropController::render(const f32 transitionAlpha) const {
    using namespace utils::render;

    ensureShader();

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    if (!m_shaderReady) {
        Renderer::drawFullscreen(m_config.fallbackColor);
        return;
    }

    const f32 resolution[3] = {
        static_cast<f32>(sw),
        static_cast<f32>(sh),
        1.0f
    };

    const f32 crossfadeProgress = animation::Easing::easeOutCubic(transitionAlpha);
    const f32 reveal = revealProgress();
    const f32 brightnessMix = std::clamp(
        (crossfadeProgress * m_config.transitionWeight) + (reveal * m_config.revealWeight),
        0.0f,
        1.0f
    );
    const f32 brightness = std::clamp(
        m_config.brightnessFloor +
        (m_config.brightnessCeiling - m_config.brightnessFloor) * animation::Easing::easeOutCubic(brightnessMix),
        0.0f,
        1.0f
    );

    ShaderManager::setValue(m_shader, m_resolutionLoc, resolution, SHADER_UNIFORM_VEC3);
    ShaderManager::setValue(m_shader, m_timeLoc, &m_time, SHADER_UNIFORM_FLOAT);
    ShaderManager::setValue(m_shader, m_brightnessLoc, &brightness, SHADER_UNIFORM_FLOAT);
    ShaderManager::setValue(m_shader, m_revealLoc, &reveal, SHADER_UNIFORM_FLOAT);

    ScopedShaderMode shaderScope(m_shader);
    Renderer::drawFullscreen(WHITE);
}

f32 ScreenBackdropController::revealProgress() const noexcept {
    const f32 delayed = std::max(0.0f, m_revealElapsed - m_config.revealDelay);
    const f32 normalized = (m_config.revealDuration > 0.0f)
        ? std::clamp(delayed / m_config.revealDuration, 0.0f, 1.0f)
        : 1.0f;
    return animation::Easing::easeOutCubic(normalized);
}

void ScreenBackdropController::setFloat(std::string_view uniformName, const f32 value) const {
    ensureShader();
    if (!m_shaderReady) {
        return;
    }
    // Check cache first to avoid per-frame GL lookups
    i32 loc = -1;
    const std::string key{uniformName};
    auto it = m_uniformCache.find(key);
    if (it != m_uniformCache.end()) {
        loc = it->second;
    } else {
        loc = utils::render::ShaderManager::getLocation(m_shader, uniformName);
        m_uniformCache.emplace(key, loc);
    }
    utils::render::ShaderManager::setValue(m_shader, loc, &value, SHADER_UNIFORM_FLOAT);
}

Shader ScreenBackdropController::shader() const noexcept {
    ensureShader();
    return m_shader;
}

bool ScreenBackdropController::ready() const noexcept {
    ensureShader();
    return m_shaderReady;
}

void ScreenBackdropController::ensureShader() const {
    if (m_shaderReady) {
        return;
    }

    auto& shaderManager = utils::render::ShaderManager::instance();
    m_shader = shaderManager.tryGet(m_config.shaderName);
    if (m_shader.id == 0) {
        m_shaderReady = false;
        return;
    }

    m_resolutionLoc = shaderManager.getLocation(m_shader, UNIFORM_IRESOLUTION);
    m_timeLoc = shaderManager.getLocation(m_shader, UNIFORM_ITIME);
    m_brightnessLoc = shaderManager.getLocation(m_shader, UNIFORM_UBRIGHTNESS);
    m_revealLoc = shaderManager.getLocation(m_shader, UNIFORM_UREVEAL_PROGRESS);
    m_shaderReady = true;
}

} // namespace biofuel::animation::screen
