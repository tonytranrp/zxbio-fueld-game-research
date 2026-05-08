#pragma once

#include <raylib.h>
#include <string>

namespace biofuel::utils::render {

// ------------------------------------------------------------------------------
// Renderer - Raylib rendering utilities
// Wraps common draw calls with std::string support and C++ conveniences.
// ------------------------------------------------------------------------------
class Renderer {
public:
    static void beginFrame(Color clearColor = RAYWHITE);
    static void endFrame();

    static void drawText(const std::string& text, int x, int y, int fontSize, Color color);
    static void drawTextCentered(const std::string& text, int x, int y, int fontSize, Color color);
    static void drawRect(int x, int y, int width, int height, Color color);
    static void drawRectLines(int x, int y, int width, int height, Color color);
    static void drawSprite(Texture2D texture, int x, int y, int width, int height);

    [[nodiscard]] static int screenWidth();
    [[nodiscard]] static int screenHeight();
};

} // namespace biofuel::utils::render
