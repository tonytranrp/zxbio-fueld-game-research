#include "MainMenuScreen.hpp"
#include "PausePopupScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include <raylib.h>
#include <cmath>
#include <memory>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void MainMenuScreen::onEnter() {
    m_selected  = 0;
    m_cooldown  = 0.0f;
    m_titlePulse = 0.0f;

#ifdef BIOFUEL_DEV_STARTUP_PAUSE_POPUP
    if (auto* sm = manager()) {
        sm->queuePush(std::make_unique<PausePopupScreen>());
    }
#endif
}

void MainMenuScreen::onUpdate(const f32 dt) {
    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
    }
    m_titlePulse += dt;
}

// ------------------------------------------------------------------------------
// Rendering
// ------------------------------------------------------------------------------

void MainMenuScreen::onRender() {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    // ---- Pulsing title at top-left ----
    const f32 pulse = (std::sin(m_titlePulse * TITLE_PULSE_SPEED) * 0.5f + 0.5f)
                      * TITLE_PULSE_RANGE + TITLE_PULSE_MIN;
    const Color titleColor = {
        static_cast<u8>(pulse),
        static_cast<u8>(pulse * 0.85f),
        static_cast<u8>(pulse * 0.5f),
        255
    };

    static constexpr std::string_view titleStr = "FUEL FARM";
    Renderer::drawText(
        std::string{titleStr},
        TITLE_X,
        TITLE_Y,
        TITLE_FONT_SIZE,
        titleColor
    );

    // ---- Subtitle below title ----
    const i32 subtitleY = TITLE_Y + TITLE_FONT_SIZE + TITLE_SUBTITLE_GAP;
    static constexpr std::string_view subtitleStr = "2D Pixel-Art Biofuel Management Sim";
    Renderer::drawText(
        std::string{subtitleStr},
        TITLE_X,
        subtitleY,
        SUBTITLE_FONT_SIZE,
        COLOR_GRAY_DIM
    );

    // ---- Controls hint below subtitle ----
    const i32 hintsY = subtitleY + SUBTITLE_FONT_SIZE + SUBTITLE_HINTS_GAP;
    static constexpr std::string_view hintsStr = "ESC Pause  |  \x11\x10 Navigate  |  ENTER Select";
    Renderer::drawText(
        std::string{hintsStr},
        TITLE_X,
        hintsY,
        HINTS_FONT_SIZE,
        COLOR_GRAY_DIM
    );

    // ---- Horizontal menu bar at bottom-middle ----
    const i32 barY = sh - MENU_BAR_Y_OFFSET;
    renderMenuBar(sw / 2, barY);

    // ---- Version footer ----
    static constexpr std::string_view versionStr = "v0.1.0 | Raylib 5.5 | C++20";
    // Bottom-right: right-aligned
    const i32 versionX = sw - FOOTER_MARGIN_X;
    const i32 versionY = sh - FOOTER_BOTTOM_OFFSET;
    Renderer::drawText(
        std::string{versionStr},
        versionX - MeasureText(versionStr.data(), FOOTER_FONT_SIZE),
        versionY,
        FOOTER_FONT_SIZE,
        COLOR_VERSION
    );

}

// ------------------------------------------------------------------------------
// Menu bar — horizontal row of 3 items centered at bottom of screen
// Selected item: gold with underline bar and arrow indicators
// Unselected: dim gray. Locked: dark gray with "(locked)" label.
// ------------------------------------------------------------------------------

void MainMenuScreen::renderMenuBar(const i32 centerX, const i32 barY) const {
    using namespace utils::render;

    const i32 itemCount = static_cast<i32>(s_items.size());

    // Calculate starting X so items are evenly spaced and centered
    // Item centers: centerX + (i - 1) * MENU_ITEM_SPACING for 3 items
    // General: centerX + (i - (itemCount-1)/2) * MENU_ITEM_SPACING
    const f32 halfSpan = (itemCount - 1) * MENU_ITEM_SPACING * 0.5f;
    const i32 startX = static_cast<i32>(centerX - halfSpan);

    for (i32 i = 0; i < itemCount; ++i) {
        const i32 itemX = startX + i * MENU_ITEM_SPACING;
        const bool isSelected = (i == m_selected);
        const bool isLocked    = s_items[i].locked;

        // Choose color based on state
        Color itemColor = COLOR_GRAY_DIM;
        if (isLocked) {
            itemColor = COLOR_GRAY_LOCKED;
        } else if (isSelected) {
            itemColor = COLOR_GOLD;
        }

        // Draw item label centered at itemX
        Renderer::drawTextCentered(
            std::string{s_items[i].label},
            itemX,
            barY,
            MENU_FONT_SIZE,
            itemColor
        );

        // Draw "(locked)" label below locked items
        if (isLocked) {
            Renderer::drawTextCentered(
                "(locked)",
                itemX,
                barY + MENU_FONT_SIZE + 4,
                HINTS_FONT_SIZE,
                COLOR_GRAY_LOCKED_LABEL
            );
        }

        // Draw selection indicators for the selected item
        if (isSelected && !isLocked) {
            // Gold underline bar beneath the selected item
            const i32 underlineX = itemX - UNDERLINE_WIDTH / 2;
            const i32 underlineY = barY + MENU_FONT_SIZE + UNDERLINE_OFFSET_Y;
            Renderer::drawRect(
                underlineX,
                underlineY,
                UNDERLINE_WIDTH,
                UNDERLINE_HEIGHT,
                COLOR_GOLD
            );

            // Arrow indicators on left and right of selected item
            static constexpr std::string_view arrowLeft  = "\x11"; // left arrow
            static constexpr std::string_view arrowRight = "\x10"; // right arrow

            // Measure label width to position arrows just outside the text
            const i32 labelW = MeasureText(s_items[m_selected].label.data(), MENU_FONT_SIZE);
            const i32 arrowLeftX  = itemX - labelW / 2 - ARROW_OFFSET_X - 8;
            const i32 arrowRightX = itemX + labelW / 2 + ARROW_OFFSET_X;

            Renderer::drawTextCentered(
                std::string{arrowLeft},
                arrowLeftX,
                barY,
                ARROW_FONT_SIZE,
                COLOR_GOLD_DIM
            );
            Renderer::drawTextCentered(
                std::string{arrowRight},
                arrowRightX,
                barY,
                ARROW_FONT_SIZE,
                COLOR_GOLD_DIM
            );
        }
    }
}

