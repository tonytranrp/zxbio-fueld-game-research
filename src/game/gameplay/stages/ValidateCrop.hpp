#pragma once

#include "game/gameplay/stages/HarvestTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Validates that a tile is harvestable (is a crop, has age > 0, is in bounds).
/// If invalid, returns zero-valued output.
struct ValidateCrop {
    static constexpr std::string_view name = "ValidateCrop";

    using input_type = HarvestInput;
    using output_type = HarvestInput;

    [[nodiscard]] HarvestInput operator()(HarvestInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages