#pragma once

#include "engine/core/Types.hpp"
#include "game/data/FuelFarmData.hpp"
#include <array>
#include <optional>
#include <string_view>
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
    Count,
};

inline constexpr usize kTileTypeCount = static_cast<usize>(TileType::Count);

struct TileRenderColor {
    u8 r = 0U;
    u8 g = 0U;
    u8 b = 0U;
    u8 a = 255U;
};

enum class TilePhysicsMaterial : u8 {
    Soil,
    Crop,
    Vegetation,
    Water,
    Structure,
};

struct TilePhysicsTraits {
    TilePhysicsMaterial material = TilePhysicsMaterial::Soil;
    f32 friction = 0.8F;
    f32 restitution = 0.0F;
    f32 density = 0.0F;
};

struct TileTypeMetadata {
    TileType type = TileType::Fallow;
    std::string_view name{};
    TileRenderColor renderColor{};
    std::optional<data::CropId> cropId = std::nullopt;
    bool crop = false;
    bool walkable = true;
    bool solid = false;
    bool physicsCollider = false;
    TilePhysicsTraits physics{};
};

inline constexpr std::array<TileTypeMetadata, kTileTypeCount> kTileTypeMetadata{{
    {TileType::Fallow, "Fallow", TileRenderColor{139,  90,  43, 255}, std::nullopt, false, true,  false, false, TilePhysicsTraits{TilePhysicsMaterial::Soil,       0.85F, 0.0F, 0.0F}},
    {TileType::Corn, "Corn", TileRenderColor{218, 165,  32, 255}, data::CropId::Corn, true, false, true, true, TilePhysicsTraits{TilePhysicsMaterial::Crop,       0.70F, 0.0F, 0.0F}},
    {TileType::Sugarcane, "Sugarcane", TileRenderColor{144, 238, 144, 255}, data::CropId::Sugarcane, true, false, true, true, TilePhysicsTraits{TilePhysicsMaterial::Crop,       0.72F, 0.0F, 0.0F}},
    {TileType::Soybean, "Soybean", TileRenderColor{ 34, 139,  34, 255}, data::CropId::Soybean, true, false, true, true, TilePhysicsTraits{TilePhysicsMaterial::Crop,       0.68F, 0.0F, 0.0F}},
    {TileType::Switchgrass, "Switchgrass", TileRenderColor{107, 142,  35, 255}, data::CropId::Switchgrass, true, false, true, true, TilePhysicsTraits{TilePhysicsMaterial::Crop,       0.74F, 0.0F, 0.0F}},
    {TileType::Algae, "Algae", TileRenderColor{  0, 128, 128, 255}, data::CropId::Algae, true, false, true, true, TilePhysicsTraits{TilePhysicsMaterial::Water,      0.35F, 0.0F, 0.0F}},
    {TileType::Forest, "Forest", TileRenderColor{  0, 100,   0, 255}, std::nullopt, false, false, true, true, TilePhysicsTraits{TilePhysicsMaterial::Vegetation, 0.95F, 0.0F, 0.0F}},
    {TileType::Water, "Water", TileRenderColor{ 65, 105, 225, 255}, std::nullopt, false, false, false, false, TilePhysicsTraits{TilePhysicsMaterial::Water,      0.20F, 0.0F, 0.0F}},
    {TileType::Built, "Built", TileRenderColor{128, 128, 128, 255}, std::nullopt, false, false, true, true, TilePhysicsTraits{TilePhysicsMaterial::Structure,  0.90F, 0.0F, 0.0F}},
}};

static_assert(kTileTypeMetadata.size() == kTileTypeCount,
              "kTileTypeMetadata must contain exactly one row for each TileType before Count");

[[nodiscard]] constexpr bool tileMetadataMatchesEnumOrder() noexcept {
    for (usize i = 0U; i < kTileTypeMetadata.size(); ++i) {
        if (static_cast<usize>(kTileTypeMetadata[i].type) != i) {
            return false;
        }
    }
    return true;
}
static_assert(tileMetadataMatchesEnumOrder(),
              "kTileTypeMetadata rows must stay indexed by TileType ordinal");

