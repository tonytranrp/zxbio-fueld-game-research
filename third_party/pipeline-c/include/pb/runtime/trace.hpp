#pragma once

#include <chrono>
#include <cstdint>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "pb/runtime/error.hpp"
#include "pb/runtime/observer.hpp"

namespace pb::runtime {

inline constexpr std::string_view trace_schema_version = "pb.trace.v1";

/// Streaming NDJSON trace-file format identifier.  The streaming
/// format differs from the batch JSON shape produced by
/// `pb::runtime::export_json(span<trace_event>)`: NDJSON writes one
/// JSON object per line, terminated by '\n', and is suitable for
/// tailing, line-grepping, and streaming consumers that can't
/// hold the full event list in memory.
///
/// The per-event JSON shape itself matches the batch format — both
/// share `detail::append_trace_event_json`.  Only the framing differs
/// (batch wraps events in a single `{"schema_version":...,"events":[...]}`
/// envelope; NDJSON has no envelope and emits one record per line).
inline constexpr std::string_view trace_ndjson_schema_version = "pb.trace.ndjson.v1";

enum class trace_event_kind : std::uint8_t {
  stage_start,
  stage_success,
  stage_failure,
  stage_exception,
  case_selected,
  case_skipped,
  case_failed,
  fan_in_started,
  fan_in_case_scheduled,
  fan_in_case_completed,
  fan_in_completed,
  pipeline_start,
  pipeline_complete,
};

struct trace_event {
  trace_event_kind kind{trace_event_kind::stage_start};
  std::string stage_key{};
  std::string stage_name{};
  std::size_t stage_index{};
  std::size_t case_index{};
  std::size_t case_count{};
  std::size_t selected_count{};
  std::size_t completed_count{};
  std::size_t failed_count{};
  bool success{};
  std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
  bool has_error{false};
  error diagnostic{};
};

struct trace_sink {
  virtual ~trace_sink() = default;
  virtual void write(const trace_event& event) = 0;
};

class trace_recorder final : public trace_sink {
public:
  void write(const trace_event& event) override { events_.push_back(event); }

  void clear() noexcept { events_.clear(); }

  [[nodiscard]] auto empty() const noexcept -> bool { return events_.empty(); }
  [[nodiscard]] auto size() const noexcept -> std::size_t { return events_.size(); }

  [[nodiscard]] auto events() const noexcept -> const std::vector<trace_event>& { return events_; }

private:
  std::vector<trace_event> events_{};
};

class trace_observer final : public observer {
public:
  explicit trace_observer(trace_sink& sink) noexcept : sink_(&sink) {}
  explicit trace_observer(trace_sink* sink) noexcept : sink_(sink) {}

  void set_sink(trace_sink* sink) noexcept { sink_ = sink; }

  [[nodiscard]] auto get_sink() const noexcept -> trace_sink* { return sink_; }

  void on_stage_start(const stage_id& stage) override {
    emit(trace_event{.kind = trace_event_kind::stage_start,
                     .stage_key = stage.key,
                     .stage_name = stage.name});
  }

  void on_stage_success(const stage_id& stage) override {
    emit(trace_event{.kind = trace_event_kind::stage_success,
                     .stage_key = stage.key,
                     .stage_name = stage.name});
  }

  void on_stage_failure(const stage_id& stage, const error& diagnostic) override {
    emit(trace_event{.kind = trace_event_kind::stage_failure,
                     .stage_key = stage.key,
                     .stage_name = stage.name,
                     .has_error = true,
                     .diagnostic = diagnostic});
  }

  void on_stage_exception(const stage_id& stage, const error& diagnostic) override {
    emit(trace_event{.kind = trace_event_kind::stage_exception,
                     .stage_key = stage.key,
                     .stage_name = stage.name,
                     .has_error = true,
                     .diagnostic = diagnostic});
  }

  void on_case_selected(const stage_id& branch_id, std::size_t case_index,
                        const stage_id& /*predicate_id*/) override {
    emit(trace_event{.kind = trace_event_kind::case_selected,
                     .stage_key = branch_id.key,
                     .stage_name = branch_id.name,
                     .case_index = case_index});
  }

