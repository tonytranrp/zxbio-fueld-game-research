#include "game/gameplay/SampleFarm.hpp"

namespace biofuel::game::gameplay {

std::unique_ptr<FarmState> createSampleFarm() {
    constexpr usize kWidth = 12;
    constexpr usize kHeight = 10;
    auto farm = std::make_unique<FarmState>(kWidth, kHeight);

    // Place a border of "built" (solid) tiles around the perimeter.
    for (usize x = 0U; x < kWidth; ++x) {
        (void)farm->setTileType(x, 0U, TileType::Built);
        (void)farm->setTileType(x, kHeight - 1U, TileType::Built);
    }
    for (usize y = 1U; y < kHeight - 1U; ++y) {
        (void)farm->setTileType(0U, y, TileType::Built);
        (void)farm->setTileType(kWidth - 1U, y, TileType::Built);
    }

    // Place a few obstacles in the interior.
    (void)farm->setTileType(3U, 3U, TileType::Forest);
    (void)farm->setTileType(4U, 3U, TileType::Forest);
    (void)farm->setTileType(7U, 5U, TileType::Forest);
    (void)farm->setTileType(7U, 6U, TileType::Forest);
    (void)farm->setTileType(8U, 5U, TileType::Forest);
    (void)farm->setTileType(8U, 6U, TileType::Forest);

    // Water pool in the corner.
    (void)farm->setTileType(2U, 7U, TileType::Water);
    (void)farm->setTileType(2U, 8U, TileType::Water);
    (void)farm->setTileType(3U, 8U, TileType::Water);

    return farm;
}

} // namespace biofuel::game::gameplay
