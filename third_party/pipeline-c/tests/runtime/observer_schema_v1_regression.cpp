// pb.observer.v1 + pb.observer.verbose.v1 schema regression.
//
// docs/observer-schema-v1.md formalizes the observer ABI shape AND the
// verbose event line format.  This test pins both:
//
//   * observer_schema_version and verbose_observer_schema_version
//     literals.
//   * The eleven-method vtable shape via compile-time `requires` checks
//     on a derived observer.
//   * Every documented [pb.verbose] event line as a byte-equal
//     substring assertion.
//
// A regression here means either the v1 contract is being broken
// silently, or the schema is being intentionally bumped (in which case
// this test moves to v2 and the v1 emitter must still ship with this
// test passing).

#include <pb/pipeline.hpp>

#include <concepts>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace observer_schema_v1 {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

// ────────────────────────────────────────────────────────────────────────────
// ABI shape — derived observer with all 11 overrides must compile, and
// each method must be virtual with the documented signature.
// ────────────────────────────────────────────────────────────────────────────

struct probe_observer final : pb::runtime::observer {
  void on_stage_start    (const pb::runtime::stage_id&) override {}
  void on_stage_success  (const pb::runtime::stage_id&) override {}
  void on_stage_failure  (const pb::runtime::stage_id&, const pb::runtime::error&) override {}
  void on_stage_exception(const pb::runtime::stage_id&, const pb::runtime::error&) override {}
  void on_case_selected  (const pb::runtime::stage_id&, std::size_t, const pb::runtime::stage_id&) override {}
  void on_case_skipped   (const pb::runtime::stage_id&, std::size_t, const pb::runtime::stage_id&) override {}
  void on_case_failed    (const pb::runtime::stage_id&, std::size_t,
                          const pb::runtime::stage_id&, const pb::runtime::error&) override {}
  void on_fan_in_started (const pb::runtime::stage_id&, std::size_t) override {}
  void on_fan_in_case_scheduled(const pb::runtime::stage_id&, std::size_t,
                                const pb::runtime::stage_id&) override {}
  void on_fan_in_case_completed(const pb::runtime::stage_id&, std::size_t,
                                const pb::runtime::stage_id&, bool) override {}
  void on_fan_in_completed(const pb::runtime::stage_id&, std::size_t, std::size_t,
                           std::size_t) override {}
};

static_assert(std::is_polymorphic_v<pb::runtime::observer>,
              "pb.observer.v1 ABI requires a polymorphic base — virtual dtor + vtable");
static_assert(std::has_virtual_destructor_v<pb::runtime::observer>,
              "pb.observer.v1 ABI requires a virtual destructor");

// All eleven methods MUST be reachable on the derived type — a vtable
// reordering or signature change shows up as a constraint failure.
static_assert(requires(probe_observer& o,
                       const pb::runtime::stage_id& id,
                       const pb::runtime::error& err) {
  { o.on_stage_start(id) }     -> std::same_as<void>;
  { o.on_stage_success(id) }   -> std::same_as<void>;
  { o.on_stage_failure(id, err) }   -> std::same_as<void>;
  { o.on_stage_exception(id, err) } -> std::same_as<void>;
}, "pb.observer.v1: all four single-stage hooks must remain reachable with documented signatures");

static_assert(requires(probe_observer& o,
                       const pb::runtime::stage_id& id,
                       const pb::runtime::error& err,
                       std::size_t idx) {
  { o.on_case_selected(id, idx, id) } -> std::same_as<void>;
  { o.on_case_skipped (id, idx, id) } -> std::same_as<void>;
  { o.on_case_failed  (id, idx, id, err) } -> std::same_as<void>;
}, "pb.observer.v1: all three case hooks must remain reachable with documented signatures");

static_assert(requires(probe_observer& o,
                       const pb::runtime::stage_id& id,
                       std::size_t idx,
                       bool success) {
  { o.on_fan_in_started(id, idx) } -> std::same_as<void>;
  { o.on_fan_in_case_scheduled(id, idx, id) } -> std::same_as<void>;
  { o.on_fan_in_case_completed(id, idx, id, success) } -> std::same_as<void>;
  { o.on_fan_in_completed(id, idx, idx, idx) } -> std::same_as<void>;
}, "pb.observer.v1: all four fan-in lifecycle hooks must remain reachable with documented signatures");

// ────────────────────────────────────────────────────────────────────────────
// Test pipelines for verbose-line regression
// ────────────────────────────────────────────────────────────────────────────

