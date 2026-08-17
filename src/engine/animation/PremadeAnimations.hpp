#pragma once

#include "Animation.hpp"
#include <memory>
#include <raylib.h>

namespace biofuel::engine::animation::PremadeAnimations {

// ------------------------------------------------------------------------------
// makeFloatLerp
// Generic float interpolation for any numeric value.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<f32>> makeFloatLerp(
    std::string name,
    f32 start,
    f32 end,
    f32 duration,
    Easing::Fn easing = Easing::easeInOutQuad
) {
    return std::make_unique<Animation<f32>>(
        std::move(name),
        start,
        end,
        duration,
        easing
    );
}

} // namespace biofuel::engine::animation::PremadeAnimations
