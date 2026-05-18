#pragma once

#include "game/gameplay/stages/HarvestTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Harvest finalization stage — resets the harvested tile to fallow using the
/// farmState/tileX/tileY fields in HarvestOutput, keeping the pipeline self-contained.
struct UpdateInventory {
    using input_type = HarvestOutput;
    using output_type = HarvestOutput;

    [[nodiscard]] HarvestOutput operator()(HarvestOutput output) const noexcept;
};

} // namespace biofuel::game::gameplay::stages