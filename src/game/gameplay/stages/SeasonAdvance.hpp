#pragma once

#include "game/gameplay/stages/TurnTypes.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

/// Advances the season and increments crop ages.
/// Wraps the logic from FarmState::advanceSeason() into a pipeline stage.
/// After Winter, the year increments and season wraps back to Spring.
struct SeasonAdvance {
    using input_type = TurnInput;
    using output_type = TurnOutput;

    [[nodiscard]] TurnOutput operator()(TurnInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages