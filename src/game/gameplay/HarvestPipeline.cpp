#include "game/gameplay/HarvestPipeline.hpp"

namespace biofuel::game::gameplay {

HarvestPipelineRunner::HarvestPipelineRunner()
    : m_engine(pb::runtime::compile<HarvestPipeline>(pb::runtime::sequential{})) {
    m_engine.set_observer(&m_observer);
}

stages::HarvestOutput HarvestPipelineRunner::run(stages::HarvestInput input) {
    return m_engine.run(std::move(input));
}

} // namespace biofuel::game::gameplay