  void on_case_skipped(const stage_id& branch_id, std::size_t case_index,
                       const stage_id& /*predicate_id*/) override {
    emit(trace_event{.kind = trace_event_kind::case_skipped,
                     .stage_key = branch_id.key,
                     .stage_name = branch_id.name,
                     .case_index = case_index});
  }

  void on_case_failed(const stage_id& branch_id, std::size_t case_index,
                      const stage_id& /*case_stage_id*/, const error& diagnostic) override {
    emit(trace_event{.kind = trace_event_kind::case_failed,
                     .stage_key = branch_id.key,
                     .stage_name = branch_id.name,
                     .case_index = case_index,
                     .has_error = true,
                     .diagnostic = diagnostic});
  }

  void on_fan_in_started(const stage_id& branch_id, std::size_t case_count) override {
    emit(trace_event{.kind = trace_event_kind::fan_in_started,
                     .stage_key = branch_id.key,
                     .stage_name = branch_id.name,
                     .case_count = case_count});
  }

  void on_fan_in_case_scheduled(const stage_id& branch_id, std::size_t case_index,
                                const stage_id& /*case_stage_id*/) override {
    emit(trace_event{.kind = trace_event_kind::fan_in_case_scheduled,
                     .stage_key = branch_id.key,
                     .stage_name = branch_id.name,
                     .case_index = case_index});
  }

  void on_fan_in_case_completed(const stage_id& branch_id, std::size_t case_index,
                                const stage_id& /*case_stage_id*/, bool success) override {
    emit(trace_event{.kind = trace_event_kind::fan_in_case_completed,
                     .stage_key = branch_id.key,
                     .stage_name = branch_id.name,
                     .case_index = case_index,
                     .success = success});
  }

  void on_fan_in_completed(const stage_id& branch_id, std::size_t selected_count,
                           std::size_t completed_count, std::size_t failed_count) override {
    emit(trace_event{.kind = trace_event_kind::fan_in_completed,
                     .stage_key = branch_id.key,
                     .stage_name = branch_id.name,
                     .selected_count = selected_count,
                     .completed_count = completed_count,
                     .failed_count = failed_count});
  }

private:
  void emit(const trace_event& event) {
    if (sink_ != nullptr) {
      sink_->write(event);
    }
  }

