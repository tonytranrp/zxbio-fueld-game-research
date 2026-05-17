#include "game/gameplay/stages/UpdateInventory.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

HarvestOutput UpdateInventory::operator()(HarvestOutput output) const noexcept {
    // Apply the harvest mutation to FarmState — reset the harvested tile.
    if (output.harvested && output.farmState != nullptr) {
        Tile* tile = output.farmState->tileAt(output.tileX, output.tileY);
        if (tile != nullptr) {
            tile->type = TileType::Fallow;
            tile->ageTurns = 0;
            tile->fertilizer = 0;
        }
    }
    return output;
}

} // namespace biofuel::game::gameplay::stages