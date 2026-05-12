#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::ui {
class ScreenManager;
}

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(ScreenService);
BIOFUEL_SERVICE_SPEC(ScreenService, "service.screen");

template<> struct ServiceModule<ScreenService> {
    using Service = ScreenService;
    using Backend = ::biofuel::engine::ui::ScreenManager;
    static Backend& get();
};
BIOFUEL_SERVICE_MODULE(ScreenServiceModule, ScreenService)
} // namespace biofuel::engine::runtime::typed
