#pragma once

#include "Core/Types.hpp"
#include <raylib.h>
#include <string>

namespace biofuel::utils::render {

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
    static void drawRect(i32 x, i32 y, i32 width, i32 height, Color color);
    static void drawRectLines(i32 x, i32 y, i32 width, i32 height, Color color);
    static void drawSprite(Texture2D texture, i32 x, i32 y, i32 width, i32 height);

    [[nodiscard]] static i32 screenWidth() noexcept;
    [[nodiscard]] static i32 screenHeight() noexcept;
};

} // namespace biofuel::utils::render
