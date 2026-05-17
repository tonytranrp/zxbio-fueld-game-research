#include "game/gameplay/FuelProcessPipeline.hpp"
#include "game/data/FuelFarmData.hpp"

namespace biofuel::game::gameplay {

FuelProcessPipelineRunner::FuelProcessPipelineRunner()
    : m_ethanol(pb::runtime::compile<EthanolPipeline>(pb::runtime::sequential{}))
    , m_biodiesel(pb::runtime::compile<BiodieselPipeline>(pb::runtime::sequential{}))
    , m_cellulosic(pb::runtime::compile<CellulosicPipeline>(pb::runtime::sequential{})) {
    m_ethanol.set_observer(&m_observer);
    m_biodiesel.set_observer(&m_observer);
    m_cellulosic.set_observer(&m_observer);
}

stages::ProcessingOutput FuelProcessPipelineRunner::run(stages::ProcessingInput input) {
    // Look up the FuelKind for this crop to select the pipeline.
    const std::optional<data::CropData> crop = data::cropData(input.cropId);
    if (!crop.has_value()) {
        return stages::ProcessingOutput{};
    }

    switch (crop->fuelKind) {
        case data::FuelKind::Ethanol:
            return m_ethanol.run(std::move(input));
        case data::FuelKind::Biodiesel:
            return m_biodiesel.run(std::move(input));
        case data::FuelKind::CellulosicEthanol:
            return m_cellulosic.run(std::move(input));
        default:
            return stages::ProcessingOutput{};
    }
}

} // namespace biofuel::game::gameplay