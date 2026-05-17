#include "game/gameplay/stages/Ferment.hpp"

namespace biofuel::game::gameplay::stages {

ProcessingInput Ferment::operator()(ProcessingInput input) const noexcept {
    // Pass-through: fermentation converts sugars to ethanol.
    // Yield calculation happens in Distill (final stage).
    return input;
}

} // namespace biofuel::game::gameplay::stages