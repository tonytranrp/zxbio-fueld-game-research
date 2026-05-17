#pragma once

#include "engine/core/Types.hpp"
#include "game/data/FuelFarmData.hpp"

namespace biofuel::game::gameplay::stages {

/// Input for the fuel processing pipeline.
/// Represents a harvested crop ready for processing into fuel.
struct ProcessingInput {
    data::CropId cropId = data::CropId::Corn;
    i32 quantityGallons = 0;     // Gallons of raw crop material
    i32 sourceTileX = 0;          // Source tile coordinates (for traceability)
    i32 sourceTileY = 0;
};

/// Output of the fuel processing pipeline.
/// Contains the processed fuel and byproducts.
struct ProcessingOutput {
    i32 fuelGallons = 0;          // Gallons of processed fuel
    i64 revenueCents = 0;         // Revenue from fuel sale (i64 to prevent overflow UB)
    data::FuelKind fuelKind = data::FuelKind::Ethanol;  // Type of fuel produced
};

/// Shared helper: computes fuel output from a processing input.
/// Looks up crop data, multiplies gallons by fuel price (promoted to i64),
/// and returns the appropriate ProcessingOutput. Used by Distill and Transesterify.
[[nodiscard]] inline ProcessingOutput computeFuelOutput(const ProcessingInput& input) noexcept {
    const std::optional<data::CropData> crop = data::cropData(input.cropId);
    if (!crop.has_value()) {
        return ProcessingOutput{};
    }
    const i32 gallons = input.quantityGallons;
    const i64 revenue = static_cast<i64>(gallons) * static_cast<i64>(data::fuelPriceCentsPerGallon(crop->fuelKind));
    return ProcessingOutput{
        .fuelGallons = gallons,
        .revenueCents = revenue,
        .fuelKind = crop->fuelKind,
    };
}

} // namespace biofuel::game::gameplay::stages