#include <pb/runtime/trace.hpp>

#include <sstream>
#include <string>
#include <string_view>

static_assert(pb::runtime::trace_schema_version == std::string_view{"pb.trace.v1"});
static_assert(pb::runtime::trace_ndjson_schema_version ==
              std::string_view{"pb.trace.ndjson.v1"});
int pb_public_header_runtime_trace() {
  pb::runtime::trace_recorder recorder{};
  pb::runtime::trace_observer observer{recorder};
  if (pb::runtime::trace_event_kind_name(pb::runtime::trace_event_kind::stage_start) !=
      std::string_view{"stage_start"}) {
    return 1;
  }
  if (pb::runtime::trace_event_kind_name(pb::runtime::trace_event_kind::fan_in_completed) !=
      std::string_view{"fan_in_completed"}) {
    return 2;
  }
  if (observer.get_sink() != &recorder) {
    return 3;
  }

  observer.set_sink(nullptr);
  if (observer.get_sink() != nullptr) {
    return 4;
  }

  observer.set_sink(&recorder);
  observer.on_stage_start(pb::runtime::stage_id{.key = "stage", .name = "stage"});
  if (recorder.empty() || recorder.size() != 1) {
    return 5;
  }

  const auto json = pb::runtime::export_json(recorder);
  if (json.find("\"schema_version\":\"pb.trace.v1\"") == std::string::npos) {
    return 6;
  }

  std::ostringstream stream;
  pb::runtime::tracing_sink sink{&stream};
  sink.write(pb::runtime::trace_event{});
  if (sink.stream() != &stream || stream.str().empty()) {
    return 7;
  }
  sink.set_stream(nullptr);
  return sink.stream() == nullptr ? 0 : 8;
}
