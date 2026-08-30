//! Global carbon budget -- one number every emitting/sequestering system
//! pushes, mirroring the real IPCC framing this game is grounded in (the
//! research doc this project was handed: a finite remaining CO2 budget,
//! currently ~170 GtCO2 in the real world, that gets consumed by emissions
//! and only partially offset by sinks). Deliberately a single global meter
//! with real opportunity cost, not a per-system side-counter -- see
//! Anno 2070's CO2 Reservoir design (every building pushes the same meter,
//! fixing it costs a scarce slot) for why a single shared number reads as
//! consequential where N separate counters would read as decoration.
#![forbid(unsafe_code)]

use bevy::prelude::Resource;

#[derive(Resource)]
pub(crate) struct CarbonBudget {
    // Both in arbitrary game-tonnes, not real tonnes -- the ratio between
    // them (how much of the budget a given action consumes) is what needs
    // to feel consequential, not a literal unit match to the real ~170
    // GtCO2 figure.
    total_budget: f32,
    emitted: f32,
}

impl Default for CarbonBudget {
    fn default() -> Self {
        // Deliberately generous at game start (this is a farm-building
        // phase, not yet the crisis phase) -- tune down as more emitting
        // systems (fuel combustion) come online in later iterations.
        Self {
            total_budget: 1000.0,
            emitted: 0.0,
        }
    }
}

impl CarbonBudget {
    pub(crate) fn remaining(&self) -> f32 {
        self.total_budget - self.emitted
    }

    // The raw cumulative total, not net against total_budget -- water.rs's
    // own pond-pH coupling responds to how much CO2 has actually been
    // emitted so far (the real driver of dissolved-CO2/acidification
    // chemistry), which remaining() alone can't isolate since it's also a
    // function of total_budget (a game-balance number, not a chemistry
    // input).
    pub(crate) fn emitted(&self) -> f32 {
        self.emitted
    }

    // Also read by water.rs -- pH is modeled relative to what FRACTION of
    // the budget has been consumed (emitted/total_budget), not the raw
    // emitted tonnage alone, so a game later tuned to a smaller or larger
    // total_budget still produces the same acidification curve shape.
    pub(crate) fn total_budget(&self) -> f32 {
        self.total_budget
    }

    // Positive amount = net emission (combustion, decay); negative =
    // sequestration (a crop reaching maturity, biochar). No clamping here
    // deliberately -- remaining() can go negative, same as the real budget
    // can be "exhausted" and then overshot; that's the point, not a bug to
    // guard against.
    pub(crate) fn add_emission(&mut self, amount: f32) {
        self.emitted += amount;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sequestration_reduces_net_emitted_and_grows_remaining_budget() {
        let mut budget = CarbonBudget::default();
        let before = budget.remaining();
        budget.add_emission(-10.0);
        assert!(budget.remaining() > before, "sequestration should grow the remaining budget");
    }

    #[test]
    fn emission_shrinks_remaining_budget_and_can_go_negative() {
        let mut budget = CarbonBudget {
            total_budget: 10.0,
            emitted: 0.0,
        };
        budget.add_emission(15.0);
        assert!(budget.remaining() < 0.0, "overshooting the budget must be representable, not clamped away");
    }
}
