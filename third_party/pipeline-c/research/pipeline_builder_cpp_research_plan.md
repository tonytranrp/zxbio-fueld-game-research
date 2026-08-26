---
title: "C++ Compile-Time Pipeline Builder - Research and Full Implementation Plan"
author: "Prepared by GPT-5.5 Pro"
date: "2026-05-15"
geometry: margin=0.65in
fontsize: 9pt
colorlinks: true
linkcolor: blue
urlcolor: blue
---

# C++ Compile-Time Pipeline Builder - Research and Full Implementation Plan

Document date: 2026-05-15

Scope: design a heavily typed C++ pipeline builder whose core is compile-time templates, types, `<>`, and `::`, with a readable DSL, broad user-code integration, strong diagnostics, runtime execution backends, and build practices that reduce compile-time cost.

Important interpretation: "support every single thing the user puts in" is treated as a practical engineering goal, not a literal guarantee. C++ cannot make arbitrary ill-typed, non-callable, private, deleted, platform-specific, or undefined-behavior user code valid. The plan below provides adapters, concepts, customization points, error explanations, and escape hatches so valid user code is accepted broadly and invalid user code fails with actionable diagnostics.

Research method: this plan compares direct C++ pipeline-like libraries, C++ metaprogramming libraries, standardization work, and general pipeline/orchestration systems. The source index at the end lists the primary sources used.

Live research update: On 2026-05-15, a follow-up pass checked the existing source index for reachability, compared core claims against current primary sources, and added implementation implications where the live sources materially affect the plan. The 38 pre-existing source-index URLs all returned HTTP 200 or an official redirect during the validation pass.

Source quality policy: Treat WG21/open-std papers, project-owned documentation, official repository READMEs, CMake/LLVM/oneAPI/Boost docs, and compiler support tables as decision-grade. Treat secondary articles as useful context only. When these sources disagree or support is partial, the project should prefer feature gates, adapters, and measured experiments over unconditional dependency on the feature.

## Executive Summary

- The project should target a compile-time graph/pipeline core, not only a runtime sequence of lambdas.
- Existing C++ libraries commonly offer one strong dimension: ergonomic pipe syntax, lazy ranges, async streams, task scheduling, or type-level utilities. They do not combine all of these with a type-first DSL, `::`/`<>` syntax, graph validation, user-code adapters, and explainable diagnostics.
- The product should be positioned as a typed pipeline compiler: it accepts stage definitions, validates edges at compile time, lowers the validated plan into a runtime executor, and optionally exports the graph for debugging and documentation.
- The DSL should avoid making lambdas the default. It should use named stage objects, named policies, and named adapters. Lambdas may still be supported through explicit `pb::adapt` wrappers because users will expect them.
- The compile-time engine should use small type lists, alias-template algorithms, fold expressions, concepts, normalized traits, and explicit template instantiation. Avoid recursive template explosions and avoid expression-template capture unless it is required for a specific ergonomic feature.
- The runtime engine should be modular: sequential backend first, then thread-pool backend, then optional oneTBB, Taskflow, and `std::execution`/sender-receiver backends when compiler/library support is mature.
- Error handling should use a layered model: compile-time structural errors, runtime `std::expected` style results, exception adapter policies, assertion/contract policies, and structured diagnostic records.
- The first release should be small and precise: linear pipelines, named stages, stage traits, `std::expected` result propagation, static diagnostics, and a sequential executor.
- Live verification strengthens the original architecture choice: p-ranav/pipeline and pipes validate ergonomic `|` chains, Taskflow/oneTBB/stdexec validate backend directions, and Boost.MP11 validates a small alias-template meta core, but none replaces the need for this project's own type-level validation layer.
- Current C++26 evidence supports an experimental-gate policy: pack indexing and user-generated `static_assert` messages are near-term diagnostic upgrades, while reflection, contracts, and sender/receiver backends must stay optional until the compiler matrix proves them.

## Best Product Positioning

- Name idea: `typepipe`, `PipeForge`, `ctpipe`, or `ForgePipe`.
- Tagline: "Compile-time validated C++ pipelines with readable runtime execution."
- Primary users: C++ library authors, game engine/tooling developers, data-processing libraries, embedded systems, HPC teams, compile-time-heavy codebases, and teams that need explicit pipeline correctness.
- Primary differentiator: a pipeline can be read like a DSL but is still just C++ types, concepts, and template instantiation.
- Secondary differentiator: it exposes diagnostics as first-class output, not only compiler backtraces.
- Core promise: if stages do not connect, the user sees which stage, which port, expected type, actual type, and possible fix.

## Recommended Non-Goals

- Do not try to replace all task schedulers. Provide adapters to task schedulers.
- Do not make all runtime scheduling decisions at compile time. Validate structure at compile time and choose scheduling policy at runtime or configuration time.
- Do not rely on macros as the main DSL. Keep macros optional for short names and source-location capture.
- Do not require users to rewrite existing functions. Provide adapters for free functions, functors, member functions, coroutines, senders, and `std::expected` style APIs.
- Do not promise that every compiler supports every feature equally. Provide a C++20/C++23 baseline and optional C++26 feature gates.
- Do not expose huge internal template names in public errors. Wrap internal mechanics behind concepts and named error types.

# Part 1 - Research Findings

## Direct C++ Pipeline and Pipe-Like Libraries

The following systems show the current landscape for C++ pipeline construction.

### p-ranav/pipeline

- Researched source: [p-ranav/pipeline][S1].
- What it has: C++17 header-only template library for data-processing pipelines. It wraps functions with `pipeline::fn`, composes them with `operator|`, and executes the composed pipeline by calling it.
- Live validation: The public repository still presents this as a compact header-only function-chain library with a single-include path and no published release artifact.
- Implementation implication: Use it as syntax evidence only. Do not treat it as an architecture template for graph export, diagnostics, backend lowering, or production packaging.
- What it does not fully cover for this project: Good syntax for short function chains. Too lambda/function-wrapper centered for the desired type-first `::`/`<>` DSL. Limited graph semantics and no comprehensive compile-time graph diagnostic model in the public description.

### joboccara/pipes

- Researched source: [joboccara/pipes][S2].
- What it has: C++14 header-only collection pipeline library. It chains small pipe components with a collection source and destination.
- Live validation: The repository README positions pipes as small components for expressive collection processing, not as a general pipeline compiler.
- Implementation implication: Borrow small named pipe vocabulary and examples, but keep this project's stage metadata and validation independent of collection-only semantics.
- What it does not fully cover for this project: Good collection-focused expressive style. Strong inspiration for naming small stages, but it is not a general compile-time type graph compiler.

### range-v3 and std::ranges

- Researched source: [range-v3][S3], [RangeAdaptorClosureObject][S4].
- What it has: Range-v3 was the basis for C++20 ranges. Ranges use composable views, actions, algorithms, and pipeable adaptor closure objects.
- Live validation: Current range-v3 docs and repository still make it the best comparison point for lazy, composable adapters and standard-ranges migration.
- Implementation implication: Treat range view adapters as one adapter family. Do not model the whole product as a range library, because non-range stages need ownership, error, scheduling, and graph metadata that ranges intentionally avoid.
- What it does not fully cover for this project: Excellent example of composable adapters and lazy evaluation. It is constrained to range-like data and view semantics, not arbitrary stage graphs.

### RxCpp

- Researched source: [RxCpp][S5].
- What it has: Reactive Extensions for C++ compose asynchronous and event-based programs using observable sequences and LINQ-style operators.
- What it does not fully cover for this project: Good model for event streams, errors, and completion. Less suited to static type-level graph definition with `::` chains.

### Taskflow

- Researched source: [Taskflow][S6], [Taskflow pipeline docs][S7].
- What it has: Taskflow is a general-purpose task-parallel system. Its pipeline examples combine task-graph parallelism inside stages and pipeline parallelism across stages.
- Live validation: The Taskflow processing-pipeline example runs nested taskflows inside pipeline stages and explicitly uses `executor.corun` so the calling worker participates rather than blocking.
- Implementation implication: A Taskflow backend must not blindly lower every stage to `executor.run(...).wait()` inside a pipe. Backend design needs a `corun`/worker-participation rule and storage rules for serial versus parallel pipes.
- What it does not fully cover for this project: Strong runtime scheduling and graph execution. It should be an optional backend, not the type-level DSL itself.

### oneTBB parallel_pipeline

- Researched source: [oneTBB parallel_pipeline][S8].
- What it has: oneTBB represents pipelines as filters with serial/parallel modes and type-matched input/output between adjacent filters.
- Live validation: oneTBB `parallel_pipeline` uses a strongly typed `filter<void, void>` chain, filter modes (`parallel`, `serial_in_order`, `serial_out_of_order`), and a `max_number_of_live_tokens` concurrency bound.
- Implementation implication: The optional oneTBB backend should lower validated stages into filter chains only when adjacent input/output types are exactly represented and token/concurrency policy is explicit.
- What it does not fully cover for this project: Strong scheduling and pipeline filter mode model. Useful for backend semantics and token limits. Not a compile-time DSL for arbitrary type graph construction.

### NVIDIA stdexec / P2300 std::execution

- Researched source: [stdexec][S9], [P2300][S10].
- What it has: Reference implementation and proposal lineage for C++26 sender/receiver async composition. P2300 emphasizes composability, generic execution resources, error propagation, and cancellation.
- Live validation: NVIDIA stdexec now describes itself as an experimental reference implementation for the C++26 `std::execution` additions, with C++20 requirements and specific compiler minimums; it remains subject to change.
- Implementation implication: Keep sender/receiver support behind an experimental backend boundary. Model completion signatures, error channels, and cancellation in adapter metadata before exposing a stable user-facing async API.
- What it does not fully cover for this project: Very relevant backend direction for async pipelines. Should be optional due to evolving compiler and standard library availability.

## C++ Metaprogramming Libraries Relevant to the Core

### Boost.MP11

- Researched source: [Boost.MP11][S11].
- What it shows: A C++11 metaprogramming library for compile-time manipulation of type-containing data structures. It is based on template aliases and variadic templates.
- Live validation: Boost.MP11 documentation emphasizes alias-template algorithms, list-like structures with no strict requirement on the list template, and interoperability with tuple/pair/variant-like types.
- Implementation implication: Keep the internal meta core MP11-shaped: tiny `list`, `rename`, `transform`, `fold`, `contains`, `at`, and `mp_valid`-style detection utilities before inventing heavier abstractions.
- Lesson for this project: Use its style: alias templates, simple type lists, non-intrusive algorithms, low recursion, no unnecessary runtime objects.

### Boost.Hana

- Researched source: [Boost.Hana][S12].
- What it shows: Header-only metaprogramming for computations on both types and values, with higher expressiveness and interoperability with Boost.Fusion, Boost.MPL, and the standard library.
- Live validation: Hana's docs intentionally surface human-facing `static_assert` messages around concept failures.
- Implementation implication: Copy the diagnostic philosophy, not the dependency. The project should define user-facing concepts and named diagnostics before deep template machinery instantiates.
- Lesson for this project: Use the concept of bridging type-level and value-level objects. Avoid importing a heavy dependency unless the compile-time cost is measured and acceptable.

### Eric Niebler Meta

- Researched source: [Meta][S13].
- What it shows: A tiny header-only C++11 metaprogramming library for types and lists of types. Its source shows type lists, defer, quote, compose, and logarithmic index-sequence construction patterns.
- Lesson for this project: Use as inspiration for the internal meta core. The project should not blindly copy it; build only the algorithms needed for pipeline validation.

### Boost.YAP

- Researched source: [Boost.YAP][S14].
- What it shows: A C++14-and-later expression-template library that captures expressions so they can be transformed or evaluated lazily.
- Live validation: Boost.YAP demonstrates that pipe-like expression syntax can be built with expression templates, but the docs also frame expression-template implementation/maintenance cost as a core motivation.
- Implementation implication: Keep expression capture out of MVP. Revisit it only after named type-stage syntax, compile-fail diagnostics, and graph export are stable.
- Lesson for this project: Useful if the product later adds expression syntax. For MVP, prefer type-level stage lists because expression templates can increase diagnostics complexity.

### Boost.HOF

- Researched source: [Boost.HOF][S15].
- What it shows: Higher-order function utilities for function objects, adaptors, constexpr initialization, and simpler function composition.
- Live validation: Boost.HOF exposes function adaptors such as `compose`, `flow`, `infix`, and `pipable`, which are useful adapter vocabulary.
- Implementation implication: Use HOF-like ideas for `pb::adapt` ergonomics, but keep type metadata and stage identity outside raw function-composition utilities.
- Lesson for this project: Useful for adapter design. Do not make function-composition syntax the whole product because the target is a named type DSL.

## General Pipeline and Workflow Systems

### Apache Beam

- Researched source: [Apache Beam][S16], [Beam programming guide][S17].
- What it has: Beam pipelines use a Pipeline object, PCollections as inputs/outputs, PTransforms as processing steps, IO transforms, and runners that execute the graph.
- Live validation: Current Beam docs still present the durable lifecycle as create a pipeline object, create PCollections, apply transforms, write outputs, then run the pipeline.
- Implementation implication: Documentation for this project should mirror that lifecycle: define source/input, attach typed stages, attach sink/output, inspect/export, then run via executor.
- Lesson for this project: The C++ builder should copy the mental model: source, transform, sink, graph, runner. It should not copy Beam complexity into the MVP.

### Nextflow

- Researched source: [Nextflow docs][S18], [Nextflow GitHub][S19].
- What it has: Nextflow is a DSL for scalable, portable, reproducible workflows using a dataflow model, execution abstraction, and support for multiple compute environments.
- Live validation: Current Nextflow docs explicitly emphasize scalable, portable, reproducible workflows across local machines, HPC schedulers, and cloud.
- Implementation implication: Treat reproducibility metadata, environment capture, and executor portability as post-MVP documentation/tooling features, not as a reason to invent a non-C++ DSL.
- Lesson for this project: The C++ builder should include execution abstraction and reproducibility metadata. It should keep syntax native C++ rather than a separate language.

### Dagster

- Researched source: [Dagster GitHub][S20].
- What it has: Dagster emphasizes declarative data assets, integrated lineage, observability, and testability across the development lifecycle.
- Live validation: The Dagster repository now describes it as an orchestration platform for data assets with lineage, observability, declarative modeling, and testability.
- Implementation implication: Add graph export, stage metadata, observer hooks, and test harnesses early because mature pipeline products compete on operational clarity, not only composition syntax.
- Lesson for this project: The C++ builder should include graph export, lineage metadata, and testability hooks early.

## Current and Emerging C++ Features

### Concepts and requires expressions

- Researched source: [requires expressions][S21].
- Recommendation: Use concepts to define stage requirements, user callable requirements, connectability, executor requirements, and error-policy requirements.

### std::expected

- Researched source: [std::expected][S22].
- Recommendation: Use `std::expected<T, E>` as the default runtime error carrier for C++23 builds; provide a compatible fallback for C++20.
- Live validation: `std::expected` is standardized for C++23 and has monadic operations; P0323R12 remains the design source for the class template.
- Implementation implication: Define an `expected_like` concept and a small C++20 fallback facade. Avoid baking `std::expected` directly into every public type so C++20 users and third-party expected-like types can participate.

### Modules

- Researched source: [C++ modules][S23], [Microsoft comparison][S24].
- Recommendation: Offer module interface units later to reduce include cost and improve public API hygiene, but keep headers available for adoption.

### C++26 pack indexing and static_assert message improvements

- Researched source: [C++26 cppreference][S25].
- Recommendation: Feature-gate better diagnostics and simpler pack indexing when compilers support them.
- Live validation: The C++26 compiler-support table shows uneven implementation across GCC, Clang, MSVC, and other toolchains. Pack indexing has concrete early compiler support, while reflection/contracts remain more limited.
- Implementation implication: Keep the C++20 path canonical. Add C++26 branches only when a CI matrix proves the exact feature-test macro and compiler behavior.

### C++26 reflection

- Researched source: [C++26 reflection library][S26], [P2996][S27].
- Recommendation: Plan future auto-adapters and automatic field introspection, but do not depend on reflection for MVP.
- Live validation: P2996R13 supersedes the older R12 source and includes wording updates plus final-adjustment integration for C++26 reflection.
- Implementation implication: Create a `PB_HAS_STATIC_REFLECTION` experimental adapter package later. Do not expose reflection-generated adapters as the primary API until reflection availability and syntax are stable across the target matrix.

### C++26 contracts

- Researched source: [contract assertions][S28], [P4043][S29].
- Recommendation: Treat contracts as optional because the feature has active design discussion. Provide a local contract/assertion layer now and map to standard contracts only behind feature gates.
- Live validation: P2900R14 proposes preconditions, postconditions, assertion statements, selectable evaluation semantics, and violation handlers; cppreference lists contract assertions as C++26 with partial compiler support.
- Implementation implication: Design `pb::contract_policy` as a stable local abstraction first. Map to standard contracts only in an opt-in feature gate once compilers and build modes agree on semantics.

### Pipeline operator design

- Researched source: [P2672][S30].
- Recommendation: The standard pipeline-operator discussions show the ergonomic tradeoffs between left-threading, placeholders, and language-bind models. The product can avoid waiting for a language operator by using type-level `::then<Stage>` syntax.

## Compile-Time Optimization Research Sources

### CMake precompiled headers

- Researched source: [CMake target_precompile_headers][S31].
- Recommendation: Use PCH for large stable headers, standard library headers, and third-party dependencies, but not for the most frequently edited pipeline headers.

### CMake unity builds

- Researched source: [CMake unity builds][S32].
- Recommendation: Offer developer-controlled unity builds for implementation targets and tests. Disable per target when ODR or macro issues appear.

### Clang time tracing

- Researched source: [Clang user manual][S33].
- Recommendation: Use `-ftime-trace` in CI and local profiling to identify hot headers, template instantiations, and expensive translation units.

### Include What You Use

- Researched source: [IWYU][S34].
- Recommendation: Maintain strict include boundaries so the heavily templated public API does not drag excessive dependencies into every user translation unit.

### ccache

- Researched source: [ccache][S35].
- Recommendation: Use compiler caching for local and CI rebuilds where deterministic compiler inputs allow cache hits.

### sccache

- Researched source: [sccache][S36].
- Recommendation: Use remote compiler caching for CI and team builds when infrastructure is available.

### Ninja

- Researched source: [Ninja manual][S37].
- Recommendation: Prefer Ninja generators for fast incremental builds and traceable `.ninja_log` data.

### CMake IPO/LTO

- Researched source: [CMake IPO][S38].
- Recommendation: Use IPO/LTO for release/runtime targets, not as a compile-time speed tool. It can improve runtime code but may slow final links.

# Part 2 - Similarities Across Existing Pipeline Builders

The research shows repeated design patterns across C++ and non-C++ systems. These are the similarities the project should cover.

- Stage or transform abstraction: Every mature system has a unit of computation: filter, transform, pipe, task, op, process, sender adaptor, or asset function.
- Input and output types: Each stage has an input and output. In C++ systems this often appears as function signatures or template arguments. In workflow systems it appears as data collections, channels, assets, or files.
- Composition operator: Composition appears as `|`, `>>=`, `apply`, graph dependencies, DSL processes, or sender adaptors. The exact syntax varies, but a chain is always present.
- Source and sink boundary: Pipelines generally start with a source and end with a sink, even when those names are implicit.
- Validation before execution: Systems validate some structure before running: types, graph edges, runtime options, executor support, or shape of IO.
- Lazy graph construction: Most systems separate defining the pipeline from running it. Ranges are lazy views, Beam constructs a graph then runs it, Taskflow builds a graph, and sender chains create senders before consumption.
- Execution abstraction: Runtime systems decouple the pipeline definition from where it runs: local executor, thread pool, scheduler, cloud, HPC, GPU, or runner.
- Error propagation: Good systems propagate errors in a formal way: Rx has errors/completion, sender/receiver has error channels, `std::expected` has unexpected values, and workflow systems report task failures.
- Observability: Mature pipeline tools expose graphs, statuses, logs, lineage, or timing.
- Reusability: Pipelines are not just one-off chains; stages and subgraphs can be reused and composed.
- Testing hooks: Good pipeline systems allow testing at the stage level and graph level.
- Configuration: There is usually per-stage or per-pipeline configuration: parallelism, tokens, error policy, resources, retries, input/output paths, schedulers, and metadata.

## Capabilities This Project Should Cover

- Linear chains: `A -> B -> C`.
- Named stages with explicit input and output types.
- Compile-time validation that adjacent stage types connect.
- Source and sink modeling.
- Branch and join modeling after MVP.
- Fan-out, fan-in, map, filter, reduce, and transform utilities after basic chains are stable.
- Runtime executor abstraction.
- Error policy abstraction.
- Diagnostics that identify the failing edge.
- Graph export for documentation and debugging.
- Per-stage metadata: name, version, category, source location, cost hint, concurrency hint.
- Adapter layer for existing user functions and classes.
- Compile-time benchmark suite.
- Runtime benchmark suite.
- Compatibility matrix for compilers and language modes.

## Common Missing Pieces in Existing Systems

- A single C++ library rarely provides a strict type-level DSL, broad adapters, graph execution, readable diagnostics, and multiple execution backends together.
- Pipe operator libraries often compose functions nicely but do not expose a type-level graph object that can be inspected, exported, or compiled into backends.
- Runtime task systems schedule work well but usually make the user build runtime nodes rather than a rich compile-time type graph.
- Metaprogramming libraries provide the raw type manipulation but are not product-level pipeline builders.
- Workflow systems provide observability and execution abstraction but are not native C++ compile-time DSLs.
- Most libraries depend heavily on lambdas or callables. They do not make named stages the default product language.
- Most compile-time-heavy C++ libraries leave error messages to the compiler. This project should intentionally design errors as an API.

# Part 3 - Product Requirements

## Functional Requirements

- Users can define a stage as a named type.
- Users can define input and output types explicitly.
- Users can define runtime implementation logic separately from type metadata.
- Users can compose pipelines with type-level syntax using `<>` and `::`.
- Users can run a validated pipeline using a runtime executor.
- The library rejects incompatible adjacent stages at compile time.
- The library reports the exact stage edge that failed validation.
- The library supports free functions.
- The library supports function objects.
- The library supports stateless lambdas through an explicit adapter.
- The library supports stateful lambdas or stateful callables through runtime-bound adapters.
- The library supports member functions through adapters.
- The library supports `std::expected`-returning user code.
- The library supports exception-throwing user code through an exception policy.
- The library supports `void` stages.
- The library supports move-only input and output types when the executor supports them.
- The library supports references only through explicit reference policies.
- The library supports optional graph export.
- The library supports per-stage tags and metadata.
- The library supports compile-time feature flags.

## Non-Functional Requirements

- The public DSL must be readable without studying template internals.
- Compile-time overhead must be measured and reduced continuously.
- The core must compile on GCC, Clang, and MSVC where practical.
- The public API should work in C++20 baseline mode.
- C++23 features should be enabled when available.
- C++26 features should be experimental and feature-gated.
- The library should avoid forcing a heavy dependency on Boost or oneTBB.
- Optional backends can depend on Taskflow, oneTBB, or stdexec.
- The core should be header-only only where necessary. Heavy runtime code should live in compiled units.
- The public headers must include minimal standard headers.
- The code should be deterministic under CI.
- Error messages must be tested with compile-fail tests.
- Documentation must include full examples and failure examples.

## Quality Requirements

- No stage connects if output type is not accepted by next input type.
- No branch joins if outputs are not join-compatible.
- No sink receives wrong type.
- No hidden data copies unless policy allows them.
- No unbounded runtime exception path in no-exception builds.
- No silent loss of error information.
- No dependency cycle in DAG mode unless a future feedback-loop feature is explicitly designed.
- No public template backtrace as the only diagnostic.
- No runtime execution of an invalid compile-time graph.

# Part 4 - Proposed DSL

## DSL Principles

- Prefer named stage types over lambdas.
- Keep stage definitions short.
- Use `::then<Stage>` for compile-time chaining.
- Use `<>` for configuration and compile-time metadata.
- Use explicit adapters when adapting user code.
- Allow readable aliases for dependent `typename` and `template` syntax.
- Separate declaration from execution.
- Make invalid edges impossible to ignore.
- Make graph introspection available without running the pipeline.

## Primary Type-Level Chain Syntax

Target user-facing style:

```cpp
using OrderPipeline =
    pb::from<domain::RawText>
      ::then<stages::ParseJson>
      ::then<stages::ValidateOrder>
      ::then<stages::NormalizeOrder>
      ::then<stages::PersistOrder>
      ::to<domain::PersistedOrder>;

static_assert(pb::valid<OrderPipeline>);

auto engine = pb::compile<OrderPipeline>(pb::runtime::sequential{});
auto result = engine.run(domain::RawText{"..."});
```

Design note: this is implementable when `pb::from<T>` returns a non-dependent type with alias-template members such as `then<Stage>` and `to<Out>`. Inside templates, users may need `typename` and `template`; the library should provide helper aliases to reduce that noise.

## Stage Definition Style

The preferred stage definition is a named type with metadata and a runtime body.

```cpp
struct ParseJson
{
    using input_type  = domain::RawText;
    using output_type = domain::OrderDraft;
    using error_type  = domain::ParseError;

    static constexpr auto name = pb::fixed_string{"parse_json"};

    static auto run(domain::RawText input)
        -> std::expected<domain::OrderDraft, domain::ParseError>;
};
```

## Stage Trait Fallback

For existing code where the user cannot modify the type, provide traits:

```cpp
template <>
struct pb::stage_traits<legacy::Parser>
{
    using input_type  = domain::RawText;
    using output_type = domain::OrderDraft;
    using error_type  = domain::ParseError;
    static constexpr auto name = pb::fixed_string{"legacy_parser"};
};
```

## Adapter Syntax for Existing Functions

Named adapters keep the DSL readable while still accepting existing user code.

```cpp
auto parse_impl(domain::RawText) -> std::expected<domain::OrderDraft, domain::ParseError>;

using ParseStage = pb::adapt<
    pb::name<"parse_json">,
    pb::fn<parse_impl>,
    pb::in<domain::RawText>,
    pb::out<domain::OrderDraft>,
    pb::err<domain::ParseError>
>;
```

If non-type template parameters for string literals are not supported in the selected compiler mode, use a `fixed_string` wrapper or named tag type.

## DSL for Policies

Policies should be type parameters, not runtime strings.

```cpp
using P = pb::from<domain::RawText>
    ::with<pb::policy::errors::expected>
    ::with<pb::policy::copying::move_only>
    ::with<pb::policy::diagnostics::verbose>
    ::then<stages::ParseJson>
    ::then<stages::ValidateOrder>
    ::to<domain::ValidOrder>;
```

## DSL for Branches

Branching should be added after linear chains are stable.

```cpp
using P = pb::from<domain::Order>
    ::branch<
        pb::case_<predicates::IsHighRisk>::then<stages::ManualReview>,
        pb::case_<predicates::IsNormal>::then<stages::AutoApprove>
    >
    ::join<joins::ReviewDecision>
    ::to<domain::Decision>;
```

## DSL for Graph Export

The graph should be exportable at compile time into a runtime descriptor.

```cpp
constexpr auto desc = pb::describe<OrderPipeline>();
pb::export_dot(desc, "order_pipeline.dot");
pb::export_json(desc, "order_pipeline.json");
```

## DSL Anti-Patterns

- Do not force `auto pipeline = fn(lambda) | fn(lambda) | fn(lambda);` as the main API.
- Do not make the user write nested `pb::pipeline<pb::pipeline<pb::pipeline<...>>>` types.
- Do not expose internal type-list names in the DSL.
- Do not make every stage inherit from a heavy CRTP base if traits are enough.
- Do not require macros for core operation.
- Do not hide runtime side effects behind compile-time-looking names.

# Part 5 - Internal Architecture

## Layered Architecture

- Layer 0: configuration: Feature macros, compiler detection, language-mode detection, ABI options, exception options, and diagnostic settings.
- Layer 1: meta core: Small type-list algorithms, type identity, type map, stage list, edge list, index utilities, and compile-time string tags.
- Layer 2: stage traits: Extraction of input type, output type, error type, name, category, run function, state model, and resource hints.
- Layer 3: concepts: Named constraints for `Stage`, `CallableStage`, `ExpectedStage`, `Connectable`, `Executor`, `ErrorPolicy`, and `SerializableGraph`.
- Layer 4: graph model: Type-level nodes, edges, ports, labels, topology, source, sink, branch, join, and pipeline state.
- Layer 5: validator: Compile-time checks that emit targeted diagnostics and produce a validated graph type.
- Layer 6: lowering: Turn a validated graph type into a runtime plan with function pointers, invokers, storage policy, and executor hooks.
- Layer 7: runtime engine: Sequential executor, result propagation, structured diagnostics, tracing, cancellation, and backpressure support.
- Layer 8: optional backends: Thread pool, oneTBB, Taskflow, P2300 sender/receiver, and GPU or accelerator backends.
- Layer 9: tooling: Graph export, compile-time profiler integration, test macros, diagnostic report generation, and examples.

## Core Type Model

The internal type model should be intentionally small.

```cpp
namespace pb::meta {
    template <class... Ts> struct list {};
    template <class T> struct id { using type = T; };
    template <class List> struct size;
    template <class List, class T> struct push_back;
    template <class List, template<class> class Pred> struct all_of;
    template <class List, template<class, class> class F> struct adjacent_all;
}

namespace pb::graph {
    template <class Id, class Stage> struct node;
    template <class From, class To> struct edge;
    template <class Nodes, class Edges> struct dag;
}
```

## Pipeline State Type

A `pipeline_state` can carry input, current output, policies, stages, and validation status.

```cpp
template <class Initial, class Current, class Stages, class Policies>
struct pipeline_state
{
    using initial_type = Initial;
    using current_type = Current;
    using stages = Stages;
    using policies = Policies;

    template <class Stage>
    using then = typename add_stage<pipeline_state, Stage>::type;

    template <class Out>
    using to = typename finalize<pipeline_state, Out>::type;
};
```

## Stage Concepts

Concepts should make constraints readable and keep substitution failures local.

```cpp
template <class S>
concept Stage = requires {
    typename pb::stage_traits<S>::input_type;
    typename pb::stage_traits<S>::output_type;
    typename pb::stage_traits<S>::error_type;
};

template <class A, class B>
concept Connectable =
    std::same_as<
        typename pb::stage_traits<A>::output_type,
        typename pb::stage_traits<B>::input_type>
    || pb::is_adaptable_edge_v<A, B>;
```

## Validator Strategy

- Normalize every stage into `stage_info<Stage>`.
- Check every stage models `Stage`.
- Check adjacent stages are `Connectable`.
- Check source type matches first stage input type.
- Check final output matches sink type.
- Check error policy can represent every stage error type.
- Check stateful stages have a compatible storage policy.
- Check executor supports required stage categories.
- Check branch predicates return bool-like outputs.
- Check join stages can consume branch outputs.
- Produce `validated_pipeline<...>` on success.
- Produce targeted `static_assert` diagnostics on failure.

## Lowering Strategy

- Validated type graph is lowered into a runtime plan.
- Each stage becomes an invoker object.
- Stateless stages can be stored as empty objects.
- Stateful stages are stored in the engine state.
- Function pointers are used when possible for stable ABI and smaller type names.
- Type-erased invokers are allowed at graph boundaries when compile-time fanout becomes costly.
- Runtime plan contains stage names and source locations for diagnostics.
- Executor receives a normalized sequence of runtime tasks.

## Dependency Policy

- Core should depend on the standard library only.
- Optional Boost.MP11/Hana comparison tests may be kept in development tools, not public core.
- Optional Taskflow backend should be a separate target.
- Optional oneTBB backend should be a separate target.
- Optional stdexec backend should be experimental until standard library support stabilizes.
- Optional JSON export can use a lightweight JSON library or a no-dependency writer.

# Part 6 - Compile-Time Optimization Plan

## Optimization Principle

The project must treat compile time as a first-class product metric. The goal is not only to make code expressive; the goal is to make expressive template code measurable, bounded, and explainable.

## Compile-Time Budget Targets

- MVP linear chain of 10 named stages should compile quickly enough for interactive development on common hardware.
- A 50-stage chain should not create unreadable diagnostics or exponential instantiation growth.
- Public headers should avoid including backend headers.
- Changing a runtime implementation file should not force every template-heavy header to rebuild.
- Examples should include build-time measurements.
- CI should track compile-time regression using at least one Clang build with `-ftime-trace`.

## Template Design Rules

- Prefer alias-template algorithms when no partial specialization state is needed.
- Prefer fold expressions over recursive binary templates for simple checks.
- Prefer `std::index_sequence` or compiler builtins where available.
- Cache normalized stage traits in one `stage_info<Stage>` type.
- Avoid repeated instantiation of `stage_traits<Stage>` across many checks.
- Avoid deeply nested `std::conditional_t` trees.
- Avoid recursive graph walkers that instantiate the same suffix many times.
- Avoid expression templates in MVP because they capture syntax at the cost of more types.
- Avoid variable-template logic that repeats heavy trait computation.
- Avoid `decltype` probes in multiple locations. Centralize them in concepts.
- Avoid global overload sets that create accidental ADL costs.
- Use named customization points sparingly.
- Use CRTP only if it reduces user boilerplate without increasing compile-time fanout.
- Use local concepts as documentation and compile-time filters.
- Use `requires` clauses to stop invalid code before it enters heavy internals.

## Header Structure Rules

- `pb/core/meta.hpp` contains only type-list and identity utilities.
- `pb/core/fixed_string.hpp` contains compile-time names.
- `pb/core/stage_traits.hpp` contains trait extraction.
- `pb/core/concepts.hpp` contains concepts.
- `pb/core/pipeline.hpp` contains the type-level DSL.
- `pb/core/validate.hpp` contains validation internals.
- `pb/runtime/result.hpp` contains result and error abstractions.
- `pb/runtime/sequential.hpp` contains the first executor.
- `pb/backends/taskflow.hpp` includes Taskflow only in the optional backend target.
- `pb/backends/tbb.hpp` includes oneTBB only in the optional backend target.
- `pb/backends/stdexec.hpp` includes stdexec only in experimental builds.
- Public umbrella header `pb/pipeline.hpp` should be convenient but not forced.

## Build-System Optimization Rules

