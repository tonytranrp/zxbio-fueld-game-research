#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Ferments the ground mash into ethanol.
/// Used in ethanol and cellulosic processing pipelines.
struct Ferment {
    using input_type = ProcessingInput;
    using output_type = ProcessingInput;

    ProcessingInput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages