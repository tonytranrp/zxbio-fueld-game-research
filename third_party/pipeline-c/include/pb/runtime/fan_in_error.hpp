#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "pb/core/pipeline_state.hpp"
#include "pb/runtime/error.hpp"

namespace pb {

/// Stable schema-version identifier for the multi-line text rendered by
/// `fan_in_error_envelope::to_string()` / `::format()`.  Bumped only when the
/// rendered line layout changes in a way that breaks downstream consumers.
inline constexpr std::string_view fan_in_error_schema_version = "pb.fan_in.errors.v1";

/// One failed case from a fan-in run, captured as a structured record.
///
/// `case_index` is the declaration-order index of the branch case inside the
/// fan-in aggregate.  `stage` is the stage identity attributed to the failure
/// (synthesized from the case index when the aggregate does not carry an
/// explicit per-case `stage_id`).  `diagnostic` is the full runtime error,
/// reconstructed from the per-case diagnostic string with category
/// `error_category::stage_failure`.
struct fan_in_case_error {
  std::size_t case_index{};
  pb::runtime::stage_id stage{};
  pb::runtime::error diagnostic{};
};

/// Structured collection of every failed-case error gathered from a single
/// fan-in run.
///
/// Unlike the per-case diagnostic string carried on each
/// `pb::fan_in_case_result`, an envelope aggregates *all* failures from one run
/// into a single object that can be iterated, indexed, and rendered as a stable
/// text report.  Records are stored in branch-declaration order.
///
/// The `to_string()` / `format()` rendering conforms to the
/// `pb.fan_in.errors.v1` schema:
///
/// ```text
/// pb.fan_in.errors.v1
/// failures=<count>
/// case[<case_index>] stage=<stage-describe> :: <error-describe>
/// case[<case_index>] stage=<stage-describe> :: <error-describe>
/// ...
/// ```
///
/// * The first line is always the literal schema version.
/// * The second line is always `failures=<N>` where `<N>` is `size()`.
/// * Each subsequent line describes one failed case, in declaration order,
///   using `pb::runtime::describe(const stage_id&)` for the stage segment and
///   `pb::runtime::describe(const error&)` for the diagnostic segment.
/// * Lines are separated by a single `'\n'`; there is no trailing newline.
///
/// An empty envelope renders exactly two lines (`pb.fan_in.errors.v1` and
/// `failures=0`).
class fan_in_error_envelope {
public:
  using value_type = fan_in_case_error;
  using container_type = std::vector<fan_in_case_error>;
  using const_iterator = container_type::const_iterator;
  using iterator = container_type::const_iterator;
  using size_type = container_type::size_type;

  fan_in_error_envelope() = default;

  /// Append one failed-case record, preserving insertion order.
  void push(fan_in_case_error record) { errors_.push_back(std::move(record)); }

  [[nodiscard]] auto size() const noexcept -> size_type { return errors_.size(); }
  [[nodiscard]] auto empty() const noexcept -> bool { return errors_.empty(); }

  /// Whether this envelope collected at least one failed case.
  [[nodiscard]] auto has_failures() const noexcept -> bool { return !errors_.empty(); }

  [[nodiscard]] auto begin() const noexcept -> const_iterator { return errors_.begin(); }
  [[nodiscard]] auto end() const noexcept -> const_iterator { return errors_.end(); }
  [[nodiscard]] auto cbegin() const noexcept -> const_iterator { return errors_.cbegin(); }
  [[nodiscard]] auto cend() const noexcept -> const_iterator { return errors_.cend(); }

  [[nodiscard]] auto operator[](size_type index) const -> const fan_in_case_error& { return errors_[index]; }
  [[nodiscard]] auto at(size_type index) const -> const fan_in_case_error& { return errors_.at(index); }

  /// Read-only access to the underlying record sequence.
  [[nodiscard]] auto records() const noexcept -> const container_type& { return errors_; }

  /// Render the envelope as a stable multi-line `pb.fan_in.errors.v1` report.
  /// See the class documentation for the exact line layout.
  [[nodiscard]] auto format() const -> std::string {
    std::string out;
    out.append(fan_in_error_schema_version);
    out.push_back('\n');
    out.append("failures=");
    out.append(std::to_string(errors_.size()));
    for (const auto& record : errors_) {
      out.push_back('\n');
      out.append("case[");
      out.append(std::to_string(record.case_index));
      out.append("] stage=");
      out.append(pb::runtime::describe(record.stage));
      out.append(" :: ");
      out.append(pb::runtime::describe(record.diagnostic));
    }
    return out;
  }

  /// Alias for `format()`; renders the `pb.fan_in.errors.v1` report.
  [[nodiscard]] auto to_string() const -> std::string { return format(); }

private:
  container_type errors_{};
};

namespace detail {

/// Synthesize a stable stage identity for a fan-in case from its index when the
/// aggregate carries no explicit per-case `stage_id`.  Key form
/// `fan_in.case.<N>`, name form `case <N>`.
[[nodiscard]] inline auto fan_in_case_stage_id(std::size_t case_index) -> pb::runtime::stage_id {
  auto suffix = std::to_string(case_index);
  return pb::runtime::stage_id{.key = "fan_in.case." + suffix, .name = "case " + suffix};
}

template <std::size_t Index, class... CaseResults>
void collect_fan_in_case(const pb::core::fan_in_results<CaseResults...>& aggregate,
                         fan_in_error_envelope& envelope) {
  const auto& case_result = aggregate.template get<Index>();
  if (!case_result.failed()) {
    return;
  }
  const std::size_t case_index = case_result.index;
  auto stage = fan_in_case_stage_id(case_index);
  pb::runtime::error diagnostic{.stage = stage,
                                .category = pb::runtime::error_category::stage_failure,
                                .message = std::string{case_result.diagnostic_message()}};
  envelope.push(fan_in_case_error{.case_index = case_index,
                                  .stage = std::move(stage),
                                  .diagnostic = std::move(diagnostic)});
}

template <class... CaseResults, std::size_t... Indexes>
void collect_fan_in_errors_impl(const pb::core::fan_in_results<CaseResults...>& aggregate,
                                fan_in_error_envelope& envelope, std::index_sequence<Indexes...>) {
  (collect_fan_in_case<Indexes>(aggregate, envelope), ...);
}

} // namespace detail

/// Walk a fan-in aggregate and gather every failed case into a structured
/// `fan_in_error_envelope`, preserving branch-declaration order.
///
/// This is a pure read-only consumer of an existing
/// `pb::fan_in_results<...>` produced by a fan-in run: it inspects each case
/// result's `failed()` flag, captures the case index, a synthesized stage
/// identity, and the per-case diagnostic (lifted into a
/// `pb::runtime::error`), and appends a `fan_in_case_error` for every failure.
/// Skipped and completed cases contribute nothing.  The aggregate is never
/// mutated.
template <class... CaseResults>
[[nodiscard]] auto collect_fan_in_errors(const pb::core::fan_in_results<CaseResults...>& aggregate)
    -> fan_in_error_envelope {
  fan_in_error_envelope envelope;
  detail::collect_fan_in_errors_impl(aggregate, envelope,
                                     std::make_index_sequence<sizeof...(CaseResults)>{});
  return envelope;
}

} // namespace pb
