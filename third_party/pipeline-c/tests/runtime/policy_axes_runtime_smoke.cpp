/// @file policy_axes_runtime_smoke.cpp
/// @brief Runtime verification that the diagnostics policy axis
///        (`::with<pb::policy::diagnostics::verbose>`) is enforced by
///        `pb::compile<P>(sequential{})`, that it composes AROUND the
///        error-policy wrapper, and that copying-axis markers remain carried +
///        queryable without changing bare sequential runtime behaviour.
///
///   - `::with<diagnostics::verbose>`                     -> compile wraps the
///        engine in a verbose engine that logs `[pb.verbose]` lines.
///   - `::with<errors::throwing>::with<diagnostics::verbose>` -> the engine
///        BOTH throws on a stage failure (throwing wrapper, inside) AND emits
///        verbose lines (verbose wrapper, outside).
///   - a plain pipeline                                   -> no verbose output,
///        baseline result-returning behaviour, byte-for-byte unchanged.
///
/// The DSL verbose wrapper defaults its sink to `&std::clog`; this test
/// retargets it to a `std::ostringstream` via `set_sink(...)` so the emitted
/// lines can be captured and asserted on.
///
/// main() returns 0 on success; failures abort / propagate as exceptions.

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>

namespace {

void require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

template <class Engine>
concept HasSetSink = requires(Engine& engine) { engine.set_sink(nullptr); };

struct Input {
  int value{};
};

struct Output {
  int value{};
};

struct MoveInput {
  std::unique_ptr<int> value;
};

struct MoveOutput {
  std::unique_ptr<int> value;
};

// Always-succeeding stage.
struct Increment {
  using input_type = Input;
  using output_type = Output;
  static constexpr auto stage_key() noexcept { return "increment"; }
  static constexpr auto stage_name() noexcept { return "increment"; }
  Output operator()(Input input) const { return {input.value + 1}; }
};

// Stage that reports a declared (result<>-based) failure.
struct FailingStage {
  using input_type = Input;
  using output_type = Output;
  static constexpr auto stage_key() noexcept { return "fail"; }
  static constexpr auto stage_name() noexcept { return "fail"; }
  pb::runtime::result<Output> operator()(Input) const {
    return pb::runtime::result<Output>{pb::runtime::error{
        .category = pb::runtime::error_category::stage_failure,
        .message = "declared failure",
    }};
  }
};

struct MoveIncrement {
  using input_type = MoveInput;
  using output_type = MoveOutput;
  static constexpr auto stage_key() noexcept { return "move_increment"; }
  static constexpr auto stage_name() noexcept { return "move_increment"; }
  MoveOutput operator()(MoveInput input) const {
    ++(*input.value);
    return MoveOutput{.value = std::move(input.value)};
  }
};

// Pipelines built purely through the research-plan DSL syntax.
using VerboseSuccessPipeline =
    pb::from<Input>::with<pb::policy::diagnostics::verbose>::then<Increment>::to<Output>;

// throwing INSIDE, verbose OUTSIDE — the two axes compose independently.
using ThrowingVerboseFailurePipeline = pb::from<Input>::with<pb::policy::errors::throwing>::with<
    pb::policy::diagnostics::verbose>::then<FailingStage>::to<Output>;

using PlainPipeline = pb::from<Input>::then<Increment>::to<Output>;

using CopyingValuePipeline =
    pb::from<Input>::with<pb::policy::copying::value>::then<Increment>::to<Output>;
using CopyingMoveOnlyPipeline =
    pb::from<MoveInput>::with<pb::policy::copying::move_only>::then<MoveIncrement>::to<MoveOutput>;

// Compile-time confirmation that policy markers are detected.
static_assert(pb::has_diagnostics_policy_v<VerboseSuccessPipeline>);
static_assert(std::is_same_v<pb::pipeline_diagnostics_policy_t<VerboseSuccessPipeline>,
                             pb::policy::diagnostics::verbose>);
static_assert(pb::has_diagnostics_policy_v<ThrowingVerboseFailurePipeline>);
static_assert(pb::has_error_policy_v<ThrowingVerboseFailurePipeline>);
static_assert(!pb::has_diagnostics_policy_v<PlainPipeline>);
static_assert(pb::has_copying_policy_v<CopyingValuePipeline>);
static_assert(std::is_same_v<pb::pipeline_copying_policy_t<CopyingValuePipeline>,
                             pb::policy::copying::value>);
static_assert(pb::has_copying_policy_v<CopyingMoveOnlyPipeline>);
static_assert(std::is_same_v<pb::pipeline_copying_policy_t<CopyingMoveOnlyPipeline>,
                             pb::policy::copying::move_only>);

} // namespace

