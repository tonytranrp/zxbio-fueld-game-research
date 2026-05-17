#pragma once

#include "game/gameplay/FarmState.hpp"
#include "game/data/FuelFarmData.hpp"

namespace biofuel::game::gameplay::stages {

/// Input for the harvest pipeline.
/// Identifies which tile to harvest from a FarmState reference.
struct HarvestInput {
    usize x = 0;
    usize y = 0;
    FarmState* farmState = nullptr;
};

/// Output of the harvest pipeline.
/// Mirrors FarmState::HarvestResult but as an independent struct for pipeline compatibility.
/// Carries a FarmState pointer so UpdateInventory can apply the harvest mutation.
struct HarvestOutput {
    bool harvested = false;
    i32 fuelGallons = 0;
    i32 revenueCents = 0;
    FarmState* farmState = nullptr;
    usize tileX = 0;
    usize tileY = 0;
};

} // namespace biofuel::game::gameplay::stages