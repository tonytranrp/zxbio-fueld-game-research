# Examples

The examples are intentionally small and mirror the current supported API. Use them as copyable starting points for a linear pipeline with explicit stage types and compile-time validation.

## Successful order pipeline

`examples/basic_order_pipeline.cpp` demonstrates the happy path:

1. Define domain input/output types (`RawText`, `OrderDraft`, `ValidatedOrder`, `Receipt`).
2. Adapt legacy callables with the explicit `pb::adapt<pb::name<...>, pb::fn<...>, pb::in<...>, pb::out<...>, pb::err<...>>` shape (or `pb::functor<...>` for function objects) when the callable already exists outside the pipeline library.
3. Define any new stage as a type with `input_type`, `output_type`, `error_type`, `stage_name()`, and `operator()`.
4. Compose the chain with `pb::from<T>::then<S>::to<U>`.
5. Guard the chain with `static_assert(pb::valid<Pipeline>)`.
6. Run the validated chain with `pb::compile<Pipeline>(pb::runtime::sequential{})`.

Build and run only the example target during tutorial work:

```bash
cmake --build --preset clang-dev-ninja --target pb_basic_order_pipeline
./build/clang-dev-ninja/examples/pb_basic_order_pipeline
```

A zero exit code means the sequential executor preserved the order id through the adapted parse stage, adapted validate stage, and explicit persist stage.

## Stateful stage storage

`pb::runtime::sequential{}` keeps the default policy explicit: stage objects are constructed for each run, so mutable stage members are not preserved between calls. When a pipeline intentionally needs stage-local state to live with the compiled engine, compile it with the stateful storage policy:

```cpp
auto engine = pb::compile<MyPipeline>(pb::runtime::stateful_sequential{});
```

`pb::runtime::stateful_sequential` stores the pipeline stage objects inside the engine and reuses them for `run()` and `try_run()`. Use it only for stages whose mutable state is part of the intended runtime contract; keep the default `pb::runtime::sequential{}` for stateless or per-run-isolated stages.

The same storage choice is available through the narrow policy namespace when code wants policy names rather than runtime aliases:

```cpp
auto per_run = pb::compile<MyPipeline>(
    pb::policy::sequential<pb::policy::storage::construct_per_run>{});
auto stateful = pb::compile<MyPipeline>(
    pb::policy::sequential<pb::policy::storage::store_in_engine>{});
```

This policy seam currently covers sequential stage storage only; broader error, exception, reference-lifetime, and executor-capability policies remain roadmap work.

The supported stateful storage guarantee is intentionally narrow: stage objects must still be default-initializable, but they may own non-copyable state such as `std::unique_ptr` members. With `pb::runtime::sequential{}` those stage objects are reconstructed for each call, while `pb::runtime::stateful_sequential{}` / `pb::policy::storage::store_in_engine` keeps the same stage instances in the compiled engine for both `run()` and `try_run()`.

## Runtime result and diagnostics quick check

For a stage that can fail and return `pb::runtime::result<T>`:

1. Use `try_run()` when you want a typed error-path return.
2. Keep stage-local failures as `pb::runtime::error` when you need explicit category and stage metadata.
3. Keep other failures as expected-like types (for example `std::string`) and let the runtime normalize them to `expected_error` with diagnostics.

Use `error` fields to drive logs and retry policies:

- `category`: `stage_failure`, `expected_error`, `exception`, or `contract_violation`.
- `stage.key` / `stage.name`: where the failure originated.
- `message`: the stage-level reason.

A zero-stage pipeline (`pb::from<T>::to<T>`) is an identity path in the sequential runtime. Because no stage executes, observer start/success/failure/exception callbacks are not emitted for that path.

## Inspecting pipeline metadata

The public API now exposes lightweight compile-time introspection helpers for validated pipeline aliases:

```cpp
static_assert(pb::pipeline_size_v<OrderPipeline> == 3);
static_assert(std::same_as<pb::pipeline_input_t<OrderPipeline>, domain::RawText>);
static_assert(std::same_as<pb::pipeline_output_t<OrderPipeline>, domain::Receipt>);
static_assert(std::same_as<pb::pipeline_stage_t<OrderPipeline, 0>, ParseOrder>);
static_assert(pb::describe<OrderPipeline>().stage_name<0>() == "parse_order");
static_assert(pb::describe<OrderPipeline>().edge_count == 2);
```

Use `pb::describe<Pipeline>().stage_names()`, `stage_records()`, or `edge_records()` when tooling needs stable linear stage/edge order without depending on template internals. `stage_record` carries the stage index, key, and name; `edge_record` carries adjacent stage indexes plus the source/target stage keys and names for each linear stage-to-stage edge.

## Diagnostic failure example

`examples/error_diagnostic.cpp` keeps the invalid pipeline behind `PB_EXAMPLE_ENABLE_COMPILE_FAILURE` so normal example builds stay green. Enable that definition only when you want to inspect the compile-time edge-mismatch diagnostic.

The failing path intentionally connects `ParseOrder`, which outputs `OrderDraft`, to `PersistOrder`, which declares `Receipt` as its input. The expected diagnostic includes `Pipeline edge mismatch`.

Use CTest for the stable failure check instead of hand-running a compiler command:

```bash
ctest --preset clang-dev-ninja --output-on-failure -R pb_example_error_diagnostic_compile_fail
```

You can also inspect the compile command path by building only that example target:

```bash
cmake --build --preset clang-dev-ninja --target pb_error_diagnostic_example
```

## Copying an example into user code

When adapting these examples for an application:

- Keep each stage's declared `input_type` and `output_type` aligned with the neighboring stage.
- Prefer adapters for existing free functions and function objects; write explicit stage types when the stage owns pipeline-specific metadata.
- Keep `static_assert(pb::valid<YourPipeline>)` next to the pipeline alias so type mismatches fail before runtime code is involved.
- Start with `pb::runtime::sequential` before introducing future executor or backend assumptions.
- Treat only the branch/join and export slices listed in `docs/production-readiness.md` as supported; preemptive cancellation, dependency backend fan-in, stable descriptor/export compatibility, CLI/file export, and optional dependency backends remain roadmap items.
