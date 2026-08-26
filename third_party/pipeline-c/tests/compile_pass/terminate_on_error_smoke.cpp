// pb::with_terminate_on_error and pb::with_ignore_errors compile-pass smoke.
//
// Validates that wrapping an engine in pb::with_terminate_on_error:
//   * exposes the correct input_type / output_type / underlying_engine typedefs,
//   * try_run() is forwarded and returns a result<> that is engaged on success,
//   * underlying() lvalue / const-lvalue / rvalue overloads are reachable,
//   * run() is NOT invoked on a failing pipeline (std::terminate must never fire).
//
// Also validates pb::with_ignore_errors symmetrically:
//   * typedef surface matches,
//   * try_run() returns engaged result<> on success,
//   * set_fallback() / get_fallback() round-trip,
//   * run() on the success path returns the value (no fallback needed).

#include <pb/pipeline.hpp>

#include <cassert>
#include <type_traits>

namespace terminate_on_error_smoke {

struct Input  { int value{}; };
struct Output { int value{}; };

struct Identity {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "identity"; }
  static constexpr auto stage_name() noexcept { return "identity"; }
  Output operator()(Input in) const { return {in.value}; }
};

using SimplePipeline = pb::from<Input>::then<Identity>::to<Output>;

// ---------------------------------------------------------------------------
// terminating_engine typedef surface
// ---------------------------------------------------------------------------

using BaseEngine = decltype(pb::compile<SimplePipeline>(pb::runtime::sequential{}));
using TermEngine = pb::terminating_engine<BaseEngine>;

static_assert(std::is_same_v<TermEngine::underlying_engine, BaseEngine>);
static_assert(std::is_same_v<TermEngine::input_type,  Input>);
static_assert(std::is_same_v<TermEngine::output_type, Output>);

// try_result_type must be an expected-like type with a value() member.
static_assert(!std::is_void_v<TermEngine::try_result_type>);

// ---------------------------------------------------------------------------
// ignoring_engine typedef surface
// ---------------------------------------------------------------------------

using IgnEngine = pb::ignoring_engine<BaseEngine>;

static_assert(std::is_same_v<IgnEngine::underlying_engine, BaseEngine>);
static_assert(std::is_same_v<IgnEngine::input_type,  Input>);
static_assert(std::is_same_v<IgnEngine::output_type, Output>);
static_assert(!std::is_void_v<IgnEngine::try_result_type>);

// ---------------------------------------------------------------------------
// underlying() overload reachability (compile-time only via decltype)
// ---------------------------------------------------------------------------

// lvalue -> BaseEngine&
static_assert(std::is_same_v<
    decltype(std::declval<TermEngine&>().underlying()),
    BaseEngine&>);

// const lvalue -> const BaseEngine&
static_assert(std::is_same_v<
    decltype(std::declval<const TermEngine&>().underlying()),
    const BaseEngine&>);

// rvalue -> BaseEngine&&
static_assert(std::is_same_v<
    decltype(std::declval<TermEngine&&>().underlying()),
    BaseEngine&&>);

// Same checks for ignoring_engine
static_assert(std::is_same_v<
    decltype(std::declval<IgnEngine&>().underlying()),
    BaseEngine&>);
static_assert(std::is_same_v<
    decltype(std::declval<const IgnEngine&>().underlying()),
    const BaseEngine&>);
static_assert(std::is_same_v<
    decltype(std::declval<IgnEngine&&>().underlying()),
    BaseEngine&&>);

} // namespace terminate_on_error_smoke

int main() {
  using namespace terminate_on_error_smoke;

  // ── 1. with_terminate_on_error: factory deduction + try_run() success path. ──
  {
    auto engine = pb::with_terminate_on_error(pb::compile<SimplePipeline>(pb::runtime::sequential{}));

    // Factory must produce terminating_engine<BaseEngine>.
    static_assert(std::is_same_v<decltype(engine), pb::terminating_engine<BaseEngine>>);

    // try_run() must return an engaged result on the success path.
    // Crucially, this does NOT call run() — no terminate() risk.
    auto result = engine.try_run(Input{.value = 42});
    assert(result.has_value());
    assert(result.value().value == 42);

    // Verify underlying() grants mutable access to the engine.
    // Setting the observer to nullptr is a no-op that exercises the accessor.
    engine.underlying().set_observer(nullptr);
  }

  // ── 2. with_terminate_on_error: run() on the success path is safe. ──
  {
    // run() only terminates on failure; success path returns normally.
    auto engine = pb::with_terminate_on_error(pb::compile<SimplePipeline>(pb::runtime::sequential{}));
    auto output = engine.run(Input{.value = 7});
    assert(output.value == 7);
  }

  // ── 3. with_ignore_errors: factory deduction + try_run() success path. ──
  {
    auto engine = pb::with_ignore_errors(
        pb::compile<SimplePipeline>(pb::runtime::sequential{}),
        Output{.value = -1});

    static_assert(std::is_same_v<decltype(engine), pb::ignoring_engine<BaseEngine>>);

    auto result = engine.try_run(Input{.value = 99});
    assert(result.has_value());
    assert(result.value().value == 99);
  }

  // ── 4. with_ignore_errors: run() on the success path returns the value. ──
  {
    auto engine = pb::with_ignore_errors(
        pb::compile<SimplePipeline>(pb::runtime::sequential{}),
        Output{.value = -1});
    auto output = engine.run(Input{.value = 5});
    assert(output.value == 5);
  }

  // ── 5. ignoring_engine: set_fallback / get_fallback round-trip. ──
  {
    auto engine = pb::with_ignore_errors(
        pb::compile<SimplePipeline>(pb::runtime::sequential{}),
        Output{.value = 0});

    assert(engine.get_fallback().value == 0);
    engine.set_fallback(Output{.value = 99});
    assert(engine.get_fallback().value == 99);
  }

  return 0;
}
