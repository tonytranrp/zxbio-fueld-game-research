#include "game/gameplay/TurnPipeline.hpp"

namespace biofuel::game::gameplay {

TurnPipelineRunner::TurnPipelineRunner()
    : m_engine(pb::runtime::compile<TurnPipeline>(pb::runtime::sequential{})) {
    m_engine.set_observer(&m_observer);
}

stages::TurnOutput TurnPipelineRunner::run(stages::TurnInput input) {
    return m_engine.run(std::move(input));
}

} // namespace biofuel::game::gameplay