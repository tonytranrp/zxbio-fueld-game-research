# Stages — Pipeline-c- Integration

The `stages/` folder contains pure-functional pipeline stages for the four core gameplay pipelines: **Turn**, **Harvest**, **Fuel Process**, and **Tech Tree**. Each stage is a stateless callable struct that conforms to the Pipeline-c- Stage concept, making it composable into compile-time-validated linear pipelines.

## Where stages fit in the architecture

```
┌──────────────────────────────────────────────────┐
│  Service System (FutureServiceModule)            │
│  EconomyService, EcologyService, SeasonService,  │
│  TechService, SaveService, GameStateService      │
│                                                  │
│  Each service owns one or more PipelineRunner    │
│  objects and calls runner.run() when the         │
│  corresponding game logic needs to execute.      │
└──────────────┬───────────────────────────────────┘
               │
┌──────────────▼───────────────────────────────────┐
│  Pipeline Runners                                │
│  TurnPipelineRunner, HarvestPipelineRunner,      │
│  FuelProcessPipelineRunner, TechTreePipelineRunner│
│                                                  │
│  Compile pipelines, wire observers, expose run(). │
└──────────────┬───────────────────────────────────┘
               │
┌──────────────▼───────────────────────────────────┐
│  Pipeline Definitions (in ../*Pipeline.hpp)      │
│  from<T>::then<S1>::then<S2>::to<U>              │
│  Compile-time type checking via static_assert.   │
└──────────────┬───────────────────────────────────┘
               │
┌──────────────▼───────────────────────────────────┐
│  Stages (this folder)                            │
│  Pure, stateless, noexcept callable structs.     │
│  Each stage: input → transform → output.         │
└──────────────────────────────────────────────────┘
```

Stages are the **leaves** of the pipeline tree. They know nothing about pipelines, observers, or services. They only transform data.

## The four gameplay pipelines

| Pipeline | Alias | Stages | Input → Output |
|---|---|---|---|
| **Turn** (P0) | `TurnPipeline` | `SeasonAdvance` → `CropGrowth` → `EcologyUpdate` → `EconomyUpdate` | `TurnInput` → `TurnOutput` |
| **Harvest** (P1) | `HarvestPipeline` | `ValidateCrop` → `CalculateYield` → `UpdateInventory` | `HarvestInput` → `HarvestOutput` |
| **Fuel Process** (P1) | 3 sub-pipelines | See below | `ProcessingInput` → `ProcessingOutput` |
| **Tech Tree** (P3) | `TechTreePipeline` | `QueueResearch` → `AdvanceResearch` → `UnlockTech` | `TechTreeInput` → `TechTreeOutput` |

### Fuel process sub-pipelines

The `FuelProcessPipelineRunner` uses **compile-time branch dispatch** via `pb::branch` to route a `ProcessingInput` to the correct sub-pipeline based on `FuelKind`. Branch predicates (`IsEthanol`, `IsBiodiesel`, `IsCellulosicEthanol`) inspect the crop's fuel kind and select one of three linear pipelines wrapped in a `PipelineEngineStage<T>`:

| Pipeline | Stages |
|---|---|
| `EthanolPipeline` | `WashCrop` → `GrindCrop` → `Ferment` → `Distill` |
| `BiodieselPipeline` | `WashCrop` → `PressExtract` → `Transesterify` |
| `CellulosicPipeline` | `WashCrop` → `GrindCrop` → `Pretreat` → `Ferment` → `Distill` |

All three share `ProcessingInput` / `ProcessingOutput` types, making them interchangeable behind a single `run()` call. The routing pipeline `FuelProcessRoutingPipeline` is a single-stage pipeline whose one stage is the `pb::branch` node — this is verified at compile time with `pipeline_size_v == 1`.

## How to create a new stage

A stage is a struct satisfying the **Pipeline-c- Stage concept**. The concept requires:

| Requirement | Description |
|---|---|
| `input_type` | Type alias for the input this stage accepts |
| `output_type` | Type alias for the output this stage produces |
| `operator()(input_type) const noexcept` | The transformation function |
| `error_type` (optional) | Defaults to `pb::no_error` if omitted |

