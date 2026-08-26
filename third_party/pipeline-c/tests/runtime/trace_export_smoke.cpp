#include <pb/pipeline.hpp>

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>


namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}
}  // namespace

// ---------------------------------------------------------------------------
// Linear pipeline stages (existing coverage)
// ---------------------------------------------------------------------------

struct Input {
  int value{};
};

struct Middle {
  int value{};
};

struct Output {
  int value{};
};

struct AddOne {
  using input_type = Input;
  using output_type = Middle;

  static constexpr auto stage_name() noexcept { return "add_one"; }

  Middle operator()(Input input) const { return {input.value + 1}; }
};

struct CheckedDouble {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "checked_double"; }
  static constexpr auto stage_key() noexcept { return "math.double"; }

  pb::runtime::result<Output> operator()(Middle input) const {
    if (input.value == 0) {
      return pb::runtime::error{.category = pb::runtime::error_category::stage_failure,
                                .message = "zero \"middle\"\nline"};
    }
    return Output{input.value * 2};
  }
};

using LinearPipeline = pb::from<Input>::then<AddOne>::then<CheckedDouble>::to<Output>;
static_assert(pb::valid<LinearPipeline>);

// ---------------------------------------------------------------------------
// Branch pipeline stages (new coverage)
// ---------------------------------------------------------------------------

struct Raw {
  int value{};
};

struct Routed {
  int value{};
};

struct IsEven {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is_even"; }
  static constexpr auto stage_key() noexcept { return "pred.even"; }
  bool operator()(Raw raw) const { return raw.value % 2 == 0; }
};

struct IsOdd {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is_odd"; }
  static constexpr auto stage_key() noexcept { return "pred.odd"; }
  bool operator()(Raw raw) const { return raw.value % 2 != 0; }
};

struct EvenRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "even_route"; }
  static constexpr auto stage_key() noexcept { return "route.even"; }
  Routed operator()(Raw raw) const { return {raw.value * 10}; }
};

struct OddRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "odd_route"; }
  static constexpr auto stage_key() noexcept { return "route.odd"; }
  Routed operator()(Raw raw) const { return {raw.value * 100}; }
};

struct FailingRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "failing_route"; }
  static constexpr auto stage_key() noexcept { return "route.fail"; }
  pb::runtime::result<Routed> operator()(Raw) const {
    return pb::runtime::error{.category = pb::runtime::error_category::stage_failure,
                              .message = "fan-in case failed \"route\"\nline"};
  }
};

using EvenCase = pb::case_<IsEven>::then<EvenRoute>;
using OddCase = pb::case_<IsOdd>::then<OddRoute>;
using FailingEvenCase = pb::case_<IsEven>::then<FailingRoute>;
using BranchPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::to<Routed>;
static_assert(pb::valid<BranchPipeline>);
using FanInOutput = pb::fan_in_output_t<EvenCase, OddCase>;
using FailingFanInOutput = pb::fan_in_output_t<FailingEvenCase, OddCase>;

struct FanInJoin {
  using input_type = FanInOutput;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "fan_in_join"; }
  static constexpr auto stage_key() noexcept { return "join.fan_in"; }
  Routed operator()(const FanInOutput& input) const {
    int total = 0;
    if (input.template get<0>().completed()) {
      total += input.template get<0>().get().value;
    }
    if (input.template get<1>().completed()) {
      total += input.template get<1>().get().value;
    }
    return {total};
  }
};

using FanInPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::fan_in<FanInJoin>::to<Routed>;
static_assert(pb::valid<FanInPipeline>);

struct FailingFanInJoin {
  using input_type = FailingFanInOutput;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "failing_fan_in_join"; }
  static constexpr auto stage_key() noexcept { return "join.failing_fan_in"; }
  Routed operator()(const FailingFanInOutput& input) const {
    int total = 0;
    if (input.template get<0>().completed()) {
      total += input.template get<0>().get().value;
    }
    if (input.template get<1>().completed()) {
      total += input.template get<1>().get().value;
    }
    return {total};
  }
};

using FailingFanInPipeline = pb::from<Raw>::branch<FailingEvenCase, OddCase>::fan_in<FailingFanInJoin>::to<Routed>;
static_assert(pb::valid<FailingFanInPipeline>);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

