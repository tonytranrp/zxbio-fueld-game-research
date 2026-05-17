#include "game/gameplay/stages/EcologyUpdate.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

TurnOutput EcologyUpdate::operator()(TurnOutput state) const noexcept {
    FarmState& farm = state.farmState;
    const Season season = farm.season();

    for (usize y = 0; y < farm.height(); ++y) {
        for (usize x = 0; x < farm.width(); ++x) {
            Tile* tile = farm.tileAt(x, y);
            if (tile == nullptr) {
                continue;
            }

            // Soil health mechanics:
            // - Fallow/Forest: +5 soil health per turn (recovery)
            // - Legume (Soybean): +3 soil health per turn (nitrogen fixation)
            // - Monocrop (Corn, Sugarcane): -2 soil health per turn (depletion)
            // - Switchgrass/Algae: -1 soil health per turn (moderate)
            switch (tile->type) {
                case TileType::Fallow:
                case TileType::Forest:
                    tile->soilHealth = (tile->soilHealth < 95) ? tile->soilHealth + 5 : 100;
                    break;
                case TileType::Soybean:
                    tile->soilHealth = (tile->soilHealth < 97) ? tile->soilHealth + 3 : 100;
                    break;
                case TileType::Corn:
                case TileType::Sugarcane:
                    tile->soilHealth = (tile->soilHealth > 2) ? tile->soilHealth - 2 : 0;
                    break;
                case TileType::Switchgrass:
                case TileType::Algae:
                    tile->soilHealth = (tile->soilHealth > 1) ? tile->soilHealth - 1 : 0;
                    break;
                default:
                    break;
            }

            // Moisture mechanics by season:
            // Spring: +10 moisture, Summer: -15 moisture, Fall: -5, Winter: +5
            switch (season) {
                case Season::Spring:
                    tile->moisture = (tile->moisture > 90) ? 100 : tile->moisture + 10;
                    break;
                case Season::Summer:
                    tile->moisture = (tile->moisture < 15) ? 0 : tile->moisture - 15;
                    break;
                case Season::Fall:
                    tile->moisture = (tile->moisture < 5) ? 0 : tile->moisture - 5;
                    break;
                case Season::Winter:
                    tile->moisture = (tile->moisture > 95) ? 100 : tile->moisture + 5;
                    break;
                default:
                    break;
            }
        }
    }

    return state;
}

} // namespace biofuel::game::gameplay::stages