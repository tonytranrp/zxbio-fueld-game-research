#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Grinds the washed crop into a fine mash.
/// Used in ethanol and cellulosic processing pipelines.
struct GrindCrop {
    using input_type = ProcessingInput;
    using output_type = ProcessingInput;

    ProcessingInput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages