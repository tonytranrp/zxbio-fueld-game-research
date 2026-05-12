#pragma once

#include "engine/runtime/typed/ServiceDeclare.hpp"
#include "engine/fonts/FontUtils.hpp"

namespace biofuel::engine::runtime::typed {
BIOFUEL_SERVICE_TAG(FontService);
BIOFUEL_RUNTIME_SERVICE(FontService, "service.font", ::biofuel::engine::fonts::FontManager,
    ::biofuel::engine::fonts::FontManager::instance());
BIOFUEL_SERVICE_MODULE(FontServiceModule, FontService)
} // namespace biofuel::engine::runtime::typed

