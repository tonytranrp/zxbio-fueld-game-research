#include "LoadingScreen.hpp"
#include "MainMenuScreen.hpp"
#include "UI/ScreenManager.hpp"
#include "Utils/render/Render.hpp"
#include "Utils/render/ShaderManager.hpp"
#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"
#include "Utils/render/Shader/CrossfadeModule.hpp"
#include "Data/Data.hpp"
#include "AnimationController/AnimationManager.hpp"
#include <raylib.h>

namespace biofuel::ui::screens {

// ------------------------------------------------------------------------------
// Task Setup
// ------------------------------------------------------------------------------

void LoadingScreen::buildTasks() {
    // Shader compilation — the heavy part. Runs once, cached by GPU driver.
    m_tasks.add({
        .name = "Compiling shaders...",
        .weight = 1.0f,
        .work = []() {
            using namespace utils::render::shader;
            auto& sm = utils::render::ShaderManager::instance();
            sm.loadFromMemory(
                BlurHModule::NAME.data(),
                BlurHModule::VERTEX_SOURCE,
                BlurHModule::FRAGMENT_SOURCE.data()
            );
            sm.loadFromMemory(
                BlurVModule::NAME.data(),
                BlurVModule::VERTEX_SOURCE,
                BlurVModule::FRAGMENT_SOURCE.data()
            );
            sm.loadFromMemory(
                CrossfadeModule::NAME.data(),
                CrossfadeModule::VERTEX_SOURCE,
                CrossfadeModule::FRAGMENT_SOURCE.data()
            );
        }
    });

    // Future: add more tasks as init grows
    // m_tasks.add({ .name = "Loading assets...", .weight = 1.0f, .work = ... });
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

void LoadingScreen::onEnter() {
    buildTasks();

    if (m_tasks.totalTasks() == 0) {
        m_tasksDone = true;
        m_actualProgress = 1.0f;
        m_allowSkip = true;
    }
}

void LoadingScreen::onUpdate(const f32 dt) {
    m_elapsed += dt;

    // Process one task per frame
    if (!m_tasks.isDone()) {
        m_tasks.processNext();
        m_actualProgress = m_tasks.progress();

        if (m_tasks.isDone()) {
            m_tasksDone = true;
            m_actualProgress = 1.0f;
            m_allowSkip = true;
        }
    }

    // Smooth display progress toward actual
    if (m_displayProgress < m_actualProgress) {
        m_displayProgress += (m_actualProgress - m_displayProgress) * PROGRESS_LERP_SPEED * dt;
        if (m_displayProgress > 0.995f && m_actualProgress >= 1.0f) {
            m_displayProgress = 1.0f;
        }
    }

    // Transition when conditions met
    if (m_tasksDone && m_displayProgress >= 1.0f && m_elapsed >= MIN_DISPLAY_SECONDS) {
        transitionToNext();
    }
}

void LoadingScreen::onRender() {
    using namespace utils::render;

    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();

    // Title
    static constexpr std::string_view TITLE = "FUEL FARM";
    const Color TITLE_COLOR = {200, 155, 60, 255};
    const i32 titleW = MeasureText(TITLE.data(), TITLE_SIZE);
    Renderer::drawText(
        std::string{TITLE},
        (sw - titleW) / 2,
        sh / 3 - TITLE_SIZE / 2,
        TITLE_SIZE,
        TITLE_COLOR
    );

    // Loading bar outline
    const i32 barX = (sw - BAR_WIDTH) / 2;
    const i32 barY = sh / 2 - BAR_HEIGHT / 2;
    const Color BAR_OUTLINE = {80, 80, 100, 255};
    Renderer::drawRectLines(barX, barY, BAR_WIDTH, BAR_HEIGHT, BAR_OUTLINE);

    // Loading bar fill
    const i32 fillW = static_cast<i32>((BAR_WIDTH - 4) * m_displayProgress);
    if (fillW > 0) {
        const Color BAR_FILL = {200, 155, 60, 255};
        Renderer::drawRect(barX + 2, barY + 2, fillW, BAR_HEIGHT - 4, BAR_FILL);
    }

    // Status text
    const std::string status = m_tasks.isDone() ? "Ready." : m_tasks.currentName();
    const i32 statusW = MeasureText(status.c_str(), STATUS_SIZE);
    Renderer::drawText(
        status,
        (sw - statusW) / 2,
        barY + BAR_HEIGHT + 16,
        STATUS_SIZE,
        LIGHTGRAY
    );

    // Animated dots: "Loading" + 0..3 dots cycling every 0.5s
    const i32 dotCount = static_cast<i32>(m_elapsed / 0.5f) % 4;
    const std::string dots(dotCount, '.');
    const std::string loadingText = "Loading" + dots;
    const i32 loadingW = MeasureText(loadingText.c_str(), STATUS_SIZE);
    const Color LOADING_COLOR = {100, 100, 120, 255};
    Renderer::drawText(
        loadingText,
        (sw - loadingW) / 2,
        barY + BAR_HEIGHT + 38,
        STATUS_SIZE,
        LOADING_COLOR
    );

    // Skip hint — only shown when tasks are done and waiting on timer
    if (m_allowSkip && m_elapsed < MIN_DISPLAY_SECONDS) {
        static constexpr std::string_view SKIP_HINT = "Press any key to continue...";
        constexpr i32 HINT_SIZE = 14;
        const i32 hintW = MeasureText(SKIP_HINT.data(), HINT_SIZE);
        const Color HINT_COLOR = {80, 80, 100, 255};
        Renderer::drawText(
            std::string{SKIP_HINT},
            (sw - hintW) / 2,
            barY + BAR_HEIGHT + 64,
            HINT_SIZE,
            HINT_COLOR
        );
    }

    // Footer
    static constexpr std::string_view FOOTER = "v0.1.0  |  C++20  |  Raylib 5.5";
    constexpr i32 FOOTER_SIZE = 12;
    const i32 footerW = MeasureText(FOOTER.data(), FOOTER_SIZE);
    const Color FOOTER_COLOR = {60, 60, 70, 255};
    Renderer::drawText(
        std::string{FOOTER},
        (sw - footerW) / 2,
        sh - 30,
        FOOTER_SIZE,
        FOOTER_COLOR
    );
}

void LoadingScreen::onInput() {
    // Skip remaining wait time when tasks are done
    if (m_allowSkip) {
        if (GetKeyPressed() != 0 ||
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            transitionToNext();
        }
    }
}

// ------------------------------------------------------------------------------
// Transition
// ------------------------------------------------------------------------------

void LoadingScreen::transitionToNext() {
    if (auto* sm = manager()) {
        sm->queueReplace(std::make_unique<MainMenuScreen>());
    }
}

} // namespace biofuel::ui::screens