// ------------------------------------------------------------------------------
// Input handling
// ------------------------------------------------------------------------------

void MainMenuScreen::onInput() {
    using namespace utils::render;

    // ESC -> push pause popup
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (auto* sm = manager()) {
            sm->push(std::make_unique<PausePopupScreen>());
        }
        return;
    }

    // Keyboard navigation
    if (navigateMenu()) {
        activateSelected();
        return;
    }

    // Mouse hit-testing on menu bar items
    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();
    const i32 barY = sh - MENU_BAR_Y_OFFSET;
    const i32 hitIndex = hitTestMenuBar(sw / 2, barY);
    if (hitIndex >= 0) {
        m_selected = hitIndex;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            activateSelected();
        }
    }
}

// ------------------------------------------------------------------------------
// Horizontal menu navigation — LEFT/A decrement, RIGHT/D increment,
// wrapping around and skipping locked items. ENTER/SPACE to activate.
// ------------------------------------------------------------------------------

bool MainMenuScreen::navigateMenu() {
    const i32 itemCount = static_cast<i32>(s_items.size());

    // Enter/Space to activate
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (!s_items[m_selected].locked) {
            return true;
        }
        return false;
    }

    if (m_cooldown > 0.0f) {
        return false;
    }

    i32 dir = 0;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        dir = -1;
    } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        dir = 1;
    } else {
        return false;
    }

    // Wrap around and skip locked items
    do {
        m_selected = (m_selected + dir + itemCount) % itemCount;
    } while (s_items[m_selected].locked);

    m_cooldown = KEY_REPEAT_DELAY;
    return false;
}

// ------------------------------------------------------------------------------
// Mouse hit-testing for menu bar items
// ------------------------------------------------------------------------------

i32 MainMenuScreen::hitTestMenuBar(const i32 centerX, const i32 barY) const {
    using namespace utils::render;

    const i32 itemCount = static_cast<i32>(s_items.size());
    const f32 halfSpan = (itemCount - 1) * MENU_ITEM_SPACING * 0.5f;
    const i32 startX = static_cast<i32>(centerX - halfSpan);
    const Vector2 mouse = GetMousePosition();

    static constexpr i32 HIT_PAD_X = 20;
    static constexpr i32 HIT_PAD_Y = 10;

    for (i32 i = 0; i < itemCount; ++i) {
        if (s_items[i].locked) {
            continue;
        }

        const i32 itemX = startX + i * MENU_ITEM_SPACING;
        const i32 labelW = MeasureText(s_items[i].label.data(), MENU_FONT_SIZE);

        const Rectangle hitbox = {
            static_cast<f32>(itemX - labelW / 2 - HIT_PAD_X),
            static_cast<f32>(barY - HIT_PAD_Y),
            static_cast<f32>(labelW + HIT_PAD_X * 2),
            static_cast<f32>(MENU_FONT_SIZE + HIT_PAD_Y * 2)
        };

        if (CheckCollisionPointRec(mouse, hitbox)) {
            return i;
        }
    }

    return -1;
}

// ------------------------------------------------------------------------------
// Activation logic
// ------------------------------------------------------------------------------

void MainMenuScreen::activateSelected() {
    switch (m_selected) {
    case 0: // New Game — placeholder until game screen exists
        break;
    case 2: // Quit
        if (auto* sm = manager()) {
            sm->requestQuit();
        }
        break;
    default:
        break;
    }
}

bool MainMenuScreen::isLocked(const i32 index) const {
    return s_items[index].locked;
}

} // namespace biofuel::ui::screens