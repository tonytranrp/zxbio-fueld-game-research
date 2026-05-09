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

i32 Renderer::screenWidth() noexcept {
    return GetScreenWidth();
}

i32 Renderer::screenHeight() noexcept {
    return GetScreenHeight();
}

} // namespace biofuel::utils::render
