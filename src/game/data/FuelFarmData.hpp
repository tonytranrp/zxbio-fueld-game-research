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
};

enum class CropId : u8 {
    Corn,
    Sugarcane,
    Soybean,
    Switchgrass,
    Algae,
};

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

static_assert(kCropData.size() == 5,
              "kCropData must contain exactly 5 CropData entries (Corn, Sugarcane, Soybean, Switchgrass, Algae)");

inline constexpr std::array<FuelMarketData, 3> kFuelMarketData{{
    {FuelKind::Ethanol, "Ethanol", 220},
    {FuelKind::Biodiesel, "Biodiesel", 310},
    {FuelKind::CellulosicEthanol, "Cellulosic Ethanol", 260},
}};

static_assert(kFuelMarketData.size() == 3,
              "kFuelMarketData must contain exactly 3 FuelMarketData entries (Ethanol, Biodiesel, CellulosicEthanol)");

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
