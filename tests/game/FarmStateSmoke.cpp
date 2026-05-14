#include "game/data/FuelFarmData.hpp"
#include "game/gameplay/FarmState.hpp"
#include <cstdlib>
#include <iostream>

namespace {

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    using namespace ::biofuel::game::data;
    using namespace ::biofuel::game::gameplay;

    static_assert(kCropData.size() == 5U);
    static_assert(kCropData[0].yieldGallonsPerAcre == 400);
    static_assert(kCropData[4].energyPerGallonBtu == 118300);
    static_assert(fuelPriceCentsPerGallon(FuelKind::Ethanol) == 220);

    bool ok = true;

    FarmState farm{3U, 2U};
    ok = check(farm.width() == 3U && farm.height() == 2U, "farm dimensions failed") && ok;
    ok = check(farm.season() == Season::Spring && farm.year() == 1, "initial season/year failed") && ok;
    ok = check(farm.moneyCents() == 100000, "initial money failed") && ok;
    ok = check(farm.tileAt(2U, 1U) != nullptr, "in-bounds tile lookup failed") && ok;
    ok = check(farm.tileAt(3U, 0U) == nullptr, "out-of-bounds tile lookup failed") && ok;

    ok = check(farm.setTileType(1U, 1U, TileType::Corn), "tile mutation rejected valid tile") && ok;
    ok = check(!farm.setTileType(4U, 1U, TileType::Soybean), "tile mutation accepted invalid tile") && ok;
    const Tile* planted = farm.tileAt(1U, 1U);
    ok = check(planted != nullptr && planted->type == TileType::Corn && planted->ageTurns == 0, "tile mutation did not plant corn") && ok;

    farm.advanceSeason();
    planted = farm.tileAt(1U, 1U);
    ok = check(farm.season() == Season::Summer && farm.year() == 1, "spring to summer failed") && ok;
    ok = check(planted != nullptr && planted->ageTurns == 1, "crop did not age on season advance") && ok;
    farm.advanceSeason();
    farm.advanceSeason();
    farm.advanceSeason();
    ok = check(farm.season() == Season::Spring && farm.year() == 2, "season/year wrap failed") && ok;

    const auto moneyBeforeHarvest = farm.moneyCents();
    const HarvestResult harvest = farm.harvestTile(1U, 1U);
    ok = check(harvest.harvested, "harvest did not occur") && ok;
    ok = check(harvest.fuelGallons == 400, "corn harvest gallons failed") && ok;
    ok = check(harvest.revenueCents == 88000, "corn harvest revenue failed") && ok;
    ok = check(farm.moneyCents() == moneyBeforeHarvest + harvest.revenueCents, "farm money did not increase by revenue") && ok;
    ok = check(farm.inventory().fuelGallons == harvest.fuelGallons, "fuel inventory did not increase") && ok;
    const Tile* harvestedTile = farm.tileAt(1U, 1U);
    ok = check(harvestedTile != nullptr && harvestedTile->type == TileType::Fallow && harvestedTile->ageTurns == 0, "harvest did not reset tile") && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
