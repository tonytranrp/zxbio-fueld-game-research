#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Distills fermented mash into fuel-grade ethanol.
/// Final stage in ethanol and cellulosic processing pipelines.
struct Distill {
    static constexpr std::string_view name = "Distill";

    using input_type = ProcessingInput;
    using output_type = ProcessingOutput;

    [[nodiscard]] ProcessingOutput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages