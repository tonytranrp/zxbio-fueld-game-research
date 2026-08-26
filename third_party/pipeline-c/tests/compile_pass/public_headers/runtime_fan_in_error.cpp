#include <pb/runtime/fan_in_error.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

static_assert(pb::fan_in_error_schema_version == std::string_view{"pb.fan_in.errors.v1"});
static_assert(std::is_same_v<decltype(pb::fan_in_case_error::case_index), std::size_t>);
static_assert(std::is_same_v<decltype(pb::fan_in_case_error::stage), pb::runtime::stage_id>);
static_assert(std::is_same_v<decltype(pb::fan_in_case_error::diagnostic), pb::runtime::error>);

using Aggregate = pb::core::fan_in_results<pb::core::fan_in_case_result<0, int>>;
static_assert(
    std::is_same_v<decltype(pb::collect_fan_in_errors(std::declval<const Aggregate&>())),
                   pb::fan_in_error_envelope>);

int pb_public_header_runtime_fan_in_error() {
  using Aggregate = pb::core::fan_in_results<pb::core::fan_in_case_result<0, int>,
                                             pb::core::fan_in_case_result<1, int>>;
  Aggregate aggregate;
  aggregate.get<0>().mark_completed(42);
  aggregate.get<1>().mark_failed("fan-in failed");

  auto envelope = pb::collect_fan_in_errors(aggregate);
  const auto& records = envelope.records();
  const auto first = envelope.cbegin();
  const bool accessors_visible = envelope.has_failures() && !envelope.empty() && envelope.size() == 1 &&
                                 envelope.begin() != envelope.end() && first != envelope.cend() &&
                                 &envelope.at(0) == &envelope[0] && &records.front() == &envelope[0];
  const bool shape_stable = envelope[0].case_index == 1 &&
                            envelope[0].stage.key == "fan_in.case.1" &&
                            envelope[0].stage.name == "case 1" &&
                            envelope[0].diagnostic.category == pb::runtime::error_category::stage_failure &&
                            envelope[0].diagnostic.message == "fan-in failed";
  return accessors_visible && shape_stable && !envelope.to_string().empty() ? 0 : 1;
}
