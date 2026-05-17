#include "game/gameplay/HarvestPipeline.hpp"
#include "game/gameplay/FarmState.hpp"
#include "game/data/FuelFarmData.hpp"
#include "engine/core/Types.hpp"
#include <cstdlib>
#include <cstdio>

namespace {

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        return false;
    }
    return true;
}

} // namespace

int main() {
    using namespace biofuel::game::gameplay;
    using namespace biofuel::game::data;
    using biofuel::usize;

    bool ok = true;
    biofuel::game::gameplay::HarvestPipelineRunner runner;

    // Helper: plant a crop and advance season so it's harvestable (ageTurns > 0)
    auto makeHarvestable = [](FarmState& farm, usize x, usize y, TileType type) {
        (void)farm.setTileType(x, y, type);
        farm.advanceSeason();
    };

    // --- Scenario 1: Harvest Corn tile (yield=400 gal/acre) ---
    {
        FarmState farm(10, 10);
        makeHarvestable(farm, 2, 2, TileType::Corn);

        stages::HarvestInput input{2, 2, &farm};
        auto output = runner.run(std::move(input));

        ok = check(output.harvested, "corn harvest should succeed") && ok;
        ok = check(output.fuelGallons == 400, "corn harvest fuelGallons mismatch") && ok;
        ok = check(output.revenueCents == 88000, "corn harvest revenueCents mismatch") && ok;

        const auto* harvestedTile = farm.tileAt(2, 2);
        ok = check(harvestedTile != nullptr && harvestedTile->type == TileType::Fallow
            && harvestedTile->ageTurns == 0, "harvested tile not reset") && ok;
    }

    // --- Scenario 2: Harvest Soybean tile (yield=48 gal/acre) ---
    {
        FarmState farm(10, 10);
        makeHarvestable(farm, 3, 3, TileType::Soybean);

        stages::HarvestInput input{3, 3, &farm};
        auto output = runner.run(std::move(input));

        ok = check(output.harvested, "soybean harvest should succeed") && ok;
        ok = check(output.fuelGallons == 48, "soybean harvest fuelGallons mismatch") && ok;
    }

    // --- Scenario 3: Harvest Sugarcane tile (yield=590 gal/acre) ---
    {
        FarmState farm(10, 10);
        makeHarvestable(farm, 1, 1, TileType::Sugarcane);

        stages::HarvestInput input{1, 1, &farm};
        auto output = runner.run(std::move(input));

        ok = check(output.harvested, "sugarcane harvest should succeed") && ok;
        ok = check(output.fuelGallons == 590, "sugarcane harvest fuelGallons mismatch") && ok;
    }

    // --- Scenario 4: Harvest Switchgrass tile (yield=300 gal/acre) ---
    {
        FarmState farm(10, 10);
        makeHarvestable(farm, 4, 4, TileType::Switchgrass);

        stages::HarvestInput input{4, 4, &farm};
        auto output = runner.run(std::move(input));

        ok = check(output.harvested, "switchgrass harvest should succeed") && ok;
        ok = check(output.fuelGallons == 300, "switchgrass harvest fuelGallons mismatch") && ok;
    }

    // --- Scenario 5: Harvest Algae tile (yield=5000 gal/acre) ---
    {
        FarmState farm(10, 10);
        makeHarvestable(farm, 0, 0, TileType::Algae);

        stages::HarvestInput input{0, 0, &farm};
        auto output = runner.run(std::move(input));

        ok = check(output.harvested, "algae harvest should succeed") && ok;
        ok = check(output.fuelGallons == 5000, "algae harvest fuelGallons mismatch") && ok;
    }

    // --- Scenario 6: Invalid harvest — Fallow tile (never planted) ---
    {
        FarmState farm(10, 10);
        stages::HarvestInput input{5, 5, &farm};
        auto output = runner.run(std::move(input));

        ok = check(!output.harvested, "fallow tile should not harvest") && ok;
        ok = check(output.fuelGallons == 0, "fallow harvest should yield 0 fuel") && ok;
        ok = check(output.revenueCents == 0, "fallow harvest should yield 0 revenue") && ok;
    }

    // --- Scenario 7: Pipeline matches FarmState::harvestTile() for all crop types ---
    {
        const TileType cropTypes[] = {TileType::Corn, TileType::Sugarcane, TileType::Soybean, TileType::Switchgrass, TileType::Algae};
        usize x = 1;
        for (const auto type : cropTypes) {
            FarmState pipelineFarm(10, 10);
            FarmState manualFarm(10, 10);
            makeHarvestable(pipelineFarm, x, x, type);
            makeHarvestable(manualFarm, x, x, type);

            stages::HarvestInput input{x, x, &pipelineFarm};
            auto pipeResult = runner.run(std::move(input));
            auto manResult = manualFarm.harvestTile(x, x);

            ok = check(pipeResult.harvested == manResult.harvested, "harvest status mismatch") && ok;
            ok = check(pipeResult.fuelGallons == manResult.fuelGallons, "fuelGallons mismatch vs manual harvest") && ok;
            ok = check(pipeResult.revenueCents == manResult.revenueCents, "revenueCents mismatch vs manual harvest") && ok;
            ++x;
        }
    }

    if (ok) {
        std::printf("\nAll HarvestPipeline smoke tests PASSED.\n");
        return EXIT_SUCCESS;
    }
    std::printf("\nHarvestPipeline smoke test(s) FAILED.\n");
    return EXIT_FAILURE;
}
