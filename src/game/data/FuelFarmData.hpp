#pragma once

#include "engine/core/Types.hpp"
#include <array>
#include <optional>
#include <string_view>

namespace biofuel::game::data {

enum class FuelKind : u8 {
    Ethanol,
    Biodiesel,
    CellulosicEthanol,
    Count,
};

enum class CropId : u8 {
    Corn,
    Sugarcane,
    Soybean,
    Switchgrass,
    Algae,
    Count,
};

inline constexpr usize kFuelKindCount = static_cast<usize>(FuelKind::Count);
inline constexpr usize kCropIdCount = static_cast<usize>(CropId::Count);

struct CropData {
    CropId id;
    std::string_view name;
    FuelKind fuelKind;
    i32 yieldGallonsPerAcre;
    i32 waterNeed;
    i32 fertilizerNeed;
    i32 landImpact;
    i32 carbonScore;
    i32 energyPerGallonBtu;
};

struct FuelMarketData {
    FuelKind kind;
    std::string_view name;
    i32 priceCentsPerGallon;
};

// Thin vertical-slice balance table from the approved Agents.md plan.
inline constexpr std::array<CropData, 5> kCropData{{
    {CropId::Corn, "Corn (Ethanol)", FuelKind::Ethanol, 400, 4, 4, 4, 3, 76330},
    {CropId::Sugarcane, "Sugarcane (Ethanol)", FuelKind::Ethanol, 590, 5, 4, 3, 5, 76330},
    {CropId::Soybean, "Soybean (Biodiesel)", FuelKind::Biodiesel, 48, 3, 2, 4, 4, 118300},
    {CropId::Switchgrass, "Switchgrass (Cel.)", FuelKind::CellulosicEthanol, 300, 2, 1, 1, 9, 76330},
    {CropId::Algae, "Algae (Biodiesel)", FuelKind::Biodiesel, 5000, 2, 1, 1, 10, 118300},
}};

static_assert(kCropData.size() == kCropIdCount,
              "kCropData must contain exactly one entry per CropId");

inline constexpr std::array<FuelMarketData, 3> kFuelMarketData{{
    {FuelKind::Ethanol, "Ethanol", 220},
    {FuelKind::Biodiesel, "Biodiesel", 310},
    {FuelKind::CellulosicEthanol, "Cellulosic Ethanol", 260},
}};

static_assert(kFuelMarketData.size() == kFuelKindCount,
              "kFuelMarketData must contain exactly one entry per FuelKind");

namespace detail {

// CropId/FuelKind are never switch()'d on anywhere in the codebase (only
// compared with ==), so adding Count above cannot break an exhaustive switch.

[[nodiscard]] consteval bool cropIdBijective() {
    for (usize i = 0; i < kCropIdCount; ++i) {
        usize count = 0;
        for (const CropData& crop : kCropData) {
            if (crop.id == static_cast<CropId>(i)) {
                ++count;
            }
        }
        if (count != 1) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] consteval bool fuelKindBijective() {
    for (usize i = 0; i < kFuelKindCount; ++i) {
        usize count = 0;
        for (const FuelMarketData& fuel : kFuelMarketData) {
            if (fuel.kind == static_cast<FuelKind>(i)) {
                ++count;
            }
        }
        if (count != 1) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] consteval bool everyCropHasFuelMarketEntry() {
    for (const CropData& crop : kCropData) {
        bool found = false;
        for (const FuelMarketData& fuel : kFuelMarketData) {
            if (fuel.kind == crop.fuelKind) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

} // namespace detail

// Bijection checks: catch a missing id, a duplicate id, and (combined with the
// size checks above) an out-of-range id in either table at compile time.
static_assert(detail::cropIdBijective(),
              "kCropData must contain exactly one entry per CropId (no missing, no duplicate)");
static_assert(detail::fuelKindBijective(),
              "kFuelMarketData must contain exactly one entry per FuelKind (no missing, no duplicate)");
static_assert(detail::everyCropHasFuelMarketEntry(),
              "Every CropData::fuelKind must have a kFuelMarketData entry (fuelPriceCentsPerGallon "
              "silently returns 0 for an unmatched kind otherwise)");

[[nodiscard]] constexpr std::optional<CropData> cropData(const CropId id) noexcept {
    for (const CropData& crop : kCropData) {
        if (crop.id == id) {
            return crop;
        }
    }
    return std::nullopt;
}

[[nodiscard]] constexpr i32 fuelPriceCentsPerGallon(const FuelKind kind) noexcept {
    for (const FuelMarketData& fuel : kFuelMarketData) {
        if (fuel.kind == kind) {
            return fuel.priceCentsPerGallon;
        }
    }
    return 0;
}

} // namespace biofuel::game::data
