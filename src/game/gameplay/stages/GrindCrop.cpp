#include "game/gameplay/stages/GrindCrop.hpp"

namespace biofuel::game::gameplay::stages {

ProcessingInput GrindCrop::operator()(ProcessingInput input) const noexcept {
    // Pass-through: grinding breaks down biomass for fermentation.
    // In a full simulation, this would apply an efficiency factor based on crop type.
    return input;
}

} // namespace biofuel::game::gameplay::stages