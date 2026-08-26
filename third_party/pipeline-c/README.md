# Pipeline-c++

Compile-time validated C++20 pipeline builder foundation.

This first base follows `research/pipeline_builder_cpp_research_plan.md`:

- target-based CMake project (`pb::core`, `pb::runtime`, plus `pb::pipeline` compatibility target)
- LLVM/Clang-friendly presets and `compile_commands.json`
- public `.hpp` headers under `include/pb/`
- implementation files under `src/`
- examples, tests, docs, and benchmark smoke folders
- MVP `pb::from<T>::then<S>::to<U>` typed linear pipeline validation
- sequential runtime executor and free/function-object adapters

## Docs and release readiness

- [Build and Verification](docs/build.md) lists the supported configure/build/test presets, benchmark smoke targets, and benchmark result recording template.
- [Examples](docs/examples.md) walks through the successful order pipeline, the intentional diagnostic example, and the public introspection helpers.
- [Pipeline-c++ docs hub](docs/README.md) links all guides used during the review lane.
- [Production Readiness Status](docs/production-readiness.md) tracks supported capabilities, known release gaps, and the package-consumer release gate.
- [Research Verification Matrix](docs/research-verification-matrix.md) maps research-plan gaps to shipped evidence, tests, release status, and next slices.
- [Current implementation / not-done map](research/not%20done.md) summarizes what is done, partial, missing, and required for production-grade completion against the original research plan.
- [Graph Export Roadmap / Status](docs/graph-export-roadmap.md) explains the DOT/JSON helper boundary and what must land before docs can present stable graph export as supported.
- [Cross-Compiler Validation Status](docs/cross-compiler-validation.md) records the latest GCC/Clang/MSVC/package validation evidence and remaining release boundaries.

For package consumers, the release-readiness path is the `package-release-clang-ninja` configure, build, CTest, and package target sequence below. That CTest run includes `pb_package_config_smoke`, which installs the package into a temporary prefix, verifies that `find_package(pipebuilder CONFIG REQUIRED)` exposes `pb::core`, `pb::runtime`, and the `pb::pipeline` compatibility target, builds separate downstream consumers against each target, runs those consumers, and checks the generated TGZ for key headers, CMake config files, and the runtime library. Treat the package lane as release evidence only when this preset and package target pass freshly on the candidate SHA.

## Configure, build, test

```sh
cmake --preset dev-ninja
cmake --build --preset dev-ninja
ctest --preset dev-ninja

cmake --preset clang-dev-ninja
cmake --build --preset clang-dev-ninja
ctest --preset clang-dev-ninja

cmake --preset clang-time-trace
cmake --build --preset clang-time-trace
ctest --preset clang-time-trace

cmake --preset clang-tidy-ninja
cmake --build --preset clang-tidy-ninja
ctest --preset clang-tidy-ninja

cmake --preset release-clang-ninja
cmake --build --preset release-clang-ninja
ctest --preset release-clang-ninja

cmake --preset release-ninja
cmake --build --preset release-ninja
ctest --preset release-ninja

cmake --preset package-dev-ninja
cmake --build --preset package-dev-ninja
ctest --preset package-dev-ninja

cmake --preset package-release-clang-ninja
cmake --build --preset package-release-clang-ninja
ctest --preset package-release-clang-ninja
cmake --build --preset package-release-clang-ninja --target package
```

C++20 named module build (clang 22 + CMake 4.3 + Ninja; verified on llvm-mingw):

```sh
cmake --preset modules-ninja
cmake --build --preset modules-ninja
ctest --preset modules-ninja --output-on-failure -R pb_use_module
```

The `modules-ninja` preset sets `PB_BUILD_MODULE=ON` and compiles
`include/pb/pipeline.mpp` as a `FILE_SET CXX_MODULES` target.
`tests/module/use_module.cpp` imports the module with `import pb.pipeline;` and
exercises compile + run. The default header-only build is unaffected.

Benchmarks and package smoke scaffolding:

```sh
cmake --preset bench-dev-ninja
cmake --build --preset bench-dev-ninja
ctest --preset bench-dev-ninja -R '^pb_bench_' --output-on-failure

cmake --build --preset package-dev-ninja --target package
```

## CLI: `pb_cli`

The `pb_cli` tool is built into every preset and exposes graph export and
feature-gate introspection for the built-in example pipelines via
`pb::tooling::pipeline_registry` (header: `include/pb/tooling/cli_registry.hpp`):

```sh
pb_cli list                                       # list registered pipelines
pb_cli describe order-linear --format=dot          # render DOT to stdout
pb_cli describe order-branch --format=json         # render JSON to stdout
pb_cli describe order-fan-in --format=json \
       --out=order_fan_in.json                     # write to file
pb_cli describe order-branch --format=text         # compact text (compiler-error friendly)
pb_cli features                                    # print C++ feature matrix
```

