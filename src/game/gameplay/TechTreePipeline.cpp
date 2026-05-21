#include "game/gameplay/TechTreePipeline.hpp"

namespace biofuel::game::gameplay {

TechTreePipelineRunner::TechTreePipelineRunner() = default;

stages::TechTreeOutput TechTreePipelineRunner::run(stages::TechTreeInput input) {
    return m_runner.run(std::move(input));
}

} // namespace biofuel::game::gameplay
