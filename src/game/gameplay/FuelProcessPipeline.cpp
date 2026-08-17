#include "game/gameplay/FuelProcessPipeline.hpp"
#include "game/data/FuelFarmData.hpp"

namespace biofuel::game::gameplay {

namespace {

// The sequential engine invokes branch stages as Stage{input}, so the parent
// runner's observer cannot be passed through the stage call operator. The
// runner publishes it here for PipelineEngineStage to install on its inner engine.
thread_local pb::runtime::observer* t_activeObserver = nullptr;

// RAII save/restore for t_activeObserver so a thrown exception during run()
// can't leave a stale pointer installed for the next call on this thread.
class ActiveObserverScope {
public:
    explicit ActiveObserverScope(pb::runtime::observer* const observer) noexcept
        : m_previous(t_activeObserver) {
        t_activeObserver = observer;
    }
    ~ActiveObserverScope() noexcept { t_activeObserver = m_previous; }

    ActiveObserverScope(const ActiveObserverScope&) = delete;
    ActiveObserverScope& operator=(const ActiveObserverScope&) = delete;

private:
    pb::runtime::observer* m_previous;
};

[[nodiscard]] bool isFuelKind(const stages::ProcessingInput& input, const data::FuelKind kind) noexcept {
    const std::optional<data::CropData> crop = data::cropData(input.cropId);
    return crop.has_value() && crop->fuelKind == kind;
}

} // namespace

// ── Predicate implementations ─────────────────────────────────────────────

bool IsEthanol::operator()(const stages::ProcessingInput& input) const noexcept {
    return isFuelKind(input, data::FuelKind::Ethanol);
}

bool IsBiodiesel::operator()(const stages::ProcessingInput& input) const noexcept {
    return isFuelKind(input, data::FuelKind::Biodiesel);
}

bool IsCellulosicEthanol::operator()(const stages::ProcessingInput& input) const noexcept {
    return isFuelKind(input, data::FuelKind::CellulosicEthanol);
}

// ── PipelineEngineStage implementation ────────────────────────────────────

template <typename Pipeline>
    requires pb::core::ValidPipeline<Pipeline>
stages::ProcessingOutput PipelineEngineStage<Pipeline>::operator()(stages::ProcessingInput input) const {
    auto engine = pb::runtime::compile<Pipeline>(pb::runtime::sequential{});
    engine.set_observer(t_activeObserver);
    return engine.run(std::move(input));
}

// Explicit instantiations for the three fuel pipelines.
template struct PipelineEngineStage<EthanolPipeline>;
template struct PipelineEngineStage<BiodieselPipeline>;
template struct PipelineEngineStage<CellulosicPipeline>;

// ── Runner ────────────────────────────────────────────────────────────────

FuelProcessPipelineRunner::FuelProcessPipelineRunner() = default;

stages::ProcessingOutput FuelProcessPipelineRunner::run(stages::ProcessingInput input) {
    const ActiveObserverScope observerScope(&m_runner.observer());
    auto result = m_runner.run(std::move(input));
    if (result.has_value()) {
        return std::move(result).value();
    }
    return stages::ProcessingOutput{};
}

} // namespace biofuel::game::gameplay
