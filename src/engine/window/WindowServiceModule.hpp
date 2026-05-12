#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(WindowService);
struct WindowServiceBackend {};
BIOFUEL_STATIC_SERVICE(WindowService, "service.window", WindowServiceBackend);
BIOFUEL_SERVICE_MODULE(WindowServiceModule, WindowService)
} // namespace biofuel::engine::runtime::typed

