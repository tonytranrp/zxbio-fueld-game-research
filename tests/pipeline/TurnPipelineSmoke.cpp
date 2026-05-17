#include "game/gameplay/TurnPipeline.hpp"
#include "game/gameplay/FarmState.hpp"
#include "game/data/FuelFarmData.hpp"
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

    bool ok = true;
    biofuel::game::gameplay::TurnPipelineRunner runner;

    // Note: The turn pipeline does MORE than manual advanceSeason().
    // SeasonAdvance wraps advanceSeason(), but CropGrowth adds seasonal growth
    // modifiers, EcologyUpdate modifies soil/moisture, and EconomyUpdate is a
    // pass-through. Tests verify pipeline behavior, not equality to manual
    // advanceSeason() calls.

    // --- Scenario 1: Spring → Summer with a planted crop ---
    {
        FarmState farm(10, 10);
        ok = check(farm.season() == Season::Spring && farm.year() == 1, "spring init failed") && ok;

        (void)farm.setTileType(5, 5, TileType::Corn);

        stages::TurnInput input{farm};
        auto output = runner.run(std::move(input));

        ok = check(output.farmState.season() == Season::Summer, "season did not advance to summer") && ok;
        ok = check(output.farmState.year() == 1, "year changed prematurely") && ok;

        const auto* t2 = output.farmState.tileAt(5, 5);
        // advanceSeason() increments age (0→1), then CropGrowth adds Summer +1 = 2
        ok = check(t2 != nullptr && t2->ageTurns == 2, "crop age should be 2 after spring→summer pipeline") && ok;
    }

    // --- Scenario 2: Season wrap (Winter → Spring, year increment) ---
    {
        FarmState farm(10, 10);
        // Advance manually to Winter first
        for (int i = 0; i < 3; ++i) farm.advanceSeason();

        stages::TurnInput input{farm};
        auto output = runner.run(std::move(input));

        ok = check(output.farmState.season() == Season::Spring, "winter did not wrap to spring") && ok;
        ok = check(output.farmState.year() == 2, "year did not increment after winter") && ok;
    }

    // --- Scenario 3: Full year cycle through pipeline ---
    {
        FarmState farm(10, 10);
        (void)farm.setTileType(3, 3, TileType::Corn);

        // Run 4 turns through pipeline (Spring→Summer→Fall→Winter→Spring)
        for (int turnIdx = 0; turnIdx < 4; ++turnIdx) {
            stages::TurnInput input{farm};
            auto output = runner.run(std::move(input));
            farm = output.farmState;
        }

        ok = check(farm.season() == Season::Spring, "full year should end on spring") && ok;
        ok = check(farm.year() == 2, "full year should increment year") && ok;

        const auto* tile = farm.tileAt(3, 3);
        // advanceSeason increments each turn (+4 total), plus CropGrowth:
        // Spring+2, Summer+1, Fall+1 (age>=2), Winter+0 = +4, total +8
        ok = check(tile != nullptr && tile->ageTurns == 8, "crop age should be 8 after full pipeline year") && ok;
    }

    // --- Scenario 4: Pipeline season transitions are correct ---
    {
        FarmState farm(10, 10);

        // Spring → Summer
        auto r1 = runner.run(stages::TurnInput{farm});
        ok = check(r1.farmState.season() == Season::Summer, "turn 1: should be Summer") && ok;

        // Summer → Fall
        auto r2 = runner.run(stages::TurnInput{r1.farmState});
        ok = check(r2.farmState.season() == Season::Fall, "turn 2: should be Fall") && ok;

        // Fall → Winter
        auto r3 = runner.run(stages::TurnInput{r2.farmState});
        ok = check(r3.farmState.season() == Season::Winter, "turn 3: should be Winter") && ok;

        // Winter → Spring (year 2)
        auto r4 = runner.run(stages::TurnInput{r3.farmState});
        ok = check(r4.farmState.season() == Season::Spring, "turn 4: should be Spring") && ok;
        ok = check(r4.farmState.year() == 2, "turn 4: should be year 2") && ok;
    }

    if (ok) {
        std::printf("\nAll TurnPipeline smoke tests PASSED.\n");
        return EXIT_SUCCESS;
    }
    std::printf("\nTurnPipeline smoke test(s) FAILED.\n");
    return EXIT_FAILURE;
}
