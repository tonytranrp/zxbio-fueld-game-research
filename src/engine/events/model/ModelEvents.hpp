#pragma once

#include "engine/core/Types.hpp"
#include <string>

namespace biofuel::engine::events::model {

struct ModelSetStateEvent {
    u64 instanceId = 0;
    std::string stateName;
    f32 transitionSeconds = 0.0f;
};

struct ModelPlayActionEvent {
    u64 instanceId = 0;
    std::string actionState;
    f32 transitionSeconds = 0.0f;
};

} // namespace biofuel::engine::events::model