### Minimal example

```cpp
// MyStage.hpp
#pragma once
#include "game/gameplay/stages/MyTypes.hpp"

namespace biofuel::game::gameplay::stages {

struct MyStage {
    using input_type = MyInput;
    using output_type = MyOutput;

    MyOutput operator()(MyInput input) const noexcept;
};

} // namespace biofuel::game::gameplay::stages
```

```cpp
// MyStage.cpp
#include "game/gameplay/stages/MyStage.hpp"

namespace biofuel::game::gameplay::stages {

MyOutput MyStage::operator()(MyInput input) const noexcept {
    // Transform input into output.
    // Must be noexcept — no exceptions allowed.
    return MyOutput{/* ... */};
}

} // namespace biofuel::game::gameplay::stages
```

### Rules

1. **Stateless.** Stages must not hold mutable state. All data flows through the input/output types.
2. **`noexcept`.** Every `operator()` must be marked `noexcept`. Pipeline-c- enforces this at compile time.
3. **Pure transform.** A stage reads its input and returns its output. No side effects, no global state access.
4. **Type aliases are mandatory.** `input_type` and `output_type` must be defined as public type aliases.
5. **Use `pb::no_error` when infallible.** If a stage cannot fail, omit `error_type` (it defaults to `pb::no_error`).

### Stages that use `pb::no_error` (void input/output)

Loading stages use `void` for both input and output with `pb::no_error`:

```cpp
struct CompileShaderStage {
    using input_type = void;
    using output_type = void;
    using error_type = pb::no_error;

    void operator()() const {}
};
```

## How to create a new pipeline

Pipelines are composed using the `from<T>::then<S>::to<U>` pattern:

```cpp
#include <pb/pipeline.hpp>
#include <pb/runtime/sequential.hpp>

using MyPipeline = pb::core::from<MyInput>
    ::then<StageOne>
    ::then<StageTwo>
    ::then<StageThree>
    ::to<MyOutput>;

// Compile-time validation
static_assert(pb::core::ValidPipeline<MyPipeline>,
    "MyPipeline must be a valid pipeline");
```

### Type compatibility rules

- `from<T>`: `T` must match `StageOne::input_type`.
- `then<S>`: `S::input_type` must match the previous stage's `output_type`.
- `to<U>`: `U` must match the last stage's `output_type`.
- Mismatched types produce a compile-time error inside `ValidPipeline`.

### Compile-time introspection

```cpp
// Number of stages in the pipeline
static_assert(pb::core::pipeline_size_v<MyPipeline> == 3);

// Input and output types
static_assert(std::same_as<pb::core::pipeline_input_t<MyPipeline>, MyInput>);
static_assert(std::same_as<pb::core::pipeline_output_t<MyPipeline>, MyOutput>);
```

## How pipeline runners work

Each pipeline has a corresponding **Runner** class that owns the compiled engine and observer:

### 1. Compile the pipeline

```cpp
using EngineType = decltype(pb::runtime::compile<MyPipeline>(
    pb::runtime::sequential{}));
EngineType m_engine;
```

`pb::runtime::compile<Pipeline>()` takes a runtime backend (here `sequential`) and returns an engine object. The engine type is inferred with `decltype` because it is a complex template instantiation.

### 2. Wire the observer

```cpp
PipelineEventObserver m_observer;

MyPipelineRunner::MyPipelineRunner()
    : m_engine(pb::runtime::compile<MyPipeline>(pb::runtime::sequential{})) {
    m_engine.set_observer(&m_observer);
}
```

The observer is set once in the constructor. The engine calls observer hooks before and after each stage executes.

### 3. Run the pipeline

```cpp
MyOutput MyPipelineRunner::run(MyInput input) {
    return m_engine.run(std::move(input));
}
```

The engine takes ownership of the input, runs each stage in sequence, and returns the final output.

### Full runner template

