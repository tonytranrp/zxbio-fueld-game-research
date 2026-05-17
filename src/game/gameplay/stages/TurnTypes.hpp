#pragma once

#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

/// Input for the turn processing pipeline.
/// Wraps a FarmState snapshot to pass through SeasonAdvance → CropGrowth → EcologyUpdate → EconomyUpdate.
struct TurnInput {
    FarmState farmState;
};

/// Output of the turn processing pipeline.
/// Contains the updated FarmState after all turn stages have been applied.
struct TurnOutput {
    FarmState farmState;
};

} // namespace biofuel::game::gameplay::stages