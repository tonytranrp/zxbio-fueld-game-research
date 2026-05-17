#pragma once

#include "game/gameplay/stages/TurnTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Stub placeholder: processes market prices and updates money.
/// Intended to apply seasonal fluctuation to fuel prices and deduct operating costs.
/// Currently a pass-through no-op; implementation is deferred to a future milestone.
struct EconomyUpdate {
    using input_type = TurnOutput;
    using output_type = TurnOutput;

    TurnOutput operator()(TurnOutput state) const noexcept;
};

} // namespace biofuel::game::gameplay::stages