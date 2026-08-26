#pragma once

#include <ostream>
#include <string>
#include <string_view>

namespace pb::runtime {

/// Stable schema-version identifier for the JSON shape produced by
/// `pb::runtime::to_json(const error&)` and
/// `pb::runtime::to_json(const error_record&)`.  Bumped only when
/// the JSON field layout changes in a way that breaks downstream
/// consumers.  See docs/error-schema-v1.md for the formal contract.
inline constexpr std::string_view error_schema_version = "pb.error.v1";

enum class error_category {
  stage_failure,
  expected_error,
  exception,
  contract_violation,
};

struct stage_id {
  std::string key{};
  std::string name{};
};

struct error {
  stage_id stage{};
  error_category category{error_category::stage_failure};
  std::string message{};
};

struct error_record {
  std::string stage_key{};
  std::string stage_name{};
  std::string category{};
  std::string message{};
};

[[nodiscard]] auto category_name(error_category category) noexcept -> std::string_view;
[[nodiscard]] auto has_category(const error& value, error_category category) noexcept -> bool;
[[nodiscard]] auto has_stage(const stage_id& stage) noexcept -> bool;
[[nodiscard]] auto has_stage(const error& value) noexcept -> bool;
[[nodiscard]] auto has_message(const error& value) noexcept -> bool;
[[nodiscard]] auto to_record(const error& value) -> error_record;
[[nodiscard]] auto describe(const stage_id& stage) -> std::string;
[[nodiscard]] auto describe(const error& value) -> std::string;

/// Serialize a runtime error (or its `error_record` projection) as a
/// JSON object conforming to `pb.error.v1`.  The shape is:
///
/// ```json
/// {
///   "schema_version": "pb.error.v1",
///   "stage": { "key": "<stage_key>", "name": "<stage_name>" },
///   "category": "stage_failure|expected_error|exception|contract_violation",
///   "message":  "<free-form text>"
/// }
/// ```
///
/// String fields are JSON-escaped: `"` → `\"`, `\` → `\\`, control
/// bytes 0x00-0x1F are emitted as `\u00xx` (except the named escapes
/// `\b \f \n \r \t`).  The shape is regression-locked in
/// `tests/runtime/error_json_serialization_smoke.cpp`.
[[nodiscard]] auto to_json(const error& value) -> std::string;
[[nodiscard]] auto to_json(const error_record& value) -> std::string;

auto operator<<(std::ostream& stream, error_category category) -> std::ostream&;
auto operator<<(std::ostream& stream, const stage_id& stage) -> std::ostream&;
auto operator<<(std::ostream& stream, const error& value) -> std::ostream&;

} // namespace pb::runtime
