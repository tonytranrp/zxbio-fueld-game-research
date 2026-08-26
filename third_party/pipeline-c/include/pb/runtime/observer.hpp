#pragma once

#include <cstddef>
#include <string_view>

#include "pb/runtime/error.hpp"

namespace pb::runtime {

/// Observer ABI version identifier.  Bumped only when the observer
/// vtable shape — method names, signatures, or count — changes in a
/// way that breaks ABI for derived observer implementations.  Adding
/// a new virtual method with a default no-op body is NOT a bump.
/// Removing a method, renaming one, changing a signature, or
/// reordering the vtable IS a bump.
///
/// Downstream consumers (custom observers) SHOULD pin against this
/// identifier in their build system or static_assert to detect drift.
inline constexpr std::string_view observer_schema_version = "pb.observer.v1";

/// Base observer for pipeline lifecycle events.
///
/// All virtual methods are non-pure with default no-op bodies so a
/// derived observer only overrides the events it cares about.  The
/// default no-op bodies are part of the v1 ABI promise — removing
/// them would require a v2 bump.
///
/// v1 event surface (locked):
///
///   on_stage_start          (stage_id)
///   on_stage_success        (stage_id)
///   on_stage_failure        (stage_id, error)
///   on_stage_exception      (stage_id, error)
///   on_case_selected        (branch_id, case_index, predicate_id)
///   on_case_skipped         (branch_id, case_index, predicate_id)
///   on_case_failed          (branch_id, case_index, case_stage_id, diagnostic)
///   on_fan_in_started       (branch_id, case_count)
///   on_fan_in_case_scheduled(branch_id, case_index, case_stage_id)
///   on_fan_in_case_completed(branch_id, case_index, case_stage_id, success)
///   on_fan_in_completed     (branch_id, selected_count, completed_count, failed_count)
///
/// The four `on_fan_in_*` methods are additive lifecycle hooks layered
/// on top of the original v1 surface; they carry no-op default bodies
/// and so do NOT bump the ABI schema (existing derived observers stay
/// source- and ABI-compatible).  They bracket a fan-in branch's
/// execution on both the sequential and thread-pool backends:
///
///   started  → emitted once before any case predicate is evaluated,
///              reporting the total number of cases that will be
///              considered (`case_count`).
///   scheduled→ emitted once per case whose predicate selected it,
///              immediately before its branch stage begins running
///              (so on the thread-pool backend it precedes the actual
///              task dispatch).  Always paired with on_case_selected.
///   completed→ emitted once per scheduled case after its branch stage
///              finishes, reporting `success` (false on stage failure
///              or exception).  Always paired with a matching scheduled.
///   completed (fan-in) → emitted once after the whole branch has been
///              aggregated, reporting how many cases were `selected`
///              (predicate matched), how many `completed` successfully,
///              and how many `failed` (selected-but-errored plus
///              predicate failures).
///
/// Ordering for a single branch is therefore:
///   started → N×scheduled → N×case_completed → completed
/// where N == selected_count.  On the thread-pool backend the
/// per-case events are serialized through the synchronized observer
/// wrapper, so the scheduled/case_completed events for distinct cases
/// may interleave, but each case's scheduled always precedes its own
/// case_completed.
///
/// See docs/observer-schema-v1.md for the formal ABI contract.
struct observer {
  virtual ~observer() = default;

  virtual void on_stage_start(const stage_id&) {}
  virtual void on_stage_success(const stage_id&) {}
  virtual void on_stage_failure(const stage_id&, const error&) {}
  virtual void on_stage_exception(const stage_id&, const error&) {}
  virtual void on_case_selected(const stage_id& /*branch_id*/, std::size_t /*case_index*/,
                                const stage_id& /*predicate_id*/) {}
  virtual void on_case_skipped(const stage_id& /*branch_id*/, std::size_t /*case_index*/,
                               const stage_id& /*predicate_id*/) {}
  virtual void on_case_failed(const stage_id& /*branch_id*/, std::size_t /*case_index*/,
                              const stage_id& /*case_stage_id*/, const error& /*diagnostic*/) {}

  /// Emitted once before a fan-in branch evaluates any case.
  virtual void on_fan_in_started(const stage_id& /*branch_id*/, std::size_t /*case_count*/) {}

  /// Emitted when a selected case's branch stage is about to run.
  virtual void on_fan_in_case_scheduled(const stage_id& /*branch_id*/, std::size_t /*case_index*/,
                                        const stage_id& /*case_stage_id*/) {}

  /// Emitted after a scheduled case's branch stage finishes.
  virtual void on_fan_in_case_completed(const stage_id& /*branch_id*/, std::size_t /*case_index*/,
                                        const stage_id& /*case_stage_id*/, bool /*success*/) {}

  /// Emitted once after the whole fan-in branch has been aggregated.
  virtual void on_fan_in_completed(const stage_id& /*branch_id*/, std::size_t /*selected_count*/,
                                   std::size_t /*completed_count*/, std::size_t /*failed_count*/) {}
};

/// Verbose event-line schema identifier.  The `[pb.verbose] <event>
/// <field>=<value>` line format produced by `pb::verbose_observer` is
/// a stable text contract.  Downstream log-parsing tools can rely on
/// the v1 line schema documented in docs/observer-schema-v1.md.
///
/// The schema is independent from the observer-ABI schema above: a
/// non-ABI-breaking event re-ordering (e.g. swapping field order on a
/// line) still bumps this constant.
inline constexpr std::string_view verbose_observer_schema_version = "pb.observer.verbose.v1";

} // namespace pb::runtime
