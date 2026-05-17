#pragma once

#include "game/gameplay/stages/QueueResearch.hpp"
#include "game/gameplay/stages/AdvanceResearch.hpp"
#include "game/gameplay/stages/UnlockTech.hpp"
#include "game/gameplay/PipelineEventObserver.hpp"
#include <pb/pipeline.hpp>
#include <pb/runtime/sequential.hpp>

namespace biofuel::game::gameplay {

/// P3: Tech tree processing pipeline.
/// Applies: QueueResearch → AdvanceResearch → UnlockTech
using TechTreePipeline = pb::core::from<stages::TechTreeInput>
    ::then<stages::QueueResearch>
    ::then<stages::AdvanceResearch>
    ::then<stages::UnlockTech>
    ::to<stages::TechTreeOutput>;

static_assert(pb::core::ValidPipeline<TechTreePipeline>, "TechTreePipeline must be a valid pipeline");

/// Runner for the tech tree processing pipeline.
class TechTreePipelineRunner {
public:
    TechTreePipelineRunner();

    /// Run the tech tree pipeline.
    [[nodiscard]] stages::TechTreeOutput run(stages::TechTreeInput input);

private:
    using EngineType = decltype(pb::runtime::compile<TechTreePipeline>(
        pb::runtime::sequential{}));
    PipelineEventObserver m_observer;
    EngineType m_engine;
};

} // namespace biofuel::game::gameplay