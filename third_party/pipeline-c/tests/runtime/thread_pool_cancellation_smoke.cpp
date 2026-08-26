/// @file  thread_pool_cancellation_smoke.cpp
/// @brief Cooperative cancellation for the thread-pool fan-in backend.
///
/// Cancellation in this library is COOPERATIVE: a pb::cancellation_source
/// signals an atomic flag and the fan-in scheduler checks it BEFORE a selected
/// case stage begins executing. Cases that have not yet started are skipped
/// (left in the default `skipped` slot state and reported via on_case_skipped)
/// instead of being run. Truly preemptive interruption of an already-running
/// stage is out of scope.
///
/// This test asserts:
///   • with a token whose stop is requested before scheduling, every selected
///     case is reported skipped, its stage never runs, and the run completes
///     deterministically (no hang / no UB);
///   • WITHOUT a token (default), the fan-in result matches the normal,
///     uncancelled result byte-for-byte;
///   • stop requests are idempotent, and copied token views remain valid after
///     their source object leaves scope.
/// main() returns 0 on success; assertions abort otherwise.

#include <pb/pipeline.hpp>

#include <atomic>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition) {
  if (!condition) std::abort();
}

struct Input {
  int mask{};
  int value{};
};

struct Output {
  int value{};
};

struct Done {
  std::string text{};
  int ran{};
};

// Always-selected predicates so every case is a candidate for scheduling.
struct IsA {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 1) != 0; }
};
struct IsB {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 2) != 0; }
};
struct IsC {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 4) != 0; }
};

// Each stage records that it actually executed, so the test can prove a
// cancelled case stage never ran.
struct StageA {
  using input_type = Input;
  using output_type = Output;
  static inline std::atomic<int> calls{0};
  Output operator()(Input input) const {
    calls.fetch_add(1, std::memory_order_relaxed);
    return Output{input.value + 10};
  }
};
struct StageB {
  using input_type = Input;
  using output_type = Output;
  static inline std::atomic<int> calls{0};
  Output operator()(Input input) const {
    calls.fetch_add(1, std::memory_order_relaxed);
    return Output{input.value + 100};
  }
};
struct StageC {
  using input_type = Input;
  using output_type = Output;
  static inline std::atomic<int> calls{0};
  Output operator()(Input input) const {
    calls.fetch_add(1, std::memory_order_relaxed);
    return Output{input.value + 1000};
  }
};

using CaseA = pb::case_<IsA>::then<StageA>;
using CaseB = pb::case_<IsB>::then<StageB>;
using CaseC = pb::case_<IsC>::then<StageC>;
using FanInInput = pb::fan_in_output_t<CaseA, CaseB, CaseC>;

struct Join {
  using input_type = FanInInput;
  using output_type = Done;
  Done operator()(const FanInInput& input) const {
    Done out{};
    auto append = [&](auto& slot, const char* name, int base) {
      out.text += name;
      out.text += '=';
      if (slot.completed()) {
        out.text += std::to_string(slot.get().value);
        ++out.ran;
      } else if (slot.skipped()) {
        out.text += "skip";
      } else {
        out.text += "fail";
      }
      out.text += ';';
      (void)base;
    };
    append(input.template get<0>(), "A", 10);
    append(input.template get<1>(), "B", 100);
    append(input.template get<2>(), "C", 1000);
    return out;
  }
};

using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::fan_in<Join>::to<Done>;
static_assert(pb::valid<Pipeline>);