Available built-in pipelines: `order-linear`, `order-branch`, `order-fan-in`,
`order-enrich`, `order-variant`.
The DOT/JSON/text output now follows the **stable `pb.core.graph.v1`
contract** — see [docs/export-schema-v1.md](docs/export-schema-v1.md) for
the formal spec, the migration policy, and the byte-equal regression tests
that lock the schema in place.

`pb::tooling::pipeline_registry` is the documented extension point for user
forks: register any pipeline in one line and it becomes available to
`pb_cli list` / `pb_cli describe`. A broader stable CLI contract for
arbitrary user pipelines beyond the built-in examples remains future work.

## Diagnostics contract

The public diagnostics surface includes a narrow machine-readable contract in
`include/pb/core/diagnostics.hpp`: `pb::diagnostics::diagnostics_schema_version`
is `"pb.diagnostics.v1"`, `diagnostic_record` carries severity/code/message/source
location/stage/suggestion fields, and `diagnostic_collector::add(...)` captures
`std::source_location` by default. This is a record/collector contract for tools;
full graph-aware diagnostic emission, exported diagnostic artifacts, and frozen
cross-compiler wording guarantees remain roadmap work.

## Production-grade extras

### Policy DSL (`::with<>`) — three independent axes

The `::with<Marker>` DSL carries policy markers into the finalized pipeline
type (`pipeline<Stages, Branches, Policies>`) and makes `pb::compile<P>(...)`
select the matching engine wrapper automatically. All three axes are
independent and order-insensitive — `::with<throwing>::with<verbose>::with<move_only>`
compiles and each `has_*_policy_v` reports only its own axis.

**Errors axis** — select the engine error wrapper:

```cpp
using P = pb::from<In>::then<S>::to<Out>
         ::with<pb::policy::errors::throwing>;
auto engine = pb::compile<P>(pb::runtime::sequential{});
// engine is automatically pb::with_throw_on_error(...)
static_assert(pb::has_error_policy_v<P>);
```

Available error markers: `throwing`, `terminating`, `ignoring`, `propagating`,
`result` (baseline, no wrapper). See `include/pb/core/policy.hpp`.

**Diagnostics axis** — enable verbose stage-transition logging:

```cpp
using P = MyPipeline::with<pb::policy::diagnostics::verbose>;
auto engine = pb::compile<P>(pb::runtime::sequential{});
// engine is pb::with_verbose_diagnostics(inner_engine, stream) wrapping
// whatever the errors axis selected.
static_assert(pb::has_diagnostics_policy_v<P>);
```

Available: `verbose` (wraps engine in `pb::with_verbose_diagnostics`),
`quiet` (no wrapper — records intent only).

**Copying axis** — document ownership intent (carried + queryable; runtime
enforcement beyond `pb::shared_view` / `pb::unique_clone` is forward-looking):

```cpp
using P = MyPipeline::with<pb::policy::copying::move_only>;
static_assert(pb::has_copying_policy_v<P>);
```

Available: `value`, `move_only`, `shared`, `clone`.

### Engine error-policy wrappers

Stack any of these around a `pb::compile<P>(...)` result to swap the
error model without changing the underlying pipeline definition. Each
wrapper preserves `try_run()` for callers that still want `result<>`,
and exposes `underlying()` for observer/descriptor access.

- `pb::with_throw_on_error(engine)` — converts any failure into a thrown
  `pb::pipeline_exception` on `run()`. The exception carries the full
  `pb::runtime::error` diagnostic via `ex.diagnostic()`.
- `pb::with_terminate_on_error(engine)` — calls `std::terminate()` on
  failure. Suitable for embedded / hard-real-time pipelines where any
  error is unrecoverable.
- `pb::with_ignore_errors(engine, fallback)` — substitutes a
  user-supplied fallback value on failure. The fallback is mutable via
  `set_fallback`.
- `pb::with_propagate_exceptions(engine)` — rethrows stage *exceptions*
  as `pb::pipeline_exception` while leaving declared `result<>` failures
  on the value path.
- `pb::with_verbose_diagnostics(engine, &stream)` — auto-attaches an
  observer that logs every stage transition to a `std::ostream` using
  the stable `[pb.verbose] <event> stage=<key>` line schema.

All five wrappers expose `set_observer` / `get_observer` / `describe` /
`descriptor` directly on the wrapper surface, so they compose — most
useful pattern: `pb::with_verbose_diagnostics(pb::with_throw_on_error(
pb::compile<P>(pb::runtime::sequential{})), &std::clog)` to log every
transition *and* throw on failure. Cross-wrapper `run()` / `try_run()`
parity is regression-tested across linear, selected-output branch, and
explicit fan-in topologies for success / declared `result<>` failure /
thrown exception, plus `std::expected`-shape stages where
`PB_HAS_STD_EXPECTED == 1`. See
[include/pb/runtime/error_policy.hpp](include/pb/runtime/error_policy.hpp).

### Fan-in clone / projection

- `pb::shared_view<T>` — copyable view that lets owned non-copyable
  values flow through fan-in branches via shared_ptr semantics.