- Use CMake targets for core, runtime, examples, tests, and optional backends.
- Use `target_precompile_headers` for stable large headers in tests and examples, not blindly for the core public API.
- Use developer-controlled unity builds for runtime implementation targets and examples.
- Use Ninja generator in CI for reliable incremental build behavior.
- Use ccache locally and in CI when supported; remember that ccache accelerates single-file compilations, not linking, and unsupported flags may fall back to the real compiler.
- Use sccache with remote storage for team-scale CI if available; note that current sccache has partial C++20 named-module support for Clang and does not yet support GCC/MSVC module modes in the same way.
- Keep compile commands exported for tools like IWYU and clang-tidy.
- Treat IWYU suggestions as review inputs, not automatic truth; its own README documents manual correction paths and alpha-quality automatic fixing.
- Measure full rebuilds and incremental rebuilds separately.
- Keep one benchmark for header inclusion cost: a translation unit that includes only `pb/pipeline.hpp`.
- Keep one benchmark for a 5-stage pipeline.
- Keep one benchmark for a 50-stage pipeline.
- Keep one benchmark for a branch/join graph once that feature exists.

## Compiler Profiling Workflow

- Build with Clang and `-ftime-trace`; optionally set an output path and granularity, but avoid verbose trace mode by default because it can multiply trace artifact size.
- Aggregate traces for the examples and tests.
- Identify top expensive headers.
- Identify top expensive template instantiations.
- Move runtime-heavy code from headers to implementation files.
- Replace recursive meta algorithms with folds or index-based loops when possible.
- Reduce includes and forward-declare where legal.
- Run IWYU to remove unnecessary includes.
- Compare before and after traces.
- Record results in a compile-time dashboard.

## Compile-Time Failure Modes and Fixes

- Recursive instantiation explosion: Use fold expressions, iterative index-sequence expansion, and normalized stage info.
- Repeated trait probing: Cache `stage_info<Stage>` and pass it through validators.
- Huge compiler diagnostics: Fail early with named concepts and targeted `static_assert` messages.
- Too many headers pulled by core: Split runtime and backend headers away from public DSL headers.
- Slow rebuilds when changing examples: Use PCH/cache for examples and keep examples outside core target.
- Backend dependency rebuilds: Keep optional backends isolated by target.
- Long link times after enabling LTO: Enable IPO/LTO only for release targets, not debug iteration.
- Template depth limits: Avoid deeply recursive algorithms and test with 100-stage graphs.

# Part 7 - User Code Integration Plan

## Integration Goal

The library should accept as much valid user code as possible without making invalid code appear valid. The correct goal is broad adaptation plus clear errors.

## Supported User Code Shapes

- Free function: `auto f(In) -> Out` or `auto f(In) -> std::expected<Out, E>`.
- Function pointer: `Out (*)(In)` or compatible expected-returning pointer.
- Stateless function object: `struct F { auto operator()(In) -> Out; };`.
- Stateful function object: Stored in engine state or passed as runtime binding.
- Lambda: Allowed through `pb::adapt_lambda` or `pb::adapt_runtime`, but not preferred for documentation.
- Member function: Adapted with object pointer/reference ownership policy.
- Coroutine: Adapted into async executor or sender-like backend after MVP.
- Sender/receiver object: Adapted in experimental backend only.
- Exception-throwing function: Adapted by an exception policy that catches and converts to error records.
- Noexcept function: Detected and optimized as no-throw path.
- Void-returning function: Mapped to `unit` output or pass-through depending on policy.
- Reference-returning function: Allowed only with explicit lifetime policy.
- Move-only input/output: Allowed when executor and stage signatures support move semantics.
- C API function: Adapted by wrapper with explicit ownership and error policy.

## Adapter Taxonomy

- pb::adapt - generic explicit adapter.
- pb::adapt_fn - compile-time function pointer adapter.
- pb::adapt_functor - named function object adapter.
- pb::adapt_member - member function adapter.
- pb::adapt_expected - stage returns `std::expected` or compatible result.
- pb::adapt_exception - catches exceptions and maps them into error type.
- pb::adapt_void - maps `void` results into unit or pass-through.
- pb::adapt_ref - explicit lifetime-aware reference adapter.
- pb::adapt_sender - experimental sender/receiver adapter.
- pb::adapt_coroutine - future async coroutine adapter.

## Adapter Example

```cpp
struct LegacyService
{
    domain::User lookup(domain::UserId id);
};

using LookupUser = pb::adapt_member<
    pb::name<"lookup_user">,
    LegacyService,
    &LegacyService::lookup,
    pb::in<domain::UserId>,
    pb::out<domain::User>,
    pb::state<pb::borrowed_ref<LegacyService>>
>;
```

## Integration Boundaries

- The library cannot infer private member functions unless the user exposes them.
- The library cannot safely adapt reference lifetimes without explicit policy.
- The library cannot make a throwing function safe unless an exception policy catches and maps it.
- The library cannot prove semantic correctness of user transformations. It can validate types and declared contracts.
- The library cannot support platform APIs without wrappers that specify ownership and errors.
- The library cannot guarantee zero copies unless user types and stage signatures permit moves or references safely.

## Customization Points

- stage_traits<T> - metadata extraction.
- stage_name<T> - optional display name.
- stage_category<T> - CPU, IO, GPU, blocking, async, pure, stateful.
- stage_error<T> - error type extraction.
- stage_contract<T> - declared preconditions and postconditions.
- stage_invoker<T> - runtime invocation customization.
- edge_adapter<A, B> - explicit conversion between adjacent outputs and inputs.
- error_adapter<E> - convert user error into pipeline error envelope.
- executor_traits<E> - supported scheduling and type capabilities.

# Part 8 - Error Handling, Assertions, and Diagnostics

## Error-Handling Model

- Compile-time structural errors: invalid stages, invalid edges, invalid policies, unsupported executor features.
- Runtime stage errors: parse failure, validation failure, IO failure, service failure, cancellation, timeout.
- Runtime engine errors: executor failure, allocation failure, backend failure, graph export failure.
- Assertion failures: contract/precondition/postcondition failures at stage boundaries.
- Integration errors: exception conversion, missing state, invalid lifetime, unsupported user code shape.

## Default Runtime Result Type

Default execution should return a result value rather than throw.

```cpp
template <class T>
using result = std::expected<T, pb::error>;

struct error
{
    pb::error_code code;
    std::string message;
    pb::stage_id stage;
    std::source_location location;
    pb::metadata metadata;
};
```

## Error Policy Options

- expected: default; stages return values or `expected`; errors propagate as `unexpected`.
- exception_to_expected: catch exceptions and convert them to `pb::error`.
- terminate_on_error: intended for embedded or hard real-time modes where error path is disallowed.
- throw_on_error: convert pipeline errors back into exceptions at the API boundary.
- custom_error_envelope: user provides a `pb::error_adapter` specialization.

## Assertion Policy Options

- off: no runtime assertions.
- debug: assertions enabled only in debug builds.
- always: assertions enabled in all builds.
- observe: report assertion failure but continue if safe.
- enforce: report and stop pipeline.
- contract_feature_gate: map to C++26 contracts only when compiler support and project policy allow it.

## Compile-Time Diagnostics API

The project should not rely only on compiler-generated template backtraces. It should expose named diagnostic types.

```cpp
template <class FromStage, class ToStage>
struct edge_type_mismatch
{
    using from_stage = FromStage;
    using to_stage = ToStage;
    using actual_output = typename stage_traits<FromStage>::output_type;
    using expected_input = typename stage_traits<ToStage>::input_type;
};
```

## Diagnostic Message Requirements

- Print the pipeline name when available.
- Print the index of the failed edge.
- Print previous stage name.
- Print next stage name.
- Print actual output type.
- Print expected input type.
- Print whether an edge adapter exists.
- Print whether a conversion is explicit or forbidden.
- Print whether a policy caused rejection.
- Print a suggested fix when possible.

## Example Compile-Time Failure

```cpp
using Bad = pb::from<domain::RawText>
    ::then<stages::ParseJson>        // output: OrderDraft
    ::then<stages::SendEmail>        // input: EmailMessage
    ::to<domain::EmailReceipt>;

// Desired diagnostic shape:
// Pipeline edge mismatch at edge 1:
//   from stage: ParseJson
//   actual output: domain::OrderDraft
//   to stage: SendEmail
//   expected input: domain::EmailMessage
// Suggested fix: add ConvertOrderToEmail stage or specialize pb::edge_adapter.
```

## Runtime Diagnostics API

- Every runtime error includes stage id.
- Every runtime error includes stage name.
- Every runtime error includes error category.
- Every runtime error includes source location when known.
- Every runtime error includes optional user metadata.
- Every runtime error can be serialized for logging.
- Every runtime execution can be traced with start/end timestamps when tracing is enabled.

# Part 9 - Runtime Execution Engine

## Executor Roadmap

- Sequential executor: First backend. Deterministic, easiest to debug, best for validating the type system.
- Thread-pool executor: Second backend. Basic parallelism for independent branches or map stages.
- oneTBB backend: Optional backend for filter pipelines, token limits, and parallel/serial stages.
- Taskflow backend: Optional backend for task graphs and subflow execution.
- std::execution backend: Experimental backend for sender/receiver composition after compiler support is practical.
- GPU/accelerator backend: Future backend using explicit stage category and data movement policy.

## Execution Semantics

- A compiled pipeline owns or borrows stage state according to policy.
- A run call receives an input value of the pipeline source type.
- Each stage receives the previous stage output unless branch semantics apply.
- Errors stop execution by default.
- Cancellation stops execution at cancellation checkpoints.
- Tracing records stage start and finish times.
- Executor decides scheduling only after graph validation.
- Backends may reject stage categories they cannot support.

## State Management

- Stateless stage: default-construct and store as empty object.
- Shared state: store `std::shared_ptr<T>` or user-provided handle.
- Borrowed state: store reference wrapper and require lifetime policy.
- Move-only state: move into engine at compile time or construction time.
- Thread-local state: create per-worker instances in parallel executors.
- External resource state: require explicit cleanup and error policy.

## Runtime Engine Skeleton

```cpp
template <class ValidatedPipeline, class Executor>
class engine
{
public:
    using input_type = typename ValidatedPipeline::input_type;
    using output_type = typename ValidatedPipeline::output_type;

    auto run(input_type input) -> pb::result<output_type>;
    auto describe() const -> pb::runtime_graph_view;
    auto set_observer(pb::observer*) -> void;

private:
    Executor executor_;
    typename ValidatedPipeline::stage_storage stages_;
};
```

## Observer Hooks

- on_pipeline_start.
- on_pipeline_finish.
- on_stage_start.
- on_stage_finish.
- on_stage_error.
- on_assertion_failure.
- on_cancellation.
- on_executor_event.

# Part 10 - Implementation Roadmap

## Phase 0 - Research and prototypes

- Create repository skeleton.
- Implement tiny type-list core.
- Implement `fixed_string` or tag-name mechanism.
- Prototype `from<T>::then<S>::to<U>` syntax.
- Prototype named stage traits.
- Create compile-fail test harness.
- Measure compile time of 5-stage and 20-stage chains.

## Phase 1 - MVP type core

- Implement stage traits.
- Implement concepts.
- Implement connectability checks.
- Implement linear pipeline state.
- Implement validation result type.
- Implement targeted diagnostics for edge mismatch.
- Implement source and sink validation.

## Phase 2 - Runtime sequential execution

- Implement `pb::result` wrapper.
- Implement `std::expected` integration.
- Implement sequential engine lowering.
- Implement stateless stage invocation.
- Implement stateful stage storage.
- Implement basic runtime error envelope.
- Implement observer hooks.

## Phase 3 - User adapters

- Implement free function adapter.
- Implement function object adapter.
- Implement member function adapter.
- Implement exception-to-error adapter.
- Implement void adapter.
- Implement reference policy adapter.
- Implement adapter documentation and examples.

## Phase 4 - Diagnostics and tooling

- Implement graph description API.
- Implement DOT export.
- Implement JSON export.
- Implement compile-time error examples.
- Implement runtime trace export.
- Add diagnostic golden tests.

## Phase 5 - Branches and joins

- Implement type-level branch node.
- Implement predicate stage requirements.
- Implement branch output validation.
- Implement join stage validation.
- Implement sequential branch execution.
- Implement branch diagnostics.

## Phase 6 - Optional backends

- Implement thread-pool executor.
- Prototype oneTBB backend.
- Prototype Taskflow backend.
- Prototype stdexec backend.
- Document backend feature matrix.

## Phase 7 - Packaging and release

- Finalize CMake install targets.
- Add package config files.
- Add examples.
- Add docs.
- Add CI compiler matrix.
- Publish v0.1.0.

# Part 11 - Testing Strategy

## Test Categories

- Unit tests for meta type-list algorithms.
- Unit tests for fixed-string or tag naming.
- Unit tests for stage trait extraction.
- Unit tests for concepts.
- Compile-pass tests for valid pipelines.
- Compile-fail tests for invalid stage type.
- Compile-fail tests for edge mismatch.
- Compile-fail tests for invalid sink output.
- Compile-fail tests for unsupported executor policy.
- Runtime tests for sequential execution.
- Runtime tests for error propagation.
- Runtime tests for exception conversion.
- Runtime tests for observer hooks.
- Runtime tests for graph export.
- Integration tests for user adapters.
- Benchmark tests for compile time.
- Benchmark tests for runtime overhead.

## Compile-Fail Harness

The compile-fail harness should build small files expected to fail and verify that the failure contains stable diagnostic text. This is essential because readable diagnostics are a product feature.

```cmake
add_compile_fail_test(edge_mismatch tests/fail/edge_mismatch.cpp)
expect_diagnostic(edge_mismatch "Pipeline edge mismatch")
expect_diagnostic(edge_mismatch "actual output")
expect_diagnostic(edge_mismatch "expected input")
```

## Runtime Test Example

```cpp
TEST(PipelineRuntime, ExpectedErrorStopsChain)
{
    using P = pb::from<Input>
        ::then<ParseFails>
        ::then<ShouldNotRun>
        ::to<Output>;

    auto engine = pb::compile<P>(pb::runtime::sequential{});
    auto r = engine.run(Input{});

    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().stage.name, "parse_fails");
}
```

## Compiler Matrix

- GCC latest stable in C++20 mode.
- GCC latest stable in C++23 mode.
- Clang latest stable in C++20 mode.
- Clang latest stable in C++23 mode.
- MSVC latest stable in C++20 mode.
- MSVC latest stable in C++23 mode if supported.
- Experimental C++26 build for feature gates only.

# Part 12 - Documentation Plan

- Quick start.
- First named stage.
- First pipeline.
- Running sequentially.
- Returning `std::expected`.
- Adapting a free function.
- Adapting a member function.
- Using stateful stages.
- Understanding compile-time diagnostics.
- Understanding runtime errors.
- Graph export.
- Compile-time performance guide.
- Backend guide.
- Policy guide.
- FAQ.
- Examples gallery.
- Migration guide from function/lambda pipelines.

## Documentation Tone

- Show complete examples first.
- Show failure examples intentionally.
- Avoid unexplained template internals in beginner pages.
- Put advanced metaprogramming details in appendix pages.
- Make every policy visible in examples.
- Include compile-time measurements for examples.

# Part 13 - Risk Register

- Compile-time explosion: severity=High; mitigation=Measure with `-ftime-trace`, restrict recursion, split headers, cache traits, and add compile-time benchmarks.
- Unreadable compiler errors: severity=High; mitigation=Design concepts and static assertions before adding features.
- DSL becomes too clever: severity=Medium; mitigation=Keep MVP to named stages and linear chains.
- User-code adapters become unsafe: severity=High; mitigation=Require explicit ownership, lifetime, and exception policies.
- Backend abstraction becomes too broad: severity=Medium; mitigation=Ship sequential first; keep backend concepts minimal.
- C++26 features are not portable: severity=Medium; mitigation=Feature-gate reflection, contracts, pack indexing, and static-assert improvements.
- Header-only core slows users: severity=High; mitigation=Split runtime and backends into compiled targets and keep public headers minimal.
- Graph export exposes unstable internals: severity=Low; mitigation=Define a stable runtime descriptor independent of internal templates.
- Error policy creates type bloat: severity=Medium; mitigation=Normalize error envelope and allow type-erased error at runtime boundary.
- Stateful parallel stages race: severity=High; mitigation=Require stage category and state policy; reject unsafe parallel execution at compile time when possible.

# Part 14 - Reference Implementation Skeleton

## Directory Layout

- include/pb/pipeline.hpp
- include/pb/core/meta.hpp
- include/pb/core/fixed_string.hpp
- include/pb/core/stage_traits.hpp
- include/pb/core/concepts.hpp
- include/pb/core/pipeline_state.hpp
- include/pb/core/validate.hpp
- include/pb/runtime/result.hpp
- include/pb/runtime/error.hpp
- include/pb/runtime/sequential.hpp
- include/pb/adapt/fn.hpp
- include/pb/adapt/member.hpp
- include/pb/adapt/exception.hpp
- include/pb/export/dot.hpp
- include/pb/export/json.hpp
- include/pb/backends/taskflow.hpp
- include/pb/backends/tbb.hpp
- include/pb/backends/stdexec.hpp
- src/runtime/error.cpp
- src/export/dot.cpp
- examples/basic_order_pipeline.cpp
- examples/error_diagnostic.cpp
- tests/compile_pass/
- tests/compile_fail/
- tests/runtime/
- bench/compile_time/
- bench/runtime/