struct RecordingObserver : pb::runtime::observer {
  std::atomic<int> selected{0};
  std::atomic<int> skipped{0};
  std::atomic<int> fan_in_started{0};
  std::atomic<int> fan_in_completed{0};
  std::atomic<int> fan_in_scheduled{0};
  std::atomic<int> fan_in_case_completed{0};
  std::atomic<std::size_t> started_case_count{0};
  std::atomic<std::size_t> reported_selected{0};
  std::atomic<std::size_t> reported_completed{0};
  std::atomic<std::size_t> reported_failed{0};
  void on_case_selected(const pb::runtime::stage_id&, std::size_t, const pb::runtime::stage_id&) override {
    selected.fetch_add(1, std::memory_order_relaxed);
  }
  void on_case_skipped(const pb::runtime::stage_id&, std::size_t, const pb::runtime::stage_id&) override {
    skipped.fetch_add(1, std::memory_order_relaxed);
  }
  void on_fan_in_started(const pb::runtime::stage_id&, std::size_t case_count) override {
    fan_in_started.fetch_add(1, std::memory_order_relaxed);
    started_case_count.store(case_count, std::memory_order_relaxed);
  }
  void on_fan_in_case_scheduled(const pb::runtime::stage_id&, std::size_t,
                                const pb::runtime::stage_id&) override {
    fan_in_scheduled.fetch_add(1, std::memory_order_relaxed);
  }
  void on_fan_in_case_completed(const pb::runtime::stage_id&, std::size_t,
                                const pb::runtime::stage_id&, bool) override {
    fan_in_case_completed.fetch_add(1, std::memory_order_relaxed);
  }
  void on_fan_in_completed(const pb::runtime::stage_id&, std::size_t selected_count,
                           std::size_t completed_count, std::size_t failed_count) override {
    fan_in_completed.fetch_add(1, std::memory_order_relaxed);
    reported_selected.store(selected_count, std::memory_order_relaxed);
    reported_completed.store(completed_count, std::memory_order_relaxed);
    reported_failed.store(failed_count, std::memory_order_relaxed);
  }
};

void reset_calls() {
  StageA::calls = 0;
  StageB::calls = 0;
  StageC::calls = 0;
}

} // namespace

