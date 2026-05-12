#include "ShaderManager.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include <cstring>
#include <spdlog/spdlog.h>

namespace biofuel::engine::graphics {

// ------------------------------------------------------------------------------
// Singleton
// ------------------------------------------------------------------------------

ShaderManager& ShaderManager::instance() noexcept {
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
            ::biofuel::engine::debug::MemoryTelemetry::remove(
                ::biofuel::engine::debug::ResourceKind::Shader,
                1,
                0);
        }
    }
    m_shaders.clear();
}

// ------------------------------------------------------------------------------
// Loading
// ------------------------------------------------------------------------------

void ShaderManager::load(std::string_view name, std::string_view vertPath, std::string_view fragPath) {
    unloadExisting(name);

    // Hoist strings so pointers survive until LoadShader returns
    const std::string vertStr{vertPath};
    const std::string fragStr{fragPath};
    const char* vPath = vertPath.empty() ? nullptr : vertStr.c_str();
    const char* fPath = fragPath.empty() ? nullptr : fragStr.c_str();

    Shader shader = LoadShader(vPath, fPath);

    if (!IsShaderValid(shader)) {
        spdlog::error("ShaderManager: failed to load shader '{}' (vert: {}, frag: {})",
            name, vertPath.empty() ? "<default>" : vertPath, fragPath.empty() ? "<default>" : fragPath);
        return;
    }

    spdlog::info("ShaderManager: loaded shader '{}'", name);
    m_shaders.emplace(std::string{name}, shader);
    ::biofuel::engine::debug::MemoryTelemetry::add(
        ::biofuel::engine::debug::ResourceKind::Shader,
        1,
        0);
}

void ShaderManager::loadFromMemory(std::string_view name, const char* vertCode, const char* fragCode) {
    unloadExisting(name);

    Shader shader = LoadShaderFromMemory(vertCode, fragCode);

    if (!IsShaderValid(shader)) {
        spdlog::error("ShaderManager: failed to compile shader '{}' from memory", name);
        return;
    }

    spdlog::info("ShaderManager: compiled shader '{}' from memory", name);
    m_shaders.emplace(std::string{name}, shader);
    ::biofuel::engine::debug::MemoryTelemetry::add(
        ::biofuel::engine::debug::ResourceKind::Shader,
        1,
        0);
}

// ------------------------------------------------------------------------------
// Private helpers
// ------------------------------------------------------------------------------

void ShaderManager::unloadExisting(std::string_view name) {
    // Transparent find — no temp string allocation
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        if (IsShaderValid(it->second)) {
            UnloadShader(it->second);
            ::biofuel::engine::debug::MemoryTelemetry::remove(
                ::biofuel::engine::debug::ResourceKind::Shader,
                1,
                0);
        }
        m_shaders.erase(it);
    }
}

// ------------------------------------------------------------------------------
// Unloading
// ------------------------------------------------------------------------------

void ShaderManager::unload(std::string_view name) {
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        if (IsShaderValid(it->second)) {
            UnloadShader(it->second);
            ::biofuel::engine::debug::MemoryTelemetry::remove(
                ::biofuel::engine::debug::ResourceKind::Shader,
                1,
                0);
        }
        m_shaders.erase(it);
        spdlog::info("ShaderManager: unloaded shader '{}'", name);
    }
}

// ------------------------------------------------------------------------------
// Queries
// ------------------------------------------------------------------------------

Shader ShaderManager::get(std::string_view name) const noexcept {
    const Shader shader = tryGet(name);
    if (shader.id > 0) {
        return shader;
    }

    // Only warn on miss — this path is rare (startup only)
    spdlog::warn("ShaderManager: shader '{}' not found", name);
    return Shader{};
}

Shader ShaderManager::tryGet(std::string_view name) const noexcept {
    // Transparent find — no temp string allocation
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second;
    }
    return Shader{};
}

bool ShaderManager::has(std::string_view name) const noexcept {
    return m_shaders.find(name) != m_shaders.end();
}

// ------------------------------------------------------------------------------
// Uniform helpers
// ------------------------------------------------------------------------------

i32 ShaderManager::getLocation(Shader shader, std::string_view uniformName) noexcept {
    if (!IsShaderValid(shader)) {
        return -1;
    }
    // Safely null-terminate: uniform names are short (< 64 chars), so a
    // stack buffer avoids both the heap allocation of std::string and the
    // UB risk of assuming string_view::data() is null-terminated (B037).
    char buf[64];
    if (uniformName.size() < sizeof(buf)) {
        std::memcpy(buf, uniformName.data(), uniformName.size());
        buf[uniformName.size()] = '\0';
        return GetShaderLocation(shader, buf);
    }
    // Fallback for unexpectedly long names
    const std::string owned{uniformName};
    return GetShaderLocation(shader, owned.c_str());
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

} // namespace biofuel::engine::graphics