## CMake Skeleton

```cmake
cmake_minimum_required(VERSION 3.23)
project(pipebuilder LANGUAGES CXX)

option(PB_BUILD_TESTS "Build tests" ON)
option(PB_BUILD_EXAMPLES "Build examples" ON)
option(PB_ENABLE_PCH "Enable precompiled headers for examples/tests" ON)
option(PB_ENABLE_UNITY "Enable unity builds for implementation targets" OFF)
option(PB_BACKEND_TASKFLOW "Enable Taskflow backend" OFF)
option(PB_BACKEND_TBB "Enable oneTBB backend" OFF)
option(PB_BACKEND_STDEXEC "Enable stdexec backend" OFF)

add_library(pb_core INTERFACE)
target_include_directories(pb_core INTERFACE include)
target_compile_features(pb_core INTERFACE cxx_std_20)

add_library(pb_runtime src/runtime/error.cpp src/export/dot.cpp)
target_link_libraries(pb_runtime PUBLIC pb_core)

if(PB_ENABLE_UNITY)
  set_target_properties(pb_runtime PROPERTIES UNITY_BUILD ON)
endif()
```

## Minimal Meta Core

```cpp
namespace pb::meta {
template <class... Ts> struct list {};

template <class List, class T> struct push_back;
template <class... Ts, class T>
struct push_back<list<Ts...>, T> { using type = list<Ts..., T>; };

template <class List, class T>
using push_back_t = typename push_back<List, T>::type;

template <class... Bs>
inline constexpr bool all_v = (Bs::value && ...);
}
```

## Minimal Pipeline Builder

```cpp
namespace pb {
template <class Initial, class Current, class Stages>
struct state
{
    using input_type = Initial;
    using current_type = Current;
    using stages = Stages;

    template <class Stage>
    using then = typename detail::append_stage<state, Stage>::type;

    template <class Out>
    using to = typename detail::finalize<state, Out>::type;
};

template <class In>
using from = state<In, In, meta::list<>>;
}
```

## Minimal Connectability Check

```cpp
namespace pb {
template <class A, class B>
struct can_connect
    : std::bool_constant<
        std::same_as<
            typename stage_traits<A>::output_type,
            typename stage_traits<B>::input_type>> {};

template <class A, class B>
inline constexpr bool can_connect_v = can_connect<A, B>::value;
}
```

## Minimal Runtime Sequential Chain

```cpp
template <class... Stages>
struct sequential_chain
{
    std::tuple<Stages...> stages;

    template <class Input>
    auto run(Input&& input)
    {
        return run_impl<0>(std::forward<Input>(input));
    }

private:
    template <std::size_t I, class Value>
    auto run_impl(Value&& value)
    {
        if constexpr (I == sizeof...(Stages))
            return std::forward<Value>(value);
        else
            return run_impl<I + 1>(
                std::get<I>(stages).run(std::forward<Value>(value)));
    }
};
```

# Part 15 - Detailed Build and Compile-Time Playbook

## Daily local development

- Use Ninja generator.
- Enable ccache or sccache.
- Build examples and selected tests only.
- Keep experimental backends off unless actively changing them.
- Use Clang `-ftime-trace` only during profiling, not every edit-build loop.

## CI debug build

- Build core and runtime with GCC, Clang, and MSVC.
- Run compile-pass tests.
- Run compile-fail diagnostic tests.
- Run runtime unit tests.
- Upload compiler diagnostics on failure.

## CI compile-time profile build

- Use Clang.
- Enable `-ftime-trace`.
- Build compile-time benchmark translation units.
- Collect trace JSON files.
- Compare against baseline thresholds.

## Release build

- Enable optimizations.
- Optionally enable IPO/LTO after checking support.
- Build examples.
- Build documentation.
- Run runtime benchmarks.
- Package install target.

# Part 16 - Feature Backlog

| ID | Feature | Release | Priority |
|---|---|---|---|
| F001 | Linear pipeline type chain | MVP | Required |
| F002 | Named stage traits | MVP | Required |
| F003 | Source and sink type checking | MVP | Required |
| F004 | Compile-time edge mismatch diagnostics | MVP | Required |
| F005 | Sequential runtime executor | MVP | Required |
| F006 | `std::expected` error propagation | MVP | Required |
| F007 | Free function adapter | MVP | Required |
| F008 | Function object adapter | MVP | Required |
| F009 | Exception-to-error adapter | MVP | Required |
| F010 | DOT graph export | v0.2 | Important |
| F011 | JSON graph export | v0.2 | Important |
| F012 | Member function adapter | v0.2 | Important |
| F013 | Stateful stage storage | v0.2 | Important |
| F014 | Observer hooks | v0.2 | Important |
| F015 | Thread-pool executor | v0.3 | Important |
| F016 | Branch nodes | v0.3 | Important |
| F017 | Join nodes | v0.3 | Important |
| F018 | oneTBB backend | v0.4 | Optional |
| F019 | Taskflow backend | v0.4 | Optional |
| F020 | stdexec backend | experimental | Optional |
| F021 | C++26 reflection adapter | experimental | Optional |
| F022 | C++26 contracts integration | experimental | Optional |
| F023 | Graph visualization CLI | v0.5 | Optional |
| F024 | Compile-time dashboard | v0.5 | Important |

# Part 17 - Acceptance Criteria

- A user can define three named stages and chain them with `::then<Stage>`.
- A valid chain compiles and runs sequentially.
- An invalid adjacent stage pair fails at compile time.
- The invalid pair diagnostic includes previous stage, next stage, actual output, and expected input.
- A stage returning `std::expected` propagates failure without running later stages.
- A free function can be adapted without rewriting it.
- A function object can be used as a stage.
- A throwing user function can be adapted into a runtime error.
- The public headers compile under GCC and Clang in C++20 mode.
- Compile-time benchmark results are recorded for at least 5-stage and 50-stage chains.
- The documentation includes one success example and one failure example.

# Part 18 - Practical Example Scenarios

## Order processing

- Raw text input
- Parse JSON
- Validate schema
- Normalize fields
- Persist order
- Emit receipt

## Image processing

- Read image bytes
- Decode image
- Resize
- Normalize pixels
- Run inference
- Write result

## Game asset pipeline

- Read asset file
- Parse metadata
- Compile shader or mesh
- Compress asset
- Write package

## Embedded sensor pipeline

- Read sample
- Filter noise
- Calibrate
- Detect threshold
- Emit event

## Compiler-like pipeline

- Read source
- Lex
- Parse
- Type check
- Optimize
- Emit IR

## Order Pipeline Example

```cpp
using OrderPipeline = pb::from<domain::RawText>
    ::then<stages::ParseJson>
    ::then<stages::ValidateSchema>
    ::then<stages::NormalizeOrder>
    ::then<stages::PersistOrder>
    ::to<domain::Receipt>;

auto engine = pb::compile<OrderPipeline>(pb::runtime::sequential{});
auto receipt = engine.run(domain::RawText{payload});
```

# Part 19 - Development Checklist

## Repository setup

- [ ] Create Git repository.
- [ ] Add CMake project.
- [ ] Add include directory.
- [ ] Add src directory.
- [ ] Add tests directory.
- [ ] Add examples directory.
- [ ] Add docs directory.
- [ ] Add benchmark directory.
- [ ] Add CI workflow.
- [ ] Add compiler matrix.

## Core meta

- [ ] Implement type list.
- [ ] Implement list size.
- [ ] Implement push back.
- [ ] Implement transform.
- [ ] Implement adjacent pair iteration.
- [ ] Implement find.
- [ ] Implement index of.
- [ ] Implement type identity.
- [ ] Implement normalized stage info.
- [ ] Add unit tests.

## Stage traits

- [ ] Implement default trait extraction.
- [ ] Implement specialization path.
- [ ] Implement name trait.
- [ ] Implement input type trait.
- [ ] Implement output type trait.
- [ ] Implement error type trait.
- [ ] Implement category trait.
- [ ] Implement state trait.
- [ ] Add compile-pass tests.
- [ ] Add compile-fail tests.

## Pipeline DSL

- [ ] Implement from.
- [ ] Implement then.
- [ ] Implement to.
- [ ] Implement with policy.
- [ ] Implement done/finalize.
- [ ] Implement static valid flag.
- [ ] Implement dependent-context helper alias.
- [ ] Add examples.
- [ ] Add docs.
- [ ] Add compile-time benchmark.

## Validation

- [ ] Validate first input.
- [ ] Validate adjacent edges.
- [ ] Validate final output.
- [ ] Validate stage concept.
- [ ] Validate error policy.
- [ ] Validate executor support.
- [ ] Validate state policy.
- [ ] Validate references.
- [ ] Add diagnostics.
- [ ] Add golden diagnostic tests.

## Runtime

- [ ] Implement result type.
- [ ] Implement error type.
- [ ] Implement sequential executor.
- [ ] Implement invoker.
- [ ] Implement stage storage.
- [ ] Implement expected propagation.
- [ ] Implement exception adapter.
- [ ] Implement observer hooks.
- [ ] Add runtime tests.
- [ ] Add benchmarks.

## Tooling

- [ ] Implement describe.
- [ ] Implement DOT export.
- [ ] Implement JSON export.
- [ ] Implement trace event type.
- [ ] Implement compile-time trace workflow.
- [ ] Implement docs generator snippets.
- [ ] Add graph export tests.
- [ ] Add source location tests.
- [ ] Add CLI prototype.
- [ ] Document tooling.

## Compile-time optimization

- [ ] Enable include hygiene.
- [ ] Run IWYU.
- [ ] Add PCH option.
- [ ] Add unity option.
- [ ] Add ccache instructions.
- [ ] Add sccache instructions.
- [ ] Add Clang ftime-trace option.
- [ ] Add compile-time baseline.
- [ ] Track regression.
- [ ] Document workflow.

# Part 20 - Extended Implementation Notes

- The public DSL should be tested with real compiler errors, not only type traits.
- The stage concept should be small. A stage should not need to know about the whole engine.
- The pipeline state should hold a list of stage types and a current output type.
- The validator should operate on normalized stage info, not raw user types repeatedly.
- The compile function should reject unvalidated pipelines.
- The runtime engine should not need to know the entire metaprogramming system.
- The graph descriptor should be a value-level object built from type-level metadata.
- The result type should be customizable but default to expected-like behavior.
- The exception adapter should never silently swallow exceptions without recording type and message when available.
- The pipeline should permit no-exception builds with a clear configuration.
- The core should use `std::source_location` where available for better diagnostics.
- The project should keep examples small and independent.
- The backend APIs should never leak into core headers.
- The compile-time benchmarks should be part of release quality gates.
- The docs should show how to fix an edge mismatch.

# Part 21 - Detailed Engineering Task List

The tasks below are intentionally concrete. They split the product into small deliverables so the first usable release can be built without turning the template core into an unmeasured experiment.

