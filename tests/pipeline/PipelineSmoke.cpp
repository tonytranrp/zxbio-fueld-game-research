#include "game/gameplay/FuelProcessPipeline.hpp"
#include "game/gameplay/PipelineRunner.hpp"
#include "game/gameplay/TechTreePipeline.hpp"
#include "game/data/FuelFarmData.hpp"

// Pipeline-c- compilation and execution smoke test.
// Validates that Pipeline-c- compiles under strict flags and produces
// correct results for trivial pipeline operations.

#include <cstdio>
#include <cstdlib>
#include <pb/pipeline.hpp>
#include <pb/runtime/sequential.hpp>

// ---- Minimal stage that doubles an int ----
struct DoubleStage {
    using input_type = int;
    using output_type = int;
    int operator()(int x) const noexcept { return x * 2; }
};

// ---- Stage that adds one ----
struct AddOneStage {
    using input_type = int;
    using output_type = int;
    int operator()(int x) const noexcept { return x + 1; }
};

// ---- Pipeline definitions ----
using DoublePipeline = pb::core::from<int>
    ::then<DoubleStage>
    ::to<int>;

using DoubleAddPipeline = pb::core::from<int>
    ::then<DoubleStage>
    ::then<AddOneStage>
    ::to<int>;

// ---- Compile-time assertions ----
static_assert(pb::core::Stage<DoubleStage>, "DoubleStage must satisfy Stage concept");
static_assert(pb::core::Stage<AddOneStage>, "AddOneStage must satisfy Stage concept");
static_assert(pb::core::ValidPipeline<DoublePipeline>, "DoublePipeline must be a valid pipeline");
static_assert(pb::core::ValidPipeline<DoubleAddPipeline>, "DoubleAddPipeline must be a valid pipeline");

// ---- Compile-time property assertions ----
static_assert(pb::core::pipeline_size_v<DoublePipeline> == 1, "DoublePipeline has 1 stage");
static_assert(pb::core::pipeline_size_v<DoubleAddPipeline> == 2, "DoubleAddPipeline has 2 stages");
static_assert(std::same_as<pb::core::pipeline_input_t<DoublePipeline>, int>, "Pipeline input is int");
static_assert(std::same_as<pb::core::pipeline_output_t<DoublePipeline>, int>, "Pipeline output is int");

