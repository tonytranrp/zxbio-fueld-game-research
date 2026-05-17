#include "game/gameplay/stages/PressExtract.hpp"

namespace biofuel::game::gameplay::stages {

ProcessingInput PressExtract::operator()(ProcessingInput input) const noexcept {
    // Pass-through: extraction separates oil from biomass.
    // Yield calculation happens in Transesterify (final stage).
    return input;
}

} // namespace biofuel::game::gameplay::stages