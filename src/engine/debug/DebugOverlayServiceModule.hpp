#pragma once

#include "engine/debug/DebugOverlayService.hpp"
#include "engine/runtime/typed/ServiceDeclare.hpp"

namespace biofuel::engine::runtime::typed {

BIOFUEL_SERVICE_TAG(DebugOverlayService);
BIOFUEL_STATIC_SERVICE(DebugOverlayService, "service.debug_overlay", ::biofuel::engine::debug::DebugOverlayService);
BIOFUEL_SERVICE_MODULE(DebugOverlayServiceModule, DebugOverlayService)

} // namespace biofuel::engine::runtime::typed
