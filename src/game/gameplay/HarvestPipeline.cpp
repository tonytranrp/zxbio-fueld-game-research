#include "game/gameplay/HarvestPipeline.hpp"

namespace biofuel::game::gameplay {

HarvestPipelineRunner::HarvestPipelineRunner() = default;

stages::HarvestOutput HarvestPipelineRunner::run(stages::HarvestInput input) {
    return m_runner.run(std::move(input));
}

} // namespace biofuel::game::gameplay
