#pragma once

#include "game/gameplay/stages/ValidateCrop.hpp"
#include "game/gameplay/stages/CalculateYield.hpp"
#include "game/gameplay/stages/UpdateInventory.hpp"
#include "game/gameplay/PipelineRunner.hpp"
#include <pb/pipeline.hpp>

namespace biofuel::game::gameplay {

/// P1: Harvest processing pipeline.
/// Applies: ValidateCrop → CalculateYield → UpdateInventory
/// Produces identical results to calling FarmState::harvestTile(x, y).
/// Note: HarvestInput carries a raw FarmState pointer. ValidateCrop sets it to null
/// if the tile is invalid; CalculateYield checks for null before proceeding.
using HarvestPipeline = pb::core::from<stages::HarvestInput>
    ::then<stages::ValidateCrop>
    ::then<stages::CalculateYield>
    ::then<stages::UpdateInventory>
    ::to<stages::HarvestOutput>;

static_assert(pb::core::ValidPipeline<HarvestPipeline>, "HarvestPipeline must be a valid pipeline");

/// Runner for the harvest processing pipeline.
class HarvestPipelineRunner {
public:
    HarvestPipelineRunner();

    /// Run the harvest pipeline for the given tile.
    /// Returns HarvestOutput with yield data.
    [[nodiscard]] stages::HarvestOutput run(stages::HarvestInput input);

private:
    SequentialPipelineRunner<HarvestPipeline> m_runner;
};

} // namespace biofuel::game::gameplay