```cpp
// MyPipelineRunner.hpp
#pragma once
#include "game/gameplay/stages/*.hpp"
#include "game/gameplay/PipelineEventObserver.hpp"
#include <pb/pipeline.hpp>
#include <pb/runtime/sequential.hpp>

namespace biofuel::game::gameplay {

using MyPipeline = pb::core::from<MyInput>
    ::then<StageOne>
    ::then<StageTwo>
    ::to<MyOutput>;

static_assert(pb::core::ValidPipeline<MyPipeline>);

class MyPipelineRunner {
public:
    MyPipelineRunner();
    [[nodiscard]] MyOutput run(MyInput input);

private:
    using EngineType = decltype(pb::runtime::compile<MyPipeline>(
        pb::runtime::sequential{}));
    EngineType m_engine;
    PipelineEventObserver m_observer;
};

} // namespace biofuel::game::gameplay
```

### Branch/join dispatch (compile-time routing)

`FuelProcessPipelineRunner` demonstrates compile-time branch dispatch via `pb::branch`. Instead of a manual switch, predicate stages inspect the input and the routing pipeline delegates to the correct sub-pipeline at compile time.

**Step 1 — Define branch predicates.** Each predicate is a Stage that returns `bool`:

```cpp
struct IsEthanol {
    using input_type = stages::ProcessingInput;
    using output_type = bool;
    bool operator()(const stages::ProcessingInput& input) const noexcept;
};
static_assert(pb::core::Stage<IsEthanol>);
```

**Step 2 — Wrap each sub-pipeline in a PipelineEngineStage.** The branch case expects a single Stage, so each linear pipeline is wrapped:

```cpp
template <typename Pipeline>
    requires pb::core::ValidPipeline<Pipeline>
struct PipelineEngineStage {
    using input_type = stages::ProcessingInput;
    using output_type = stages::ProcessingOutput;
    stages::ProcessingOutput operator()(stages::ProcessingInput input) const;
};
```

**Step 3 — Define branch cases and the routing pipeline:**

```cpp
using IsEthanolCase = pb::case_<IsEthanol>::then<PipelineEngineStage<EthanolPipeline>>;
using IsBiodieselCase = pb::case_<IsBiodiesel>::then<PipelineEngineStage<BiodieselPipeline>>;
using IsCellulosicEthanolCase = pb::case_<IsCellulosicEthanol>::then<PipelineEngineStage<CellulosicPipeline>>;

using FuelProcessRoutingPipeline = pb::core::from<stages::ProcessingInput>
    ::branch<IsEthanolCase, IsBiodieselCase, IsCellulosicEthanolCase>
    ::to<stages::ProcessingOutput>;

static_assert(pb::core::pipeline_size_v<FuelProcessRoutingPipeline> == 1,
              "Routing pipeline contains exactly one branch stage");
```

**Step 4 — Runner compiles the routing pipeline once:**

```cpp
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
```

The runner holds a single compiled engine. At runtime, the branch predicates are evaluated in order — the first matching case runs its `PipelineEngineStage`, which internally compiles and executes the sub-pipeline.

### PassThrough utility

Several stages are placeholder/pass-through no-ops that forward input unchanged (e.g., `WashCrop`, `GrindCrop`, `Ferment`, `PressExtract`, `Pretreat`, `EconomyUpdate`). Instead of duplicating boilerplate struct definitions, these stages use a shared generic alias:

```cpp
template <typename T>
struct PassThrough {
    using input_type  = T;
    using output_type = T;
    T operator()(T val) const noexcept { return val; }
};

// Usage (header-only, no .cpp needed):
using WashCrop = PassThrough<ProcessingInput>;
using EconomyUpdate = PassThrough<TurnOutput>;
```

A `PassThrough<T>` stage satisfies the `pb::core::Stage` concept without any .cpp file, keeping stub stages to a single header line. When a stage's real implementation is added later, replace the `using` alias with a concrete struct and add the corresponding .cpp file.

## The event observer bridge

`PipelineEventObserver` bridges Pipeline-c- lifecycle hooks to the EnTT event bus. It extends `pb::runtime::observer` and overrides four hooks:

