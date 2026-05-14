#include "FarmState.hpp"

namespace biofuel::game::gameplay {

FarmState::FarmState(const usize width, const usize height)
    : m_width(width),
      m_height(height),
      m_tiles(width * height) {}

const Tile* FarmState::tileAt(const usize x, const usize y) const noexcept {
    if (!inBounds(x, y)) {
        return nullptr;
    }
    return &m_tiles[indexOf(x, y)];
}

Tile* FarmState::tileAt(const usize x, const usize y) noexcept {
    if (!inBounds(x, y)) {
        return nullptr;
    }
    return &m_tiles[indexOf(x, y)];
}

bool FarmState::setTileType(const usize x, const usize y, const TileType type) noexcept {
    Tile* tile = tileAt(x, y);
    if (tile == nullptr) {
        return false;
    }

    tile->type = type;
    tile->ageTurns = 0;
    tile->buildingId = (type == TileType::Built) ? 0 : -1;
    return true;
}

void FarmState::advanceSeason() noexcept {
    if (m_season == Season::Winter) {
        ++m_year;
    }
    m_season = nextSeason(m_season);

    for (Tile& tile : m_tiles) {
        if (isCropTile(tile.type)) {
            ++tile.ageTurns;
        }
    }
}

HarvestResult FarmState::harvestTile(const usize x, const usize y) noexcept {
    Tile* tile = tileAt(x, y);
    if (tile == nullptr || tile->ageTurns <= 0) {
        return {};
    }

    const std::optional<data::CropId> cropId = cropForTile(tile->type);
    if (!cropId.has_value()) {
        return {};
    }

    const std::optional<data::CropData> crop = data::cropData(*cropId);
    if (!crop.has_value()) {
        return {};
    }

    const i32 gallons = crop->yieldGallonsPerAcre;
    const i32 revenue = gallons * data::fuelPriceCentsPerGallon(crop->fuelKind);
    m_inventory.fuelGallons += gallons;
    m_inventory.foodUnits += crop->landImpact;
    m_moneyCents += revenue;

    tile->type = TileType::Fallow;
    tile->ageTurns = 0;
    tile->fertilizer = 0;

    return HarvestResult{
        .harvested = true,
        .fuelGallons = gallons,
        .revenueCents = revenue,
    };
}

bool FarmState::inBounds(const usize x, const usize y) const noexcept {
    return x < m_width && y < m_height;
}

usize FarmState::indexOf(const usize x, const usize y) const noexcept {
    return (y * m_width) + x;
}

} // namespace biofuel::game::gameplay
