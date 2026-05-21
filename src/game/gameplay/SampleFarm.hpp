#pragma once

#include "game/gameplay/FarmState.hpp"
#include <memory>

namespace biofuel::game::gameplay {

[[nodiscard]] std::unique_ptr<FarmState> createSampleFarm();

} // namespace biofuel::game::gameplay
