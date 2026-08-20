#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::bevy {
class BevyRenderService; // forward-declare only -- keeps the cxx-bridge header out of Runtime.hpp consumers
}

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(BevyRendererService);
BIOFUEL_SERVICE_SPEC(BevyRendererService, "service.bevy_renderer");

template<> struct ServiceModule<BevyRendererService> {
    using Service = BevyRendererService;
    using Backend = ::biofuel::engine::bevy::BevyRenderService;
    static Backend& get();
};
BIOFUEL_SERVICE_MODULE(BevyRenderServiceModule, BevyRendererService)
} // namespace biofuel::engine::runtime::typed
