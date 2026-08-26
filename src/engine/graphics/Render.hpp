#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <string>
#include <string_view>
#include <vector>

namespace biofuel::engine::graphics {

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

// Raylib's EndTextureMode() unconditionally rebinds the screen framebuffer —
// it has no concept of "the previously active render target" — so a naive
// nested BeginTextureMode/EndTextureMode pair corrupts whichever outer render
// texture was active (e.g. a screen drawing its own offscreen effect while
// already being captured into a crossfade/blur surface by ScreenManager).
// This stack restores the outer target on the way out so texture-mode scopes
// nest correctly regardless of call order.
class ScopedTextureMode final {
public:
    explicit ScopedTextureMode(RenderTexture2D target) noexcept
        : m_active(target.id > 0) {
        if (m_active) {
            stack().push_back(target);
            BeginTextureMode(target);
        }
    }

    ~ScopedTextureMode() noexcept {
        if (m_active) {
            EndTextureMode();
            stack().pop_back();
            if (!stack().empty()) {
                BeginTextureMode(stack().back());
            }
        }
    }

    ScopedTextureMode(const ScopedTextureMode&) = delete;
    ScopedTextureMode& operator=(const ScopedTextureMode&) = delete;
    ScopedTextureMode(ScopedTextureMode&&) = delete;
    ScopedTextureMode& operator=(ScopedTextureMode&&) = delete;

private:
    static std::vector<RenderTexture2D>& stack() noexcept {
        static std::vector<RenderTexture2D> instance;
        return instance;
    }

    bool m_active = false;
};

// ------------------------------------------------------------------------------
// Renderer - Raylib rendering utilities
// Wraps common draw calls with std::string support and C++ conveniences.
// All coordinates use project types (i32, f32) per coding standards.
//
// Trivial passthroughs are defined inline so the compiler can flatten the
// wrapper call entirely, even across translation units.
// ------------------------------------------------------------------------------
class Renderer {
public:
    // ---- Frame lifecycle (inline — direct Raylib passthroughs) ----
    static BIOFUEL_FORCE_INLINE void beginFrame(Color clearColor = RAYWHITE) noexcept {
        BeginDrawing();
        ClearBackground(clearColor);
    }
    static BIOFUEL_FORCE_INLINE void endFrame() noexcept { EndDrawing(); }

    // ---- Text drawing (simple overloads — inline) ----
    static BIOFUEL_FORCE_INLINE void drawText(const std::string& text, i32 x, i32 y, i32 fontSize, Color color) noexcept {
        ::DrawText(text.c_str(), x, y, fontSize, color);
    }
    static BIOFUEL_FORCE_INLINE void drawTextCentered(const std::string& text, i32 centerX, i32 y, i32 fontSize, Color color) noexcept {
        const i32 textWidth = MeasureText(text.c_str(), fontSize);
        ::DrawText(text.c_str(), centerX - textWidth / 2, y, fontSize, color);
    }
    static BIOFUEL_FORCE_INLINE void drawText(const char* text, i32 x, i32 y, i32 fontSize, Color color) noexcept {
        ::DrawText(text, x, y, fontSize, color);
    }
    static BIOFUEL_FORCE_INLINE void drawTextCentered(const char* text, i32 centerX, i32 y, i32 fontSize, Color color) noexcept {
        const i32 textWidth = MeasureText(text, fontSize);
        ::DrawText(text, centerX - textWidth / 2, y, fontSize, color);
    }

    // ---- Text drawing (string_view — needs stack-buffer null-termination) ----
    static void drawText(std::string_view text, i32 x, i32 y, i32 fontSize, Color color) noexcept;
    static void drawTextCentered(std::string_view text, i32 centerX, i32 y, i32 fontSize, Color color) noexcept;
    static void drawText(Font font, std::string_view text, i32 x, i32 y, i32 fontSize, Color color, f32 spacing = 0.0f) noexcept;
    static void drawTextCentered(
        Font font,
        std::string_view text,
        i32 centerX,
        i32 y,
        i32 fontSize,
        Color color,
        f32 spacing = 0.0f) noexcept;

    // ---- Shapes (inline — direct Raylib passthroughs) ----
    static BIOFUEL_FORCE_INLINE void drawRect(i32 x, i32 y, i32 width, i32 height, Color color) noexcept {
        DrawRectangle(x, y, width, height, color);
    }
    static BIOFUEL_FORCE_INLINE void drawRectLines(i32 x, i32 y, i32 width, i32 height, Color color) noexcept {
        DrawRectangleLines(x, y, width, height, color);
    }

    // ---- Sprites & textures ----
    static BIOFUEL_FORCE_INLINE void drawFullscreen(Color color) noexcept {
        drawRect(0, 0, screenWidth(), screenHeight(), color);
    }
    static BIOFUEL_FORCE_INLINE void drawFullscreenTexture(Texture2D texture, Color tint = WHITE) noexcept {
        DrawTexturePro(
            texture,
            Rectangle{0.0f, 0.0f, static_cast<f32>(texture.width), static_cast<f32>(texture.height)},
            Rectangle{0.0f, 0.0f, static_cast<f32>(screenWidth()), static_cast<f32>(screenHeight())},
            Vector2{0.0f, 0.0f},
            0.0f,
            tint
        );
    }
    static BIOFUEL_FORCE_INLINE void drawTextureRec(Texture2D texture, Rectangle source, Vector2 position, Color tint) noexcept {
        DrawTextureRec(texture, source, position, tint);
    }
    static void drawRenderTexture(Texture2D texture, i32 x = 0, i32 y = 0, Color tint = WHITE) noexcept;
    static void drawRenderTexture(Texture2D texture, i32 x, i32 y, i32 width, i32 height, Color tint) noexcept;

    [[nodiscard]] static i32 measureText(std::string_view text, i32 fontSize) noexcept;
    [[nodiscard]] static i32 measureText(Font font, std::string_view text, i32 fontSize, f32 spacing = 0.0f) noexcept;

    // ---- Screen dimensions (inline — direct Raylib passthroughs) ----
    [[nodiscard]] static BIOFUEL_FORCE_INLINE i32 screenWidth() noexcept { return GetScreenWidth(); }
    [[nodiscard]] static BIOFUEL_FORCE_INLINE i32 screenHeight() noexcept { return GetScreenHeight(); }
};

} // namespace biofuel::engine::graphics
