#pragma once

#include "engine/core/Types.hpp"
#include "game/data/FuelFarmData.hpp"
#include <optional>
#include <vector>

namespace biofuel::game::gameplay {

enum class TileType : u8 {
    Fallow,
    Corn,
    Sugarcane,
    Soybean,
    Switchgrass,
    Algae,
    Forest,
    Water,
    Built,
};

enum class Season : u8 {
    Spring,
    Summer,
    Fall,
    Winter,
};

struct Tile {
    TileType type = TileType::Fallow;
    i32 soilHealth = 100;
    i32 moisture = 50;
    i32 fertilizer = 0;
    i32 ageTurns = 0;
    i32 buildingId = -1;
};

struct FarmInventory {
    i32 fuelGallons = 0;
    i32 foodUnits = 0;
};

struct HarvestResult {
    bool harvested = false;
    i32 fuelGallons = 0;
    i32 revenueCents = 0;
};

class FarmState {
public:
    FarmState(usize width, usize height);

    [[nodiscard]] usize width() const noexcept { return m_width; }
    [[nodiscard]] usize height() const noexcept { return m_height; }
    [[nodiscard]] Season season() const noexcept { return m_season; }
    [[nodiscard]] i32 year() const noexcept { return m_year; }
    [[nodiscard]] i32 moneyCents() const noexcept { return m_moneyCents; }
    [[nodiscard]] const FarmInventory& inventory() const noexcept { return m_inventory; }

    [[nodiscard]] const Tile* tileAt(usize x, usize y) const noexcept;
    [[nodiscard]] Tile* tileAt(usize x, usize y) noexcept;
    [[nodiscard]] bool setTileType(usize x, usize y, TileType type) noexcept;

    void advanceSeason() noexcept;
    [[nodiscard]] HarvestResult harvestTile(usize x, usize y) noexcept;

private:
    [[nodiscard]] bool inBounds(usize x, usize y) const noexcept;
    [[nodiscard]] usize indexOf(usize x, usize y) const noexcept;

    usize m_width = 0U;
    usize m_height = 0U;
    std::vector<Tile> m_tiles;
    Season m_season = Season::Spring;
    i32 m_year = 1;
    i32 m_moneyCents = 100000;
    FarmInventory m_inventory{};
};

[[nodiscard]] constexpr bool isCropTile(TileType type) noexcept {
    switch (type) {
    case TileType::Corn:
    case TileType::Sugarcane:
    case TileType::Soybean:
    case TileType::Switchgrass:
    case TileType::Algae:
        return true;
    case TileType::Fallow:
    case TileType::Forest:
    case TileType::Water:
    case TileType::Built:
        return false;
    default:
        return false;
    }
}

[[nodiscard]] constexpr std::optional<data::CropId> cropForTile(TileType type) noexcept {
    switch (type) {
    case TileType::Corn: return data::CropId::Corn;
    case TileType::Sugarcane: return data::CropId::Sugarcane;
    case TileType::Soybean: return data::CropId::Soybean;
    case TileType::Switchgrass: return data::CropId::Switchgrass;
    case TileType::Algae: return data::CropId::Algae;
    case TileType::Fallow:
    case TileType::Forest:
    case TileType::Water:
    case TileType::Built:
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr Season nextSeason(Season season) noexcept {
    switch (season) {
    case Season::Spring: return Season::Summer;
    case Season::Summer: return Season::Fall;
    case Season::Fall: return Season::Winter;
    case Season::Winter: return Season::Spring;
    default: return Season::Spring;
    }
}

} // namespace biofuel::game::gameplay
