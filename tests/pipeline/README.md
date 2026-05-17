# Pipeline Smoke Tests

Smoke tests that verify Pipeline-c- compilation, execution, and the correctness of the three gameplay pipelines in isolation.

## Test files

| File | Scope | What it validates |
|---|---|---|
| `PipelineSmoke.cpp` | Pipeline-c- library itself | Compilation of stages, pipelines, engines, observers, `run()`, `try_run()`, `describe()` |
| `TurnPipelineSmoke.cpp` | Gameplay turn pipeline | Season transitions, year wrapping, crop growth rates per season, full year cycle |
| `HarvestPipelineSmoke.cpp` | Gameplay harvest pipeline | Per-crop yield values, revenue calculation, fallow rejection, pipeline vs. manual equivalence |

## Test design

Each smoke test is a standalone executable (`int main()`) linked against `biofuel_game`. Tests use no test framework — they report PASS/FAIL to stdout and return `EXIT_SUCCESS` or `EXIT_FAILURE`.

### Structure pattern

```cpp
namespace {
bool check(const bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        return false;
    }
    return true;
}
} // namespace

int main() {
    bool ok = true;

    // Scenario: ...
    {
        // Setup
        // Execute
        // Assert
        ok = check(actual == expected, "message") && ok;
    }

    // More scenarios...

    if (ok) {
        std::printf("\nAll ... smoke tests PASSED.\n");
        return EXIT_SUCCESS;
    }
    std::printf("\n... smoke test(s) FAILED.\n");
    return EXIT_FAILURE;
}
```

### PipelineSmoke.cpp

Validates the Pipeline-c- library in isolation with minimal stages (`DoubleStage`, `AddOneStage`):

- **Test 1:** Single-stage `run()` produces correct output (5 → 10).
- **Test 2:** Single-stage `try_run()` returns `std::optional` with value.
- **Test 3:** Multi-stage pipeline (5 → 10 → 11).
- **Test 4:** `describe()` returns non-empty stage metadata.
- **Test 5:** Observer hooks (`on_stage_start`, `on_stage_success`) fire during execution.
- **Compile-time:** `static_assert` on `Stage<DoubleStage>`, `Stage<AddOneStage>`, `ValidPipeline<DoublePipeline>`, `ValidPipeline<DoubleAddPipeline>`, `pipeline_size_v`, `pipeline_input_t`, `pipeline_output_t`.

### TurnPipelineSmoke.cpp

Validates the `TurnPipeline` (`SeasonAdvance → CropGrowth → EcologyUpdate → EconomyUpdate`):

- **Scenario 1:** Spring → Summer transition with planted Corn tile. Verifies season, year, and crop age after pipeline processing.
- **Scenario 2:** Winter → Spring wrap with year increment.
- **Scenario 3:** Full year cycle (4 turns) through the pipeline. Verifies cumulative crop growth (age = 8 after 4 seasons of pipeline growth).
- **Scenario 4:** Sequential season transitions (Spring→Summer→Fall→Winter→Spring) in isolated runs.

### HarvestPipelineSmoke.cpp

Validates the `HarvestPipeline` (`ValidateCrop → CalculateYield → UpdateInventory`) plus the post-pipeline mutation in `HarvestPipelineRunner`:

- **Scenarios 1–5:** Harvest each crop type (Corn, Soybean, Sugarcane, Switchgrass, Algae) and verify yield and revenue against known data values.
- **Scenario 6:** Attempt harvest on a Fallow tile — verifies zero output and no mutation.
- **Scenario 7:** Compare pipeline output against `FarmState::harvestTile()` for all five crop types — validates functional equivalence.

## Build and run

These tests are registered as CTest targets in `src/CMakeLists.txt`:

```cmake
add_executable(BiofuelPipelineSmokeTest
    "${CMAKE_SOURCE_DIR}/tests/pipeline/PipelineSmoke.cpp")
target_link_libraries(BiofuelPipelineSmokeTest PRIVATE biofuel_game)
add_test(NAME BiofuelPipelineSmoke COMMAND BiofuelPipelineSmokeTest)

add_executable(BiofuelTurnPipelineSmokeTest
    "${CMAKE_SOURCE_DIR}/tests/pipeline/TurnPipelineSmoke.cpp")
target_link_libraries(BiofuelTurnPipelineSmokeTest PRIVATE biofuel_game)
add_test(NAME BiofuelTurnPipelineSmoke COMMAND BiofuelTurnPipelineSmokeTest)

add_executable(BiofuelHarvestPipelineSmokeTest
    "${CMAKE_SOURCE_DIR}/tests/pipeline/HarvestPipelineSmoke.cpp")
target_link_libraries(BiofuelHarvestPipelineSmokeTest PRIVATE biofuel_game)
add_test(NAME BiofuelHarvestPipelineSmoke COMMAND BiofuelHarvestPipelineSmokeTest)
```

Run all pipeline tests:

```sh
ctest -R Pipeline
```

Run a single test:

```sh
ctest -R HarvestPipeline -V
```

## Adding a new smoke test

1. Create `tests/pipeline/MyPipelineSmoke.cpp` with the pattern above.
2. Add the executable and test to `src/CMakeLists.txt` following the existing examples.
3. Link against `biofuel_game` (for gameplay pipelines) or `biofuel_engine` (for engine-level tests).
4. Keep tests deterministic and free of external dependencies (no filesystem, no network, no GPU).
