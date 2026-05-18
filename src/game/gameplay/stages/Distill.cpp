#include "game/gameplay/stages/Distill.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] ProcessingOutput Distill::operator()(ProcessingInput input) const noexcept {
    return computeFuelOutput(input);
}

} // namespace biofuel::game::gameplay::stages