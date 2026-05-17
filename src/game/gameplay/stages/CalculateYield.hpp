#pragma once

#include "game/gameplay/stages/HarvestTypes.hpp"
#include "game/data/FuelFarmData.hpp"

namespace biofuel::game::gameplay::stages {

/// Computes yield from CropData lookup.
/// Looks up the crop in kCropData and calculates fuel gallons and revenue.
struct CalculateYield {
    using input_type = HarvestInput;
    using output_type = HarvestOutput;

    HarvestOutput operator()(HarvestInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages