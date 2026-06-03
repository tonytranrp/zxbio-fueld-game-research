#pragma once

// ------------------------------------------------------------------------------
// MainMenuScreen render-element executors
//
// These RenderElementExecutor specializations drive each layer of the main menu
// render pipeline (declared in MainMenuScreenModule.hpp). They live in a header
// so the specializations are visible at the point RenderPipeline<MainMenuScreen>
// is instantiated (MainMenuScreen::onRender). RenderElementExecutor is a friend
// of MainMenuScreen, so the executors may read its private state directly.
// ------------------------------------------------------------------------------

#include "game/screens/main_menu/MainMenuScreen.hpp"
#include "game/screens/main_menu/MainMenuScreenModule.hpp"
#include "game/presentation/widgets/MenuHelper.hpp"
#include "engine/core/Types.hpp"
#include "engine/ui/typed/RenderContext.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/shaders/ProceduralBackdropModule.hpp"
#include <cmath>
#include <span>
#include <string_view>

namespace biofuel::engine::ui::typed {

// Local aliases keep the executor bodies readable; without them every constant
// reference expands to ::biofuel::game::screens::MainMenuScreen::FOO.
namespace mms = ::biofuel::game::screens;
using MMS = mms::MainMenuScreen;

template<>
struct RenderElementExecutor<mainmenu::BackdropElement, MMS> {
    static void render(MMS& screen, RenderContext& context) {
        screen.m_backdrop.setFloat(
            ::biofuel::engine::graphics::shader::ProceduralBackdropModule::UNIFORM_UDIMENSION_SHIFT,
            screen.m_dimensionShift
        );
        screen.m_backdrop.setFloat("uIdleDim", screen.m_idleTransitionDim);
        screen.m_cameraComponent.apply(screen.m_backdrop.shader());
        screen.m_backdrop.render(context.transitionAlpha);
    }
};

template<>
struct RenderElementExecutor<mainmenu::TitleBlockElement, MMS> {
    static void render(MMS& screen, RenderContext&) {
        using namespace ::biofuel::engine::graphics;

        const f32 titleDismiss = screen.m_dismiss.progress(mms::UIDismissState::ELEM_TITLE);
        if (titleDismiss >= 1.0f) {
            return;
        }

        const i32 titleSlideX = -static_cast<i32>(static_cast<f32>(MMS::DISMISS_SLIDE_LEFT) * titleDismiss);
        const f32 titleFadeMul = 1.0f - titleDismiss;

        if (screen.m_introPhase >= mms::IntroPhase::TitleFade) {
            const f32 pulse = (std::sin(screen.m_titlePulse * MMS::TITLE_PULSE_SPEED) * 0.5f + 0.5f)
                * MMS::TITLE_PULSE_RANGE + MMS::TITLE_PULSE_MIN;
            const u8 fadeAlpha = static_cast<u8>(screen.m_titleFade.alpha() * 255.0f * titleFadeMul);
            const Color titleColor = {
                static_cast<u8>(pulse),
                static_cast<u8>(pulse * 0.85f),
                static_cast<u8>(pulse * 0.5f),
                fadeAlpha
            };

            static constexpr std::string_view titleStr = "FUEL FARM";
            Renderer::drawText(titleStr, MMS::TITLE_X + titleSlideX, MMS::TITLE_Y, MMS::TITLE_FONT_SIZE, titleColor);
        }

        if (screen.m_introPhase >= mms::IntroPhase::SubtitleFade) {
            const u8 alpha = static_cast<u8>(screen.m_subtitleFade.alpha() * 255.0f * titleFadeMul);
            const Color subColor = {
                MMS::COLOR_GRAY_DIM.r,
                MMS::COLOR_GRAY_DIM.g,
                MMS::COLOR_GRAY_DIM.b,
                alpha
            };
            static constexpr std::string_view subtitleStr = "2D Pixel-Art Biofuel Management Sim";
            Renderer::drawText(
                subtitleStr,
                MMS::TITLE_X + titleSlideX,
                MMS::TITLE_Y + MMS::TITLE_FONT_SIZE + MMS::TITLE_SUBTITLE_GAP,
                MMS::SUBTITLE_FONT_SIZE,
                subColor
            );
        }
    }
};

template<>
struct RenderElementExecutor<mainmenu::HintTextElement, MMS> {
    static void render(MMS& screen, RenderContext&) {
        using namespace ::biofuel::engine::graphics;

        const f32 hintsDismiss = screen.m_dismiss.progress(mms::UIDismissState::ELEM_HINTS);
        if (screen.m_introPhase < mms::IntroPhase::HintsFade || hintsDismiss >= 1.0f) {
            return;
        }

        const i32 hintsSlideX = -static_cast<i32>(static_cast<f32>(MMS::DISMISS_SLIDE_LEFT) * hintsDismiss);
        const f32 hintsFadeMul = 1.0f - hintsDismiss;
        const u8 alpha = static_cast<u8>(screen.m_hintsFade.alpha() * 255.0f * hintsFadeMul);
        const Color hintColor = {
            MMS::COLOR_GRAY_DIM.r,
            MMS::COLOR_GRAY_DIM.g,
            MMS::COLOR_GRAY_DIM.b,
            alpha
        };
        static constexpr std::string_view hintsStr = "ESC Pause  |  LEFT / RIGHT Navigate  |  ENTER Select";
        const i32 subtitleY = MMS::TITLE_Y + MMS::TITLE_FONT_SIZE + MMS::TITLE_SUBTITLE_GAP;
        const i32 hintsY = subtitleY + MMS::SUBTITLE_FONT_SIZE + MMS::SUBTITLE_HINTS_GAP;
        Renderer::drawText(hintsStr, MMS::TITLE_X + hintsSlideX, hintsY, MMS::HINTS_FONT_SIZE, hintColor);
    }
};

template<>
struct RenderElementExecutor<mainmenu::HorizontalMenuElement, MMS> {
    static void render(MMS& screen, RenderContext& context) {
        const f32 menuDismiss = screen.m_dismiss.progress(mms::UIDismissState::ELEM_MENU);
        if (screen.m_introPhase < mms::IntroPhase::MenuFade || menuDismiss >= 1.0f) {
            return;
        }

        const i32 menuSlideY = static_cast<i32>(static_cast<f32>(MMS::DISMISS_SLIDE_DOWN) * menuDismiss);
        const f32 menuFadeMul = 1.0f - menuDismiss;

        auto layout = MMS::MENU_LAYOUT;
        const u8 menuAlpha = static_cast<u8>(screen.m_menuFade.alpha() * 255.0f * menuFadeMul);
        layout.colorSelected.a = menuAlpha;
        layout.colorSelectedGlow.a = menuAlpha;
        layout.colorSide.a = menuAlpha;
        layout.colorSideLocked.a = menuAlpha;
        layout.colorLockedLabel.a = menuAlpha;

        game::presentation::widgets::renderHorizontalCarousel(
            std::span{MMS::s_items},
            screen.m_selected,
            screen.m_hovered,
            context.screenWidth / 2,
            context.screenHeight - MMS::MENU_BAR_Y_OFFSET + menuSlideY,
            layout,
            screen.m_menuSlide.motion(),
            screen.m_menuFxTime
        );
    }
};

template<>
struct RenderElementExecutor<mainmenu::FooterTextElement, MMS> {
    static void render(MMS& screen, RenderContext& context) {
        using namespace ::biofuel::engine::graphics;

        const f32 footerDismiss = screen.m_dismiss.progress(mms::UIDismissState::ELEM_FOOTER);
        if (footerDismiss >= 1.0f) {
            return;
        }

        const i32 footerSlideY = static_cast<i32>(static_cast<f32>(MMS::DISMISS_SLIDE_DOWN) * footerDismiss);
        const f32 footerFadeMul = 1.0f - footerDismiss;
        static constexpr std::string_view versionStr = "v0.1.0 | Raylib 5.5 | C++20";
        const i32 versionX = context.screenWidth - MMS::FOOTER_MARGIN_X;
        const i32 versionY = context.screenHeight - MMS::FOOTER_BOTTOM_OFFSET + footerSlideY;
        const Color footerColor = {
            MMS::COLOR_VERSION.r,
            MMS::COLOR_VERSION.g,
            MMS::COLOR_VERSION.b,
            static_cast<u8>(static_cast<f32>(MMS::COLOR_VERSION.a) * footerFadeMul)
        };
        Renderer::drawText(
            versionStr,
            versionX - Renderer::measureText(versionStr, MMS::FOOTER_FONT_SIZE),
            versionY,
            MMS::FOOTER_FONT_SIZE,
            footerColor
        );
    }
};

} // namespace biofuel::engine::ui::typed
