#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Presses/extracts oil from oilseed crops (soybean, algae).
/// Used in biodiesel processing pipeline (instead of Ferment).
struct PressExtract {
    using input_type = ProcessingInput;
    using output_type = ProcessingInput;

    ProcessingInput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages