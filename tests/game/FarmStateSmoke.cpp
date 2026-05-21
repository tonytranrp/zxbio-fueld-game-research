#include "game/data/FuelFarmData.hpp"
#include "game/gameplay/FarmState.hpp"
#include "game/gameplay/SampleFarm.hpp"
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
    using ::biofuel::usize;
    using namespace ::biofuel::game::data;
    using namespace ::biofuel::game::gameplay;

    static_assert(kCropData.size() == 5U);
    static_assert(kCropData[0].yieldGallonsPerAcre == 400);
    static_assert(kCropData[4].energyPerGallonBtu == 118300);
    static_assert(fuelPriceCentsPerGallon(FuelKind::Ethanol) == 220);
    static_assert(kTileTypeMetadata.size() == kTileTypeCount);
    static_assert(tileTypeName(TileType::Soybean) == "Soybean");
    static_assert(tileRenderColor(TileType::Corn).r == 218U);
    static_assert(isTileWalkable(TileType::Fallow));
    static_assert(!isTileWalkable(TileType::Corn));
    static_assert(!isTileSolid(TileType::Water));
    static_assert(tileHasPhysicsCollider(TileType::Built));
    static_assert(!tileHasPhysicsCollider(TileType::Fallow));
    static_assert(tilePhysicsTraits(TileType::Algae).material == TilePhysicsMaterial::Water);
    static_assert(tileEcologyTraits(TileType::Soybean).soilHealthDeltaPerTurn == 3);
    static_assert(tileTypeName(static_cast<TileType>(255U)) == "Fallow");

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

    const auto sampleFarm = createSampleFarm();
    ok = check(sampleFarm != nullptr, "sample farm factory returned null") && ok;
    ok = check(sampleFarm->width() == 12U && sampleFarm->height() == 10U, "sample farm dimensions failed") && ok;
    const Tile* border = sampleFarm->tileAt(0U, 0U);
    ok = check(border != nullptr && border->type == TileType::Built, "sample farm border is not built") && ok;
    const Tile* forest = sampleFarm->tileAt(3U, 3U);
    ok = check(forest != nullptr && forest->type == TileType::Forest, "sample farm forest obstacle missing") && ok;
    const Tile* water = sampleFarm->tileAt(2U, 7U);
    ok = check(water != nullptr && water->type == TileType::Water, "sample farm water pool missing") && ok;
    bool sampleLayoutMatches = true;
    for (usize y = 0U; y < sampleFarm->height(); ++y) {
        for (usize x = 0U; x < sampleFarm->width(); ++x) {
            TileType expected = TileType::Fallow;
            if (x == 0U || y == 0U || x + 1U == sampleFarm->width() || y + 1U == sampleFarm->height()) {
                expected = TileType::Built;
            } else if ((x == 3U && y == 3U) ||
                       (x == 4U && y == 3U) ||
                       (x == 7U && y == 5U) ||
                       (x == 7U && y == 6U) ||
                       (x == 8U && y == 5U) ||
                       (x == 8U && y == 6U)) {
                expected = TileType::Forest;
            } else if ((x == 2U && y == 7U) ||
                       (x == 2U && y == 8U) ||
                       (x == 3U && y == 8U)) {
                expected = TileType::Water;
            }

            const Tile* tile = sampleFarm->tileAt(x, y);
            sampleLayoutMatches = sampleLayoutMatches && tile != nullptr && tile->type == expected;
        }
    }
    ok = check(sampleLayoutMatches, "sample farm full layout mismatch") && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
