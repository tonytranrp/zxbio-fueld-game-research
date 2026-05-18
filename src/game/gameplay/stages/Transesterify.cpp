#include "game/gameplay/stages/Transesterify.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] ProcessingOutput Transesterify::operator()(ProcessingInput input) const noexcept {
    return computeFuelOutput(input);
}

} // namespace biofuel::game::gameplay::stages