- `pb::unique_clone<T, Clone>` — owned per-case deep-copy fan-in policy;
  each passing case receives an independently-owned clone. Closes the
  "owned per-case unique-clone" research-plan gap.
- `pb::projected<From, Projection, Stage>` — wrap any stage so it
  accepts an upstream `From` type and applies `Projection{}(source)`
  before forwarding to `Stage`. Lets one stage participate in pipelines
  over different surrounding payload types without duplication.
- `pb::fan_in_uses_copy_v<Node>` / `pb::fan_in_uses_borrow_v<Node>` —
  compile-time introspection of the fan-in clone strategy.

See [include/pb/runtime/clone.hpp](include/pb/runtime/clone.hpp).

### Fan-in multi-error envelope

`pb::fan_in_error_envelope` + `pb::collect_fan_in_errors` aggregate every
failed fan-in case into one structured object with a stable text schema
(schema version `"pb.fan_in.errors.v1"`). Call `collect_fan_in_errors(results)`
to build the envelope from a `pb::fan_in_results<...>` and inspect
`.errors()` or render with `.to_string()` / `.format()`.
Header: [include/pb/runtime/fan_in_error.hpp](include/pb/runtime/fan_in_error.hpp).

### Fan-in observer lifecycle events

Four additive no-op virtual methods on `pb::runtime::observer` fire around
each fan-in execution (v1-ABI-safe defaults; emitted on sequential and
thread-pool fan-in):

- `on_fan_in_started(branch_id, case_count)`
- `on_fan_in_case_scheduled(branch_id, case_index, case_stage_id)`
- `on_fan_in_case_completed(branch_id, case_index, case_stage_id, success)`
- `on_fan_in_completed(branch_id, selected_count, completed_count, failed_count)`

Header: [include/pb/runtime/observer.hpp](include/pb/runtime/observer.hpp).

### Cooperative cancellation

```cpp
pb::cancellation_source src;
pb::cancellation_token  tok = src.token();
// pass tok to thread_pool_backend::cancel
src.request_stop();   // not-yet-started fan-in cases are skipped
```

Schema version `"pb.cancel.v1"`. Preemptive interruption of a stage
already running is out of scope — only not-yet-started cases are affected.
Header: [include/pb/runtime/cancellation.hpp](include/pb/runtime/cancellation.hpp).

### Runtime-bound callable adapters

For callables whose state is determined at runtime (stateful lambdas,
`std::function` from configuration, bound member functions):

- `pb::runtime_callable<In, Out>` — type-erased stage that owns an
  arbitrary callable at construction; requires the stateful storage policy.
- `pb::bind_callable(callable)` — factory returning a `runtime_callable`.
- `pb::c_function_stage<In, Out, Fn>` — C-style free-function-pointer
  adapter (non-type template parameter); no runtime state, works with the
  default `sequential{}` policy.

Header: [include/pb/adapt/runtime_callable.hpp](include/pb/adapt/runtime_callable.hpp).

### Synchronous coroutine stage adapter

Write stages as C++20 coroutines that `co_return` their result:

```cpp
pb::coro::sync_task<std::string> enrich(Order o) {
    co_return o.id + "-enriched";
}
using P = pb::from<Order>::then<pb::coroutine_stage<decltype(&enrich)>>::to<std::string>;
```

`pb::coro::sync_task<T>` runs eagerly to completion synchronously so the
sequential engine sees a plain value-returning function. True
async/sender backends remain future work.
Header: [include/pb/adapt/coroutine.hpp](include/pb/adapt/coroutine.hpp).

### Optional backend integration seams (dormant scaffolds)

Compile-guarded seams for oneTBB, Taskflow, and stdexec reserve the
integration point without adding any dependencies to the default build:

```cpp
// Always false unless the build was configured with PB_ENABLE_TBB=ON
// and find_package(TBB) succeeded:
static_assert(!pb::runtime::tbb_backend_available_v);
static_assert(!pb::runtime::taskflow_backend_available_v);
static_assert(!pb::runtime::stdexec_backend_available_v);
```

Headers: `include/pb/backends/{tbb,taskflow,stdexec}.hpp`.
Working external backends are NOT implemented — these are forward-compat
seams only. See [docs/optional-backends-roadmap.md](docs/optional-backends-roadmap.md).

### Typed C++26 feature gates

- `pb::features::has_cpp26 / has_reflection / has_contracts /
  has_pack_indexing / has_std_expected / has_deducing_this` for
  `if constexpr` and concept gating in user code.
- Every `PB_HAS_*` macro respects user pre-defines so downstream builds
  can force-disable a feature to test their fallback path.
- `pb::reflect_stage<T>` is declared on every compiler but only emits a
  working reflection-driven adapter when `PB_HAS_REFLECTION == 1`. On
  C++20 baselines, instantiating it produces a clear `static_assert`
  pointing at the gate.

See [docs/cpp26-feature-gates.md](docs/cpp26-feature-gates.md) and
[include/pb/adapt/reflect.hpp](include/pb/adapt/reflect.hpp).