| Hook | When called | Behavior |
|---|---|---|
| `on_stage_start(id)` | Before each stage executes | Trace log only |
| `on_stage_success(id)` | After each stage completes | Publishes EnTT event via `publishEventForKey()` |
| `on_stage_failure(id, err)` | Stage returned an error | Warning log; error pushed to `m_errors` |
| `on_stage_exception(id, err)` | Stage threw an exception | Error log; error pushed to `m_errors` |

`PipelineEventObserver` also accumulates `pb::runtime::error` records from failures and exceptions, exposing `errors()`, `has_errors()`, `last_error()`, and `clear_errors()` for callers to inspect pipeline health after execution.

### Stage-to-event mapping

| Stage key | EnTT event published |
|---|---|
| `SeasonAdvance`, `turn.season_advance` | `SeasonAdvancedEvent` |
| `EcologyUpdate`, `turn.ecology_update` | `EcologyTickEvent` |
| `EconomyUpdate`, `turn.economy_update` | `EconomyTickEvent` |
| `ValidateCrop`, `CalculateYield`, `UpdateInventory`, `harvest.*` | `GameStateChangedEvent` |
| `WashCrop`, `GrindCrop`, `Ferment`, `PressExtract`, `Pretreat`, `Distill`, `Transesterify` | `EconomyTickEvent` |
| `QueueResearch`, `AdvanceResearch`, `UnlockTech` | `TechUnlockedEvent` |
| Unknown keys | Silently ignored (no crash) |

These events are consumed by other engine systems (UI updates, save triggers, achievement tracking) through the standard `Events::publish<T>()` / `Events::on<T>()` EnTT pattern.

### How stage keys are generated

Pipeline-c- generates stage keys from the struct type name. For `SeasonAdvance`, the key is `"SeasonAdvance"`. The observer also accepts dotted keys (e.g., `"turn.season_advance"`) for forward compatibility with explicit stage naming.

## FutureServiceModule backends

Each game service holds one or more pipeline runners and calls them when its domain logic executes:

| Service | Runners owned | When invoked |
|---|---|---|
| `EconomyService` | `TurnPipelineRunner`, `HarvestPipelineRunner`, `FuelProcessPipelineRunner` | Economic ticks, harvest actions, fuel processing |
| `EcologyService` | `TurnPipelineRunner` | Ecology calculations per turn |
| `SeasonService` | `TurnPipelineRunner` | Season advancement per turn |
| `TechService` | `TechTreePipelineRunner` | Research queue and unlock |
| `SaveService` | *(none)* | Save/load coordination |
| `GameStateService` | *(none)* | Game state management |

Services are declared with `BIOFUEL_STATIC_SERVICE` and registered in `FutureServiceModule`:

```cpp
BIOFUEL_STATIC_SERVICE(EconomyService, "service.economy", EconomyServiceBackend);
BIOFUEL_STATIC_SERVICE(EcologyService, "service.ecology", EcologyServiceBackend);
// ...
BIOFUEL_SERVICE_MODULE(FutureServiceModule,
    EconomyService, EcologyService, SeasonService,
    TechService, SaveService, GameStateService)
```

Multiple services can own the same runner type (e.g., `TurnPipelineRunner` is owned by `EconomyService`, `EcologyService`, and `SeasonService`) because each service's backend is a separate instance. The pipeline is re-entrant since stages are stateless.

## Loading stages

The `src/game/screens/loading/` folder defines pipeline stages for the loading screen task queue. These stages differ from gameplay stages:

| Stage | Purpose |
|---|---|
| `CompileShaderStage` | Shader compilation task |
| `LoadModelStage` | Model preload task |
| `InitSystemStage` | System initialization task |

Loading stages use `void` input/output with `pb::no_error` and are wrapped via `makeLoadingTask<Pipeline>()`:

```cpp
template<typename Pipeline>
LoadingTask makeLoadingTask(std::string name, f32 weight) {
    return LoadingTask{
        .name = std::move(name),
        .weight = weight,
        .work = []() {
            auto engine = pb::runtime::compile<Pipeline>(pb::runtime::sequential{});
            engine.run();
        },
    };
}
```

