#include "Render.hpp"

namespace biofuel::utils::render {

void Renderer::beginFrame(Color clearColor) {
    BeginDrawing();
    ClearBackground(clearColor);
}

void Renderer::endFrame() {
    EndDrawing();
}

void Renderer::drawText(const std::string& text, int x, int y, int fontSize, Color color) {
    ::DrawText(text.c_str(), x, y, fontSize, color);
}

void Renderer::drawTextCentered(const std::string& text, int x, int y, int fontSize, Color color) {
    const int textWidth = MeasureText(text.c_str(), fontSize);
    ::DrawText(text.c_str(), x - textWidth / 2, y, fontSize, color);
}

void Renderer::drawRect(int x, int y, int width, int height, Color color) {
    DrawRectangle(x, y, width, height, color);
}

void Renderer::drawRectLines(int x, int y, int width, int height, Color color) {
    DrawRectangleLines(x, y, width, height, color);
}

void Renderer::drawSprite(Texture2D texture, int x, int y, int width, int height) {
    DrawTexturePro(
        texture,
        {0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)},
        {static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)},
        {0.0f, 0.0f},
        0.0f,
        WHITE
    );
}

int Renderer::screenWidth() {
    return GetScreenWidth();
}

int Renderer::screenHeight() {
    return GetScreenHeight();
}

} // namespace biofuel::utils::render
