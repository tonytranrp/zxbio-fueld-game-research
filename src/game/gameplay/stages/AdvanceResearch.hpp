#pragma once

#include "game/gameplay/stages/TechTreeTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Advances research by one turn (decrements turnsRemaining).
/// If turnsRemaining reaches 0, marks the research as completed.
struct AdvanceResearch {
    static constexpr std::string_view name = "AdvanceResearch";

    using input_type = TechTreeInput;
    using output_type = TechTreeInput;

    [[nodiscard]] TechTreeInput operator()(TechTreeInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages