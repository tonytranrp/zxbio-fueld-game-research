#pragma once

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace pb::diagnostics {

/// Schema version string for machine-readable consumers.
/// Bump the version when the diagnostic_record layout changes in an
/// incompatible way so that tooling can detect schema drift early.
inline constexpr std::string_view diagnostics_schema_version = "pb.diagnostics.v1";

/// Severity level for a diagnostic record.
/// Encoded as std::uint8_t so it packs tightly in vectors and can be
/// serialised without platform-dependent enum size variance.
enum class severity : std::uint8_t {
  error,       ///< Hard failure — the pipeline cannot be constructed or run.
  warning,     ///< Suspicious but legal — the pipeline will still execute.
  info,        ///< Informational — stage names, key assignments, topology.
  suggestion,  ///< Proposed fix — emitted alongside an error to guide the user.
};

/// Single machine-readable diagnostic record.
///
/// Every record carries a structured error code, a human-readable message,
/// source-location information, and an optional per-stage key for filtering
/// diagnostics by pipeline stage.  Consumers (IDEs, CI loggers, JSON exporters)
/// can rely on the field order and types.
struct diagnostic_record {
  severity level{severity::error};
  std::string code;
  std::string message;
  std::string file;
  std::size_t line{};
  std::size_t column{};
  std::string stage_key;
  std::string suggestion;
};

/// Growable diagnostic collector for runtime use.
///
/// Accumulates diagnostic_record entries and provides a convenience ::add()
/// that fills source_location automatically.  The records vector can be
/// iterated, serialised, or drained after pipeline construction or execution.
struct diagnostic_collector {
  std::vector<diagnostic_record> records;

  /// Append a diagnostic record.
  ///
  /// \param level   severity classification.
  /// \param code    machine-readable error / warning code (e.g. "PB-EDGE-001").
  /// \param message human-readable description.
  /// \param loc     captured via std::source_location::current() by default.
  void add(severity level, std::string code, std::string message,
           std::source_location loc = std::source_location::current()) {
    records.push_back(diagnostic_record{
        .level = level,
        .code = std::move(code),
        .message = std::move(message),
        .file = loc.file_name(),
        .line = loc.line(),
        .column = loc.column(),
        .stage_key = {},
        .suggestion = {},
    });
  }
};

} // namespace pb::diagnostics

/// Suggested-fix strings for common pipeline construction errors.
///
/// Each constant is a human-readable hint that tooling can attach to a
/// diagnostic_record::suggestion field.  The suggestion text is stable
/// and may be relied upon by downstream consumers (IDE quick-fix,
/// documentation, etc.).
namespace pb::diagnostics::suggestions {

inline constexpr std::string_view join_requires_branch =
    "Add ::branch<CaseA, CaseB> before ::join<JoinStage>";

inline constexpr std::string_view type_mismatch =
    "Check that previous output_type matches next stage input_type";

} // namespace pb::diagnostics::suggestions
