#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Transesterifies pressed oil into biodiesel.
/// Final stage in biodiesel processing pipeline.
struct Transesterify {
    using input_type = ProcessingInput;
    using output_type = ProcessingOutput;

    [[nodiscard]] ProcessingOutput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages