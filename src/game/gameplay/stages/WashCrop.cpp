#include "game/gameplay/stages/WashCrop.hpp"

namespace biofuel::game::gameplay::stages {

ProcessingInput WashCrop::operator()(ProcessingInput input) const noexcept {
    // Pass-through: washing removes dirt/debris but doesn't change yield quantity.
    // In a full simulation, this would apply a small efficiency factor.
    return input;
}

} // namespace biofuel::game::gameplay::stages