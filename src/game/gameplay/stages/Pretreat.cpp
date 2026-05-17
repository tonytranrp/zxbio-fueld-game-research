#include "game/gameplay/stages/Pretreat.hpp"

namespace biofuel::game::gameplay::stages {

ProcessingInput Pretreat::operator()(ProcessingInput input) const noexcept {
    // Pass-through: pretreatment breaks down lignin before fermentation.
    // Yield modification happens in Distill (final stage for cellulosic).
    return input;
}

} // namespace biofuel::game::gameplay::stages