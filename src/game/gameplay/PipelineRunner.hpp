#pragma once

#include "game/gameplay/PipelineEventObserver.hpp"
#include <pb/pipeline.hpp>
#include <pb/runtime/sequential.hpp>
#include <utility>

namespace biofuel::game::gameplay {

template<typename TPipeline>
    requires pb::core::ValidPipeline<TPipeline>
class SequentialPipelineRunner {
public:
    SequentialPipelineRunner()
        : m_engine(pb::runtime::compile<TPipeline>(pb::runtime::sequential{})) {
        m_engine.set_observer(&m_observer);
    }

    template<typename TInput>
    [[nodiscard]] decltype(auto) run(TInput&& input) {
        return m_engine.run(std::forward<TInput>(input));
    }

    [[nodiscard]] const PipelineEventObserver& observer() const noexcept { return m_observer; }
    [[nodiscard]] PipelineEventObserver& observer() noexcept { return m_observer; }

private:
    using EngineType = decltype(pb::runtime::compile<TPipeline>(pb::runtime::sequential{}));

    PipelineEventObserver m_observer;
    EngineType m_engine;
};

} // namespace biofuel::game::gameplay