- Task 001 - Define API for repository bootstrap: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 002 - Implement minimal path for repository bootstrap: build the smallest working implementation that compiles in the supported compiler matrix; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 003 - Add concept guard for repository bootstrap: add a user-facing concept or requires-expression so failures stop at the boundary; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 004 - Add positive test for repository bootstrap: add a compile-pass and runtime-pass test proving the supported use case works; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 005 - Add negative test for repository bootstrap: add a compile-fail test proving the invalid use case fails with the intended message; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 006 - Measure compile time for repository bootstrap: record clean and incremental compile time impact with the benchmark harness; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 007 - Document usage for repository bootstrap: write one good example and one bad example that a new user can understand; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 008 - Review diagnostics for repository bootstrap: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 009 - Review integration risk for repository bootstrap: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 010 - Add performance check for repository bootstrap: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: create the initial repository, CMake targets, package metadata, examples folder, and policy for third-party dependencies.
- Task 011 - Define API for public header boundaries: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 012 - Implement minimal path for public header boundaries: build the smallest working implementation that compiles in the supported compiler matrix; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 013 - Add concept guard for public header boundaries: add a user-facing concept or requires-expression so failures stop at the boundary; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 014 - Add positive test for public header boundaries: add a compile-pass and runtime-pass test proving the supported use case works; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 015 - Add negative test for public header boundaries: add a compile-fail test proving the invalid use case fails with the intended message; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 016 - Measure compile time for public header boundaries: record clean and incremental compile time impact with the benchmark harness; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 017 - Document usage for public header boundaries: write one good example and one bad example that a new user can understand; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 018 - Review diagnostics for public header boundaries: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 019 - Review integration risk for public header boundaries: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 020 - Add performance check for public header boundaries: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: separate stable public API headers from internal implementation headers and reduce transitive include load.
- Task 021 - Define API for type-list core: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 022 - Implement minimal path for type-list core: build the smallest working implementation that compiles in the supported compiler matrix; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 023 - Add concept guard for type-list core: add a user-facing concept or requires-expression so failures stop at the boundary; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 024 - Add positive test for type-list core: add a compile-pass and runtime-pass test proving the supported use case works; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 025 - Add negative test for type-list core: add a compile-fail test proving the invalid use case fails with the intended message; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 026 - Measure compile time for type-list core: record clean and incremental compile time impact with the benchmark harness; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 027 - Document usage for type-list core: write one good example and one bad example that a new user can understand; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 028 - Review diagnostics for type-list core: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 029 - Review integration risk for type-list core: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 030 - Add performance check for type-list core: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: implement the minimum type-list operations required for chains, branches, indices, and validation.
- Task 031 - Define API for type identity and names: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 032 - Implement minimal path for type identity and names: build the smallest working implementation that compiles in the supported compiler matrix; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 033 - Add concept guard for type identity and names: add a user-facing concept or requires-expression so failures stop at the boundary; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 034 - Add positive test for type identity and names: add a compile-pass and runtime-pass test proving the supported use case works; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 035 - Add negative test for type identity and names: add a compile-fail test proving the invalid use case fails with the intended message; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 036 - Measure compile time for type identity and names: record clean and incremental compile time impact with the benchmark harness; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 037 - Document usage for type identity and names: write one good example and one bad example that a new user can understand; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 038 - Review diagnostics for type identity and names: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 039 - Review integration risk for type identity and names: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 040 - Add performance check for type identity and names: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: provide stable stage identifiers, fixed-string names, pretty names, and source-location metadata.
- Task 041 - Define API for stage trait extraction: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 042 - Implement minimal path for stage trait extraction: build the smallest working implementation that compiles in the supported compiler matrix; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 043 - Add concept guard for stage trait extraction: add a user-facing concept or requires-expression so failures stop at the boundary; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 044 - Add positive test for stage trait extraction: add a compile-pass and runtime-pass test proving the supported use case works; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 045 - Add negative test for stage trait extraction: add a compile-fail test proving the invalid use case fails with the intended message; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 046 - Measure compile time for stage trait extraction: record clean and incremental compile time impact with the benchmark harness; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 047 - Document usage for stage trait extraction: write one good example and one bad example that a new user can understand; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 048 - Review diagnostics for stage trait extraction: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 049 - Review integration risk for stage trait extraction: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 050 - Add performance check for stage trait extraction: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: extract input type, output type, state type, result type, and exception policy from user stages.
- Task 051 - Define API for fixed-string DSL support: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 052 - Implement minimal path for fixed-string DSL support: build the smallest working implementation that compiles in the supported compiler matrix; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 053 - Add concept guard for fixed-string DSL support: add a user-facing concept or requires-expression so failures stop at the boundary; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 054 - Add positive test for fixed-string DSL support: add a compile-pass and runtime-pass test proving the supported use case works; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 055 - Add negative test for fixed-string DSL support: add a compile-fail test proving the invalid use case fails with the intended message; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 056 - Measure compile time for fixed-string DSL support: record clean and incremental compile time impact with the benchmark harness; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 057 - Document usage for fixed-string DSL support: write one good example and one bad example that a new user can understand; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 058 - Review diagnostics for fixed-string DSL support: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 059 - Review integration risk for fixed-string DSL support: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 060 - Add performance check for fixed-string DSL support: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: support `pb::name<"stage">` style names with a fallback for compilers that reject class NTTP strings.
- Task 061 - Define API for concept gates: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 062 - Implement minimal path for concept gates: build the smallest working implementation that compiles in the supported compiler matrix; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 063 - Add concept guard for concept gates: add a user-facing concept or requires-expression so failures stop at the boundary; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 064 - Add positive test for concept gates: add a compile-pass and runtime-pass test proving the supported use case works; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 065 - Add negative test for concept gates: add a compile-fail test proving the invalid use case fails with the intended message; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 066 - Measure compile time for concept gates: record clean and incremental compile time impact with the benchmark harness; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 067 - Document usage for concept gates: write one good example and one bad example that a new user can understand; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 068 - Review diagnostics for concept gates: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 069 - Review integration risk for concept gates: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 070 - Add performance check for concept gates: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: define user-facing concepts that reject invalid stages before internal templates instantiate deeply.
- Task 071 - Define API for connectability checker: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 072 - Implement minimal path for connectability checker: build the smallest working implementation that compiles in the supported compiler matrix; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 073 - Add concept guard for connectability checker: add a user-facing concept or requires-expression so failures stop at the boundary; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 074 - Add positive test for connectability checker: add a compile-pass and runtime-pass test proving the supported use case works; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 075 - Add negative test for connectability checker: add a compile-fail test proving the invalid use case fails with the intended message; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 076 - Measure compile time for connectability checker: record clean and incremental compile time impact with the benchmark harness; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 077 - Document usage for connectability checker: write one good example and one bad example that a new user can understand; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 078 - Review diagnostics for connectability checker: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 079 - Review integration risk for connectability checker: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 080 - Add performance check for connectability checker: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: validate adjacent stage edges and produce one error record per invalid edge.
- Task 081 - Define API for linear chain builder: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 082 - Implement minimal path for linear chain builder: build the smallest working implementation that compiles in the supported compiler matrix; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 083 - Add concept guard for linear chain builder: add a user-facing concept or requires-expression so failures stop at the boundary; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 084 - Add positive test for linear chain builder: add a compile-pass and runtime-pass test proving the supported use case works; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 085 - Add negative test for linear chain builder: add a compile-fail test proving the invalid use case fails with the intended message; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 086 - Measure compile time for linear chain builder: record clean and incremental compile time impact with the benchmark harness; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 087 - Document usage for linear chain builder: write one good example and one bad example that a new user can understand; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 088 - Review diagnostics for linear chain builder: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 089 - Review integration risk for linear chain builder: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 090 - Add performance check for linear chain builder: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: implement `pb::start<T>::then<Stage>::then<Stage>::done` for the first release.
- Task 091 - Define API for branch graph builder: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 092 - Implement minimal path for branch graph builder: build the smallest working implementation that compiles in the supported compiler matrix; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 093 - Add concept guard for branch graph builder: add a user-facing concept or requires-expression so failures stop at the boundary; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 094 - Add positive test for branch graph builder: add a compile-pass and runtime-pass test proving the supported use case works; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 095 - Add negative test for branch graph builder: add a compile-fail test proving the invalid use case fails with the intended message; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 096 - Measure compile time for branch graph builder: record clean and incremental compile time impact with the benchmark harness; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 097 - Document usage for branch graph builder: write one good example and one bad example that a new user can understand; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 098 - Review diagnostics for branch graph builder: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 099 - Review integration risk for branch graph builder: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 100 - Add performance check for branch graph builder: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: add `fork`, `select`, `join`, and branch label types after linear chains are stable.
- Task 101 - Define API for graph metadata model: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 102 - Implement minimal path for graph metadata model: build the smallest working implementation that compiles in the supported compiler matrix; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 103 - Add concept guard for graph metadata model: add a user-facing concept or requires-expression so failures stop at the boundary; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 104 - Add positive test for graph metadata model: add a compile-pass and runtime-pass test proving the supported use case works; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 105 - Add negative test for graph metadata model: add a compile-fail test proving the invalid use case fails with the intended message; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 106 - Measure compile time for graph metadata model: record clean and incremental compile time impact with the benchmark harness; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 107 - Document usage for graph metadata model: write one good example and one bad example that a new user can understand; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 108 - Review diagnostics for graph metadata model: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 109 - Review integration risk for graph metadata model: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 110 - Add performance check for graph metadata model: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: store names, categories, version strings, cost hints, concurrency hints, and documentation fields.
- Task 111 - Define API for graph export pipeline: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 112 - Implement minimal path for graph export pipeline: build the smallest working implementation that compiles in the supported compiler matrix; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 113 - Add concept guard for graph export pipeline: add a user-facing concept or requires-expression so failures stop at the boundary; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 114 - Add positive test for graph export pipeline: add a compile-pass and runtime-pass test proving the supported use case works; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 115 - Add negative test for graph export pipeline: add a compile-fail test proving the invalid use case fails with the intended message; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 116 - Measure compile time for graph export pipeline: record clean and incremental compile time impact with the benchmark harness; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 117 - Document usage for graph export pipeline: write one good example and one bad example that a new user can understand; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 118 - Review diagnostics for graph export pipeline: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 119 - Review integration risk for graph export pipeline: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 120 - Add performance check for graph export pipeline: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: export validated graphs to DOT, JSON, and a compact text format for compiler-error debugging.
- Task 121 - Define API for default result model: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: standardize success values, error values, moved values, references, and void stages.
- Task 122 - Implement minimal path for default result model: build the smallest working implementation that compiles in the supported compiler matrix; scope: standardize success values, error values, moved values, references, and void stages.
- Task 123 - Add concept guard for default result model: add a user-facing concept or requires-expression so failures stop at the boundary; scope: standardize success values, error values, moved values, references, and void stages.
- Task 124 - Add positive test for default result model: add a compile-pass and runtime-pass test proving the supported use case works; scope: standardize success values, error values, moved values, references, and void stages.
- Task 125 - Add negative test for default result model: add a compile-fail test proving the invalid use case fails with the intended message; scope: standardize success values, error values, moved values, references, and void stages.
- Task 126 - Measure compile time for default result model: record clean and incremental compile time impact with the benchmark harness; scope: standardize success values, error values, moved values, references, and void stages.
- Task 127 - Document usage for default result model: write one good example and one bad example that a new user can understand; scope: standardize success values, error values, moved values, references, and void stages.
- Task 128 - Review diagnostics for default result model: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: standardize success values, error values, moved values, references, and void stages.
- Task 129 - Review integration risk for default result model: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: standardize success values, error values, moved values, references, and void stages.
- Task 130 - Add performance check for default result model: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: standardize success values, error values, moved values, references, and void stages.
- Task 131 - Define API for expected adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 132 - Implement minimal path for expected adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 133 - Add concept guard for expected adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 134 - Add positive test for expected adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 135 - Add negative test for expected adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 136 - Measure compile time for expected adapter: record clean and incremental compile time impact with the benchmark harness; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 137 - Document usage for expected adapter: write one good example and one bad example that a new user can understand; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 138 - Review diagnostics for expected adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 139 - Review integration risk for expected adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 140 - Add performance check for expected adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: normalize `std::expected<T,E>` and fallback expected-like types into the runtime error path.
- Task 141 - Define API for exception adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: convert throwing callables into expected results according to an explicit policy.
- Task 142 - Implement minimal path for exception adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: convert throwing callables into expected results according to an explicit policy.
- Task 143 - Add concept guard for exception adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: convert throwing callables into expected results according to an explicit policy.
- Task 144 - Add positive test for exception adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: convert throwing callables into expected results according to an explicit policy.
- Task 145 - Add negative test for exception adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: convert throwing callables into expected results according to an explicit policy.
- Task 146 - Measure compile time for exception adapter: record clean and incremental compile time impact with the benchmark harness; scope: convert throwing callables into expected results according to an explicit policy.
- Task 147 - Document usage for exception adapter: write one good example and one bad example that a new user can understand; scope: convert throwing callables into expected results according to an explicit policy.
- Task 148 - Review diagnostics for exception adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: convert throwing callables into expected results according to an explicit policy.
- Task 149 - Review integration risk for exception adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: convert throwing callables into expected results according to an explicit policy.
- Task 150 - Add performance check for exception adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: convert throwing callables into expected results according to an explicit policy.
- Task 151 - Define API for assertion layer: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 152 - Implement minimal path for assertion layer: build the smallest working implementation that compiles in the supported compiler matrix; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 153 - Add concept guard for assertion layer: add a user-facing concept or requires-expression so failures stop at the boundary; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 154 - Add positive test for assertion layer: add a compile-pass and runtime-pass test proving the supported use case works; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 155 - Add negative test for assertion layer: add a compile-fail test proving the invalid use case fails with the intended message; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 156 - Measure compile time for assertion layer: record clean and incremental compile time impact with the benchmark harness; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 157 - Document usage for assertion layer: write one good example and one bad example that a new user can understand; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 158 - Review diagnostics for assertion layer: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 159 - Review integration risk for assertion layer: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 160 - Add performance check for assertion layer: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: implement debug, release, and hardened assertion policies without depending on future contracts.
- Task 161 - Define API for compile-time diagnostics: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 162 - Implement minimal path for compile-time diagnostics: build the smallest working implementation that compiles in the supported compiler matrix; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 163 - Add concept guard for compile-time diagnostics: add a user-facing concept or requires-expression so failures stop at the boundary; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 164 - Add positive test for compile-time diagnostics: add a compile-pass and runtime-pass test proving the supported use case works; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 165 - Add negative test for compile-time diagnostics: add a compile-fail test proving the invalid use case fails with the intended message; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 166 - Measure compile time for compile-time diagnostics: record clean and incremental compile time impact with the benchmark harness; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 167 - Document usage for compile-time diagnostics: write one good example and one bad example that a new user can understand; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 168 - Review diagnostics for compile-time diagnostics: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 169 - Review integration risk for compile-time diagnostics: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 170 - Add performance check for compile-time diagnostics: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: create named error types for missing input, output mismatch, invalid callable, and bad policy.
- Task 171 - Define API for runtime diagnostic records: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 172 - Implement minimal path for runtime diagnostic records: build the smallest working implementation that compiles in the supported compiler matrix; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 173 - Add concept guard for runtime diagnostic records: add a user-facing concept or requires-expression so failures stop at the boundary; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 174 - Add positive test for runtime diagnostic records: add a compile-pass and runtime-pass test proving the supported use case works; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 175 - Add negative test for runtime diagnostic records: add a compile-fail test proving the invalid use case fails with the intended message; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 176 - Measure compile time for runtime diagnostic records: record clean and incremental compile time impact with the benchmark harness; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 177 - Document usage for runtime diagnostic records: write one good example and one bad example that a new user can understand; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 178 - Review diagnostics for runtime diagnostic records: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 179 - Review integration risk for runtime diagnostic records: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 180 - Add performance check for runtime diagnostic records: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: record stage id, edge id, input type, output type, source location, elapsed time, and error cause.
- Task 181 - Define API for sequential executor: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 182 - Implement minimal path for sequential executor: build the smallest working implementation that compiles in the supported compiler matrix; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 183 - Add concept guard for sequential executor: add a user-facing concept or requires-expression so failures stop at the boundary; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 184 - Add positive test for sequential executor: add a compile-pass and runtime-pass test proving the supported use case works; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 185 - Add negative test for sequential executor: add a compile-fail test proving the invalid use case fails with the intended message; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 186 - Measure compile time for sequential executor: record clean and incremental compile time impact with the benchmark harness; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 187 - Document usage for sequential executor: write one good example and one bad example that a new user can understand; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 188 - Review diagnostics for sequential executor: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 189 - Review integration risk for sequential executor: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 190 - Add performance check for sequential executor: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: execute a validated chain with predictable value-category propagation and deterministic errors.
- Task 191 - Define API for thread-pool executor: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 192 - Implement minimal path for thread-pool executor: build the smallest working implementation that compiles in the supported compiler matrix; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 193 - Add concept guard for thread-pool executor: add a user-facing concept or requires-expression so failures stop at the boundary; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 194 - Add positive test for thread-pool executor: add a compile-pass and runtime-pass test proving the supported use case works; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 195 - Add negative test for thread-pool executor: add a compile-fail test proving the invalid use case fails with the intended message; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 196 - Measure compile time for thread-pool executor: record clean and incremental compile time impact with the benchmark harness; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 197 - Document usage for thread-pool executor: write one good example and one bad example that a new user can understand; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 198 - Review diagnostics for thread-pool executor: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 199 - Review integration risk for thread-pool executor: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 200 - Add performance check for thread-pool executor: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: add a simple internal thread-pool backend only after sequential semantics are frozen.
- Task 201 - Define API for coroutine adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 202 - Implement minimal path for coroutine adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 203 - Add concept guard for coroutine adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 204 - Add positive test for coroutine adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 205 - Add negative test for coroutine adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 206 - Measure compile time for coroutine adapter: record clean and incremental compile time impact with the benchmark harness; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 207 - Document usage for coroutine adapter: write one good example and one bad example that a new user can understand; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 208 - Review diagnostics for coroutine adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 209 - Review integration risk for coroutine adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 210 - Add performance check for coroutine adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: adapt coroutine-producing stages without forcing coroutine support into the base API.
- Task 211 - Define API for sender receiver adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 212 - Implement minimal path for sender receiver adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 213 - Add concept guard for sender receiver adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 214 - Add positive test for sender receiver adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 215 - Add negative test for sender receiver adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 216 - Measure compile time for sender receiver adapter: record clean and incremental compile time impact with the benchmark harness; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 217 - Document usage for sender receiver adapter: write one good example and one bad example that a new user can understand; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 218 - Review diagnostics for sender receiver adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 219 - Review integration risk for sender receiver adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 220 - Add performance check for sender receiver adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: prototype a sender/receiver backend behind a feature gate and treat it as experimental.
- Task 221 - Define API for Taskflow adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 222 - Implement minimal path for Taskflow adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 223 - Add concept guard for Taskflow adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 224 - Add positive test for Taskflow adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 225 - Add negative test for Taskflow adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 226 - Measure compile time for Taskflow adapter: record clean and incremental compile time impact with the benchmark harness; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 227 - Document usage for Taskflow adapter: write one good example and one bad example that a new user can understand; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 228 - Review diagnostics for Taskflow adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 229 - Review integration risk for Taskflow adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 230 - Add performance check for Taskflow adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: lower a validated graph to Taskflow tasks only through a separate optional module.
- Task 231 - Define API for oneTBB adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 232 - Implement minimal path for oneTBB adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 233 - Add concept guard for oneTBB adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 234 - Add positive test for oneTBB adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 235 - Add negative test for oneTBB adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 236 - Measure compile time for oneTBB adapter: record clean and incremental compile time impact with the benchmark harness; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 237 - Document usage for oneTBB adapter: write one good example and one bad example that a new user can understand; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 238 - Review diagnostics for oneTBB adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 239 - Review integration risk for oneTBB adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 240 - Add performance check for oneTBB adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: map compatible linear and filter-style pipelines to oneTBB token-based execution.
- Task 241 - Define API for range adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 242 - Implement minimal path for range adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 243 - Add concept guard for range adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 244 - Add positive test for range adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 245 - Add negative test for range adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 246 - Measure compile time for range adapter: record clean and incremental compile time impact with the benchmark harness; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 247 - Document usage for range adapter: write one good example and one bad example that a new user can understand; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 248 - Review diagnostics for range adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 249 - Review integration risk for range adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 250 - Add performance check for range adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: support range-like stages for map, filter, transform, take, reduce, and sink patterns.
- Task 251 - Define API for Rx adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: adapt observable-like event streams as an optional integration target.
- Task 252 - Implement minimal path for Rx adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: adapt observable-like event streams as an optional integration target.
- Task 253 - Add concept guard for Rx adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: adapt observable-like event streams as an optional integration target.
- Task 254 - Add positive test for Rx adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: adapt observable-like event streams as an optional integration target.
- Task 255 - Add negative test for Rx adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: adapt observable-like event streams as an optional integration target.
- Task 256 - Measure compile time for Rx adapter: record clean and incremental compile time impact with the benchmark harness; scope: adapt observable-like event streams as an optional integration target.
- Task 257 - Document usage for Rx adapter: write one good example and one bad example that a new user can understand; scope: adapt observable-like event streams as an optional integration target.
- Task 258 - Review diagnostics for Rx adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: adapt observable-like event streams as an optional integration target.
- Task 259 - Review integration risk for Rx adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: adapt observable-like event streams as an optional integration target.
- Task 260 - Add performance check for Rx adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: adapt observable-like event streams as an optional integration target.
- Task 261 - Define API for free function adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 262 - Implement minimal path for free function adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 263 - Add concept guard for free function adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 264 - Add positive test for free function adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 265 - Add negative test for free function adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 266 - Measure compile time for free function adapter: record clean and incremental compile time impact with the benchmark harness; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 267 - Document usage for free function adapter: write one good example and one bad example that a new user can understand; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 268 - Review diagnostics for free function adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 269 - Review integration risk for free function adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 270 - Add performance check for free function adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: wrap free functions with explicit input and output traits when deduction is ambiguous.
- Task 271 - Define API for member function adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 272 - Implement minimal path for member function adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 273 - Add concept guard for member function adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 274 - Add positive test for member function adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 275 - Add negative test for member function adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 276 - Measure compile time for member function adapter: record clean and incremental compile time impact with the benchmark harness; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 277 - Document usage for member function adapter: write one good example and one bad example that a new user can understand; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 278 - Review diagnostics for member function adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 279 - Review integration risk for member function adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 280 - Add performance check for member function adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: wrap const, non-const, ref-qualified, and overloaded member functions with named adapters.
- Task 281 - Define API for stateful functor adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 282 - Implement minimal path for stateful functor adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 283 - Add concept guard for stateful functor adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 284 - Add positive test for stateful functor adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 285 - Add negative test for stateful functor adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 286 - Measure compile time for stateful functor adapter: record clean and incremental compile time impact with the benchmark harness; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 287 - Document usage for stateful functor adapter: write one good example and one bad example that a new user can understand; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 288 - Review diagnostics for stateful functor adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 289 - Review integration risk for stateful functor adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 290 - Add performance check for stateful functor adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: support copyable, movable-only, and shared-state functors with explicit lifetime policies.
- Task 291 - Define API for lambda adapter: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 292 - Implement minimal path for lambda adapter: build the smallest working implementation that compiles in the supported compiler matrix; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 293 - Add concept guard for lambda adapter: add a user-facing concept or requires-expression so failures stop at the boundary; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 294 - Add positive test for lambda adapter: add a compile-pass and runtime-pass test proving the supported use case works; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 295 - Add negative test for lambda adapter: add a compile-fail test proving the invalid use case fails with the intended message; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 296 - Measure compile time for lambda adapter: record clean and incremental compile time impact with the benchmark harness; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 297 - Document usage for lambda adapter: write one good example and one bad example that a new user can understand; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 298 - Review diagnostics for lambda adapter: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 299 - Review integration risk for lambda adapter: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 300 - Add performance check for lambda adapter: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: support lambdas through `pb::adapt` while keeping named stage types as the recommended style.
- Task 301 - Define API for configuration system: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 302 - Implement minimal path for configuration system: build the smallest working implementation that compiles in the supported compiler matrix; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 303 - Add concept guard for configuration system: add a user-facing concept or requires-expression so failures stop at the boundary; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 304 - Add positive test for configuration system: add a compile-pass and runtime-pass test proving the supported use case works; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 305 - Add negative test for configuration system: add a compile-fail test proving the invalid use case fails with the intended message; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 306 - Measure compile time for configuration system: record clean and incremental compile time impact with the benchmark harness; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 307 - Document usage for configuration system: write one good example and one bad example that a new user can understand; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 308 - Review diagnostics for configuration system: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 309 - Review integration risk for configuration system: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 310 - Add performance check for configuration system: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: provide compile-time and runtime configuration objects with strict override precedence.
- Task 311 - Define API for policy system: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 312 - Implement minimal path for policy system: build the smallest working implementation that compiles in the supported compiler matrix; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 313 - Add concept guard for policy system: add a user-facing concept or requires-expression so failures stop at the boundary; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 314 - Add positive test for policy system: add a compile-pass and runtime-pass test proving the supported use case works; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 315 - Add negative test for policy system: add a compile-fail test proving the invalid use case fails with the intended message; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 316 - Measure compile time for policy system: record clean and incremental compile time impact with the benchmark harness; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 317 - Document usage for policy system: write one good example and one bad example that a new user can understand; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 318 - Review diagnostics for policy system: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 319 - Review integration risk for policy system: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 320 - Add performance check for policy system: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: define policies for errors, assertions, scheduling, ownership, tracing, and compile-time diagnostics.
- Task 321 - Define API for debug tracing: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 322 - Implement minimal path for debug tracing: build the smallest working implementation that compiles in the supported compiler matrix; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 323 - Add concept guard for debug tracing: add a user-facing concept or requires-expression so failures stop at the boundary; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 324 - Add positive test for debug tracing: add a compile-pass and runtime-pass test proving the supported use case works; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 325 - Add negative test for debug tracing: add a compile-fail test proving the invalid use case fails with the intended message; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 326 - Measure compile time for debug tracing: record clean and incremental compile time impact with the benchmark harness; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 327 - Document usage for debug tracing: write one good example and one bad example that a new user can understand; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 328 - Review diagnostics for debug tracing: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 329 - Review integration risk for debug tracing: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 330 - Add performance check for debug tracing: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: emit structured start, finish, error, and value-category events for every stage execution.
- Task 331 - Define API for compile-fail harness: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 332 - Implement minimal path for compile-fail harness: build the smallest working implementation that compiles in the supported compiler matrix; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 333 - Add concept guard for compile-fail harness: add a user-facing concept or requires-expression so failures stop at the boundary; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 334 - Add positive test for compile-fail harness: add a compile-pass and runtime-pass test proving the supported use case works; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 335 - Add negative test for compile-fail harness: add a compile-fail test proving the invalid use case fails with the intended message; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 336 - Measure compile time for compile-fail harness: record clean and incremental compile time impact with the benchmark harness; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 337 - Document usage for compile-fail harness: write one good example and one bad example that a new user can understand; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 338 - Review diagnostics for compile-fail harness: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 339 - Review integration risk for compile-fail harness: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 340 - Add performance check for compile-fail harness: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: use compile-fail tests to lock in readable diagnostics and reject template-regression noise.
- Task 341 - Define API for compile-time benchmark harness: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 342 - Implement minimal path for compile-time benchmark harness: build the smallest working implementation that compiles in the supported compiler matrix; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 343 - Add concept guard for compile-time benchmark harness: add a user-facing concept or requires-expression so failures stop at the boundary; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 344 - Add positive test for compile-time benchmark harness: add a compile-pass and runtime-pass test proving the supported use case works; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 345 - Add negative test for compile-time benchmark harness: add a compile-fail test proving the invalid use case fails with the intended message; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 346 - Measure compile time for compile-time benchmark harness: record clean and incremental compile time impact with the benchmark harness; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 347 - Document usage for compile-time benchmark harness: write one good example and one bad example that a new user can understand; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 348 - Review diagnostics for compile-time benchmark harness: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 349 - Review integration risk for compile-time benchmark harness: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 350 - Add performance check for compile-time benchmark harness: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: measure clean build cost, incremental build cost, header parse cost, and template instantiation cost.
- Task 351 - Define API for runtime benchmark harness: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 352 - Implement minimal path for runtime benchmark harness: build the smallest working implementation that compiles in the supported compiler matrix; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 353 - Add concept guard for runtime benchmark harness: add a user-facing concept or requires-expression so failures stop at the boundary; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 354 - Add positive test for runtime benchmark harness: add a compile-pass and runtime-pass test proving the supported use case works; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 355 - Add negative test for runtime benchmark harness: add a compile-fail test proving the invalid use case fails with the intended message; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 356 - Measure compile time for runtime benchmark harness: record clean and incremental compile time impact with the benchmark harness; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 357 - Document usage for runtime benchmark harness: write one good example and one bad example that a new user can understand; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 358 - Review diagnostics for runtime benchmark harness: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 359 - Review integration risk for runtime benchmark harness: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 360 - Add performance check for runtime benchmark harness: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: measure dispatch overhead, value movement, error propagation, and executor throughput.
- Task 361 - Define API for CMake options: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 362 - Implement minimal path for CMake options: build the smallest working implementation that compiles in the supported compiler matrix; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 363 - Add concept guard for CMake options: add a user-facing concept or requires-expression so failures stop at the boundary; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 364 - Add positive test for CMake options: add a compile-pass and runtime-pass test proving the supported use case works; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 365 - Add negative test for CMake options: add a compile-fail test proving the invalid use case fails with the intended message; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 366 - Measure compile time for CMake options: record clean and incremental compile time impact with the benchmark harness; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 367 - Document usage for CMake options: write one good example and one bad example that a new user can understand; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 368 - Review diagnostics for CMake options: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 369 - Review integration risk for CMake options: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 370 - Add performance check for CMake options: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: provide options for examples, tests, benchmarks, optional backends, warnings, PCH, unity, and modules.
- Task 371 - Define API for precompiled header plan: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 372 - Implement minimal path for precompiled header plan: build the smallest working implementation that compiles in the supported compiler matrix; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 373 - Add concept guard for precompiled header plan: add a user-facing concept or requires-expression so failures stop at the boundary; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 374 - Add positive test for precompiled header plan: add a compile-pass and runtime-pass test proving the supported use case works; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 375 - Add negative test for precompiled header plan: add a compile-fail test proving the invalid use case fails with the intended message; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 376 - Measure compile time for precompiled header plan: record clean and incremental compile time impact with the benchmark harness; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 377 - Document usage for precompiled header plan: write one good example and one bad example that a new user can understand; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 378 - Review diagnostics for precompiled header plan: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 379 - Review integration risk for precompiled header plan: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 380 - Add performance check for precompiled header plan: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: apply PCH only to stable dependencies and never hide excessive includes in core headers.
- Task 381 - Define API for unity build plan: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 382 - Implement minimal path for unity build plan: build the smallest working implementation that compiles in the supported compiler matrix; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 383 - Add concept guard for unity build plan: add a user-facing concept or requires-expression so failures stop at the boundary; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 384 - Add positive test for unity build plan: add a compile-pass and runtime-pass test proving the supported use case works; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 385 - Add negative test for unity build plan: add a compile-fail test proving the invalid use case fails with the intended message; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 386 - Measure compile time for unity build plan: record clean and incremental compile time impact with the benchmark harness; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 387 - Document usage for unity build plan: write one good example and one bad example that a new user can understand; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 388 - Review diagnostics for unity build plan: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 389 - Review integration risk for unity build plan: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 390 - Add performance check for unity build plan: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: enable unity builds for tests and internal implementation targets, not as the default public integration path.
- Task 391 - Define API for module interface plan: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 392 - Implement minimal path for module interface plan: build the smallest working implementation that compiles in the supported compiler matrix; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 393 - Add concept guard for module interface plan: add a user-facing concept or requires-expression so failures stop at the boundary; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 394 - Add positive test for module interface plan: add a compile-pass and runtime-pass test proving the supported use case works; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 395 - Add negative test for module interface plan: add a compile-fail test proving the invalid use case fails with the intended message; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 396 - Measure compile time for module interface plan: record clean and incremental compile time impact with the benchmark harness; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 397 - Document usage for module interface plan: write one good example and one bad example that a new user can understand; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 398 - Review diagnostics for module interface plan: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 399 - Review integration risk for module interface plan: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 400 - Add performance check for module interface plan: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: prototype named modules after header API stabilizes and measure against header-only usage.
- Task 401 - Define API for compiler matrix: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 402 - Implement minimal path for compiler matrix: build the smallest working implementation that compiles in the supported compiler matrix; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 403 - Add concept guard for compiler matrix: add a user-facing concept or requires-expression so failures stop at the boundary; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 404 - Add positive test for compiler matrix: add a compile-pass and runtime-pass test proving the supported use case works; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 405 - Add negative test for compiler matrix: add a compile-fail test proving the invalid use case fails with the intended message; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 406 - Measure compile time for compiler matrix: record clean and incremental compile time impact with the benchmark harness; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 407 - Document usage for compiler matrix: write one good example and one bad example that a new user can understand; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 408 - Review diagnostics for compiler matrix: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 409 - Review integration risk for compiler matrix: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 410 - Add performance check for compiler matrix: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: test GCC, Clang, MSVC, C++20, C++23, and selected C++26 feature gates separately.
- Task 411 - Define API for documentation examples: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 412 - Implement minimal path for documentation examples: build the smallest working implementation that compiles in the supported compiler matrix; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 413 - Add concept guard for documentation examples: add a user-facing concept or requires-expression so failures stop at the boundary; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 414 - Add positive test for documentation examples: add a compile-pass and runtime-pass test proving the supported use case works; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 415 - Add negative test for documentation examples: add a compile-fail test proving the invalid use case fails with the intended message; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 416 - Measure compile time for documentation examples: record clean and incremental compile time impact with the benchmark harness; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 417 - Document usage for documentation examples: write one good example and one bad example that a new user can understand; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 418 - Review diagnostics for documentation examples: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 419 - Review integration risk for documentation examples: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 420 - Add performance check for documentation examples: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: write examples that show success cases, failure cases, adapters, policies, and graph export.
- Task 421 - Define API for migration guide: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 422 - Implement minimal path for migration guide: build the smallest working implementation that compiles in the supported compiler matrix; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 423 - Add concept guard for migration guide: add a user-facing concept or requires-expression so failures stop at the boundary; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 424 - Add positive test for migration guide: add a compile-pass and runtime-pass test proving the supported use case works; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 425 - Add negative test for migration guide: add a compile-fail test proving the invalid use case fails with the intended message; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 426 - Measure compile time for migration guide: record clean and incremental compile time impact with the benchmark harness; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 427 - Document usage for migration guide: write one good example and one bad example that a new user can understand; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 428 - Review diagnostics for migration guide: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 429 - Review integration risk for migration guide: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 430 - Add performance check for migration guide: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: provide a path from raw lambdas, ranges pipelines, Taskflow graphs, and manual function chains.
- Task 431 - Define API for release packaging: write the public type names, namespace layout, invariants, and supported language mode for this area; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 432 - Implement minimal path for release packaging: build the smallest working implementation that compiles in the supported compiler matrix; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 433 - Add concept guard for release packaging: add a user-facing concept or requires-expression so failures stop at the boundary; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 434 - Add positive test for release packaging: add a compile-pass and runtime-pass test proving the supported use case works; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 435 - Add negative test for release packaging: add a compile-fail test proving the invalid use case fails with the intended message; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 436 - Measure compile time for release packaging: record clean and incremental compile time impact with the benchmark harness; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 437 - Document usage for release packaging: write one good example and one bad example that a new user can understand; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 438 - Review diagnostics for release packaging: check whether the failure mentions the stage, edge, expected type, actual type, and fix; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 439 - Review integration risk for release packaging: identify ownership, lifetime, ABI, exception, and platform assumptions for this area; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.
- Task 440 - Add performance check for release packaging: add a runtime or compile-time benchmark threshold to prevent silent regressions; scope: ship CMake package config, pkg-config data if useful, license notices, and versioned documentation.

