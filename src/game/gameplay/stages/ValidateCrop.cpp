#include "game/gameplay/stages/ValidateCrop.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

HarvestInput ValidateCrop::operator()(HarvestInput input) const noexcept {
    // If the farm state pointer is null or the tile is not a crop, zero out.
    // The pipeline will check for null later.
    // This stage passes through valid input unchanged.
    if (input.farmState == nullptr) {
        return input;
    }

    const Tile* tile = input.farmState->tileAt(input.x, input.y);
    if (tile == nullptr || !isCropTile(tile->type) || tile->ageTurns <= 0) {
        // Mark as invalid by setting farmState to nullptr
        input.farmState = nullptr;
    }

    return input;
}

} // namespace biofuel::game::gameplay::stages