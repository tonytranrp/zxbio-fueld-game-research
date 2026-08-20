#include "BevyDemoScreen.hpp"
#include "BevyDemoScreenModule.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/bevy/BevyRenderService.hpp"
#include "engine/graphics/Render.hpp"
#include <raylib.h>

namespace biofuel::engine::ui::typed {

bool bevydemo::BevyFrameTag::visible(const ::biofuel::game::screens::BevyDemoScreen&) noexcept {
    return true;
}

Texture2D bevydemo::BevyFrameTag::frame(const ::biofuel::game::screens::BevyDemoScreen&) noexcept {
    return ::biofuel::engine::runtime::Runtime::bevyRenderer().getFrameTexture();
}

} // namespace biofuel::engine::ui::typed

namespace biofuel::game::screens {

void BevyDemoScreen::onUpdate([[maybe_unused]] f32 dt) {
    // BevyRenderService is pumped centrally from Application::update(), the
    // same "always pumped, screen doesn't drive it" shape VideoScreen uses
    // for VideoManager -- nothing screen-specific to do here.
}

void BevyDemoScreen::onRender() {
    ::biofuel::engine::ui::typed::RenderContext context{
        .manager = manager(),
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = screenId(),
        .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
        .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
        .transitionAlpha = transitionAlpha(),
        .frameTime = GetFrameTime(),
    };
    ::biofuel::engine::ui::typed::RenderPipeline<BevyDemoScreen>::render(*this, context);
}

void BevyDemoScreen::onInput() {
    const Vector2 delta = GetMouseDelta();
    ::biofuel::engine::runtime::Runtime::bevyRenderer().addLookDelta(delta.x, delta.y);

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (auto* sm = manager()) {
            sm->pop();
        }
    }
}

} // namespace biofuel::game::screens