# Part 22 - Diagnostic Catalog

Each diagnostic should have a named internal error type, a short compiler-facing message, a longer documentation entry, and a test that locks the wording at the public boundary.

- Diagnostic 01 `missing_stage_input`: message `stage does not declare an input type`; remediation: add `using input_type = T` or provide `pb::stage_traits<Stage>`.
- Diagnostic 02 `missing_stage_output`: message `stage does not declare an output type`; remediation: add `using output_type = T` or specialize traits for the stage.
- Diagnostic 03 `invalid_call_operator`: message `stage is not invocable with the expected input`; remediation: implement `operator()(Input)` or adapt a compatible callable.
- Diagnostic 04 `edge_type_mismatch`: message `previous output type cannot feed the next input type`; remediation: insert a conversion stage or change one of the declared types.
- Diagnostic 05 `void_to_value_mismatch`: message `a void stage is followed by a stage requiring a value`; remediation: use a context stage, side-effect sink, or produce a real output.
- Diagnostic 06 `value_to_void_sink_mismatch`: message `a value-producing stage is connected to a sink that only accepts void`; remediation: consume the value explicitly or mark the sink as accepting the value type.
- Diagnostic 07 `ambiguous_overload`: message `callable overload cannot be selected by traits`; remediation: wrap the overload with an explicit function pointer or adapter.
- Diagnostic 08 `deleted_callable`: message `selected callable is deleted or inaccessible`; remediation: choose a public non-deleted callable or expose a wrapper.
- Diagnostic 09 `non_movable_output`: message `stage output must move between stages but is not movable`; remediation: use references, shared ownership, or an in-place policy.
- Diagnostic 10 `dangling_reference_risk`: message `stage returns a reference that cannot outlive the stage call`; remediation: return an owning value or declare a safe borrowing policy.
- Diagnostic 11 `invalid_reference_policy`: message `reference output requires a reference lifetime policy`; remediation: add `pb::borrowed`, `pb::owned`, or `pb::view` metadata.
- Diagnostic 12 `throwing_stage_without_policy`: message `stage may throw while the pipeline uses no-throw execution`; remediation: select exception-to-error policy or mark the stage no-throw.
- Diagnostic 13 `expected_error_type_mismatch`: message `expected-like stage error type is incompatible with pipeline error type`; remediation: add an error mapper or choose a common error variant.
- Diagnostic 14 `missing_error_mapper`: message `custom error type cannot be normalized`; remediation: provide `pb::map_error` or specialize the error adapter.
- Diagnostic 15 `bad_assertion_policy`: message `assertion policy does not satisfy the assertion-policy concept`; remediation: implement the required hooks for precondition and invariant failure.
- Diagnostic 16 `bad_executor_policy`: message `executor policy cannot execute the validated graph form`; remediation: choose sequential executor or enable a compatible backend.
- Diagnostic 17 `branch_join_mismatch`: message `branch outputs cannot be joined into the requested type`; remediation: provide a join stage or a common variant/output mapping.
- Diagnostic 18 `cycle_detected`: message `graph contains a cycle unsupported by the selected executor`; remediation: remove the cycle or choose a feedback-capable backend if implemented.
- Diagnostic 19 `duplicate_stage_name`: message `two stages use the same public name in one graph`; remediation: rename one stage or provide a stable unique id.
- Diagnostic 20 `missing_source_stage`: message `pipeline has no source input or start type`; remediation: use `pb::start<T>` or define a source stage.
- Diagnostic 21 `missing_sink_stage`: message `pipeline output is unused in a context requiring a sink`; remediation: call `.run(input)` for value output or attach a sink.
- Diagnostic 22 `policy_conflict`: message `two policies set contradictory behavior`; remediation: remove one policy or set precedence explicitly.
- Diagnostic 23 `metadata_string_too_long`: message `compile-time metadata exceeds configured limit`; remediation: shorten the name or raise the metadata limit.
- Diagnostic 24 `unsupported_compiler_feature`: message `selected DSL feature requires a newer compiler`; remediation: disable the feature gate or update the compiler.
- Diagnostic 25 `module_header_mix_error`: message `module mode and header mode were mixed incorrectly`; remediation: use one integration mode per target.
- Diagnostic 26 `backend_not_enabled`: message `requested optional backend was not compiled`; remediation: turn on the CMake option or use a core backend.
- Diagnostic 27 `graph_export_not_available`: message `graph export format is not enabled`; remediation: enable the export component or choose text diagnostics.
- Diagnostic 28 `invalid_stage_state`: message `stage state does not satisfy the selected ownership policy`; remediation: make it copyable, movable, shared, or externally owned.
- Diagnostic 29 `non_const_call_in_const_pipeline`: message `stage requires mutable access in a const execution context`; remediation: mark pipeline mutable or make the call operator const.
- Diagnostic 30 `over_eager_template_instantiation`: message `pipeline instantiated heavy templates before validation completed`; remediation: move heavy logic behind a concept gate or lazy trait.
- Diagnostic 31 `recursive_template_limit`: message `validation exceeded compiler template recursion limits`; remediation: replace recursive algorithm with index or fold based implementation.
- Diagnostic 32 `bad_custom_trait`: message `user-provided trait misses required aliases or has contradictory aliases`; remediation: complete the trait or remove the specialization.
- Diagnostic 33 `adapter_requires_signature`: message `adapter cannot infer the callable signature`; remediation: provide explicit `pb::fn<Input, Output>` annotation.
- Diagnostic 34 `context_type_missing`: message `stage asks for execution context but pipeline has none`; remediation: add a context object or remove context-aware stage requirement.
- Diagnostic 35 `context_type_mismatch`: message `stage requires a different context type`; remediation: use a context adapter or align context aliases.
- Diagnostic 36 `scheduler_affinity_conflict`: message `two stages require incompatible scheduler affinity`; remediation: split the graph or define a scheduler transfer stage.
- Diagnostic 37 `token_limit_invalid`: message `pipeline token limit is zero or unsupported`; remediation: set a positive token count supported by the backend.
- Diagnostic 38 `parallel_side_effect_warning`: message `parallel stage mutates shared state without a safety policy`; remediation: use a serial policy, lock policy, or immutable data.
- Diagnostic 39 `unhandled_cancellation`: message `stage supports cancellation but pipeline policy ignores it`; remediation: enable cancellation propagation or disable cancellation-aware stage.
- Diagnostic 40 `unsupported_coroutine_return`: message `coroutine return type is not recognized by the adapter`; remediation: write a coroutine trait or wrap it in a supported sender/task type.
- Diagnostic 41 `sender_completion_mismatch`: message `sender completion signatures do not match the expected stage output`; remediation: map completions to a single result type or choose another backend.
- Diagnostic 42 `range_value_mismatch`: message `range adapter value type does not match the stage input`; remediation: add a transform or change the range stage traits.
- Diagnostic 43 `sink_returns_value`: message `sink policy forbids a sink from returning a value`; remediation: change the stage to a transform or discard explicitly.
- Diagnostic 44 `unstable_abi_export`: message `public graph export depends on compiler-specific type spelling`; remediation: provide stable aliases or user-defined names.
- Diagnostic 45 `unsupported_dynamic_plugin`: message `dynamic plugin stage cannot be validated as a compile-time stage`; remediation: use runtime adapter boundary with explicit erased signature.
- Diagnostic 46 `private_nested_alias`: message `stage aliases are private and cannot be inspected`; remediation: make trait aliases public or specialize external traits.
- Diagnostic 47 `invalid_nttp_name`: message `stage name cannot be represented as a non-type template parameter`; remediation: use the fallback named tag type.
- Diagnostic 48 `source_location_unavailable`: message `source location capture is unavailable for the selected compiler mode`; remediation: omit location or use macro-assisted capture.
- Diagnostic 49 `bad_allocator_policy`: message `allocator policy cannot construct required runtime storage`; remediation: provide a compatible allocator or remove storage customization.
- Diagnostic 50 `excessive_header_dependency`: message `public header includes a forbidden dependency`; remediation: move dependency to implementation or optional adapter header.
- Diagnostic 51 `compile_time_budget_exceeded`: message `compile benchmark exceeded the configured budget`; remediation: inspect time trace and reduce instantiation count.
- Diagnostic 52 `runtime_budget_exceeded`: message `runtime benchmark exceeded the configured budget`; remediation: profile executor, movement, allocation, and error mapping.
- Diagnostic 53 `test_matrix_gap`: message `feature lacks coverage in one required compiler mode`; remediation: add a test or mark the feature experimental.
- Diagnostic 54 `doc_example_outdated`: message `documentation example no longer compiles`; remediation: update the example and include it in CI.
- Diagnostic 55 `version_policy_missing`: message `stage metadata lacks version while graph export requires versioning`; remediation: add version metadata or relax export policy.
- Diagnostic 56 `bad_serialization_trait`: message `graph export cannot serialize custom metadata`; remediation: provide a serialization hook or remove the metadata field.
- Diagnostic 57 `unregistered_named_stage`: message `named stage was referenced but no stage type is registered`; remediation: include the stage header or register the name in the namespace.
- Diagnostic 58 `namespace_collision`: message `stage name collides across namespaces in generated docs`; remediation: use qualified names or explicit public labels.
- Diagnostic 59 `invalid_compile_mode`: message `feature is used in a language mode below its requirement`; remediation: raise the C++ standard or use fallback API.
- Diagnostic 60 `bad_feature_gate`: message `feature gate claims support but compiler/library check failed`; remediation: fix detection logic and add configure-time tests.
- Diagnostic 61 `unsupported_gpu_boundary`: message `GPU execution boundary lacks memory-transfer policy`; remediation: define host/device transfer policy before enabling backend.
- Diagnostic 62 `invalid_retry_policy`: message `retry policy is attached to a non-idempotent stage`; remediation: mark idempotency or disable retries.
- Diagnostic 63 `invalid_cache_policy`: message `cache policy cannot hash or compare the stage input`; remediation: provide key function or disable caching.
- Diagnostic 64 `bad_trace_sink`: message `trace sink cannot consume the diagnostic event type`; remediation: adapt the trace sink signature.
- Diagnostic 65 `pipeline_too_large_for_export`: message `graph export exceeds configured memory or size limit`; remediation: raise limit or export filtered graph view.
- Diagnostic 66 `bad_precondition_result`: message `precondition returns a non-boolean and non-expected-like value`; remediation: return `bool`, `expected<void,E>`, or supported assertion result.
- Diagnostic 67 `postcondition_mismatch`: message `postcondition cannot inspect the produced value`; remediation: change postcondition input type or attach after conversion.
- Diagnostic 68 `state_reset_missing`: message `reusable pipeline has state but no reset policy`; remediation: add reset hook or mark pipeline single-use.
- Diagnostic 69 `multi_input_not_supported`: message `stage requires multiple inputs before join support exists`; remediation: wrap inputs in a tuple or wait for branch/join release.
- Diagnostic 70 `multi_output_not_supported`: message `stage produces multiple outputs before branch support exists`; remediation: return a tuple or explicit branch object.
- Diagnostic 71 `invalid_tuple_mapping`: message `tuple output cannot map to requested branch labels`; remediation: provide labels or a branch mapping policy.
- Diagnostic 72 `erased_stage_missing_signature`: message `type-erased stage lacks explicit erased signature`; remediation: declare `pb::erased<Input, Output>`.
- Diagnostic 73 `plugin_boundary_error`: message `plugin boundary cannot expose compile-time type information`; remediation: treat plugin as runtime stage with explicit dynamic checks.
- Diagnostic 74 `bad_build_option_combo`: message `CMake options select incompatible backend and language mode`; remediation: change options or feature-gate the backend.
- Diagnostic 75 `warning_policy_violation`: message `warning-as-error policy detected public API diagnostic noise`; remediation: fix the warning or narrow the warning flag scope.
- Diagnostic 76 `odr_risk_detected`: message `unity build revealed conflicting macros or duplicate definitions`; remediation: isolate the target or remove macro-dependent public headers.
- Diagnostic 77 `pch_pollution_detected`: message `PCH hides a missing include in a public header`; remediation: disable PCH for header self-sufficiency tests.

