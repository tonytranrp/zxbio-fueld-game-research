#include "MainMenuScreen.hpp"
#include "PausePopupScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include <raylib.h>
#include <cmath>
#include <memory>

namespace biofuel::ui::screens {

void MainMenuScreen::onEnter() {
    m_selected = 0;
    m_cooldown = 0.0f;
    m_titlePulse = 0.0f;
}

void MainMenuScreen::onUpdate(const f32 dt) {
    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
    }
    m_titlePulse += dt;
}

void MainMenuScreen::onRender() {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    // ---- Pulsing title ----
    const f32 pulse = (std::sin(m_titlePulse * 1.8f) * 0.5f + 0.5f) * 30.0f + 225.0f;
    const Color titleColor = {
        static_cast<u8>(pulse),
        static_cast<u8>(pulse * 0.85f),
        static_cast<u8>(pulse * 0.5f),
        255
    };

    static constexpr std::string_view title = "FUEL FARM";
    const i32 titleW = MeasureText(title.data(), TITLE_SIZE);
    Renderer::drawText(std::string{title}, (sw - titleW) / 2, sh / 6, TITLE_SIZE, titleColor);

    static constexpr std::string_view subtitle = "2D Pixel-Art Biofuel Management Sim";
    const i32 subW = MeasureText(subtitle.data(), SUBTITLE_SIZE);
    Renderer::drawText(std::string{subtitle}, (sw - subW) / 2, sh / 6 + 56, SUBTITLE_SIZE, GRAY);

    // ---- Menu ----
    const i32 menuStartY = sh / 2 - 40;
    utils::ui::renderVerticalMenu(
        std::span{s_items},
        m_selected,
        sw / 2,
        menuStartY,
        MENU_LAYOUT
    );

    // ---- Footer ----
    static constexpr std::string_view hint = "ESC - Pause Menu  |  \x1A\x1B - Navigate  |  ENTER - Select";
    constexpr i32 hintSize = 14;
    const i32 hintW = MeasureText(hint.data(), hintSize);
    Renderer::drawText(std::string{hint}, (sw - hintW) / 2, sh - 50, hintSize, {100, 100, 120, 255});

    static constexpr std::string_view version = "v0.1.0  |  Raylib 5.5  |  C++20";
    constexpr i32 verSize = 12;
    const i32 verW = MeasureText(version.data(), verSize);
    Renderer::drawText(std::string{version}, (sw - verW) / 2, sh - 30, verSize, {60, 60, 70, 255});
}

void MainMenuScreen::onInput() {
    // ESC → push pause popup
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (auto* sm = manager()) {
            sm->push(std::make_unique<PausePopupScreen>());
        }
        return;
    }

    // Keyboard navigation (via MenuHelper)
    if (utils::ui::navigateVerticalMenu(
            m_selected,
            static_cast<i32>(s_items.size()),
            m_cooldown,
            0.0f,  // dt unused — cooldown ticked in onUpdate
            std::span{s_items},
            MENU_LAYOUT)) {
        activateSelected();
        return;
    }

    // Mouse hit-testing
    const i32 sw = utils::render::Renderer::screenWidth();
    const i32 sh = utils::render::Renderer::screenHeight();
    const i32 menuStartY = sh / 2 - 40;

    const auto hit = utils::ui::hitTestVerticalMenu(
        std::span{s_items},
        sw / 2,
        menuStartY,
        MENU_LAYOUT
    );

    if (hit.hoveredIndex >= 0) {
        m_selected = hit.hoveredIndex;
        if (hit.clicked) {
            activateSelected();
        }
    }
}

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
