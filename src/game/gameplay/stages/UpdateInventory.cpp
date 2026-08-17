#include "game/gameplay/stages/UpdateInventory.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] HarvestOutput UpdateInventory::operator()(HarvestOutput output) const noexcept {
    // Apply the harvest through FarmState::harvestTile so the tile reset and the
    // inventory/food/money crediting match a direct manual harvest exactly.
    if (output.harvested && output.farmState != nullptr) {
        const HarvestResult result = output.farmState->harvestTile(output.tileX, output.tileY);
        output.harvested = result.harvested;
        output.fuelGallons = result.fuelGallons;
        output.revenueCents = result.revenueCents;
    }
    return output;
}

} // namespace biofuel::game::gameplay::stages