[[nodiscard]] constexpr bool isKnownTileType(const TileType type) noexcept {
    return static_cast<usize>(type) < kTileTypeCount;
}

[[nodiscard]] constexpr const TileTypeMetadata& tileMetadata(const TileType type) noexcept {
    return isKnownTileType(type)
        ? kTileTypeMetadata[static_cast<usize>(type)]
        : kTileTypeMetadata[static_cast<usize>(TileType::Fallow)];
}

[[nodiscard]] constexpr std::string_view tileTypeName(const TileType type) noexcept {
    return tileMetadata(type).name;
}

[[nodiscard]] constexpr TileRenderColor tileRenderColor(const TileType type) noexcept {
    return tileMetadata(type).renderColor;
}

[[nodiscard]] constexpr bool isTileWalkable(const TileType type) noexcept {
    return tileMetadata(type).walkable;
}

[[nodiscard]] constexpr bool isTileSolid(const TileType type) noexcept {
    return tileMetadata(type).solid;
}

[[nodiscard]] constexpr bool tileHasPhysicsCollider(const TileType type) noexcept {
    return tileMetadata(type).physicsCollider;
}

[[nodiscard]] constexpr TilePhysicsTraits tilePhysicsTraits(const TileType type) noexcept {
    return tileMetadata(type).physics;
}

[[nodiscard]] constexpr bool tileCropMappingsComplete() noexcept {
    usize cropTileCount = 0U;
    for (const TileTypeMetadata& tile : kTileTypeMetadata) {
        if (tile.crop != tile.cropId.has_value()) {
            return false;
        }
        if (!tile.cropId.has_value()) {
            continue;
        }
        ++cropTileCount;
        if (!data::cropData(*tile.cropId).has_value()) {
            return false;
        }
    }

    for (const data::CropData& crop : data::kCropData) {
        bool found = false;
        for (const TileTypeMetadata& tile : kTileTypeMetadata) {
            if (tile.cropId.has_value() && *tile.cropId == crop.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return cropTileCount == data::kCropData.size();
}
static_assert(tileCropMappingsComplete(),
              "Every crop tile must map to CropData and every CropId must have a TileType");

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
static_assert(sizeof(Tile) == 24,
              "Tile must be exactly 24 bytes (u8 TileType + 3 padding + 5 i32)");

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

    /// Unchecked tile access — caller guarantees (x,y) are in bounds.
    /// Use in hot loops where bounds have already been verified once at the
    /// loop perimeter.  Skips the per-access branch to improve pipeline
    //  throughput on large grids.
    [[nodiscard]] const Tile& atUnsafe(usize x, usize y) const noexcept {
        return m_tiles[y * m_width + x];
    }
    [[nodiscard]] Tile& atUnsafe(usize x, usize y) noexcept {
        return m_tiles[y * m_width + x];
    }

    [[nodiscard]] bool setTileType(usize x, usize y, TileType type) noexcept;

    void advanceSeason() noexcept;
    [[nodiscard]] HarvestResult harvestTile(usize x, usize y) noexcept;

private:
    [[nodiscard]] BIOFUEL_FORCE_INLINE bool inBounds(usize x, usize y) const noexcept {
        return x < m_width && y < m_height;
    }
    [[nodiscard]] BIOFUEL_FORCE_INLINE usize indexOf(usize x, usize y) const noexcept {
        return (y * m_width) + x;
    }

    usize m_width = 0U;
    usize m_height = 0U;
    std::vector<Tile> m_tiles;
    Season m_season = Season::Spring;
    i32 m_year = 1;
    i32 m_moneyCents = 100000;
    FarmInventory m_inventory{};
};

[[nodiscard]] constexpr bool isCropTile(TileType type) noexcept {
    return tileMetadata(type).crop;
}

[[nodiscard]] constexpr std::optional<data::CropId> cropForTile(TileType type) noexcept {
    return tileMetadata(type).cropId;
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
