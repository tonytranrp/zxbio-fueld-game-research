#include "game/gameplay/FuelProcessPipeline.hpp"
#include "game/data/FuelFarmData.hpp"

namespace biofuel::game::gameplay {

// ── Predicate implementations ─────────────────────────────────────────────

bool IsEthanol::operator()(const stages::ProcessingInput& input) const noexcept {
    const std::optional<data::CropData> crop = data::cropData(input.cropId);
    return crop.has_value() && crop->fuelKind == data::FuelKind::Ethanol;
}

bool IsBiodiesel::operator()(const stages::ProcessingInput& input) const noexcept {
    const std::optional<data::CropData> crop = data::cropData(input.cropId);
    return crop.has_value() && crop->fuelKind == data::FuelKind::Biodiesel;
}

bool IsCellulosicEthanol::operator()(const stages::ProcessingInput& input) const noexcept {
    const std::optional<data::CropData> crop = data::cropData(input.cropId);
    return crop.has_value() && crop->fuelKind == data::FuelKind::CellulosicEthanol;
}

// ── PipelineEngineStage implementation ────────────────────────────────────

template <typename Pipeline>
    requires pb::core::ValidPipeline<Pipeline>
stages::ProcessingOutput PipelineEngineStage<Pipeline>::operator()(stages::ProcessingInput input) const {
    auto engine = pb::runtime::compile<Pipeline>(pb::runtime::sequential{});
    return engine.run(std::move(input));
}

// Explicit instantiations for the three fuel pipelines.
template struct PipelineEngineStage<EthanolPipeline>;
template struct PipelineEngineStage<BiodieselPipeline>;
template struct PipelineEngineStage<CellulosicPipeline>;

// ── Runner ────────────────────────────────────────────────────────────────

FuelProcessPipelineRunner::FuelProcessPipelineRunner()
    : m_engine(pb::runtime::compile<FuelProcessRoutingPipeline>(pb::runtime::sequential{})) {
    m_engine.set_observer(&m_observer);
}

stages::ProcessingOutput FuelProcessPipelineRunner::run(stages::ProcessingInput input) {
    auto result = m_engine.run(std::move(input));
    if (result.has_value()) {
        return std::move(result).value();
    }
    return stages::ProcessingOutput{};
}

} // namespace biofuel::game::gameplay