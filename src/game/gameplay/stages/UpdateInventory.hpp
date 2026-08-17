#pragma once

#include "game/gameplay/stages/HarvestTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Harvest finalization stage — applies the harvest mutation through
/// FarmState::harvestTile() (tile reset plus inventory/food/money crediting)
/// using the farmState/tileX/tileY fields in HarvestOutput, keeping the
/// pipeline equivalent to a direct manual harvest.
struct UpdateInventory {
    static constexpr std::string_view name = "UpdateInventory";

    using input_type = HarvestOutput;
    using output_type = HarvestOutput;

    [[nodiscard]] HarvestOutput operator()(HarvestOutput output) const noexcept;
};

} // namespace biofuel::game::gameplay::stages