# Part 23 - Adapter Contract Matrix

The adapter layer is the main tool for integrating existing user code. Each adapter must declare what it accepts, how it deduces types, how it handles errors, and what it refuses.

- Adapter 01 `free function pointer`: deduce signature from pointer type; reject overload sets without explicit cast.
- Adapter 02 `free function reference`: preserve reference semantics; require a stable function object wrapper for storage.
- Adapter 03 `static member function`: treat as a free function after extracting the class-qualified symbol.
- Adapter 04 `non-static member function`: bind object ownership explicitly using reference, pointer, unique ownership, or shared ownership.
- Adapter 05 `const member function`: allow execution in const pipelines when object storage is const-safe.
- Adapter 06 `ref-qualified member function`: preserve lvalue/rvalue requirements and reject unsafe storage combinations.
- Adapter 07 `overloaded function`: require explicit signature annotation to prevent accidental overload selection.
- Adapter 08 `generic lambda`: require explicit input/output annotation unless deduction is unambiguous at the adapter boundary.
- Adapter 09 `capturing lambda`: store capture by value unless user selects external reference policy.
- Adapter 10 `move-only lambda`: allow single-use pipeline or move-only runtime object; reject copy-required executors.
- Adapter 11 `stateless function object`: store as an empty object and use empty-base optimization when possible.
- Adapter 12 `stateful function object`: require copy/move policy and document whether pipeline instances share state.
- Adapter 13 `mutable function object`: reject const execution or mark the stage as state-mutating.
- Adapter 14 `noexcept callable`: record no-throw property and allow optimized error path.
- Adapter 15 `throwing callable`: wrap exceptions into error records under exception adapter policy.
- Adapter 16 `callable returning void`: convert to `expected<void,E>` or continue with unit marker if next stage accepts it.
- Adapter 17 `callable returning reference`: require lifetime metadata to prevent dangling output.
- Adapter 18 `callable returning pointer`: treat pointer nullability as policy-controlled, not automatically safe.
- Adapter 19 `callable returning optional`: map empty value through configured missing-value error or branch path.
- Adapter 20 `callable returning variant`: allow variant output only if next stage accepts variant or branch selector exists.
- Adapter 21 `callable returning tuple`: support tuple as one value in MVP; map to multi-output only after branch release.
- Adapter 22 `callable returning expected`: propagate expected error without invoking later stages.
- Adapter 23 `callable returning future`: adapt only through async boundary; do not block silently by default.
- Adapter 24 `callable returning coroutine task`: adapt through coroutine support and cancellation policy.
- Adapter 25 `callable returning sender`: adapt through sender/receiver backend and completion-signature checks.
- Adapter 26 `range-producing function`: preserve laziness when compatible with ranges and document single-pass behavior.
- Adapter 27 `input iterator stage`: require iterator category metadata before parallel scheduling.
- Adapter 28 `output iterator sink`: treat as sink with side effects and require ownership/thread-safety policy.
- Adapter 29 `span-based function`: preserve view lifetime and reject temporary-to-span unsafe calls.
- Adapter 30 `string-view function`: reject temporary-producing previous stages unless lifetime is guaranteed.
- Adapter 31 `filesystem path stage`: separate path validation from IO side effects for testability.
- Adapter 32 `C API function`: wrap error code, output pointer, and ownership convention explicitly.
- Adapter 33 `errno-style API`: capture errno immediately and normalize into pipeline error type.
- Adapter 34 `COM-style API`: map HRESULT and output parameters through a dedicated adapter.
- Adapter 35 `POSIX-style API`: map negative return and errno into structured errors.
- Adapter 36 `callback-style API`: support only with an async adapter that controls callback lifetime.
- Adapter 37 `visitor-style API`: turn visitor callbacks into explicit stages or range producers.
- Adapter 38 `polymorphic base class`: adapt virtual call behind type-erased stage with explicit signature.
- Adapter 39 `plugin function`: use runtime-erased boundary and explicit ABI metadata.
- Adapter 40 `template function`: instantiate through explicit template arguments provided by adapter.
- Adapter 41 `constrained template callable`: respect requires-clause and surface concept failure at boundary.
- Adapter 42 `operator pipeline library stage`: wrap existing `operator|` pipe as a named transform stage.
- Adapter 43 `range view adaptor`: adapt view closure as a stage when input is range-like.
- Adapter 44 `Taskflow task`: treat as backend-specific node, not a core stage.
- Adapter 45 `oneTBB filter`: adapt filter mode and token compatibility as backend metadata.
- Adapter 46 `Rx observable operator`: adapt event stream stages only in stream pipeline mode.
- Adapter 47 `allocator-aware component`: thread allocator policy into construction and runtime storage.
- Adapter 48 `logging component`: inject observer sink rather than hard-coding logger dependency.
- Adapter 49 `metrics component`: emit structured events and let metrics backend aggregate.
- Adapter 50 `serialization function`: declare schema version and error handling for malformed input.
- Adapter 51 `deserialization function`: return expected-like error with source position when possible.
- Adapter 52 `GPU kernel wrapper`: require memory-space metadata and transfer policy.
- Adapter 53 `SIMD transform`: declare alignment, vector width, and scalar fallback.
- Adapter 54 `database query function`: treat connection ownership and transaction policy as stage state.
- Adapter 55 `network request function`: separate timeout, retry, cancellation, and error normalization policy.
- Adapter 56 `file IO stage`: declare whether it is source, sink, transform, or side-effect stage.
- Adapter 57 `configuration reader`: make configuration a stage input or context, not a hidden global.
- Adapter 58 `global-state function`: allow only with explicit unsafe/shared-state policy.
- Adapter 59 `thread-local function`: mark thread-affinity and initialization requirements.
- Adapter 60 `signal handler unsafe function`: reject in signal-safe pipeline contexts.
- Adapter 61 `embedded ISR-safe function`: require no allocation, no exceptions, and deterministic timing policy.
- Adapter 62 `test double`: allow mock stage with same declared input/output traits.
- Adapter 63 `legacy macro API`: wrap macro expansion in a normal function before pipeline integration.
- Adapter 64 `preprocessor-generated stage`: require generated headers to be self-contained and tested.
- Adapter 65 `reflection-assisted adapter`: future feature; use C++26 reflection only under feature gates.

# Part 24 - Compile-Time Optimization Experiments

The project should keep a permanent experiment log. Each item below is a measurement, not an assumption. A feature is accepted only if the measured cost fits the budget or the tradeoff is documented.

- Experiment 01 `baseline empty include`: measure translation unit including only the root public header.
- Experiment 02 `stage traits include split`: compare monolithic traits header against specialized small headers.
- Experiment 03 `type-list algorithm recursion`: compare recursive validation against fold-expression validation.
- Experiment 04 `index sequence construction`: compare standard index sequence against custom logarithmic helper only if needed.
- Experiment 05 `concept boundary placement`: measure failures caught at concept boundary versus deep trait instantiation.
- Experiment 06 `static assert wording`: measure diagnostic clarity and compile cost of detailed named error types.
- Experiment 07 `fixed string NTTP`: compare compile cost of string NTTP names against tag-type names.
- Experiment 08 `metadata size limit`: measure graph metadata instantiation cost at 10, 100, and 1000 stages.
- Experiment 09 `header self-sufficiency`: compile every public header alone with PCH disabled.
- Experiment 10 `PCH effectiveness`: measure stable dependency PCH effect on examples and tests.
- Experiment 11 `PCH pollution`: verify missing includes are not hidden by PCH in CI.
- Experiment 12 `unity build effect`: measure unity builds for tests and examples separately from library usage.
- Experiment 13 `ccache hit rate`: record cache hit rate for normal local edit cycles.
- Experiment 14 `sccache remote hit rate`: record cache hit rate for CI runners when remote storage is configured.
- Experiment 15 `Ninja incremental behavior`: track no-op build and single-header edit rebuild counts.
- Experiment 16 `Clang ftime trace top nodes`: record top template and header costs after each major feature.
- Experiment 17 `MSVC build insights`: record MSVC-specific hotspots if the team uses Visual Studio tooling.
- Experiment 18 `GCC template diagnostics`: measure whether concepts improve or worsen failure noise under GCC.
- Experiment 19 `Clang template diagnostics`: measure boundary failure readability under Clang.
- Experiment 20 `MSVC template diagnostics`: measure concept failure readability and line mapping under MSVC.
- Experiment 21 `explicit instantiation`: test explicit instantiation for common stage and executor combinations.
- Experiment 22 `extern template`: test extern template declarations for examples and benchmark targets.
- Experiment 23 `module interface prototype`: compare named module import against header inclusion on supported compilers.
- Experiment 24 `header unit prototype`: test whether header units help or introduce build complexity.
- Experiment 25 `dependency include audit`: run include analysis and remove unjustified transitive includes.
- Experiment 26 `adapter header isolation`: verify optional backend headers are not included by core headers.
- Experiment 27 `diagnostic catalog cost`: measure cost of named diagnostics and string messages.
- Experiment 28 `graph export cost`: measure compile and runtime cost of DOT and JSON export metadata.
- Experiment 29 `branch validation cost`: compare branch graph validation with linear validation baseline.
- Experiment 30 `error variant size`: measure compile/runtime impact of many error alternatives.
- Experiment 31 `expected fallback cost`: compare standard expected with fallback expected implementation.
- Experiment 32 `exception adapter cost`: measure overhead of exception-to-expected wrapper in hot paths.
- Experiment 33 `source location cost`: measure `std::source_location` capture in metadata and diagnostics.
- Experiment 34 `observer hook cost`: measure tracing disabled, compile-time disabled, and runtime enabled modes.
- Experiment 35 `executor abstraction cost`: compare direct sequential executor against policy-dispatched executor.
- Experiment 36 `value category propagation`: measure copies and moves across long pipelines.
- Experiment 37 `reference stage safety`: measure diagnostics and runtime overhead for borrowing policy.
- Experiment 38 `stateful stage storage`: compare by-value, reference, shared, and arena storage.
- Experiment 39 `small object optimization`: test type-erased runtime stage storage with and without small object optimization.
- Experiment 40 `type-erased boundary cost`: measure plugin/dynamic boundary overhead against typed chain.
- Experiment 41 `range adapter cost`: measure lazy range integration versus materialized container handoff.
- Experiment 42 `thread pool scheduling cost`: measure one-stage, many-stage, and skewed workload scheduling.
- Experiment 43 `Taskflow lowering cost`: compare generated Taskflow graph build time and runtime speed.
- Experiment 44 `oneTBB lowering cost`: measure token counts and filter mode mapping overhead.
- Experiment 45 `sender backend cost`: measure sender chain compile time and runtime once backend is enabled.
- Experiment 46 `C++26 feature gate cost`: measure pack indexing/reflection/contracts prototypes separately.
- Experiment 47 `compile-fail suite time`: track total compile-fail test duration and split slow cases.
- Experiment 48 `documentation examples time`: compile every documentation snippet and record total time.
- Experiment 49 `benchmarks isolation`: ensure benchmark headers do not leak into product API.
- Experiment 50 `binary size baseline`: measure binary size for sequential example before and after features.
- Experiment 51 `link time baseline`: measure link cost with and without IPO/LTO.
- Experiment 52 `sanitizer build cost`: measure ASan/UBSan builds separately from normal development.
- Experiment 53 `warnings-as-errors cost`: track extra compiler work caused by aggressive warning options.
- Experiment 54 `generated code inspection`: inspect optimized assembly for simple chains to ensure no unnecessary abstraction remains.
- Experiment 55 `cold-start runtime`: measure first execution cost when graph metadata is emitted.
- Experiment 56 `large-chain runtime`: measure 10, 100, and 1000 stage linear chains for dispatch and value movement.
- Experiment 57 `large-chain compile time`: measure 10, 100, and 1000 stage linear chains for template cost.
- Experiment 58 `bad-chain compile time`: measure invalid edge at early, middle, and late positions.
- Experiment 59 `diagnostic truncation`: verify huge type names are abbreviated without hiding the failing edge.
- Experiment 60 `namespace hygiene`: measure whether public API names collide in user namespaces via tests.
- Experiment 61 `macro-free mode`: verify core can build without DSL helper macros.
- Experiment 62 `macro-assisted mode`: measure compile/debug benefit of macros that capture source location.
- Experiment 63 `ABI stability`: measure exported symbols for non-header-only runtime components.
- Experiment 64 `package integration`: test find_package usage in a clean external project.
- Experiment 65 `subproject integration`: test FetchContent/add_subdirectory usage.
- Experiment 66 `cross-compilation`: test an embedded-style toolchain if the product targets embedded users.
- Experiment 67 `CI artifact size`: track generated traces, exports, and benchmark logs.
- Experiment 68 `regression threshold tuning`: set thresholds loose enough for CI variance but strict enough to detect regressions.

