#pragma once

#include "engine/core/Types.hpp"

namespace biofuel::game::gameplay::stages {

/// Research tier information for the tech tree pipeline.
enum class ResearchTier : u8 {
    Tier1,  // Corn/Soy → basic ethanol/biodiesel
    Tier2,  // Cellulosic → switchgrass processing
    Tier3,  // Algae → advanced biodiesel
    Tier4,  // Synthetic → next-gen fuels
};

/// Research status for a tech tree entry.
enum class ResearchStatus : u8 {
    Locked,       // Prerequisites not met
    Available,    // Can be queued for research
    InProgress,   // Currently researching
    Completed,    // Research complete, unlocked
};

/// Input for the tech tree pipeline.
struct TechTreeInput {
    ResearchTier tier = ResearchTier::Tier1;
    ResearchStatus status = ResearchStatus::Locked;
    i32 turnsRemaining = 0;     // Turns left for in-progress research
    i32 moneyCents = 0;         // Available money for research cost
    i32 researchCostCents = 0;  // Cost to research this tech
};

/// Output of the tech tree pipeline.
struct TechTreeOutput {
    ResearchStatus status = ResearchStatus::Locked;
    i32 turnsRemaining = 0;
    i32 moneyCents = 0;         // Money after research cost deduction
    bool unlocked = false;       // Whether the tech was unlocked this turn
};

} // namespace biofuel::game::gameplay::stages