#include "game/gameplay/stages/CropGrowth.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] TurnOutput CropGrowth::operator()(TurnOutput state) const noexcept {
    // Apply seasonal bonus growth on top of the base +1 aging already applied
    // by SeasonAdvance (FarmState::advanceSeason).
    FarmState& farm = state.farmState;
    const Season season = farm.season();

    for (usize y = 0; y < farm.height(); ++y) {
        for (usize x = 0; x < farm.width(); ++x) {
            Tile& tile = farm.atUnsafe(x, y);
            if (!isCropTile(tile.type)) {
                continue;
            }

            // Growth depends on season:
            // Spring: +2 bonus, Summer: +1 bonus, Fall: +1 bonus (if mature), Winter: no bonus
            switch (season) {
                case Season::Spring:
                    tile.ageTurns += 2;
                    break;
                case Season::Summer:
                    tile.ageTurns += 1;
                    break;
                case Season::Fall:
                    // Only mature crops continue growing in fall
                    if (tile.ageTurns >= 2) {
                        tile.ageTurns += 1;
                    }
                    break;
                case Season::Winter:
                    // No growth in winter
                    break;
            }
        }
    }

    return state;
}

} // namespace biofuel::game::gameplay::stages