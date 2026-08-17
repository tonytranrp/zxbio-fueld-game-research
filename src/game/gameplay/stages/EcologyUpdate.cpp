#include "game/gameplay/stages/EcologyUpdate.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

namespace {

void applyClampedDelta(i32& value, const i32 delta) noexcept {
    if (delta > 0) {
        value = (value > 100 - delta) ? 100 : value + delta;
        return;
    }

    if (delta < 0) {
        const i32 loss = -delta;
        value = (value < loss) ? 0 : value - loss;
    }
}

[[nodiscard]] constexpr i32 moistureDeltaForSeason(const Season season) noexcept {
    switch (season) {
    case Season::Spring: return 10;
    case Season::Summer: return -15;
    case Season::Fall: return -5;
    case Season::Winter: return 5;
    default: return 0;
    }
}

} // namespace

[[nodiscard]] TurnOutput EcologyUpdate::operator()(TurnOutput state) const noexcept {
    FarmState& farm = state.farmState;
    const Season season = farm.season();
    const i32 moistureDelta = moistureDeltaForSeason(season);

    for (usize y = 0; y < farm.height(); ++y) {
        for (usize x = 0; x < farm.width(); ++x) {
            Tile& tile = farm.atUnsafe(x, y);

            // Soil health mechanics:
            // - Fallow/Forest: +5 soil health per turn (recovery)
            // - Legume (Soybean): +3 soil health per turn (nitrogen fixation)
            // - Monocrop (Corn, Sugarcane): -2 soil health per turn (depletion)
            // - Switchgrass/Algae: -1 soil health per turn (moderate)
            applyClampedDelta(tile.soilHealth, tileEcologyTraits(tile.type).soilHealthDeltaPerTurn);

            // Moisture mechanics by season:
            // Spring: +10 moisture, Summer: -15 moisture, Fall: -5, Winter: +5
            applyClampedDelta(tile.moisture, moistureDelta);
        }
    }

    return state;
}

} // namespace biofuel::game::gameplay::stages
