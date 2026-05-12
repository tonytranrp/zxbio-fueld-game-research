#include "Render.hpp"
#include <array>
#include <cstring>

namespace biofuel::engine::graphics {

namespace {

// Stack-buffer null termination for string_view — avoids heap allocation
// for short strings (covers all UI text). Falls back to std::string for long text.
const char* nullTerminate(std::string_view sv, std::array<char, 256>& buf) {
    if (sv.size() < buf.size()) {
        std::memcpy(buf.data(), sv.data(), sv.size());
        buf[sv.size()] = '\0';
        return buf.data();
    }
    // Fallback: caller must keep the returned string alive
    return nullptr;
}

} // namespace

void Renderer::beginFrame(Color clearColor) {
    BeginDrawing();
    ClearBackground(clearColor);
}

void Renderer::endFrame() {
    EndDrawing();
}

void Renderer::drawText(const std::string& text, i32 x, i32 y, i32 fontSize, Color color) {
    ::DrawText(text.c_str(), x, y, fontSize, color);
}

void Renderer::drawTextCentered(const std::string& text, i32 x, i32 y, i32 fontSize, Color color) {
    const i32 textWidth = MeasureText(text.c_str(), fontSize);
    ::DrawText(text.c_str(), x - textWidth / 2, y, fontSize, color);
}

void Renderer::drawText(std::string_view text, i32 x, i32 y, i32 fontSize, Color color) {
    std::array<char, 256> buf;
    if (const char* cstr = nullTerminate(text, buf)) {
        ::DrawText(cstr, x, y, fontSize, color);
    } else {
        const std::string ownedText{text};
        ::DrawText(ownedText.c_str(), x, y, fontSize, color);
    }
}

void Renderer::drawTextCentered(std::string_view text, i32 centerX, i32 y, i32 fontSize, Color color) {
    std::array<char, 256> buf;
    if (const char* cstr = nullTerminate(text, buf)) {
        const i32 textWidth = MeasureText(cstr, fontSize);
        ::DrawText(cstr, centerX - textWidth / 2, y, fontSize, color);
    } else {
        const std::string ownedText{text};
        const i32 textWidth = MeasureText(ownedText.c_str(), fontSize);
        ::DrawText(ownedText.c_str(), centerX - textWidth / 2, y, fontSize, color);
    }
}

void Renderer::drawText(const char* text, i32 x, i32 y, i32 fontSize, Color color) {
    ::DrawText(text, x, y, fontSize, color);
}

void Renderer::drawTextCentered(const char* text, i32 centerX, i32 y, i32 fontSize, Color color) {
    const i32 textWidth = MeasureText(text, fontSize);
    ::DrawText(text, centerX - textWidth / 2, y, fontSize, color);
}

void Renderer::drawText(
    Font font,
    std::string_view text,
    i32 x,
    i32 y,
    i32 fontSize,
    Color color,
    const f32 spacing)
{
    std::array<char, 256> buf;
    const char* cstr = nullTerminate(text, buf);
    const std::string fallback = cstr ? std::string{} : std::string{text};
    const char* ptr = cstr ? cstr : fallback.c_str();
    DrawTextEx(
        font,
        ptr,
        Vector2{static_cast<f32>(x), static_cast<f32>(y)},
        static_cast<f32>(fontSize),
        spacing,
        color
    );
}

void Renderer::drawTextCentered(
    Font font,
    std::string_view text,
    i32 centerX,
    i32 y,
    i32 fontSize,
    Color color,
    const f32 spacing)
{
    const i32 textWidth = measureText(font, text, fontSize, spacing);
    drawText(font, text, centerX - textWidth / 2, y, fontSize, color, spacing);
}

void Renderer::drawRect(i32 x, i32 y, i32 width, i32 height, Color color) {
    DrawRectangle(x, y, width, height, color);
}

void Renderer::drawRectLines(i32 x, i32 y, i32 width, i32 height, Color color) {
    DrawRectangleLines(x, y, width, height, color);
}

void Renderer::drawSprite(Texture2D texture, i32 x, i32 y, i32 width, i32 height) {
    DrawTexturePro(
        texture,
        {0.0f, 0.0f, static_cast<f32>(texture.width), static_cast<f32>(texture.height)},
        {static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(width), static_cast<f32>(height)},
        {0.0f, 0.0f},
        0.0f,
        WHITE
    );
}

void Renderer::drawFullscreen(Color color) {
    drawRect(0, 0, screenWidth(), screenHeight(), color);
}

void Renderer::drawFullscreenTexture(Texture2D texture, Color tint) {
    DrawTexturePro(
        texture,
        Rectangle{0.0f, 0.0f, static_cast<f32>(texture.width), static_cast<f32>(texture.height)},
        Rectangle{0.0f, 0.0f, static_cast<f32>(screenWidth()), static_cast<f32>(screenHeight())},
        Vector2{0.0f, 0.0f},
        0.0f,
        tint
    );
}

void Renderer::drawRenderTexture(Texture2D texture, i32 x, i32 y, Color tint) {
    DrawTextureRec(
        texture,
        // Negative height flips Y — OpenGL render textures are vertically
        // inverted relative to screen coordinates. This is Raylib convention.
        Rectangle{
            0.0f,
            0.0f,
            static_cast<f32>(texture.width),
            static_cast<f32>(-texture.height)
        },
        Vector2{
            static_cast<f32>(x),
            static_cast<f32>(y)
        },
        tint
    );
}

void Renderer::drawRenderTexture(Texture2D texture, i32 x, i32 y, i32 width, i32 height, Color tint) {
    DrawTexturePro(
        texture,
        Rectangle{
            0.0f,
            0.0f,
            static_cast<f32>(texture.width),
            static_cast<f32>(-texture.height)
        },
        Rectangle{
            static_cast<f32>(x),
            static_cast<f32>(y),
            static_cast<f32>(width),
            static_cast<f32>(height)
        },
        Vector2{0.0f, 0.0f},
        0.0f,
        tint
    );
}

i32 Renderer::measureText(std::string_view text, i32 fontSize) noexcept {
    std::array<char, 256> buf;
    if (const char* cstr = nullTerminate(text, buf)) {
        return MeasureText(cstr, fontSize);
    }
    const std::string ownedText{text};
    return MeasureText(ownedText.c_str(), fontSize);
}

i32 Renderer::measureText(Font font, std::string_view text, i32 fontSize, const f32 spacing) noexcept {
    std::array<char, 256> buf;
    const char* cstr = nullTerminate(text, buf);
    const std::string fallback = cstr ? std::string{} : std::string{text};
    const char* ptr = cstr ? cstr : fallback.c_str();
    return static_cast<i32>(MeasureTextEx(
        font,
        ptr,
        static_cast<f32>(fontSize),
        spacing
    ).x);
}

i32 Renderer::screenWidth() noexcept {
    return GetScreenWidth();
}

i32 Renderer::screenHeight() noexcept {
    return GetScreenHeight();
}

} // namespace biofuel::engine::graphics
