#include "ShaderManager.hpp"
#include <spdlog/spdlog.h>

namespace biofuel::utils::render {

// ------------------------------------------------------------------------------
// Singleton
// ------------------------------------------------------------------------------

ShaderManager& ShaderManager::instance() {
    static ShaderManager instance;
    return instance;
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void ShaderManager::init() {
    // Shaders are loaded on-demand via load()
}

void ShaderManager::shutdown() {
    for (auto& [name, shader] : m_shaders) {
        if (IsShaderValid(shader)) {
            UnloadShader(shader);
        }
    }
    m_shaders.clear();
}

// ------------------------------------------------------------------------------
// Loading
// ------------------------------------------------------------------------------

void ShaderManager::load(std::string_view name, std::string_view vertPath, std::string_view fragPath) {
    const std::string key{name};
    unloadExisting(key);

    const char* vPath = vertPath.empty() ? nullptr : std::string{vertPath}.c_str();
    const char* fPath = fragPath.empty() ? nullptr : std::string{fragPath}.c_str();

    Shader shader = LoadShader(vPath, fPath);

    if (!IsShaderValid(shader)) {
        spdlog::error("ShaderManager: failed to load shader '{}' (vert: {}, frag: {})",
            key, vertPath.empty() ? "<default>" : vertPath, fragPath.empty() ? "<default>" : fragPath);
        return;
    }

    spdlog::info("ShaderManager: loaded shader '{}'", key);
    m_shaders.emplace(key, shader);
}

void ShaderManager::loadFromMemory(std::string_view name, const char* vertCode, const char* fragCode) {
    const std::string key{name};
    unloadExisting(key);

    Shader shader = LoadShaderFromMemory(vertCode, fragCode);

    if (!IsShaderValid(shader)) {
        spdlog::error("ShaderManager: failed to compile shader '{}' from memory", key);
        return;
    }

    spdlog::info("ShaderManager: compiled shader '{}' from memory", key);
    m_shaders.emplace(key, shader);
}

// ------------------------------------------------------------------------------
// Private helpers
// ------------------------------------------------------------------------------

void ShaderManager::unloadExisting(std::string_view name) {
    const std::string key{name};
    auto it = m_shaders.find(key);
    if (it != m_shaders.end()) {
        if (IsShaderValid(it->second)) {
            UnloadShader(it->second);
        }
        m_shaders.erase(it);
    }
}

// ------------------------------------------------------------------------------
// Unloading
// ------------------------------------------------------------------------------

void ShaderManager::unload(std::string_view name) {
    const std::string key{name};
    auto it = m_shaders.find(key);
    if (it != m_shaders.end()) {
        if (IsShaderValid(it->second)) {
            UnloadShader(it->second);
        }
        m_shaders.erase(it);
        spdlog::info("ShaderManager: unloaded shader '{}'", key);
    }
}

// ------------------------------------------------------------------------------
// Queries
// ------------------------------------------------------------------------------

Shader ShaderManager::get(std::string_view name) const noexcept {
    const std::string key{name};
    auto it = m_shaders.find(key);
    if (it != m_shaders.end()) {
        return it->second;
    }
    spdlog::warn("ShaderManager: shader '{}' not found", key);
    return Shader{};
}

bool ShaderManager::has(std::string_view name) const noexcept {
    return m_shaders.find(std::string{name}) != m_shaders.end();
}

// ------------------------------------------------------------------------------
// Uniform helpers
// ------------------------------------------------------------------------------

i32 ShaderManager::getLocation(Shader shader, std::string_view uniformName) noexcept {
    if (!IsShaderValid(shader)) {
        return -1;
    }
    return GetShaderLocation(shader, std::string{uniformName}.c_str());
}

void ShaderManager::setValue(Shader shader, i32 loc, const void* value, i32 uniformType) noexcept {
    if (!IsShaderValid(shader) || loc < 0 || value == nullptr) {
        return;
    }
    SetShaderValue(shader, loc, value, uniformType);
}

void ShaderManager::setValueTexture(Shader shader, i32 loc, Texture2D texture) noexcept {
    if (!IsShaderValid(shader) || loc < 0) {
        return;
    }
    SetShaderValueTexture(shader, loc, texture);
}

} // namespace biofuel::utils::render
