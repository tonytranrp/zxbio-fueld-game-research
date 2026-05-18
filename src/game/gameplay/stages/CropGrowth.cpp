#include "game/gameplay/stages/CropGrowth.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] TurnOutput CropGrowth::operator()(TurnOutput state) const noexcept {
    // Apply growth modifiers based on season.
    // Season was already advanced by SeasonAdvance.
    FarmState& farm = state.farmState;
    const Season season = farm.season();

    for (usize y = 0; y < farm.height(); ++y) {
        for (usize x = 0; x < farm.width(); ++x) {
            Tile* tile = farm.tileAt(x, y);
            if (tile == nullptr || !isCropTile(tile->type)) {
                continue;
            }

            // Growth depends on season:
            // Spring: +2 age, Summer: +1 age, Fall: +1 age (if mature), Winter: no growth
            switch (season) {
                case Season::Spring:
                    tile->ageTurns += 2;
                    break;
                case Season::Summer:
                    tile->ageTurns += 1;
                    break;
                case Season::Fall:
                    // Only mature crops continue growing in fall
                    if (tile->ageTurns >= 2) {
                        tile->ageTurns += 1;
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