#include "game/gameplay/TechTreePipeline.hpp"

namespace biofuel::game::gameplay {

TechTreePipelineRunner::TechTreePipelineRunner()
    : m_engine(pb::runtime::compile<TechTreePipeline>(pb::runtime::sequential{})) {
    m_engine.set_observer(&m_observer);
}

stages::TechTreeOutput TechTreePipelineRunner::run(stages::TechTreeInput input) {
    return m_engine.run(std::move(input));
}

} // namespace biofuel::game::gameplay