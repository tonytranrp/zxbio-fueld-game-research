#include "game/gameplay/TurnPipeline.hpp"

namespace biofuel::game::gameplay {

TurnPipelineRunner::TurnPipelineRunner() = default;

stages::TurnOutput TurnPipelineRunner::run(stages::TurnInput input) {
    return m_runner.run(std::move(input));
}

} // namespace biofuel::game::gameplay
