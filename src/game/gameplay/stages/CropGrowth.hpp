#pragma once

#include "game/gameplay/stages/TurnTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Applies growth to crop tiles based on season.
/// Spring = fast growth (+2 age), Summer = normal growth (+1),
/// Fall = slow growth (+1 if mature enough), Winter = no growth.
struct CropGrowth {
    using input_type = TurnOutput;
    using output_type = TurnOutput;

    TurnOutput operator()(TurnOutput state) const noexcept;
};

} // namespace biofuel::game::gameplay::stages