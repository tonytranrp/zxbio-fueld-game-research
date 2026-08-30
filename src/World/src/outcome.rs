//! A defined win/lose condition -- the first time this game has had ANY
//! completion state at all, across eleven prior loop iterations that were
//! entirely systems/content work (crops, water chemistry, energy
//! pathways, a day/night cycle, a HUD) with no defined "you did it" or
//! "you failed" moment. An open-ended sandbox with real, citation-
//! grounded mechanics is not yet a finished GAME without something to
//! actually win or lose.
//!
//! Both conditions are tied to the same shared `CarbonBudget`/
//! `FuelStockpile` resources every other system already feeds, rather
//! than inventing new tracked state -- the same "one shared meter,
//! real opportunity cost" design `carbon.rs`'s own doc comment already
//! establishes:
//!
//! - VICTORY: `FuelStockpile` reaches `FUEL_VICTORY_LITERS`. Set just
//!   under the actual maximum possible fuel this game's fixed initial
//!   14 crops (6 corn + 4 switchgrass + 4 miscanthus, `FUEL_PER_HARVEST`
//!   = 0.3 L each, fuel.rs's own constant) can ever produce (14 * 0.3 =
//!   4.2 L) -- there is currently no replanting mechanic, so that ceiling
//!   is a real, hard limit, not a guess. Reachable within a normal play
//!   session via a small number of harvest clicks once enough crops
//!   mature.
//! - DEFEAT: `CarbonBudget::remaining()` drops to `CARBON_DEFEAT_THRESHOLD`,
//!   a meaningfully negative overshoot (not merely crossing zero) --
//!   representing the carbon budget genuinely blown through, not just
//!   momentarily dipping below intact. Reachable only through extended
//!   hydrogen-electrolyzer operation left unchecked (its own real,
//!   continuous grid-carbon draw is the dominant long-run drain on the
//!   budget -- see `hydrogen.rs`'s/`solar.rs`'s own doc comments) --
//!   thematically the right cautionary state: the game doesn't punish
//!   the player for existing, only for letting the electrolyzer's own
//!   real carbon cost run away unmitigated.
//!
//! Once decided, an outcome is STICKY -- update_outcome() never flips a
//! decided outcome back to InProgress or from one decided state to the
//! other, matching how a real game's own win/lose state works (the first
//! condition crossed is what happened, not whatever's momentarily true
//! on a later frame). Deliberately does NOT pause the simulation or
//! change scene/screen on its own -- that would need real UI-flow
//! infrastructure this first pass doesn't build; the HUD banner alone is
//! the honest scope for this iteration, said plainly here rather than
//! implied as more complete than it is.
#![forbid(unsafe_code)]

use crate::carbon::CarbonBudget;
use crate::fuel::FuelStockpile;
use bevy::app::{App, Update};
use bevy::prelude::{Res, ResMut, Resource};

#[derive(Resource, Default, Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum GameOutcome {
    #[default]
    InProgress,
    Victory,
    Defeat,
}

impl GameOutcome {
    pub(crate) fn banner(&self) -> &'static str {
        match self {
            GameOutcome::InProgress => "",
            GameOutcome::Victory => "\n*** VICTORY: sustainable fuel production achieved! ***",
            GameOutcome::Defeat => "\n*** DEFEAT: the carbon budget has collapsed. ***",
        }
    }
}

// See this module's own doc comment: just under the real 4.2 L hard
// ceiling this game's fixed initial crop count can ever produce.
const FUEL_VICTORY_LITERS: f32 = 4.0;

// See this module's own doc comment: a meaningfully negative overshoot,
// not merely crossing zero.
const CARBON_DEFEAT_THRESHOLD: f32 = -200.0;

pub(crate) fn setup(app: &mut App) {
    app.insert_resource(GameOutcome::default());
    app.add_systems(Update, update_outcome);
}

fn compute_outcome(current: GameOutcome, fuel_liters: f32, carbon_remaining: f32) -> GameOutcome {
    if current != GameOutcome::InProgress {
        return current;
    }
    if fuel_liters >= FUEL_VICTORY_LITERS {
        GameOutcome::Victory
    } else if carbon_remaining <= CARBON_DEFEAT_THRESHOLD {
        GameOutcome::Defeat
    } else {
        GameOutcome::InProgress
    }
}

fn update_outcome(fuel: Res<FuelStockpile>, carbon: Res<CarbonBudget>, mut outcome: ResMut<GameOutcome>) {
    *outcome = compute_outcome(*outcome, fuel.liters(), carbon.remaining());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reaching_the_fuel_threshold_triggers_victory() {
        let outcome = compute_outcome(GameOutcome::InProgress, FUEL_VICTORY_LITERS, 500.0);
        assert_eq!(outcome, GameOutcome::Victory, "reaching the fuel threshold should trigger victory even with a healthy carbon budget");
    }

    #[test]
    fn collapsing_the_carbon_budget_triggers_defeat() {
        let outcome = compute_outcome(GameOutcome::InProgress, 0.5, CARBON_DEFEAT_THRESHOLD);
        assert_eq!(outcome, GameOutcome::Defeat, "collapsing the carbon budget should trigger defeat even with little fuel produced");
    }

    #[test]
    fn neither_threshold_crossed_stays_in_progress() {
        let outcome = compute_outcome(GameOutcome::InProgress, 1.0, 500.0);
        assert_eq!(outcome, GameOutcome::InProgress, "with neither threshold crossed, the game should still be in progress");
    }

    #[test]
    fn a_decided_outcome_is_sticky_and_never_flips() {
        // Victory already decided -- even if carbon later collapses too,
        // the FIRST outcome reached is what happened, not the most
        // recent condition true on a given frame.
        let still_victory = compute_outcome(GameOutcome::Victory, FUEL_VICTORY_LITERS, CARBON_DEFEAT_THRESHOLD);
        assert_eq!(still_victory, GameOutcome::Victory, "a decided victory should never flip to defeat on a later frame");

        let still_defeat = compute_outcome(GameOutcome::Defeat, FUEL_VICTORY_LITERS, CARBON_DEFEAT_THRESHOLD);
        assert_eq!(still_defeat, GameOutcome::Defeat, "a decided defeat should never flip to victory on a later frame");
    }
}
