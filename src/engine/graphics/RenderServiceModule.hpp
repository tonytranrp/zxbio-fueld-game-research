#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/ShaderManager.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(RenderService);
BIOFUEL_SERVICE_TAG(ShaderService);

struct RenderServiceBackend {
    [[nodiscard]] i32 screenWidth() const noexcept { return ::biofuel::engine::graphics::Renderer::screenWidth(); }
    [[nodiscard]] i32 screenHeight() const noexcept { return ::biofuel::engine::graphics::Renderer::screenHeight(); }
};

BIOFUEL_STATIC_SERVICE(RenderService, "service.render", RenderServiceBackend);
BIOFUEL_RUNTIME_SERVICE(ShaderService, "service.shader", ::biofuel::engine::graphics::ShaderManager,
    ::biofuel::engine::graphics::ShaderManager::instance());
BIOFUEL_SERVICE_MODULE(RenderServiceModule, RenderService, ShaderService)
} // namespace biofuel::engine::runtime::typed

