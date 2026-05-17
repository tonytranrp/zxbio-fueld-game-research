#include "game/gameplay/stages/AdvanceResearch.hpp"

namespace biofuel::game::gameplay::stages {

TechTreeInput AdvanceResearch::operator()(TechTreeInput input) const noexcept {
    // Only advance research that is InProgress.
    if (input.status != ResearchStatus::InProgress) {
        return input;
    }

    if (input.turnsRemaining > 0) {
        --input.turnsRemaining;
    }

    // If turnsRemaining reaches 0, mark as ready for unlock.
    // The UnlockTech stage will transition to Completed.
    if (input.turnsRemaining == 0) {
        input.status = ResearchStatus::Completed;
    }

    return input;
}

} // namespace biofuel::game::gameplay::stages