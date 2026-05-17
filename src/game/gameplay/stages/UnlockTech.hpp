#pragma once

#include "game/gameplay/stages/TechTreeTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Unlocks completed research and publishes TechUnlocked event.
/// Transforms InProgress research with turnsRemaining == 0 into Completed status.
struct UnlockTech {
    using input_type = TechTreeInput;
    using output_type = TechTreeOutput;

    TechTreeOutput operator()(TechTreeInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages