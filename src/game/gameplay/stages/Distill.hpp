#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Distills fermented mash into fuel-grade ethanol.
/// Final stage in ethanol and cellulosic processing pipelines.
struct Distill {
    using input_type = ProcessingInput;
    using output_type = ProcessingOutput;

    ProcessingOutput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages