#include "game/gameplay/stages/SeasonAdvance.hpp"
#include "game/gameplay/FarmState.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] TurnOutput SeasonAdvance::operator()(TurnInput input) const noexcept {
    // Replicate FarmState::advanceSeason() logic:
    // After Winter, the year increments and season wraps to Spring.
    FarmState& farm = input.farmState;

    // advanceSeason is called on the mutable state
    farm.advanceSeason();

    return TurnOutput{.farmState = farm};
}

} // namespace biofuel::game::gameplay::stages