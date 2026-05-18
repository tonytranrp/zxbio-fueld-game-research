#include "game/gameplay/stages/CalculateYield.hpp"
#include "game/data/FuelFarmData.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] HarvestOutput CalculateYield::operator()(HarvestInput input) const noexcept {
    if (input.farmState == nullptr) {
        return HarvestOutput{.harvested = false, .fuelGallons = 0, .revenueCents = 0};
    }

    const Tile* tile = input.farmState->tileAt(input.x, input.y);
    if (tile == nullptr) {
        return HarvestOutput{.harvested = false, .fuelGallons = 0, .revenueCents = 0};
    }

    const std::optional<data::CropId> cropId = cropForTile(tile->type);
    if (!cropId.has_value()) {
        return HarvestOutput{.harvested = false, .fuelGallons = 0, .revenueCents = 0};
    }

    const std::optional<data::CropData> crop = data::cropData(*cropId);
    if (!crop.has_value()) {
        return HarvestOutput{.harvested = false, .fuelGallons = 0, .revenueCents = 0};
    }

    const i32 gallons = crop->yieldGallonsPerAcre;
    const i32 revenue = gallons * data::fuelPriceCentsPerGallon(crop->fuelKind);

    return HarvestOutput{
        .harvested = true,
        .fuelGallons = gallons,
        .revenueCents = revenue,
        .farmState = input.farmState,
        .tileX = input.x,
        .tileY = input.y,
    };
}

} // namespace biofuel::game::gameplay::stages