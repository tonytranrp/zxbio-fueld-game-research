#include "Render.hpp"

namespace biofuel::utils::render {

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
    const std::string ownedText{text};
    ::DrawText(ownedText.c_str(), x, y, fontSize, color);
}

void Renderer::drawTextCentered(std::string_view text, i32 centerX, i32 y, i32 fontSize, Color color) {
    const i32 textWidth = measureText(text, fontSize);
    const std::string ownedText{text};
    ::DrawText(ownedText.c_str(), centerX - textWidth / 2, y, fontSize, color);
}

void Renderer::drawText(const char* text, i32 x, i32 y, i32 fontSize, Color color) {
    ::DrawText(text, x, y, fontSize, color);
}

void Renderer::drawTextCentered(const char* text, i32 centerX, i32 y, i32 fontSize, Color color) {
    const i32 textWidth = MeasureText(text, fontSize);
    ::DrawText(text, centerX - textWidth / 2, y, fontSize, color);
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

void Renderer::drawRenderTexture(Texture2D texture, i32 x, i32 y, Color tint) {
    DrawTextureRec(
        texture,
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
    const std::string ownedText{text};
    return MeasureText(ownedText.c_str(), fontSize);
}

i32 Renderer::screenWidth() noexcept {
    return GetScreenWidth();
}

i32 Renderer::screenHeight() noexcept {
    return GetScreenHeight();
}

} // namespace biofuel::utils::render
