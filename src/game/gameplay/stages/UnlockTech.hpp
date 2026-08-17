#pragma once

#include "game/gameplay/stages/TechTreeTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Unlocks completed research and publishes TechUnlocked event.
/// Transforms InProgress research with turnsRemaining == 0 into Completed status.
struct UnlockTech {
    static constexpr std::string_view name = "UnlockTech";

    using input_type = TechTreeInput;
    using output_type = TechTreeOutput;

    [[nodiscard]] TechTreeOutput operator()(TechTreeInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages