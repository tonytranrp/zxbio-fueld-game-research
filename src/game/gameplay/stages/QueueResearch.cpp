#include "game/gameplay/stages/QueueResearch.hpp"

namespace biofuel::game::gameplay::stages {

[[nodiscard]] TechTreeInput QueueResearch::operator()(TechTreeInput input) const noexcept {
    // Can only queue if the tech is Available and we have enough money.
    if (input.status != ResearchStatus::Available) {
        return input;
    }

    if (input.moneyCents < input.researchCostCents) {
        // Not enough money to start research
        return input;
    }

    input.status = ResearchStatus::InProgress;
    input.moneyCents -= input.researchCostCents;
    return input;
}

} // namespace biofuel::game::gameplay::stages