#pragma once

#include "Core/Types.hpp"
#include <raylib.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace biofuel::utils::render {

// ---- Transparent hash for heterogeneous string/string_view lookups ----
// Enables unordered_map::find(string_view) without creating a temp std::string.
struct StringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

// ------------------------------------------------------------------------------
// ShaderManager - Loads, caches, and provides Raylib Shader objects by name.
// ------------------------------------------------------------------------------
class ShaderManager {
public:
    [[nodiscard]] static ShaderManager& instance();

    void init();
    void shutdown();

    // Load a shader from vertex + fragment file paths.
    // Pass empty string for vertex to use Raylib's built-in default vertex shader.
    void load(std::string_view name, std::string_view vertPath, std::string_view fragPath);

    // Load a shader from memory (source strings compiled into the binary).
    // Pass nullptr for vertex to use Raylib's built-in default vertex shader.
    void loadFromMemory(std::string_view name, const char* vertCode, const char* fragCode);

    // Unload a specific shader by name
    void unload(std::string_view name);

    // Get a loaded shader by name. Returns a default-invalid shader if not found.
    [[nodiscard]] Shader get(std::string_view name) const noexcept;
    [[nodiscard]] Shader tryGet(std::string_view name) const noexcept;

    // Check if a shader is loaded and valid
    [[nodiscard]] bool has(std::string_view name) const noexcept;

    // Get uniform location (convenience)
    [[nodiscard]] static i32 getLocation(Shader shader, std::string_view uniformName) noexcept;

    // Set uniform value (convenience wrappers)
    static void setValue(Shader shader, i32 loc, const void* value, i32 uniformType) noexcept;
    static void setValueTexture(Shader shader, i32 loc, Texture2D texture) noexcept;

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;

private:
    ShaderManager() = default;
    ~ShaderManager() = default;

    // Unload existing shader by name if present. Used by both load() and loadFromMemory().
    void unloadExisting(std::string_view name);

    std::unordered_map<std::string, Shader, StringHash, std::equal_to<>> m_shaders;
};

} // namespace biofuel::utils::render
