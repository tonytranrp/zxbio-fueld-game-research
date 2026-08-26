# Stateful Storage DSL (`pb.state.v1`)

This document formalises the stateful storage DSL — the orthogonal lifetime
layer that lets pipeline stages share typed state across stages and across
runs without breaking the value-type stage model.

The externally-observable contract is locked under the schema constant
`pb.state.v1`.  The internal implementation may evolve freely within this
schema; the contract is the *surface*, not the implementation.

## Motivation

Pipelines built with `pb::compile<...>(pb::sequential{})` are pure function
compositions: every stage receives an `Input` and returns an `Output`, with
no read/write access to shared state.  Most real pipelines, however, need
*some* shared state across stages: a cache, an accumulator, a resource pool,
a request-scoped log buffer, a per-call scratch arena.

Writing those as captured references on every stage type is invasive: stages
become non-default-constructible, stop satisfying the per-run construction
storage policy, and lose their ability to be `static_assert`-validated.  The
stateful DSL gives users a *third* way that keeps the stage model clean and
the lifetime contract explicit.

## Surface

### Defining a state type

A state type is any user-defined struct.  It has no required base class, no
required members, and no required default constructor (though
`reset_per_run` requires default-constructibility).

```cpp
struct cache_state {
  std::unordered_map<int, std::string> cache;
};
```

### Accessing state inside a stage

Stages access the active state via `pb::current_state<State>()`:

```cpp
struct lookup_or_fetch {
  using input_type  = int;
  using output_type = std::string;
  static constexpr auto stage_key()  noexcept { return "lookup_or_fetch"; }
  static constexpr auto stage_name() noexcept { return "lookup_or_fetch"; }

  auto operator()(int key) const -> std::string {
    auto& s = pb::current_state<cache_state>();
    if (auto it = s.cache.find(key); it != s.cache.end()) return it->second;
    auto fetched = fetch(key);
    s.cache.emplace(key, fetched);
    return fetched;
  }
};
```

`current_state<State>()` throws `pb::pipeline_exception` (category
`contract_violation`) if no `with_state<State>(...)` scope is active on the
current thread.  Use `pb::try_current_state<State>()` instead when state is
optional — it returns `State*` (`nullptr` if no scope is active).

### Wrapping an engine with state

The `pb::with_state<S>(engine)` factory wraps any engine that satisfies the
standard engine surface:

```cpp
auto base     = pb::compile<MyPipeline>(pb::sequential{});
auto stateful = pb::with_state<cache_state>(std::move(base));

stateful.run(42);   // miss — cache.size() == 1
stateful.run(42);   // hit  — cache.size() == 1 (same value returned)
```

Four storage policies cover the lifetime patterns:

| Factory                              | Storage policy                          | When to use                                                                 |
|--------------------------------------|-----------------------------------------|-----------------------------------------------------------------------------|
| `pb::with_state<S>(engine)`          | `pb::policy::state::owned` (default)    | Long-lived state owned by the engine; persists across every `run()`.        |
| `pb::with_borrowed_state<S>(engine)` | `pb::policy::state::borrowed`           | Caller supplies state per call (e.g. request-scoped).                       |
| `pb::with_shared_state<S>(engine)`   | `pb::policy::state::shared`             | Multiple engines / threads sharing the same `std::shared_ptr<S>`.           |
| `pb::with_reset_per_run_state<S>(engine)` | `pb::policy::state::reset_per_run` | Owned scratch state that must not leak between calls.                        |

### Per-call API by policy

| Policy           | `run(in)`                          | `run_with_state(in, s)`            | `state()` / `state_mut()` / `reset_state()` | `shared_state()` |
|------------------|------------------------------------|------------------------------------|---------------------------------------------|------------------|
| `owned`          | yes — uses owned state             | yes — shadows owned state          | yes                                         | no               |
| `reset_per_run`  | yes — resets, then uses owned      | yes — shadows owned state          | yes                                         | no               |
| `shared`         | yes — uses shared state            | yes — shadows shared state         | no                                          | yes              |
| `borrowed`       | *deleted*                          | yes — caller supplies state        | no                                          | no               |

`run_with_state(input, state)` is available on every policy: even owned and
shared engines can shadow their persisted state with a caller-supplied one
on a per-call basis.  The shadow lasts exactly the duration of that call.

### Multiple states

Multiple states stack cleanly via nested `with_state` wrappers, because each
state type has an independent thread-local stack:

```cpp
auto stateful = pb::with_state<counter_state>(
    pb::with_state<trace_state>(
        pb::compile<MyPipeline>(pb::sequential{})));

// inside a stage, both states are reachable simultaneously:
auto& c = pb::current_state<counter_state>();
auto& t = pb::current_state<trace_state>();
```

## Thread safety

The active-state stack is **thread-local**.  Concurrent calls on different
threads do not interfere — each thread maintains its own stack per state
type.  Reentrant calls from inside a stage are also safe; they push a new
frame on top.