int main() {
    int failures = 0;

    // ---- Test 1: Single-stage pipeline run() ----
    {
        auto engine = pb::runtime::compile<DoublePipeline>(pb::runtime::sequential{});
        int result = engine.run(5);
        if (result != 10) {
            std::printf("FAIL: DoublePipeline(5) = %d, expected 10\n", result);
            ++failures;
        } else {
            std::printf("PASS: DoublePipeline(5) = 10\n");
        }
    }

    // ---- Test 2: Single-stage pipeline try_run() ----
    {
        auto engine = pb::runtime::compile<DoublePipeline>(pb::runtime::sequential{});
        auto result = engine.try_run(5);
        if (!result.has_value()) {
            std::printf("FAIL: DoublePipeline try_run(5) returned no value\n");
            ++failures;
        } else if (result.value() != 10) {
            std::printf("FAIL: DoublePipeline try_run(5) = %d, expected 10\n", result.value());
            ++failures;
        } else {
            std::printf("PASS: DoublePipeline try_run(5) = 10\n");
        }
    }

    // ---- Test 3: Multi-stage pipeline ----
    {
        auto engine = pb::runtime::compile<DoubleAddPipeline>(pb::runtime::sequential{});
        // 5 * 2 + 1 = 11
        int result = engine.run(5);
        if (result != 11) {
            std::printf("FAIL: DoubleAddPipeline(5) = %d, expected 11\n", result);
            ++failures;
        } else {
            std::printf("PASS: DoubleAddPipeline(5) = 11\n");
        }
    }

    // ---- Test 4: Pipeline describe() ----
    {
        auto engine = pb::runtime::compile<DoublePipeline>(pb::runtime::sequential{});
        auto desc = engine.describe();
        if (desc.empty()) {
            std::printf("FAIL: DoublePipeline describe() returned empty\n");
            ++failures;
        } else {
            std::printf("PASS: DoublePipeline describe() returned %zu stages\n", desc.size());
        }
    }

    // ---- Test 5: Observer hooks ----
    {
        auto engine = pb::runtime::compile<DoublePipeline>(pb::runtime::sequential{});

        bool onStartCalled = false;
        bool onSuccessCalled = false;

        struct TestObserver : pb::runtime::observer {
            bool& onStart;
            bool& onSuccess;
            TestObserver(bool& s, bool& e) : onStart(s), onSuccess(e) {}
            void on_stage_start(const pb::runtime::stage_id&) override { onStart = true; }
            void on_stage_success(const pb::runtime::stage_id&) override { onSuccess = true; }
        };

        TestObserver obs{onStartCalled, onSuccessCalled};
        engine.set_observer(&obs);
        (void)engine.run(7);

        if (!onStartCalled) {
            std::printf("FAIL: Observer on_stage_start was not called\n");
            ++failures;
        } else {
            std::printf("PASS: Observer on_stage_start was called\n");
        }
        if (!onSuccessCalled) {
            std::printf("FAIL: Observer on_stage_success was not called\n");
            ++failures;
        } else {
            std::printf("PASS: Observer on_stage_success was called\n");
        }
    }

    // ---- Test 6: Gameplay SequentialPipelineRunner exposes observer and is reusable ----
    {
        biofuel::game::gameplay::SequentialPipelineRunner<DoubleAddPipeline> runner;
        const int first = runner.run(5);
        const int second = runner.run(8);
        if (first != 11 || second != 17) {
            std::printf("FAIL: SequentialPipelineRunner reuse output mismatch\n");
            ++failures;
        } else if (runner.observer().has_errors()) {
            std::printf("FAIL: SequentialPipelineRunner observer recorded unexpected errors\n");
            ++failures;
        } else {
            std::printf("PASS: SequentialPipelineRunner observer is hooked and runner is reusable\n");
        }
    }

    // ---- Test 7: Gameplay fuel runner keeps branch output semantics ----
    {
        biofuel::game::gameplay::FuelProcessPipelineRunner runner;
        const biofuel::game::gameplay::stages::ProcessingInput input{
            .cropId = biofuel::game::data::CropId::Soybean,
            .quantityGallons = 48,
            .sourceTileX = 3,
            .sourceTileY = 4,
        };
        const biofuel::game::gameplay::stages::ProcessingOutput output = runner.run(input);
        if (output.fuelGallons != 48 ||
            output.revenueCents != 14880 ||
            output.fuelKind != biofuel::game::data::FuelKind::Biodiesel) {
            std::printf("FAIL: FuelProcessPipelineRunner biodiesel branch output mismatch\n");
            ++failures;
        } else {
            std::printf("PASS: FuelProcessPipelineRunner biodiesel branch output matches crop data\n");
        }
    }

    // ---- Test 8: Gameplay fuel runner falls back safely for unknown crop ids ----
    {
        biofuel::game::gameplay::FuelProcessPipelineRunner runner;
        const biofuel::game::gameplay::stages::ProcessingInput input{
            .cropId = static_cast<biofuel::game::data::CropId>(255U),
            .quantityGallons = 99,
            .sourceTileX = 1,
            .sourceTileY = 2,
        };
        const biofuel::game::gameplay::stages::ProcessingOutput output = runner.run(input);
        if (output.fuelGallons != 0 ||
            output.revenueCents != 0 ||
            output.fuelKind != biofuel::game::data::FuelKind::Ethanol) {
            std::printf("FAIL: FuelProcessPipelineRunner unknown crop fallback output mismatch\n");
            ++failures;
        } else {
            std::printf("PASS: FuelProcessPipelineRunner unknown crop falls back to default output\n");
        }
    }

    // ---- Test 9: Gameplay tech runner keeps sequential stage semantics ----
    {
        biofuel::game::gameplay::TechTreePipelineRunner runner;
        const biofuel::game::gameplay::stages::TechTreeInput input{
            .tier = biofuel::game::gameplay::stages::ResearchTier::Tier2,
            .status = biofuel::game::gameplay::stages::ResearchStatus::Available,
            .turnsRemaining = 1,
            .moneyCents = 1000,
            .researchCostCents = 250,
        };
        const biofuel::game::gameplay::stages::TechTreeOutput output = runner.run(input);
        if (output.status != biofuel::game::gameplay::stages::ResearchStatus::Completed ||
            output.turnsRemaining != 0 ||
            output.moneyCents != 750 ||
            !output.unlocked) {
            std::printf("FAIL: TechTreePipelineRunner queue/advance/unlock output mismatch\n");
            ++failures;
        } else {
            std::printf("PASS: TechTreePipelineRunner queue/advance/unlock output matches expected state\n");
        }
    }

    // ---- Summary ----
    if (failures == 0) {
        std::printf("\nAll Pipeline-c- smoke tests PASSED.\n");
        return EXIT_SUCCESS;
    } else {
        std::printf("\n%d Pipeline-c- smoke test(s) FAILED.\n", failures);
        return EXIT_FAILURE;
    }
}
