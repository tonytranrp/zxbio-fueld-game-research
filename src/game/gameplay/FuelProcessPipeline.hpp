#pragma once

#include "game/gameplay/stages/WashCrop.hpp"
#include "game/gameplay/stages/GrindCrop.hpp"
#include "game/gameplay/stages/Ferment.hpp"
#include "game/gameplay/stages/Distill.hpp"
#include "game/gameplay/stages/PressExtract.hpp"
#include "game/gameplay/stages/Transesterify.hpp"
#include "game/gameplay/stages/Pretreat.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"
#include "game/data/FuelFarmData.hpp"
#include "game/gameplay/PipelineEventObserver.hpp"
#include <pb/pipeline.hpp>
#include <pb/runtime/sequential.hpp>

namespace biofuel::game::gameplay {

/// P1a: Ethanol processing pipeline.
/// Crop → Wash → Grind → Ferment → Distill → Fuel
using EthanolPipeline = pb::core::from<stages::ProcessingInput>
    ::then<stages::WashCrop>
    ::then<stages::GrindCrop>
    ::then<stages::Ferment>
    ::then<stages::Distill>
    ::to<stages::ProcessingOutput>;

static_assert(pb::core::ValidPipeline<EthanolPipeline>, "EthanolPipeline must be a valid pipeline");

/// P1b: Biodiesel processing pipeline.
/// Crop → Wash → PressExtract → Transesterify → Fuel
using BiodieselPipeline = pb::core::from<stages::ProcessingInput>
    ::then<stages::WashCrop>
    ::then<stages::PressExtract>
    ::then<stages::Transesterify>
    ::to<stages::ProcessingOutput>;

static_assert(pb::core::ValidPipeline<BiodieselPipeline>, "BiodieselPipeline must be a valid pipeline");

/// P1c: Cellulosic ethanol processing pipeline.
/// Crop → Wash → Grind → Pretreat → Ferment → Distill → Fuel
using CellulosicPipeline = pb::core::from<stages::ProcessingInput>
    ::then<stages::WashCrop>
    ::then<stages::GrindCrop>
    ::then<stages::Pretreat>
    ::then<stages::Ferment>
    ::then<stages::Distill>
    ::to<stages::ProcessingOutput>;

static_assert(pb::core::ValidPipeline<CellulosicPipeline>, "CellulosicPipeline must be a valid pipeline");

/// Runner for fuel processing pipelines.
/// Selects the correct pipeline based on FuelKind at runtime.
/// Since Pipeline-c- branch/join is compile-time only, we use a runtime switch
/// over FuelKind to select the appropriate linear pipeline.
class FuelProcessPipelineRunner {
public:
    FuelProcessPipelineRunner();

    /// Run the appropriate fuel processing pipeline based on the crop's FuelKind.
    /// Returns ProcessingOutput with fuel volume, revenue, and fuel kind.
    [[nodiscard]] stages::ProcessingOutput run(stages::ProcessingInput input);

private:
    // Three pipeline engines, one per fuel type
    using EthanolEngine = decltype(pb::runtime::compile<EthanolPipeline>(pb::runtime::sequential{}));
    using BiodieselEngine = decltype(pb::runtime::compile<BiodieselPipeline>(pb::runtime::sequential{}));
    using CellulosicEngine = decltype(pb::runtime::compile<CellulosicPipeline>(pb::runtime::sequential{}));
    PipelineEventObserver m_observer;
    EthanolEngine m_ethanol;
    BiodieselEngine m_biodiesel;
    CellulosicEngine m_cellulosic;
};

} // namespace biofuel::game::gameplay