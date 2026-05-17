#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Pretreats cellulosic biomass before fermentation.
/// Breaks down lignin and hemicellulose to make cellulose accessible.
/// Used in cellulosic processing pipeline (between GrindCrop and Ferment).
struct Pretreat {
    using input_type = ProcessingInput;
    using output_type = ProcessingInput;

    ProcessingInput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages