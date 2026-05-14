#include "GamePlayScreen.hpp"
#include "engine/graphics/Render.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::game::screens {

void GamePlayScreen::onUpdate(const f32) {}

void GamePlayScreen::onRender() {
    using namespace ::biofuel::engine::graphics;

    ClearBackground(Color{18, 24, 28, 255});

    static constexpr std::string_view title = "FUEL FARM";
    static constexpr std::string_view message = "GamePlay screen will be implemented later.";
    static constexpr i32 titleSize = 34;
    static constexpr i32 messageSize = 20;

    const i32 screenWidth = Renderer::screenWidth();
    const i32 screenHeight = Renderer::screenHeight();
    const i32 titleWidth = Renderer::measureText(title, titleSize);
    const i32 messageWidth = Renderer::measureText(message, messageSize);

    Renderer::drawText(
        title,
        (screenWidth - titleWidth) / 2,
        screenHeight / 2 - 58,
        titleSize,
        Color{215, 190, 96, 255});
    Renderer::drawText(
        message,
        (screenWidth - messageWidth) / 2,
        screenHeight / 2,
        messageSize,
        Color{220, 228, 232, 255});
}

void GamePlayScreen::onInput() {}

} // namespace biofuel::game::screens
