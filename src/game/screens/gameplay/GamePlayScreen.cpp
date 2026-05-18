#include "GamePlayScreen.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/runtime/Runtime.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::game::screens {

// ------------------------------------------------------------------------------
// WASD Direction Helpers
// ------------------------------------------------------------------------------

[[nodiscard]] static presentation::sprites::Direction readWASDDirection() noexcept {
    using presentation::sprites::Direction;

    const bool w = IsKeyDown(KEY_W);
    const bool a = IsKeyDown(KEY_A);
    const bool s = IsKeyDown(KEY_S);
    const bool d = IsKeyDown(KEY_D);

    // Determine compound direction from key combination
    if (w && d)  return Direction::UpRight;
    if (w && a)  return Direction::UpLeft;
    if (s && d)  return Direction::DownRight;
    if (s && a)  return Direction::DownLeft;
    if (w)       return Direction::Up;
    if (s)       return Direction::Down;
    if (a)       return Direction::Left;
    if (d)       return Direction::Right;

    return Direction::Idle;
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void GamePlayScreen::onEnter() {
    ensureHandTrackingForModelOverlay();
    m_handOverlay.onEnter();

    // Initialize NekoCat at screen center
    const f32 screenW = static_cast<f32>(::biofuel::engine::graphics::Renderer::screenWidth());
    const f32 screenH = static_cast<f32>(::biofuel::engine::graphics::Renderer::screenHeight());
    constexpr f32 spriteSize = 32.0f * 2.0f; // 32px texture at 2x scale
    m_neko.setPosition(
        (screenW - spriteSize) / 2.0f,
        (screenH - spriteSize) / 2.0f);
    m_neko.load();
}

void GamePlayScreen::onExit() {
    m_neko.unload();
    m_handOverlay.onExit();
}

void GamePlayScreen::onUpdate(const f32 dt) {
    const presentation::sprites::Direction direction = readWASDDirection();
    m_neko.update(dt, direction);
    m_handOverlay.update(dt);
}

void GamePlayScreen::onRender() {
    using namespace ::biofuel::engine::graphics;

    ClearBackground(Color{18, 24, 28, 255});

    // Render NekoCat after background
    m_neko.render();

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

    m_handOverlay.render();
}

void GamePlayScreen::onInput() {}

void GamePlayScreen::ensureHandTrackingForModelOverlay() {
    game::presentation::hands::ensureModelOnlyHandTracking();
}

} // namespace biofuel::game::screens
