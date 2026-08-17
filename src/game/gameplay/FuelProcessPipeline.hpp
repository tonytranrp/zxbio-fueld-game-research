#pragma once

#include "game/gameplay/stages/PassThroughStages.hpp"
#include "game/gameplay/stages/Distill.hpp"
#include "game/gameplay/stages/Transesterify.hpp"
#include "game/gameplay/stages/ProcessTypes.hpp"
#include "game/gameplay/PipelineRunner.hpp"
#include <pb/pipeline.hpp>

namespace biofuel::game::gameplay {

// ── Individual fuel processing pipelines (linear definitions) ─────────────

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

// ── Branch predicates ─────────────────────────────────────────────────────
// Each predicate inspects ProcessingInput and returns true if the crop's
// FuelKind matches the branch case. These are used by the routing pipeline
// to select the correct processing path at compile-time via pb::branch.

struct IsEthanol {
    using input_type = stages::ProcessingInput;
    using output_type = bool;

    [[nodiscard]] bool operator()(const stages::ProcessingInput& input) const noexcept;
};

struct IsBiodiesel {
    using input_type = stages::ProcessingInput;
    using output_type = bool;

    [[nodiscard]] bool operator()(const stages::ProcessingInput& input) const noexcept;
};

struct IsCellulosicEthanol {
    using input_type = stages::ProcessingInput;
    using output_type = bool;

    [[nodiscard]] bool operator()(const stages::ProcessingInput& input) const noexcept;
};

static_assert(pb::core::Stage<IsEthanol>, "IsEthanol must satisfy Stage concept");
static_assert(pb::core::Stage<IsBiodiesel>, "IsBiodiesel must satisfy Stage concept");
static_assert(pb::core::Stage<IsCellulosicEthanol>, "IsCellulosicEthanol must satisfy Stage concept");

// ── Pipeline wrapper stage ────────────────────────────────────────────────
// Wraps a compiled pipeline into a single Stage so it can appear in a
// pb::case_<Predicate>::then<Stage> branch case.

template <typename Pipeline>
    requires pb::core::ValidPipeline<Pipeline>
struct PipelineEngineStage {
    using input_type = stages::ProcessingInput;
    using output_type = stages::ProcessingOutput;

    [[nodiscard]] stages::ProcessingOutput operator()(stages::ProcessingInput input) const;
};

// ── Branch-case aliases ───────────────────────────────────────────────────

using IsEthanolCase = pb::case_<IsEthanol>::then<PipelineEngineStage<EthanolPipeline>>;
using IsBiodieselCase = pb::case_<IsBiodiesel>::then<PipelineEngineStage<BiodieselPipeline>>;
using IsCellulosicEthanolCase = pb::case_<IsCellulosicEthanol>::then<PipelineEngineStage<CellulosicPipeline>>;

// ── Routing pipeline (compile-time branch) ────────────────────────────────
/// P1: Fuel processing routing pipeline.
/// Routes ProcessingInput through the correct sub-pipeline based on FuelKind.
/// Replaces manual if/else dispatch with Pipeline-c- branch semantics.
using FuelProcessRoutingPipeline = pb::core::from<stages::ProcessingInput>
    ::branch<IsEthanolCase, IsBiodieselCase, IsCellulosicEthanolCase>
    ::to<stages::ProcessingOutput>;

static_assert(pb::core::ValidPipeline<FuelProcessRoutingPipeline>,
              "FuelProcessRoutingPipeline must be a valid pipeline");
static_assert(pb::core::pipeline_size_v<FuelProcessRoutingPipeline> == 1,
              "Routing pipeline contains exactly one branch stage");

// ── Runner ────────────────────────────────────────────────────────────────

/// Runner for fuel processing pipelines.
/// Compile-time branch routes ProcessingInput to the correct sub-pipeline
/// based on FuelKind, replacing the previous manual if/else dispatch.
class FuelProcessPipelineRunner {
public:
    FuelProcessPipelineRunner();

    /// Run the appropriate fuel processing pipeline based on the crop's FuelKind.
    /// Returns ProcessingOutput with fuel volume, revenue, and fuel kind.
    [[nodiscard]] stages::ProcessingOutput run(stages::ProcessingInput input);

private:
    SequentialPipelineRunner<FuelProcessRoutingPipeline> m_runner;
};

} // namespace biofuel::game::gameplay
