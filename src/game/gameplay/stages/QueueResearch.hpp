#pragma once

#include "game/gameplay/stages/TechTreeTypes.hpp"

namespace biofuel::game::gameplay::stages {

/// Queues a research item if prerequisites are met and money is sufficient.
/// Sets status to InProgress and deducts research cost if available.
struct QueueResearch {
    using input_type = TechTreeInput;
    using output_type = TechTreeInput;

    [[nodiscard]] TechTreeInput operator()(TechTreeInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages