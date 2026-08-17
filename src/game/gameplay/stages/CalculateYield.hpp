#pragma once

#include "game/gameplay/stages/HarvestTypes.hpp"
#include "game/data/FuelFarmData.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Computes yield from CropData lookup.
/// Looks up the crop in kCropData and calculates fuel gallons and revenue.
struct CalculateYield {
    static constexpr std::string_view name = "CalculateYield";

    using input_type = HarvestInput;
    using output_type = HarvestOutput;

    [[nodiscard]] HarvestOutput operator()(HarvestInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages