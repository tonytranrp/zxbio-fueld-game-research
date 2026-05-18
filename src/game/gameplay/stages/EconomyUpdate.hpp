#pragma once

#include "game/gameplay/stages/PassThrough.hpp"
#include "game/gameplay/stages/TurnTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Stub placeholder: processes market prices and updates money.
/// Intended to apply seasonal fluctuation to fuel prices and deduct operating costs.
/// Currently a pass-through no-op; implementation is deferred to a future milestone.
using EconomyUpdate = PassThrough<TurnOutput>;

} // namespace biofuel::game::gameplay::stages
