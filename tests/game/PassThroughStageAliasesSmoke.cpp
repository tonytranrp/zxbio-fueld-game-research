#include "game/gameplay/stages/PassThroughStages.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>

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
    using namespace biofuel::game::gameplay::stages;
    using namespace biofuel::game::data;

    static_assert(std::same_as<WashCrop, PassThrough<ProcessingInput>>);
    static_assert(std::same_as<GrindCrop, PassThrough<ProcessingInput>>);
    static_assert(std::same_as<Ferment, PassThrough<ProcessingInput>>);
    static_assert(std::same_as<PressExtract, PassThrough<ProcessingInput>>);
    static_assert(std::same_as<Pretreat, PassThrough<ProcessingInput>>);
    static_assert(std::same_as<EconomyUpdate, PassThrough<TurnOutput>>);

    bool ok = true;

    const ProcessingInput processInput{
        .cropId = CropId::Soybean,
        .quantityGallons = 48,
        .sourceTileX = 3,
        .sourceTileY = 4,
    };

    const ProcessingInput washed = WashCrop{}(processInput);
    ok = check(washed.cropId == processInput.cropId, "WashCrop changed cropId") && ok;
    ok = check(washed.quantityGallons == processInput.quantityGallons, "WashCrop changed quantity") && ok;
    ok = check(washed.sourceTileX == processInput.sourceTileX && washed.sourceTileY == processInput.sourceTileY,
        "WashCrop changed source tile") && ok;

    const ProcessingInput pretreated = Pretreat{}(processInput);
    ok = check(pretreated.cropId == processInput.cropId, "Pretreat changed cropId") && ok;
    ok = check(pretreated.quantityGallons == processInput.quantityGallons, "Pretreat changed quantity") && ok;

    FarmState farm{2, 2};
    (void)farm.setTileType(1, 1, TileType::Corn);
    const TurnOutput turnOutput{farm};
    const TurnOutput economyOutput = EconomyUpdate{}(turnOutput);
    const Tile* tile = economyOutput.farmState.tileAt(1, 1);
    ok = check(tile != nullptr && tile->type == TileType::Corn, "EconomyUpdate changed farm tile") && ok;
    ok = check(economyOutput.farmState.season() == turnOutput.farmState.season(), "EconomyUpdate changed season") && ok;
    ok = check(economyOutput.farmState.year() == turnOutput.farmState.year(), "EconomyUpdate changed year") && ok;

    if (ok) {
        std::printf("\nAll PassThroughStageAliases smoke tests PASSED.\n");
        return EXIT_SUCCESS;
    }
    std::printf("\nPassThroughStageAliases smoke test(s) FAILED.\n");
    return EXIT_FAILURE;
}
