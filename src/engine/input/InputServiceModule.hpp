#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/input/InputSystem.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(InputService);

struct InputServiceBackend {
    void poll() const noexcept { engine::input::InputSystem::poll(); }
};

BIOFUEL_STATIC_SERVICE(InputService, "service.input", InputServiceBackend);
BIOFUEL_SERVICE_MODULE(InputServiceModule, InputService)
} // namespace biofuel::engine::runtime::typed