  trace_sink* sink_{nullptr};
};

[[nodiscard]] inline auto trace_event_kind_name(trace_event_kind kind) noexcept -> std::string_view {
  switch (kind) {
  case trace_event_kind::stage_start:
    return "stage_start";
  case trace_event_kind::stage_success:
    return "stage_success";
  case trace_event_kind::stage_failure:
    return "stage_failure";
  case trace_event_kind::stage_exception:
    return "stage_exception";
  case trace_event_kind::case_selected:
    return "case_selected";
  case trace_event_kind::case_skipped:
    return "case_skipped";
  case trace_event_kind::case_failed:
    return "case_failed";
  case trace_event_kind::fan_in_started:
    return "fan_in_started";
  case trace_event_kind::fan_in_case_scheduled:
    return "fan_in_case_scheduled";
  case trace_event_kind::fan_in_case_completed:
    return "fan_in_case_completed";
  case trace_event_kind::fan_in_completed:
    return "fan_in_completed";
  case trace_event_kind::pipeline_start:
    return "pipeline_start";
  case trace_event_kind::pipeline_complete:
    return "pipeline_complete";
  }
  return "unknown";
}

namespace detail {

inline void append_json_string(std::ostream& stream, std::string_view value) {
  stream << '"';
  for (const auto ch : value) {
    switch (ch) {
    case '"':
      stream << "\\\"";
      break;
    case '\\':
      stream << "\\\\";
      break;
    case '\b':
      stream << "\\b";
      break;
    case '\f':
      stream << "\\f";
      break;
    case '\n':
      stream << "\\n";
      break;
    case '\r':
      stream << "\\r";
      break;
    case '\t':
      stream << "\\t";
      break;
    default:
      stream << ch;
      break;
    }
  }
  stream << '"';
}

inline void append_stage_json(std::ostream& stream, const trace_event& event) {
  stream << "{\"key\":";
  append_json_string(stream, event.stage_key);
  stream << ",\"name\":";
  append_json_string(stream, event.stage_name);
  stream << ",\"stage_index\":" << event.stage_index;
  if (event.case_index != 0 ||
      event.kind == trace_event_kind::case_selected ||
      event.kind == trace_event_kind::case_skipped ||
      event.kind == trace_event_kind::case_failed ||
      event.kind == trace_event_kind::fan_in_case_scheduled ||
      event.kind == trace_event_kind::fan_in_case_completed) {
    stream << ",\"case_index\":" << event.case_index;
  }
  stream << '}';
}

inline void append_error_json(std::ostream& stream, const error& diagnostic) {
  stream << "{\"stage\":{\"key\":";
  append_json_string(stream, diagnostic.stage.key);
  stream << ",\"name\":";
  append_json_string(stream, diagnostic.stage.name);
  stream << "},\"category\":";
  append_json_string(stream, category_name(diagnostic.category));
  stream << ",\"message\":";
  append_json_string(stream, diagnostic.message);
  stream << '}';
}

inline void append_trace_event_json(std::ostream& stream, const trace_event& event) {
  stream << "{\"kind\":";
  append_json_string(stream, trace_event_kind_name(event.kind));
  stream << ",\"stage\":";
  append_stage_json(stream, event);
  if (event.kind == trace_event_kind::fan_in_started) {
    stream << ",\"case_count\":" << event.case_count;
  } else if (event.kind == trace_event_kind::fan_in_case_completed) {
    stream << ",\"success\":" << (event.success ? "true" : "false");
  } else if (event.kind == trace_event_kind::fan_in_completed) {
    stream << ",\"selected_count\":" << event.selected_count
           << ",\"completed_count\":" << event.completed_count
           << ",\"failed_count\":" << event.failed_count;
  }
  if (event.has_error) {
    stream << ",\"error\":";
    append_error_json(stream, event.diagnostic);
  }
  stream << '}';
}

} // namespace detail

[[nodiscard]] inline auto export_json(std::span<const trace_event> events) -> std::string {
  std::ostringstream stream;
  stream << "{\"schema_version\":";
  detail::append_json_string(stream, trace_schema_version);
  stream << ",\"events\":[";
  for (std::size_t index = 0; index < events.size(); ++index) {
    if (index != 0) {
      stream << ',';
    }
    detail::append_trace_event_json(stream, events[index]);
  }
  stream << "]}";
  return stream.str();
}

[[nodiscard]] inline auto export_json(const trace_recorder& recorder) -> std::string {
  return export_json(std::span<const trace_event>{recorder.events().data(), recorder.events().size()});
}

/// Streaming NDJSON trace sink.  Writes one JSON object per event,
/// each terminated by '\n', to a `std::ostream` (file, stringstream,
/// std::clog).  Suitable for tailing logs in real time, line-grepping,
/// and forwarding events to external trace collectors without buffering.
///
/// Schema: `pb.trace.ndjson.v1` (see `trace_ndjson_schema_version`).
/// Per-event JSON shape is identical to the batch format produced by
/// `export_json(span<trace_event>)` — the difference is purely framing.
///
/// Pair with `trace_observer` to capture pipeline events:
///
/// @code
///   std::ofstream file{"trace.ndjson"};
///   pb::tracing_sink sink{&file};
///   pb::trace_observer obs{sink};
///   engine.set_observer(&obs);
///   engine.run(input);
/// @endcode
///
/// Passing `nullptr` makes the sink a no-op — useful for benchmarking
/// the cost of the observer indirection itself.  The sink does not
/// flush — that's the ostream's responsibility (e.g. via line buffering
/// or explicit `std::flush`).  The sink is not thread-safe; for the
/// thread-pool backend, fan-in events are forwarded under the runtime's
/// own synchronization before reaching the sink.
class tracing_sink final : public trace_sink {
public:
  tracing_sink() = default;
  explicit tracing_sink(std::ostream* stream) noexcept : stream_{stream} {}

  void set_stream(std::ostream* stream) noexcept { stream_ = stream; }
  [[nodiscard]] auto stream() const noexcept -> std::ostream* { return stream_; }

  void write(const trace_event& event) override {
    if (stream_ == nullptr) {
      return;
    }
    std::ostringstream line;
    detail::append_trace_event_json(line, event);
    *stream_ << line.str() << '\n';
  }

private:
  std::ostream* stream_{nullptr};
};

} // namespace pb::runtime