For the `shared` policy, the engine holds a `std::shared_ptr<State>` so
multiple engine instances (or threads) can share the same state.  Mutual
exclusion *inside* the state type is the caller's responsibility — wrap
critical sections in the state's own mutex, or use lock-free data
structures.

## Exception safety

The `state_context<State>` RAII frame uses a destructor to pop the active
state pointer.  Even if a stage throws, the frame is unwound and the stack
is restored to its prior state.  The stateful DSL smoke test pins this
behavior — see `tests/runtime/state_dsl_smoke.cpp::test_state_context_balanced_on_exception()`.

## Composability with the error-policy wrappers

`stateful_engine<...>` mirrors the error-policy wrappers' forwarding
surface (`set_observer`, `get_observer`, `describe`, `descriptor`,
`underlying()`), so it composes in any order with `pb::with_throw_on_error`,
`pb::with_terminate_on_error`, `pb::with_ignore_errors`,
`pb::with_propagate_exceptions`, and `pb::with_verbose_diagnostics`:

```cpp
// state inside throwing wrapper
auto a = pb::with_state<S>(pb::with_throw_on_error(engine));

// throwing wrapper outside state
auto b = pb::with_throw_on_error(pb::with_state<S>(engine));

// full stack
auto c = pb::with_verbose_diagnostics(
    pb::with_throw_on_error(
        pb::with_state<S>(engine)),
    &std::clog);
```

Both nesting orders are valid; the wrapper closer to the engine intercepts
first.

## Stability promise (`pb.state.v1`)

Within the `pb.state.v1` schema:

### Locked surface

These names, signatures, and behaviors **must not change** without a
schema bump:

- `pb::current_state<State>()` returns `State&`, throws `pipeline_exception`
  (category `contract_violation`) when no scope is active.
- `pb::try_current_state<State>()` returns `State*` (`nullptr` when no
  scope is active), `noexcept`.
- `pb::state_context<State>` is a non-movable, non-copyable RAII frame that
  pushes on construction and pops on destruction.
- Factory functions:
  - `pb::with_state<S>(Engine)` / `pb::with_state<S>(Engine, State)`
  - `pb::with_borrowed_state<S>(Engine)`
  - `pb::with_shared_state<S>(Engine, std::shared_ptr<S>)`
  - `pb::with_reset_per_run_state<S>(Engine)` /
    `pb::with_reset_per_run_state<S>(Engine, State)`
- `pb::stateful_engine<Engine, State, StoragePolicy>` exposes:
  - `using input_type`, `output_type`, `try_result_type`
  - `run`, `try_run` (non-borrowed policies only)
  - `run_with_state`, `try_run_with_state` (all policies)
  - `state` / `state_mut` / `set_state` / `reset_state`
    (owned, reset_per_run only)
  - `shared_state` / `set_shared_state` (shared only)
  - `set_observer`, `get_observer`, `describe`, `descriptor`,
    `underlying()`
- `pb::state_dsl_schema_version == "pb.state.v1"`.

### Permitted v1.x changes

- Adding new storage policies under `pb::policy::state::*` (must not break
  existing factory functions).
- Adding new factory functions (must not rename or remove existing ones).
- Adding new members to `stateful_engine` (must not break existing call
  sites).
- Improving diagnostic messages (the surface contract is the *category* and
  the function-name reference in the message, not the exact phrasing).

### Forbidden v1.x changes

- Removing or renaming any locked symbol above.
- Changing the call signatures (input / output type, exception spec) of
  locked methods.
- Changing the behavior of `current_state<S>()` when no scope is active
  (must still throw `pipeline_exception` with category `contract_violation`).
- Changing the thread-locality of the state stack.
- Bumping `pb::state_dsl_schema_version` to anything other than
  `"pb.state.v1"`.

### Migration to `pb.state.v2`

When a breaking change is unavoidable, the migration procedure mirrors the
other v1 schemas:

1. Add a new `state_dsl_schema_version_v2` constant alongside the existing
   `pb.state.v1` constant.
2. Ship the v2 surface in a sibling namespace (e.g. `pb::v2::with_state`)
   so v1 callers continue to compile.
3. Mark the v1 factories with `[[deprecated]]` *only* once the v2 surface
   is widely available; do not deprecate before then.
4. Keep the v1 cross-version regression test
   (`tests/compile_pass/schema_v1_cross_version_regression.cpp`) green
   throughout the migration window.
5. Remove the v1 surface only in a separate major release with an explicit
   migration deadline communicated in the release notes.

## Regression coverage

- `tests/runtime/state_dsl_smoke.cpp` — exercises every storage policy,
  forwarding surface, multi-state nesting, exception-safety of the
  state-context frame, composability with `with_throw_on_error`, and
  the `current_state<T>()` outside-scope diagnostic.
- `tests/compile_pass/schema_v1_cross_version_regression.cpp` pins the
  `pb::state_dsl_schema_version` literal alongside every other v1 schema
  constant.
