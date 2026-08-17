#pragma once

#include "game/gameplay/stages/TurnTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Applies seasonal bonus growth to crop tiles on top of the base +1 aging
/// already applied per turn by SeasonAdvance (via FarmState::advanceSeason()).
/// Spring = fast growth (+2 bonus), Summer = normal growth (+1),
/// Fall = slow growth (+1 if mature enough), Winter = no bonus.
struct CropGrowth {
    static constexpr std::string_view name = "CropGrowth";

    using input_type = TurnOutput;
    using output_type = TurnOutput;

    [[nodiscard]] TurnOutput operator()(TurnOutput state) const noexcept;
};

} // namespace biofuel::game::gameplay::stages