struct Input  { int value{}; };
struct Output { int value{}; };

struct Always {
  using input_type  = Input;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "always"; }
  static constexpr auto stage_name() noexcept { return "always"; }
  bool operator()(const Input&) const { return true; }
};

struct Increment {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "increment"; }
  static constexpr auto stage_name() noexcept { return "increment"; }
  Output operator()(Input input) const { return {input.value + 1}; }
};

struct FailingStage {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "fail"; }
  static constexpr auto stage_name() noexcept { return "fail"; }
  pb::runtime::result<Output> operator()(Input) const {
    return pb::runtime::result<Output>{pb::runtime::error{
        .category = pb::runtime::error_category::stage_failure,
        .message  = "intentional",
    }};
  }
};

struct ThrowingStage {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "boom"; }
  static constexpr auto stage_name() noexcept { return "boom"; }
  Output operator()(Input) const { throw std::runtime_error{"kaboom"}; }
};

using SuccessLinear   = pb::from<Input>::then<Increment>::to<Output>;
using FailureLinear   = pb::from<Input>::then<FailingStage>::to<Output>;
using ExceptionLinear = pb::from<Input>::then<ThrowingStage>::to<Output>;

using IncrementCase = pb::case_<Always>::then<Increment>;
using SuccessBranch = pb::from<Input>::branch<IncrementCase>::to<Output>;
using FanInOutput = pb::fan_in_output_t<IncrementCase>;

struct FanInJoin {
  using input_type = FanInOutput;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "join"; }
  static constexpr auto stage_name() noexcept { return "join"; }
  Output operator()(const FanInOutput& input) const {
    return input.template get<0>().get();
  }
};

using SuccessFanIn = pb::from<Input>::branch<IncrementCase>::fan_in<FanInJoin>::to<Output>;

template <class Pipeline>
auto fresh_engine() {
  return pb::compile<Pipeline>(pb::runtime::sequential{});
}

} // namespace observer_schema_v1

int main() {
  using namespace observer_schema_v1;

  // ── Identifier constants must keep their exact v1 spelling. ──
  static_assert(pb::observer_schema_version == std::string_view{"pb.observer.v1"},
                "pb.observer.v1 ABI identifier MUST stay exactly v1");
  static_assert(pb::verbose_observer_schema_version == std::string_view{"pb.observer.verbose.v1"},
                "pb.observer.verbose.v1 line-schema identifier MUST stay exactly v1");

  // ── Verbose line schema — every event format pinned. ──

  // stage_start + stage_success on a clean linear run.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<SuccessLinear>(), &sink);
    (void)engine.try_run(Input{.value = 1});
    const auto log = sink.str();
    // Exact line format: "[pb.verbose] stage_start stage=increment\n"
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=increment\n"));
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=increment\n"));
  }

  // stage_failure with category + message.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<FailureLinear>(), &sink);
    (void)engine.try_run(Input{});
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=fail\n"));
    pb_test_require(contains(log,
        "[pb.verbose] stage_failure stage=fail category=stage_failure message=intentional\n"));
  }

  // stage_exception with category + message.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<ExceptionLinear>(), &sink);
    (void)engine.try_run(Input{});
    const auto log = sink.str();
    pb_test_require(contains(log,
        "[pb.verbose] stage_exception stage=boom category=exception message=kaboom\n"));
  }

  // case_selected on a branch with one always-on case.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<SuccessBranch>(), &sink);
    (void)engine.try_run(Input{.value = 1});
    const auto log = sink.str();
    pb_test_require(contains(log,
        "[pb.verbose] case_selected branch=branch case=0 predicate=always\n"));
  }

  // fan-in lifecycle lines on an always-selected single-case fan-in branch.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<SuccessFanIn>(), &sink);
    (void)engine.try_run(Input{.value = 1});
    const auto log = sink.str();
    pb_test_require(contains(log,
        "[pb.verbose] fan_in_started branch=fan_in cases=1\n"));
    pb_test_require(contains(log,
        "[pb.verbose] fan_in_case_scheduled branch=fan_in case=0 stage=increment\n"));
    pb_test_require(contains(log,
        "[pb.verbose] fan_in_case_completed branch=fan_in case=0 stage=increment success=true\n"));
    pb_test_require(contains(log,
        "[pb.verbose] fan_in_completed branch=fan_in selected=1 completed=1 failed=0\n"));
  }

  return 0;
}
