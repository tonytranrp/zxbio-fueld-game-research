#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Washes the raw crop material (removes dirt, debris).
/// First stage in all fuel processing pipelines.
struct WashCrop {
    using input_type = ProcessingInput;
    using output_type = ProcessingInput;

    ProcessingInput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages