#include "JoinScreen.hpp"
#include "JoinScreenModule.hpp"
#include "game/screens/gameplay/GamePlayScreen.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/ui/ScreenManager.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::game::screens {

void JoinScreen::onEnter() {
    game::presentation::hands::ensureModelOnlyHandTracking();
    m_handOverlay.onEnter();
}

void JoinScreen::onExit() {
    m_handOverlay.onExit();
}

void JoinScreen::onUpdate(const f32 dt) {
    game::presentation::hands::ensureModelOnlyHandTracking();
    m_handOverlay.update(dt);
}

void JoinScreen::onRender() {
    using namespace ::biofuel::engine::graphics;

    ClearBackground(Color{14, 18, 26, 255});

    static constexpr std::string_view title = "Ready to enter Fuel Farm";
    static constexpr std::string_view hint = "Join when you are ready to continue.";
    static constexpr std::string_view label = "Join";

    const i32 screenWidth = Renderer::screenWidth();
    const i32 screenHeight = Renderer::screenHeight();
    const Rectangle button = joinButtonBounds();

    const i32 titleWidth = Renderer::measureText(title, TITLE_FONT_SIZE);
    const i32 hintWidth = Renderer::measureText(hint, HINT_FONT_SIZE);
    const i32 labelWidth = Renderer::measureText(label, BUTTON_FONT_SIZE);

    Renderer::drawText(
        title,
        (screenWidth - titleWidth) / 2,
        screenHeight / 2 - 122,
        TITLE_FONT_SIZE,
        Color{218, 202, 126, 255});
    Renderer::drawText(
        hint,
        (screenWidth - hintWidth) / 2,
        screenHeight / 2 - 74,
        HINT_FONT_SIZE,
        Color{150, 164, 178, 255});

    const bool hovered = CheckCollisionPointRec(GetMousePosition(), button);
    const Color fill = hovered ? Color{78, 126, 92, 255} : Color{48, 88, 68, 255};
    const Color border = hovered ? Color{232, 214, 130, 255} : Color{138, 154, 126, 255};

    Renderer::drawRect(
        static_cast<i32>(button.x),
        static_cast<i32>(button.y),
        static_cast<i32>(button.width),
        static_cast<i32>(button.height),
        fill);
    Renderer::drawRectLines(
        static_cast<i32>(button.x),
        static_cast<i32>(button.y),
        static_cast<i32>(button.width),
        static_cast<i32>(button.height),
        border);
    Renderer::drawText(
        label,
        static_cast<i32>(button.x) + (BUTTON_WIDTH - labelWidth) / 2,
        static_cast<i32>(button.y) + 13,
        BUTTON_FONT_SIZE,
        RAYWHITE);

    m_handOverlay.render();
}

void JoinScreen::onInput() {
    if (m_gameplayQueued) {
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        activateJoin();
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), joinButtonBounds())) {
        activateJoin();
    }
}

Rectangle JoinScreen::joinButtonBounds() const noexcept {
    const i32 screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth();
    const i32 screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight();
    return Rectangle{
        static_cast<f32>((screenWidth - BUTTON_WIDTH) / 2),
        static_cast<f32>(screenHeight / 2 - BUTTON_HEIGHT / 2),
        static_cast<f32>(BUTTON_WIDTH),
        static_cast<f32>(BUTTON_HEIGHT),
    };
}

void JoinScreen::activateJoin() {
    if (m_gameplayQueued) {
        return;
    }
    if (auto* sm = manager(); sm != nullptr && !sm->isTransitioning()) {
        m_gameplayQueued = true;
        sm->queueReplace<GamePlayScreen>();
    }
}

} // namespace biofuel::game::screens