[[nodiscard]] auto contains(std::string_view haystack, std::string_view needle) -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
  // -----------------------------------------------------------------------
  // Linear pipeline — success path
  // -----------------------------------------------------------------------
  {
    auto engine = pb::compile<LinearPipeline>(pb::runtime::sequential{});
    pb::runtime::trace_recorder recorder{};
    pb::runtime::trace_observer observer{recorder};
    engine.set_observer(&observer);

    auto output = engine.try_run(Input{20});
    pb_test_require(output.has_value());
    pb_test_require(output.value().value == 42);
    pb_test_require(recorder.size() == 4);
    pb_test_require(recorder.events()[0].kind == pb::runtime::trace_event_kind::stage_start);
    pb_test_require(recorder.events()[0].stage_name == "add_one");
    pb_test_require(!recorder.events()[0].has_error);
    pb_test_require(recorder.events()[3].kind == pb::runtime::trace_event_kind::stage_success);
    pb_test_require(recorder.events()[3].stage_key == "math.double");

    const auto success_json = pb::runtime::export_json(recorder);
    pb_test_require(contains(success_json, R"("schema_version":"pb.trace.v1")"));
    pb_test_require(contains(success_json, R"("kind":"stage_start")"));
    pb_test_require(contains(success_json, R"("kind":"stage_success")"));
    pb_test_require(contains(success_json, R"("key":"math.double")"));
  }

  // -----------------------------------------------------------------------
  // Linear pipeline — failure path
  // -----------------------------------------------------------------------
  {
    auto engine = pb::compile<LinearPipeline>(pb::runtime::sequential{});
    pb::runtime::trace_recorder recorder{};
    pb::runtime::trace_observer observer{recorder};
    engine.set_observer(&observer);

    auto failed = engine.try_run(Input{-1});
    pb_test_require(!failed.has_value());
    pb_test_require(recorder.size() == 4);
    pb_test_require(recorder.events()[3].kind == pb::runtime::trace_event_kind::stage_failure);
    pb_test_require(recorder.events()[3].has_error);
    pb_test_require(recorder.events()[3].diagnostic.message == "zero \"middle\"\nline");

    const auto failure_json = pb::runtime::export_json(recorder);
    pb_test_require(contains(failure_json, R"("kind":"stage_failure")"));
    pb_test_require(contains(failure_json, R"("category":"stage_failure")"));
    pb_test_require(contains(failure_json, "\"message\":\"zero \\\"middle\\\"\\nline\""));
  }

  // -----------------------------------------------------------------------
  // Branch pipeline — even case (case_selected)
  // -----------------------------------------------------------------------
  {
    auto engine = pb::compile<BranchPipeline>(pb::runtime::sequential{});
    pb::runtime::trace_recorder recorder{};
    pb::runtime::trace_observer observer{recorder};
    engine.set_observer(&observer);

    auto even = engine.run(Raw{4});
    pb_test_require(even.has_value());
    pb_test_require(even.value().value == 40);

    // Verify case_selected and case_skipped events
    bool saw_case_selected = false;
    bool saw_case_skipped = false;
    for (const auto& event : recorder.events()) {
      if (event.kind == pb::runtime::trace_event_kind::case_selected) {
        saw_case_selected = true;
        pb_test_require(event.stage_key == "branch");
        pb_test_require(event.case_index == 0);
      }
      if (event.kind == pb::runtime::trace_event_kind::case_skipped) {
        saw_case_skipped = true;
        pb_test_require(event.stage_key == "branch");
        pb_test_require(event.case_index == 0);
      }
    }
    // Even case should be selected (not skipped) since index 0 matches
    pb_test_require(saw_case_selected);
    pb_test_require(!saw_case_skipped);

    const auto json = pb::runtime::export_json(recorder);
    pb_test_require(contains(json, R"("kind":"case_selected")"));
    pb_test_require(contains(json, "\"key\":\"branch\""));
    pb_test_require(contains(json, "\"stage_index\":0"));
    pb_test_require(contains(json, "\"case_index\":0"));
  }

  // -----------------------------------------------------------------------
  // Branch pipeline — odd case (case_skipped then case_selected)
  // -----------------------------------------------------------------------
  {
    auto engine = pb::compile<BranchPipeline>(pb::runtime::sequential{});
    pb::runtime::trace_recorder recorder{};
    pb::runtime::trace_observer observer{recorder};
    engine.set_observer(&observer);

    auto odd = engine.run(Raw{3});
    pb_test_require(odd.has_value());
    pb_test_require(odd.value().value == 300);

    // Case 0 (IsEven) should be skipped, case 1 (IsOdd) selected
    bool saw_case_0_skipped = false;
    bool saw_case_1_selected = false;
    for (const auto& event : recorder.events()) {
      if (event.kind == pb::runtime::trace_event_kind::case_skipped &&
          event.case_index == 0) {
        saw_case_0_skipped = true;
      }
      if (event.kind == pb::runtime::trace_event_kind::case_selected &&
          event.case_index == 1) {
        saw_case_1_selected = true;
      }
    }
    pb_test_require(saw_case_0_skipped);
    pb_test_require(saw_case_1_selected);

    const auto json = pb::runtime::export_json(recorder);
    pb_test_require(contains(json, R"("kind":"case_skipped")"));
    pb_test_require(contains(json, R"("kind":"case_selected")"));
  }

  // -----------------------------------------------------------------------
  // Fan-in pipeline — lifecycle events and counts
  // -----------------------------------------------------------------------
  {
    auto engine = pb::compile<FanInPipeline>(pb::runtime::sequential{});
    pb::runtime::trace_recorder recorder{};
    pb::runtime::trace_observer observer{recorder};
    engine.set_observer(&observer);

    auto even = engine.run(Raw{4});
    pb_test_require(even.has_value());
    pb_test_require(even.value().value == 40);

    bool saw_started = false;
    bool saw_scheduled = false;
    bool saw_case_completed = false;
    bool saw_completed = false;
    for (const auto& event : recorder.events()) {
      if (event.kind == pb::runtime::trace_event_kind::fan_in_started) {
        saw_started = true;
        pb_test_require(event.stage_key == "fan_in");
        pb_test_require(event.case_count == 2);
      }
      if (event.kind == pb::runtime::trace_event_kind::fan_in_case_scheduled) {
        saw_scheduled = true;
        pb_test_require(event.stage_key == "fan_in");
        pb_test_require(event.case_index == 0);
      }
      if (event.kind == pb::runtime::trace_event_kind::fan_in_case_completed) {
        saw_case_completed = true;
        pb_test_require(event.stage_key == "fan_in");
        pb_test_require(event.case_index == 0);
        pb_test_require(event.success);
      }
      if (event.kind == pb::runtime::trace_event_kind::fan_in_completed) {
        saw_completed = true;
        pb_test_require(event.stage_key == "fan_in");
        pb_test_require(event.selected_count == 1);
        pb_test_require(event.completed_count == 1);
        pb_test_require(event.failed_count == 0);
      }
    }
    pb_test_require(saw_started);
    pb_test_require(saw_scheduled);
    pb_test_require(saw_case_completed);
    pb_test_require(saw_completed);

    const auto json = pb::runtime::export_json(recorder);
    pb_test_require(contains(json, R"("kind":"fan_in_started")"));
    pb_test_require(contains(json, R"("case_count":2)"));
    pb_test_require(contains(json, R"("kind":"fan_in_case_scheduled")"));
    pb_test_require(contains(json, R"("kind":"fan_in_case_completed")"));
    pb_test_require(contains(json, R"("success":true)"));
    pb_test_require(contains(json, R"("kind":"fan_in_completed")"));
    pb_test_require(contains(json, R"("selected_count":1)"));
    pb_test_require(contains(json, R"("completed_count":1)"));
    pb_test_require(contains(json, R"("failed_count":0)"));
  }

  // -----------------------------------------------------------------------
  // Fan-in pipeline — failed case trace schema and counts
  // -----------------------------------------------------------------------
  {
    auto engine = pb::compile<FailingFanInPipeline>(pb::runtime::sequential{});
    pb::runtime::trace_recorder recorder{};
    pb::runtime::trace_observer observer{recorder};
    engine.set_observer(&observer);

    auto even = engine.run(Raw{4});
    pb_test_require(even.has_value());
    pb_test_require(even.value().value == 0);

    bool saw_case_failed = false;
    bool saw_case_completed = false;
    bool saw_completed = false;
    for (const auto& event : recorder.events()) {
      if (event.kind == pb::runtime::trace_event_kind::case_failed) {
        saw_case_failed = true;
        pb_test_require(event.stage_key == "fan_in");
        pb_test_require(event.case_index == 0);
        pb_test_require(event.has_error);
        pb_test_require(event.diagnostic.category == pb::runtime::error_category::stage_failure);
        pb_test_require(event.diagnostic.stage.key == "route.fail");
        pb_test_require(event.diagnostic.message == "fan-in case failed \"route\"\nline");
      }
      if (event.kind == pb::runtime::trace_event_kind::fan_in_case_completed &&
          event.case_index == 0) {
        saw_case_completed = true;
        pb_test_require(event.stage_key == "fan_in");
        pb_test_require(!event.success);
      }
      if (event.kind == pb::runtime::trace_event_kind::fan_in_completed) {
        saw_completed = true;
        pb_test_require(event.selected_count == 1);
        pb_test_require(event.completed_count == 0);
        pb_test_require(event.failed_count == 1);
      }
    }
    pb_test_require(saw_case_failed);
    pb_test_require(saw_case_completed);
    pb_test_require(saw_completed);

    const auto json = pb::runtime::export_json(recorder);
    pb_test_require(contains(json, R"("kind":"case_failed")"));
    pb_test_require(contains(json, R"("case_index":0)"));
    pb_test_require(contains(json, R"("error":)"));
    pb_test_require(contains(json, R"("key":"route.fail")"));
    pb_test_require(contains(json, R"("category":"stage_failure")"));
    pb_test_require(contains(json, "\"message\":\"fan-in case failed \\\"route\\\"\\nline\""));
    pb_test_require(contains(json, R"("kind":"fan_in_case_completed")"));
    pb_test_require(contains(json, R"("success":false)"));
    pb_test_require(contains(json, R"("kind":"fan_in_completed")"));
    pb_test_require(contains(json, R"("selected_count":1)"));
    pb_test_require(contains(json, R"("completed_count":0)"));
    pb_test_require(contains(json, R"("failed_count":1)"));
  }

  // -----------------------------------------------------------------------
  // Trace recorder empty/clear round-trip
  // -----------------------------------------------------------------------
  {
    pb::runtime::trace_recorder recorder{};
    pb_test_require(recorder.empty());
    pb_test_require(recorder.size() == 0);

    recorder.write(pb::runtime::trace_event{
        .kind = pb::runtime::trace_event_kind::pipeline_start,
        .stage_key = "test",
        .stage_name = "Test Pipeline"});
    pb_test_require(!recorder.empty());
    pb_test_require(recorder.size() == 1);

    recorder.clear();
    pb_test_require(recorder.empty());
    pb_test_require(recorder.size() == 0);
  }

  // -----------------------------------------------------------------------
  // trace_event_kind_name coverage
  // -----------------------------------------------------------------------
  {
    using namespace pb::runtime;
    pb_test_require(trace_event_kind_name(trace_event_kind::stage_start) == "stage_start");
    pb_test_require(trace_event_kind_name(trace_event_kind::stage_success) == "stage_success");
    pb_test_require(trace_event_kind_name(trace_event_kind::stage_failure) == "stage_failure");
    pb_test_require(trace_event_kind_name(trace_event_kind::stage_exception) == "stage_exception");
    pb_test_require(trace_event_kind_name(trace_event_kind::case_selected) == "case_selected");
    pb_test_require(trace_event_kind_name(trace_event_kind::case_skipped) == "case_skipped");
    pb_test_require(trace_event_kind_name(trace_event_kind::case_failed) == "case_failed");
    pb_test_require(trace_event_kind_name(trace_event_kind::fan_in_started) == "fan_in_started");
    pb_test_require(trace_event_kind_name(trace_event_kind::fan_in_case_scheduled) == "fan_in_case_scheduled");
    pb_test_require(trace_event_kind_name(trace_event_kind::fan_in_case_completed) == "fan_in_case_completed");
    pb_test_require(trace_event_kind_name(trace_event_kind::fan_in_completed) == "fan_in_completed");
    pb_test_require(trace_event_kind_name(trace_event_kind::pipeline_start) == "pipeline_start");
    pb_test_require(trace_event_kind_name(trace_event_kind::pipeline_complete) == "pipeline_complete");
  }

  // -----------------------------------------------------------------------
  // trace_observer pointer constructor
  // -----------------------------------------------------------------------
  {
    pb::runtime::trace_recorder recorder{};
    pb::runtime::trace_observer observer{&recorder};
    pb_test_require(observer.get_sink() == &recorder);

    observer.set_sink(nullptr);
    pb_test_require(observer.get_sink() == nullptr);

    observer.set_sink(&recorder);
    pb_test_require(observer.get_sink() == &recorder);
  }

  // -----------------------------------------------------------------------
  // JSON export on empty recorder
  // -----------------------------------------------------------------------
  {
    pb::runtime::trace_recorder recorder{};
    const auto json = pb::runtime::export_json(recorder);
    pb_test_require(contains(json, R"("schema_version":"pb.trace.v1")"));
    pb_test_require(contains(json, R"("events":[)"));
    // No events, so events list should be empty
    pb_test_require(contains(json, R"str("events":[])str"));
  }

  return 0;
}