# Part 25 - Source Index

- [S1] [p-ranav/pipeline](https://github.com/p-ranav/pipeline) - C++17 header-only template library for data-processing pipelines using `pipeline::fn` and `operator|`.
- [S2] [joboccara/pipes](https://github.com/joboccara/pipes) - C++14 header-only collection pipeline library.
- [S3] [range-v3](https://github.com/ericniebler/range-v3) - Range library for C++14/17/20 and basis for C++20 ranges.
- [S4] [RangeAdaptorClosureObject](https://en.cppreference.com/cpp/named_req/RangeAdaptorClosureObject) - cppreference page on pipeable range adaptor closure objects.
- [S5] [RxCpp](https://github.com/ReactiveX/RxCpp) - Reactive Extensions for C++.
- [S6] [Taskflow](https://github.com/taskflow/taskflow) - General-purpose task-parallel programming system.
- [S7] [Taskflow Processing Pipeline](https://taskflow.github.io/taskflow/TaskflowProcessingPipeline.html) - Taskflow example combining task-graph parallelism and pipeline parallelism.
- [S8] [oneTBB parallel_pipeline](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/v1.4-rev-1/elements/onetbb/source/algorithms/functions/parallel_pipeline_func) - oneAPI specification for oneTBB parallel pipeline filters.
- [S9] [NVIDIA stdexec](https://github.com/NVIDIA/stdexec) - Reference implementation for C++26 std::execution sender/receiver additions.
- [S10] [P2300R10 std::execution](https://wg21.link/P2300) - WG21 proposal for asynchronous execution based on schedulers, senders, and receivers.
- [S11] [Boost.MP11](https://www.boost.org/doc/libs/latest/libs/mp11/doc/html/mp11.html) - C++11 metaprogramming library based on template aliases and variadic templates.
- [S12] [Boost.Hana](https://www.boost.org/doc/libs/latest/libs/hana/doc/html/index.html) - Header-only library for metaprogramming on types and values.
- [S13] [Eric Niebler Meta](https://github.com/ericniebler/meta) - Tiny header-only C++11 metaprogramming library.
- [S14] [Boost.YAP](https://www.boost.org/libs/yap) - C++14-and-later expression-template library.
- [S15] [Boost.HOF](https://www.boost.org/libs/hof) - Header-only higher-order function utilities for C++11/C++14.
- [S16] [Apache Beam Create Your Pipeline](https://beam.apache.org/documentation/pipelines/create-your-pipeline/) - Pipeline object, PCollections, transforms, output, and run steps.
- [S17] [Apache Beam Programming Guide](https://beam.apache.org/documentation/programming-guide/) - Pipeline, PCollection, PTransform, IO, and graph execution model.
- [S18] [Nextflow docs](https://docs.seqera.io/nextflow/) - Workflow system using dataflow programming for parallel and distributed pipelines.
- [S19] [Nextflow GitHub](https://github.com/nextflow-io/nextflow) - DSL for data-driven computational pipelines.
- [S20] [Dagster GitHub](https://github.com/dagster-io/dagster) - Cloud-native data pipeline orchestrator with lineage, observability, declarative model, and testability.
- [S21] [requires expressions](https://en.cppreference.com/cpp/language/requires) - cppreference page for C++20 requires expressions.
- [S22] [std::expected](https://en.cppreference.com/cpp/utility/expected) - cppreference page for C++23 std::expected and monadic operations.
- [S23] [C++ modules](https://en.cppreference.com/cpp/language/modules) - cppreference page for C++20 modules.
- [S24] [MS Learn modules vs headers/PCH](https://learn.microsoft.com/en-us/cpp/build/compare-inclusion-methods?view=msvc-170) - Comparison of includes, PCH, header units, and named modules.
- [S25] [C++26 compiler support](https://en.cppreference.com/cpp/compiler_support/26) - cppreference C++26 compiler support table for language and library feature gates.
- [S26] [C++26 reflection library](https://cppreference.com/cpp/meta/reflection) - cppreference page for compile-time reflection.
- [S27] [P2996R13 Reflection for C++26](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r13.html) - current WG21 proposal text for static reflection and reflective metaprogramming.
- [S28] [C++26 contract assertions](https://en.cppreference.com/cpp/language/contracts) - cppreference page for C++26 contracts.
- [S29] [P4043R0 Contracts readiness discussion](https://isocpp.org/files/papers/P4043R0.html) - WG21 paper discussing C++26 contracts readiness concerns.
- [S39] [P2900R14 Contracts for C++](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2900r14.pdf) - WG21 contracts proposal with precondition, postcondition, assertion-statement, evaluation-semantic, and handler model.
- [S40] [P2741R3 user-generated static_assert messages](https://wg21.link/P2741R3) - WG21 proposal for richer compile-time diagnostic messages.
- [S41] [P0323R12 std::expected](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p0323r12.html) - WG21 design paper for `std::expected`.
- [S30] [P2672 Pipeline Operator](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2672r0.html) - WG21 paper exploring C++ pipeline operator design space.
- [S31] [CMake target_precompile_headers](https://cmake.org/cmake/help/latest/command/target_precompile_headers.html) - CMake command for precompiled headers.
- [S32] [CMake unity builds](https://cmake.org/cmake/help/latest/variable/CMAKE_UNITY_BUILD.html) - CMake variable for unity/jumbo builds.
- [S33] [Clang user manual - ftime-trace](https://clang.llvm.org/docs/UsersManual.html) - Clang option for JSON time profiler output.
- [S34] [Include What You Use](https://github.com/include-what-you-use/include-what-you-use) - Tool for analyzing #include usage in C and C++.
- [S35] [ccache](https://ccache.dev/) - Compiler cache for speeding recompilation.
- [S36] [sccache](https://github.com/mozilla/sccache) - Compiler caching tool with local and remote storage.
- [S37] [Ninja manual](https://ninja-build.org/manual.html) - Ninja build log and build behavior documentation.
- [S38] [CMake interprocedural optimization](https://cmake.org/cmake/help/latest/prop_tgt/INTERPROCEDURAL_OPTIMIZATION.html) - CMake target property for IPO/LTO.


# Part 26 - Live Research Verification Addendum

This addendum records the 2026-05-15 autoresearch pass over the master research document. It does not replace the original plan; it tightens the claims that materially affect implementation order.

## Verification Summary

- Link validation: all 38 pre-existing source-index URLs returned HTTP 200 or an official redirect before editing; after adding S39-S41, the final source index validated at 41/41 reachable URLs.
- Primary-source emphasis: decisions were cross-checked against official repositories/docs, WG21/open-std papers, CMake/LLVM/oneAPI/Boost docs, and compiler support tables.
- Main conclusion: keep the MVP small, C++20-first, diagnostic-first, and backend-isolated. The live evidence supports optional backend adapters and feature gates rather than early dependency on any single external runtime or C++26 feature.

## Research-Backed Decision Updates

| Area | Live finding | Implementation decision |
| --- | --- | --- |
| Function-chain libraries | p-ranav/pipeline and joboccara/pipes validate `operator|` ergonomics but stay function/collection-chain focused. | Use as syntax inspiration only; keep this project's type graph and diagnostics separate. |
| Ranges | range-v3/std::ranges validate lazy adapter composition and closure-object ergonomics. | Support range-view adapters as a user-code integration family, not as the core graph model. |
| Taskflow backend | Taskflow pipeline docs rely on `executor.corun` for nested taskflows to avoid blocking/deadlock risks. | Backend lowering must model worker participation and token storage, not simply call `run().wait()` inside stage code. |
| oneTBB backend | oneTBB `parallel_pipeline` has type-matched filters, serial/parallel modes, and explicit max live tokens. | Require explicit token/concurrency metadata when lowering to oneTBB. |
| stdexec backend | stdexec is the experimental reference implementation for C++26 `std::execution`, still subject to change and compiler-bound. | Keep sender/receiver support experimental and adapter-driven until CI proves completion signatures/error/cancellation behavior. |
| Meta core | Boost.MP11 confirms alias-template and variadic-template style as the low-cost metaprogramming baseline. | Implement a tiny internal MP11-like subset before adding richer type/value abstractions. |
| Diagnostics | Hana and P2741R3 both reinforce human-readable concept/static-assert diagnostics as a product feature. | Treat diagnostic wording and compile-fail tests as API stability surface. |
| Contracts | P2900R14 gives a concrete C++26 contract model, but compiler support remains gated. | Build `pb::contract_policy` locally first; map to standard contracts later. |
| Reflection | P2996R13 supersedes older reflection references, but support remains uneven. | Reflection-assisted adapters stay future/experimental; no MVP dependency. |
| Build acceleration | CMake warns against exporting PCH usage requirements; unity builds are developer controls and can conflict with compile command export; ccache/sccache have documented scope limits. | Keep PCH/unity/cache as measured developer/CI controls, never as hidden public requirements. |

## Source Confidence Ratings

- High confidence: CMake PCH/unity behavior, oneTBB filter semantics, Taskflow `corun` guidance, Boost.MP11 shape, ccache/sccache scope, and C++26 compiler-support gating because these come from official docs or primary repositories.
- Medium confidence: precise adoption timing for C++26 features across all compilers because support tables are moving targets and must be rechecked in CI.
- Intentional uncertainty: final product naming, package manager strategy, and exact backend priority after sequential/thread-pool because those depend on user/community demand rather than source facts.

## Implementation Implications for the Next Planning Pass

1. Split implementation planning into four lanes: core type model, runtime sequential executor, diagnostic/test harness, and build/tooling measurement.
2. Defer optional backends until core graph validation and failure diagnostics are stable.
3. Treat every optional backend as an adapter target with its own feature gate, tests, and compile-time/runtime benchmarks.
4. Produce a compiler feature matrix before using C++23/26 features in public headers.
5. Add research-refresh checkpoints before committing to reflection/contracts/stdexec APIs.


# Part 26.5 - Implementation Evidence Snapshot (2026-05-18)

This snapshot records implementation and validation evidence that landed after the original research-plan recommendation. It does not rewrite the long-horizon plan; it marks what the current repository can claim and what must stay roadmap-only.

## Current supported implementation slice

The repository has progressed beyond the original linear-only first-slice recommendation in a narrow, tested way:

- Linear typed pipelines, stage metadata, validation, adapters, sequential runtime execution, observer callbacks, diagnostics scaffolding, package smoke, and benchmark smoke scaffolding are implemented for the current MVP surface.
- Sequential branch/join execution is supported for the current standard-library sequential runtime: public `::branch<...>` and `::join<JoinStage>` DSL, first-match-wins routing, selected branch-stage execution, result/error propagation, observer selected/skipped events, stateful branch predicate/stage storage, and branch examples/tests.
- Homogeneous branch outputs are supported. Heterogeneous branch outputs are represented as `std::variant<Ts...>`, including duplicate output alternatives routed by variant index. Raw case output metadata remains available separately from the unified execution output.
- Move-only selected-branch input consumption is supported when predicates inspect by `const input_type&`; predicates that consume a move-only input before routing remain unsupported.
- DOT/JSON graph export exists as a helper surface for linear and supported branch pipelines, including JSON branch topology detection, DOT label escaping, and helper-output golden regression checks.
- `pb::runtime::thread_pool` exists as a standalone standard-library utility; it is not a pipeline executor backend.

## Latest compiler/release validation evidence

Validation-only GitHub Actions evidence was collected for code SHA `6805543ede6946aa283be7f24fb3736c762f47b2`:

```text
GCC C++20 configure/build/CTest:        PASS, 150/150, g++ 13.3.0
GCC C++23 configure/build/CTest:        PASS, 150/150, g++ 13.3.0
Clang C++20 configure/build/CTest:      PASS, 150/150, Ubuntu clang 18.1.3
Clang C++23 configure/build/CTest:      PASS, 150/150, Ubuntu clang 18.1.3
MSVC C++20 configure/build/CTest:       PASS, 149/149, MSVC 19.44.35226
Clean Ubuntu package-release preset:    PASS, 150/150, TGZ package generated
Workflow run: https://github.com/tonytranrp/Pipeline-c-/actions/runs/26058848575
Normal CI run: https://github.com/tonytranrp/Pipeline-c-/actions/runs/26058841298
```

This evidence supports the exercised compiler/package matrix only. If non-doc code changes land after this SHA, rerun the matrix on the final candidate before using it as release evidence.

## Still roadmap-only after the snapshot

The following research-plan items remain missing or partial and must not be presented as shipped support:

- Parallel all-branches fan-in / true backend multi-input join execution beyond selected-output type-list joins.
- Stable descriptor/export schemas and release-grade compatibility fixtures beyond descriptor-record-backed helper DOT/JSON output.
- CLI/file export for user pipeline definitions.
- Thread-pool, oneTBB, Taskflow, or stdexec pipeline executor backends.
- Full policy DSL for error, exception, lifetime, contract, diagnostics, and executor capabilities.
- Broader stateful storage/lifetime policies: borrowed/reference/shared/unique ownership, reset, move-in stage instances, and thread-local future-backend storage.
- C++ modules.
- C++26 reflection/contracts integrations beyond feature gates.
- Measured compile-time baselines, thresholds, dashboards, and CI-enforced performance budgets.
- Fully stable/frozen diagnostic wording and machine-readable diagnostic schemas across all future roadmap slices.

## Updated planning implication

The original recommendation to ship a small first version remains valid in spirit, but the current small surface is no longer linear-only. Release-facing docs may describe the supported sequential branch/join and helper export slices only when paired with the exact validation evidence above. The next implementation milestones should stabilize descriptor/export compatibility, parallel fan-in/backend multi-input joins, and backend execution as separate phases rather than broadening claims inside routine hardening work.

# Part 27 - Final Recommendation

- Build the product as a compile-time pipeline compiler with a small meta core and a separate runtime engine.
- Keep the first usable version small and evidence-bound; the current validated surface includes linear chains plus a narrow sequential branch/join slice, while broader graph/backends remain roadmap-only.
- Make named stage types the default API and lambdas an adapter feature.
- Use concepts and targeted static assertions as the diagnostic foundation.
- Use `std::expected` as the default runtime error path where C++23 is available, with a C++20 fallback.
- Keep optional backends out of the core.
- Measure compile time from the first prototype.
- Treat documentation and failure examples as core deliverables.
- Do not promise impossible universal support for arbitrary invalid C++; promise broad adapters and actionable failures for valid user code.
- Use C++26 reflection, contracts, and pack-indexing only behind feature gates until compiler support is proven in the project matrix.

Document generated line count: 2288
