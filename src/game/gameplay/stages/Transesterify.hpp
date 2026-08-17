#pragma once

#include "game/gameplay/stages/ProcessTypes.hpp"
#include <string_view>

namespace biofuel::game::gameplay::stages {

/// Transesterifies pressed oil into biodiesel.
/// Final stage in biodiesel processing pipeline.
struct Transesterify {
    static constexpr std::string_view name = "Transesterify";

    using input_type = ProcessingInput;
    using output_type = ProcessingOutput;

    [[nodiscard]] ProcessingOutput operator()(ProcessingInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages