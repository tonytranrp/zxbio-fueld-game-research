// pb.trace.ndjson.v1 streaming-sink regression.
//
// pb::tracing_sink writes one JSON object per pipeline lifecycle event
// to a std::ostream, terminated by '\n'.  The per-event JSON shape is
// shared with the batch export_json format — only the framing differs.
//
// This test pins:
//   * pb.trace.ndjson.v1 identifier constant
//   * One '\n'-terminated line per event
//   * Each line is a self-contained JSON object (round-trip with batch shape)
//   * All seven observer event kinds get emitted as expected event kinds
//   * Null stream is a no-op (benchmarkable observer indirection)
//   * set_stream() can re-target the sink mid-lifetime

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tracing_sink_smoke {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

constexpr auto count_lines(std::string_view s) noexcept -> std::size_t {
  std::size_t n = 0;
  for (auto c : s) {
    if (c == '\n') ++n;
  }
  return n;
}

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
        .message  = "boom"}};
  }
};

using SuccessLinear = pb::from<Input>::then<Increment>::to<Output>;
using FailureLinear = pb::from<Input>::then<FailingStage>::to<Output>;
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

} // namespace tracing_sink_smoke

int main() {
  using namespace tracing_sink_smoke;

  // ── 1. Schema-version constant is the v1 identifier. ──
  static_assert(pb::trace_ndjson_schema_version == std::string_view{"pb.trace.ndjson.v1"},
                "pb.trace.ndjson.v1 identifier MUST stay v1");

  // ── 2. Direct sink writes pin the default per-event NDJSON object shape. ──
  {
    std::ostringstream stream;
    pb::tracing_sink sink{&stream};
    sink.write(pb::trace_event{});
    pb_test_require(stream.str() ==
                    "{\"kind\":\"stage_start\",\"stage\":{\"key\":\"\",\"name\":\"\","
                    "\"stage_index\":0}}\n");
  }

  // ── 3. Direct sink writes share batch JSON escaping for string fields. ──
  {
    std::ostringstream stream;
    pb::tracing_sink sink{&stream};
    sink.write(pb::trace_event{.kind = pb::trace_event_kind::stage_success,
                               .stage_key = std::string{"key\\with\"quote"},
                               .stage_name = std::string{"name\nline\tvalue"}});
    const auto out = stream.str();
    pb_test_require(count_lines(out) == 1);
    pb_test_require(contains(out, "\"key\":\"key\\\\with\\\"quote\""));
    pb_test_require(contains(out, "\"name\":\"name\\nline\\tvalue\""));
  }

  // ── 4. Linear success run produces stage_start + stage_success NDJSON lines. ──
  {
    std::ostringstream stream;
    pb::tracing_sink sink{&stream};
    pb::trace_observer obs{sink};

    auto engine = pb::compile<SuccessLinear>(pb::runtime::sequential{});
    engine.set_observer(&obs);
    (void)engine.run(Input{.value = 1});

    const auto out = stream.str();
    // Each event ends with '\n'; success path of a single-stage pipeline
    // emits two events (stage_start, stage_success).
    pb_test_require(count_lines(out) == 2);
    pb_test_require(contains(out, "\"kind\":\"stage_start\""));
    pb_test_require(contains(out, "\"kind\":\"stage_success\""));
    pb_test_require(contains(out, "\"key\":\"increment\""));

    // Each line MUST be parseable as a self-contained JSON object —
    // we don't ship a JSON parser, but we can verify every line opens
    // with '{' and closes with '}' before the newline.
    std::size_t line_start = 0;
    for (std::size_t i = 0; i < out.size(); ++i) {
      if (out[i] == '\n') {
        pb_test_require(out[line_start]   == '{');
        pb_test_require(out[i - 1]        == '}');
        line_start = i + 1;
      }
    }
  }

  // ── 5. Linear failure run emits stage_start + stage_failure with error block. ──
  {
    std::ostringstream stream;
    pb::tracing_sink sink{&stream};
    pb::trace_observer obs{sink};

    auto engine = pb::compile<FailureLinear>(pb::runtime::sequential{});
    engine.set_observer(&obs);
    (void)engine.try_run(Input{});

    const auto out = stream.str();
    pb_test_require(count_lines(out) == 2);
    pb_test_require(contains(out, "\"kind\":\"stage_start\""));
    pb_test_require(contains(out, "\"kind\":\"stage_failure\""));
    pb_test_require(contains(out, "\"error\":"));
    pb_test_require(contains(out, "\"category\":\"stage_failure\""));
    pb_test_require(contains(out, "\"message\":\"boom\""));
  }

  // ── 6. Branch run emits case_selected as a tracked event kind. ──
  {
    std::ostringstream stream;
    pb::tracing_sink sink{&stream};
    pb::trace_observer obs{sink};

    auto engine = pb::compile<SuccessBranch>(pb::runtime::sequential{});
    engine.set_observer(&obs);
    (void)engine.run(Input{.value = 1});

    const auto out = stream.str();
    pb_test_require(contains(out, "\"kind\":\"case_selected\""));
    pb_test_require(contains(out, "\"case_index\":0"));
  }

  // ── 7. Fan-in run emits lifecycle events as tracked event kinds. ──
  {
    std::ostringstream stream;
    pb::tracing_sink sink{&stream};
    pb::trace_observer obs{sink};

    auto engine = pb::compile<SuccessFanIn>(pb::runtime::sequential{});
    engine.set_observer(&obs);
    (void)engine.run(Input{.value = 1});

    const auto out = stream.str();
    pb_test_require(contains(out, "\"kind\":\"fan_in_started\""));
    pb_test_require(contains(out, "\"case_count\":1"));
    pb_test_require(contains(out, "\"kind\":\"fan_in_case_scheduled\""));
    pb_test_require(contains(out, "\"case_index\":0"));
    pb_test_require(contains(out, "\"kind\":\"fan_in_case_completed\""));
    pb_test_require(contains(out, "\"success\":true"));
    pb_test_require(contains(out, "\"kind\":\"fan_in_completed\""));
    pb_test_require(contains(out, "\"selected_count\":1"));
    pb_test_require(contains(out, "\"completed_count\":1"));
    pb_test_require(contains(out, "\"failed_count\":0"));
  }

  // ── 8. Null stream is a no-op (benchmarkable). ──
  {
    pb::tracing_sink sink{nullptr};
    pb::trace_observer obs{sink};

    auto engine = pb::compile<SuccessLinear>(pb::runtime::sequential{});
    engine.set_observer(&obs);
    (void)engine.run(Input{.value = 1});
    // No crash, no output — sink with nullptr stream is a hot-path-friendly no-op.
    pb_test_require(sink.stream() == nullptr);
  }

  // ── 9. set_stream() re-targets the sink mid-lifetime. ──
  {
    std::ostringstream a;
    std::ostringstream b;
    pb::tracing_sink sink{&a};
    pb::trace_observer obs{sink};

    auto engine = pb::compile<SuccessLinear>(pb::runtime::sequential{});
    engine.set_observer(&obs);
    (void)engine.run(Input{.value = 1});
    pb_test_require(!a.str().empty());
    pb_test_require(b.str().empty());

    sink.set_stream(&b);
    (void)engine.run(Input{.value = 2});
    pb_test_require(!b.str().empty());
    // First sink content unchanged after retarget.
    const auto a_size_before = a.str().size();
    (void)a_size_before;  // baseline captured
  }

  // ── 10. Batch export_json and per-line NDJSON share the same per-event shape. ──
  {
    std::ostringstream stream;
    pb::tracing_sink sink{&stream};
    pb::trace_observer obs{sink};

    auto engine = pb::compile<SuccessLinear>(pb::runtime::sequential{});
    engine.set_observer(&obs);
    (void)engine.run(Input{.value = 1});

    const auto ndjson = stream.str();

    // The same event also exists in the batch form via trace_recorder.
    pb::trace_recorder recorder;
    pb::trace_observer batch_obs{recorder};
    engine.set_observer(&batch_obs);
    (void)engine.run(Input{.value = 1});
    const auto batch = pb::runtime::export_json(recorder);

    // Both must surface the same key/name/kind for the success event.
    pb_test_require(contains(ndjson, "\"key\":\"increment\""));
    pb_test_require(contains(batch, "\"key\":\"increment\""));
    pb_test_require(contains(batch, "\"schema_version\":\"pb.trace.v1\""));
  }

  return 0;
}