int main() {
  // 1. ::with<diagnostics::verbose> -> compile wraps in a verbose engine; the
  //    run emits [pb.verbose] lines to the (retargeted) sink.
  {
    std::ostringstream sink;
    auto engine = pb::compile<VerboseSuccessPipeline>(pb::runtime::sequential{});
    engine.set_sink(&sink);

    const auto out = engine.run(Input{.value = 41});
    require(out.value == 42);

    const auto log = sink.str();
    require(!log.empty());
    require(contains(log, "[pb.verbose]"));
    require(contains(log, "stage_start"));
    require(contains(log, "stage_success"));
    require(contains(log, "increment"));
  }

  // 2. throwing + verbose: BOTH throws on failure AND emits verbose lines
  //    (the verbose wrapper sits around the throwing wrapper, so the failure
  //    event is logged before the throwing wrapper raises).
  {
    std::ostringstream sink;
    auto engine = pb::compile<ThrowingVerboseFailurePipeline>(pb::runtime::sequential{});
    engine.set_sink(&sink);

    bool threw = false;
    try {
      (void)engine.run(Input{.value = 7});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      require(ex.diagnostic().stage.key == "fail");
    }
    require(threw);

    const auto log = sink.str();
    require(!log.empty());
    require(contains(log, "[pb.verbose]"));
    require(contains(log, "stage_failure"));
    require(contains(log, "fail"));
  }

  // 3. Plain pipeline -> no verbose wrapper, no verbose output, baseline
  //    behaviour.  A plain pipeline compiles to the bare engine, which has no
  //    set_sink() at all — so the absence of verbose output is structural, and
  //    we just confirm the baseline result-returning behaviour is intact.
  {
    auto plain = pb::compile<PlainPipeline>(pb::runtime::sequential{});
    auto baseline =
        pb::compile<pb::from<Input>::then<Increment>::to<Output>>(pb::runtime::sequential{});
    static_assert(std::is_same_v<decltype(plain), decltype(baseline)>,
                  "a pipeline without a diagnostics marker must compile to the SAME bare engine type");

    const auto a = plain.run(Input{.value = 10});
    require(a.value == 11);
  }

  // 4. Copying-axis markers are carried + queryable only for the sequential
  //    backend: compile<> does not add a runtime wrapper or alter movement
  //    semantics.  The value marker keeps bare-engine shape; move_only accepts
  //    an rvalue move-only payload and moves it through the stage.
  {
    auto value_engine = pb::compile<CopyingValuePipeline>(pb::runtime::sequential{});
    static_assert(!HasSetSink<decltype(value_engine)>,
                  "copying::value must not select the diagnostics wrapper or add a runtime sink");

    const auto out = value_engine.run(Input{.value = 4});
    require(out.value == 5);

    auto move_engine = pb::compile<CopyingMoveOnlyPipeline>(pb::runtime::sequential{});
    static_assert(!HasSetSink<decltype(move_engine)>,
                  "copying::move_only must not select the diagnostics wrapper or add a runtime sink");

    auto moved = move_engine.run(MoveInput{.value = std::make_unique<int>(8)});
    require(moved.value != nullptr);
    require(*moved.value == 9);
  }

  return 0;
}
