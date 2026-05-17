#include "game/gameplay/stages/UnlockTech.hpp"

namespace biofuel::game::gameplay::stages {

TechTreeOutput UnlockTech::operator()(TechTreeInput input) const noexcept {
    TechTreeOutput output{};
    output.status = input.status;
    output.turnsRemaining = input.turnsRemaining;
    output.moneyCents = input.moneyCents;

    // Unlock techs that have completed research.
    if (input.status == ResearchStatus::Completed) {
        output.unlocked = true;
    }

    return output;
}

} // namespace biofuel::game::gameplay::stages