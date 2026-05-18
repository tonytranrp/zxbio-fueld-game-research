#pragma once

#include "game/gameplay/stages/HarvestTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Validates that a tile is harvestable (is a crop, has age > 0, is in bounds).
/// If invalid, returns zero-valued output.
struct ValidateCrop {
    using input_type = HarvestInput;
    using output_type = HarvestInput;

    [[nodiscard]] HarvestInput operator()(HarvestInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages