#pragma once

#include "Core/Types.hpp"
#include <raylib.h>
#include <string>
#include <string_view>

namespace biofuel::utils::render {

class ScopedShaderMode final {
public:
    explicit ScopedShaderMode(Shader shader) noexcept
        : m_active(IsShaderValid(shader)) {
        if (m_active) {
            BeginShaderMode(shader);
        }
    }

    ~ScopedShaderMode() noexcept {
        if (m_active) {
            EndShaderMode();
        }
    }

    ScopedShaderMode(const ScopedShaderMode&) = delete;
    ScopedShaderMode& operator=(const ScopedShaderMode&) = delete;
    ScopedShaderMode(ScopedShaderMode&&) = delete;
    ScopedShaderMode& operator=(ScopedShaderMode&&) = delete;

private:
    bool m_active = false;
};

class ScopedTextureMode final {
public:
    explicit ScopedTextureMode(RenderTexture2D target) noexcept
        : m_active(target.id > 0) {
        if (m_active) {
            BeginTextureMode(target);
        }
    }

    ~ScopedTextureMode() noexcept {
        if (m_active) {
            EndTextureMode();
        }
    }

    ScopedTextureMode(const ScopedTextureMode&) = delete;
    ScopedTextureMode& operator=(const ScopedTextureMode&) = delete;
    ScopedTextureMode(ScopedTextureMode&&) = delete;
    ScopedTextureMode& operator=(ScopedTextureMode&&) = delete;

private:
    bool m_active = false;
};

// ------------------------------------------------------------------------------
// Renderer - Raylib rendering utilities
// Wraps common draw calls with std::string support and C++ conveniences.
// All coordinates use project types (i32, f32) per coding standards.
// ------------------------------------------------------------------------------
class Renderer {
public:
    static void beginFrame(Color clearColor = RAYWHITE);
    static void endFrame();

    static void drawText(const std::string& text, i32 x, i32 y, i32 fontSize, Color color);
    static void drawTextCentered(const std::string& text, i32 x, i32 y, i32 fontSize, Color color);
    static void drawText(std::string_view text, i32 x, i32 y, i32 fontSize, Color color);
    static void drawTextCentered(std::string_view text, i32 centerX, i32 y, i32 fontSize, Color color);
    static void drawText(const char* text, i32 x, i32 y, i32 fontSize, Color color);
    static void drawTextCentered(const char* text, i32 centerX, i32 y, i32 fontSize, Color color);
    static void drawRect(i32 x, i32 y, i32 width, i32 height, Color color);
    static void drawRectLines(i32 x, i32 y, i32 width, i32 height, Color color);
    static void drawSprite(Texture2D texture, i32 x, i32 y, i32 width, i32 height);
    static void drawFullscreen(Color color);
    static void drawRenderTexture(Texture2D texture, i32 x = 0, i32 y = 0, Color tint = WHITE);
    static void drawRenderTexture(Texture2D texture, i32 x, i32 y, i32 width, i32 height, Color tint);

    [[nodiscard]] static i32 measureText(std::string_view text, i32 fontSize) noexcept;
    [[nodiscard]] static i32 screenWidth() noexcept;
    [[nodiscard]] static i32 screenHeight() noexcept;
};

} // namespace biofuel::utils::render
