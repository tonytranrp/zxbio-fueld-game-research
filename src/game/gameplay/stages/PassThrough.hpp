#pragma once

#include <utility>

namespace biofuel::game::gameplay::stages {

/// Generic pass-through stage that forwards input unchanged.
/// Replaces duplicate boilerplate for placeholder/stub stages (e.g. WashCrop,
/// GrindCrop, Ferment, PressExtract, Pretreat, EconomyUpdate).
///
/// Usage:
///   using WashCrop = PassThrough<ProcessingInput>;
template <typename T>
struct PassThrough {
    using input_type  = T;
    using output_type = T;

    [[nodiscard]] T operator()(T val) const noexcept { return val; }
};

} // namespace biofuel::game::gameplay::stages
