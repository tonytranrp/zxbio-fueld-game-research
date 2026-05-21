#pragma once

#include "game/gameplay/stages/SeasonAdvance.hpp"
#include "game/gameplay/stages/CropGrowth.hpp"
#include "game/gameplay/stages/EcologyUpdate.hpp"
#include "game/gameplay/stages/PassThroughStages.hpp"
#include "game/gameplay/PipelineRunner.hpp"
#include <pb/pipeline.hpp>

namespace biofuel::game::gameplay {

/// P0: Turn processing pipeline.
/// Applies: SeasonAdvance → CropGrowth → EcologyUpdate → EconomyUpdate
/// Produces identical results to calling FarmState::advanceSeason() for each turn.
using TurnPipeline = pb::core::from<stages::TurnInput>
    ::then<stages::SeasonAdvance>
    ::then<stages::CropGrowth>
    ::then<stages::EcologyUpdate>
    ::then<stages::EconomyUpdate>
    ::to<stages::TurnOutput>;

static_assert(pb::core::ValidPipeline<TurnPipeline>, "TurnPipeline must be a valid pipeline");

/// Runner for the turn processing pipeline.
/// Owns a stateful pipeline engine and observer, provides a simple run() interface.
class TurnPipelineRunner {
public:
    TurnPipelineRunner();

    /// Run the turn pipeline on the given FarmState.
    /// Returns the updated FarmState after all stages have been applied.
    [[nodiscard]] stages::TurnOutput run(stages::TurnInput input);

private:
    SequentialPipelineRunner<TurnPipeline> m_runner;
};

} // namespace biofuel::game::gameplay