int main() {
  static_assert(pb::cancellation_schema_version == std::string_view{"pb.cancel.v1"});

  // 0) Source and token copies share a single cooperative stop flag. A
  //    default token stays detached and never reports a stop. Repeated stop
  //    requests are idempotent, and token views keep the flag alive after the
  //    source object that produced them leaves scope.
  {
    pb::cancellation_token default_token{};
    require(!default_token.can_be_cancelled());
    require(!default_token.stop_requested());

    pb::cancellation_token token_after_source_lifetime{};
    {
      pb::cancellation_source source;
      auto token = source.token();
      auto token_copy = token;
      auto source_copy = source;
      auto copied_source_token = source_copy.token();
      token_after_source_lifetime = token_copy;

      require(token.can_be_cancelled());
      require(token_copy.can_be_cancelled());
      require(copied_source_token.can_be_cancelled());
      require(token_after_source_lifetime.can_be_cancelled());
      require(!source.stop_requested());
      require(!source_copy.stop_requested());
      require(!token.stop_requested());
      require(!token_copy.stop_requested());
      require(!copied_source_token.stop_requested());
      require(!token_after_source_lifetime.stop_requested());

      source_copy.request_stop();
      source.request_stop();
      source_copy.request_stop();
      require(source.stop_requested());
      require(source_copy.stop_requested());
      require(token.stop_requested());
      require(token_copy.stop_requested());
      require(copied_source_token.stop_requested());
      require(token_after_source_lifetime.stop_requested());
    }

    require(token_after_source_lifetime.can_be_cancelled());
    require(token_after_source_lifetime.stop_requested());
    require(!default_token.stop_requested());
  }

  // 1) Baseline: no token (default). All three cases selected and run; the
  //    result is the normal, uncancelled fan-in output.
  reset_calls();
  std::string normal_text;
  int normal_ran = 0;
  {
    auto engine = pb::compile<Pipeline>(pb::runtime::thread_pool_backend{.worker_count = 3});
    auto result = engine.run(Input{7, 5}); // mask 7 selects A, B, C
    require(result.has_value());
    require(result.value().ran == 3);
    require(result.value().text == "A=15;B=105;C=1005;");
    require(StageA::calls.load() == 1);
    require(StageB::calls.load() == 1);
    require(StageC::calls.load() == 1);
    normal_text = result.value().text;
    normal_ran = result.value().ran;
  }

  // 2) Token supplied but stop NOT requested: must be identical to the default
  //    (no-token) path.
  reset_calls();
  {
    pb::cancellation_source source;
    auto engine = pb::compile<Pipeline>(
        pb::runtime::thread_pool_backend{.worker_count = 3, .cancel = source.token()});
    auto result = engine.run(Input{7, 5});
    require(result.has_value());
    require(result.value().text == normal_text);
    require(result.value().ran == normal_ran);
    require(StageA::calls.load() == 1);
    require(StageB::calls.load() == 1);
    require(StageC::calls.load() == 1);
  }

  // 3) Cancellation token can be swapped between runs: a stopped token affects
  //    subsequent scheduling only until replaced by an unstopped token.
  reset_calls();
  {
    pb::cancellation_source stopped_source;
    stopped_source.request_stop();
    pb::cancellation_source resumed_source;

    auto engine = pb::compile<Pipeline>(pb::runtime::thread_pool_backend{.worker_count = 3});
    require(!engine.cancellation_token().can_be_cancelled());
    require(!engine.cancellation_token().stop_requested());

    engine.set_cancellation_token(stopped_source.token());
    require(engine.cancellation_token().can_be_cancelled());
    require(engine.cancellation_token().stop_requested());

    auto stopped = engine.run(Input{7, 5});
    require(stopped.has_value());
    require(stopped.value().ran == 0);
    require(stopped.value().text == "A=skip;B=skip;C=skip;");
    require(StageA::calls.load() == 0);
    require(StageB::calls.load() == 0);
    require(StageC::calls.load() == 0);

    engine.set_cancellation_token(resumed_source.token());
    require(engine.cancellation_token().can_be_cancelled());
    require(!engine.cancellation_token().stop_requested());

    auto resumed = engine.run(Input{7, 5});
    require(resumed.has_value());
    require(resumed.value().text == normal_text);
    require(resumed.value().ran == normal_ran);
    require(StageA::calls.load() == 1);
    require(StageB::calls.load() == 1);
    require(StageC::calls.load() == 1);
  }

  // 4) Stop requested BEFORE scheduling: every selected case is skipped, no
  //    case stage executes, and the run completes deterministically.
  reset_calls();
  {
    pb::cancellation_source source;
    source.request_stop(); // request before scheduling begins
    require(source.stop_requested());

    RecordingObserver observer;
    auto engine = pb::compile<Pipeline>(
        pb::runtime::thread_pool_backend{.worker_count = 3, .cancel = source.token()});
    engine.set_observer(&observer);

    auto result = engine.run(Input{7, 5});
    require(result.has_value());
    // Not-yet-started cases were skipped instead of run.
    require(result.value().ran == 0);
    require(result.value().text == "A=skip;B=skip;C=skip;");
    require(StageA::calls.load() == 0);
    require(StageB::calls.load() == 0);
    require(StageC::calls.load() == 0);
    // None were "selected" for execution; all three reported skipped.
    require(observer.selected.load() == 0);
    require(observer.skipped.load() == 3);
    // Stopped-before-enqueue cases are skipped before scheduling, so fan-in
    // lifecycle counts report an empty selected/completed/failed set.
    require(observer.fan_in_started.load() == 1);
    require(observer.started_case_count.load() == 3);
    require(observer.fan_in_scheduled.load() == 0);
    require(observer.fan_in_case_completed.load() == 0);
    require(observer.fan_in_completed.load() == 1);
    require(observer.reported_selected.load() == 0);
    require(observer.reported_completed.load() == 0);
    require(observer.reported_failed.load() == 0);
  }

  // 5) Re-run several times with stop requested to prove determinism / no hang.
  reset_calls();
  {
    pb::cancellation_source source;
    source.request_stop();
    auto engine = pb::compile<Pipeline>(
        pb::runtime::thread_pool_backend{.worker_count = 2, .cancel = source.token()});
    for (int i = 0; i < 32; ++i) {
      auto result = engine.run(Input{7, i});
      require(result.has_value());
      require(result.value().ran == 0);
      require(result.value().text == "A=skip;B=skip;C=skip;");
    }
    require(StageA::calls.load() == 0);
    require(StageB::calls.load() == 0);
    require(StageC::calls.load() == 0);
  }

  return 0;
}
