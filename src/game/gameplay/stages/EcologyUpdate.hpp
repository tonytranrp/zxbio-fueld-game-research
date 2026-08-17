#pragma once

#include "game/gameplay/stages/TurnTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Updates soil health, moisture, and carbon balance.
/// Soil degrades with monocropping, restores with legumes/fallow.
/// Moisture varies by season: Spring = wet, Summer = dry, Fall = moderate, Winter = frozen.
struct EcologyUpdate {
    static constexpr std::string_view name = "EcologyUpdate";

    using input_type = TurnOutput;
    using output_type = TurnOutput;

    [[nodiscard]] TurnOutput operator()(TurnOutput state) const noexcept;
};

} // namespace biofuel::game::gameplay::stages