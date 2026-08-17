#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/input/InputSystem.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(InputService);

struct InputServiceBackend {
    void poll() noexcept { m_keyPressedThisPoll = engine::input::InputSystem::poll(); }
    [[nodiscard]] bool keyPressedThisPoll() const noexcept { return m_keyPressedThisPoll; }

private:
    bool m_keyPressedThisPoll = false;
};

BIOFUEL_STATIC_SERVICE(InputService, "service.input", InputServiceBackend);
BIOFUEL_SERVICE_MODULE(InputServiceModule, InputService)
} // namespace biofuel::engine::runtime::typed