Each loading task is a self-contained pipeline compiled and executed on demand during the loading screen.

## Reference: Pipeline-c- library concepts

The project uses [Pipeline-c-](https://github.com/tonytranrp/Pipeline-c-), fetched via CPM at build time.

### Core concepts

| Concept | Location | Description |
|---|---|---|
| `pb::core::Stage<S>` | `pb/pipeline.hpp` | Concept: struct with `input_type`, `output_type`, `operator()(input_type) const noexcept` |
| `pb::core::ValidPipeline<P>` | `pb/pipeline.hpp` | Concept: validates that a `from::then::to` chain is type-compatible at compile time |
| `pb::core::from<T>` | `pb/pipeline.hpp` | Pipeline builder entry point |
| `pb::core::then<S>` | `pb/pipeline.hpp` | Appends a stage to the pipeline |
| `pb::core::to<U>` | `pb/pipeline.hpp` | Finalizes the pipeline with the output type |
| `pb::no_error` | `pb/pipeline.hpp` | Tag type for infallible stages |

### Runtime concepts

| Concept | Location | Description |
|---|---|---|
| `pb::runtime::compile<P>(backend)` | `pb/runtime/sequential.hpp` | Compiles a pipeline into a runnable engine |
| `pb::runtime::sequential` | `pb/runtime/sequential.hpp` | Single-threaded sequential execution backend |
| `pb::runtime::observer` | `pb/runtime/observer.hpp` | Observer interface for stage lifecycle hooks |
| `engine.run(input)` | Engine method | Executes the pipeline, returns `output_type` |
| `engine.try_run(input)` | Engine method | Executes the pipeline, returns `std::optional<output_type>` |
| `engine.set_observer(obs)` | Engine method | Attaches an observer to the pipeline engine |
| `engine.describe()` | Engine method | Returns stage metadata for introspection |

### Compile-time properties

```cpp
pb::core::pipeline_size_v<Pipeline>     // Number of stages
pb::core::pipeline_input_t<Pipeline>    // Input type
pb::core::pipeline_output_t<Pipeline>   // Output type
pb::core::Stage<S>                      // Concept check for a stage
pb::core::ValidPipeline<P>             // Concept check for a pipeline
```

## File index

```
stages/
├── README.md                  ← This file
├── TurnTypes.hpp              TurnInput / TurnOutput
├── HarvestTypes.hpp           HarvestInput / HarvestOutput
├── ProcessTypes.hpp           ProcessingInput / ProcessingOutput
├── TechTreeTypes.hpp          TechTreeInput / TechTreeOutput + enums
├── SeasonAdvance.hpp/.cpp     Turn pipeline stage 1
├── CropGrowth.hpp/.cpp        Turn pipeline stage 2
├── EcologyUpdate.hpp/.cpp     Turn pipeline stage 3
├── EconomyUpdate.hpp          Turn pipeline stage 4 (PassThrough)
├── ValidateCrop.hpp/.cpp      Harvest pipeline stage 1
├── CalculateYield.hpp/.cpp    Harvest pipeline stage 2
├── UpdateInventory.hpp/.cpp   Harvest pipeline stage 3
├── WashCrop.hpp              Fuel process stage (all pipelines, PassThrough)
├── GrindCrop.hpp             Fuel process stage (ethanol, cellulosic, PassThrough)
├── Ferment.hpp               Fuel process stage (ethanol, cellulosic, PassThrough)
├── Distill.hpp/.cpp          Fuel process stage (ethanol, cellulosic)
├── PressExtract.hpp          Fuel process stage (biodiesel, PassThrough)
├── Transesterify.hpp/.cpp    Fuel process stage (biodiesel)
├── Pretreat.hpp              Fuel process stage (cellulosic only, PassThrough)
├── PassThrough.hpp           Generic pass-through utility for stub/placeholder stages
├── QueueResearch.hpp/.cpp     Tech tree stage 1
├── AdvanceResearch.hpp/.cpp   Tech tree stage 2
└── UnlockTech.hpp/.cpp        Tech tree stage 3
```
