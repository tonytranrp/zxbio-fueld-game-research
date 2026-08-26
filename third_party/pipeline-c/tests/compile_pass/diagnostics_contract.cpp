#include <pb/core/diagnostics.hpp>

#include <cstdlib>
#include <cstddef>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>


namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}
}  // namespace

// ── Schema version ────────────────────────────────────────────────────────

static_assert(pb::diagnostics::diagnostics_schema_version == "pb.diagnostics.v1",
              "Schema version must match pb.diagnostics.v1 for machine consumers");

// ── severity enum ─────────────────────────────────────────────────────────

static_assert(std::is_enum_v<pb::diagnostics::severity>,
              "severity must be an enum");
static_assert(sizeof(pb::diagnostics::severity) == sizeof(std::uint8_t),
              "severity must pack into std::uint8_t for stable serialisation");

// Verify each enumerator is distinct
static_assert(static_cast<int>(pb::diagnostics::severity::error) !=
                  static_cast<int>(pb::diagnostics::severity::warning),
              "error and warning must be distinct severity values");
static_assert(static_cast<int>(pb::diagnostics::severity::warning) !=
                  static_cast<int>(pb::diagnostics::severity::info),
              "warning and info must be distinct severity values");
static_assert(static_cast<int>(pb::diagnostics::severity::info) !=
                  static_cast<int>(pb::diagnostics::severity::suggestion),
              "info and suggestion must be distinct severity values");

// ── diagnostic_record — aggregate / default constructibility ──────────────

static_assert(std::is_default_constructible_v<pb::diagnostics::diagnostic_record>,
              "diagnostic_record must be default-constructible");
static_assert(std::is_copy_constructible_v<pb::diagnostics::diagnostic_record>,
              "diagnostic_record must be copyable for vector storage");
static_assert(std::is_move_constructible_v<pb::diagnostics::diagnostic_record>,
              "diagnostic_record must be movable");

// ── diagnostic_collector — basic operations ───────────────────────────────

static_assert(std::is_default_constructible_v<pb::diagnostics::diagnostic_collector>,
              "diagnostic_collector must be default-constructible");

// ── suggestions namespace ─────────────────────────────────────────────────

static_assert(!pb::diagnostics::suggestions::join_requires_branch.empty(),
              "join_requires_branch suggestion must be defined");
static_assert(!pb::diagnostics::suggestions::type_mismatch.empty(),
              "type_mismatch suggestion must be defined");

// Verify suggestions are string_view (lightweight, no allocation)
static_assert(std::is_same_v<decltype(pb::diagnostics::suggestions::join_requires_branch),
                             const std::string_view>,
              "suggestions must be const std::string_view");
static_assert(std::is_same_v<decltype(pb::diagnostics::suggestions::type_mismatch),
                             const std::string_view>,
              "suggestions must be const std::string_view");

// ── Runtime verification ──────────────────────────────────────────────────

int main() {
  // --- diagnostic_record designated-initializer construction ---
  // (std::string fields preclude constexpr, so we test at runtime)
  {
    pb::diagnostics::diagnostic_record rec{
        .level = pb::diagnostics::severity::error,
        .code = "PB-TEST-001",
        .message = "test diagnostic",
        .file = "diagnostics_contract.cpp",
        .line = 42,
        .column = 7,
        .stage_key = "parse",
        .suggestion = "Check input type",
    };

    pb_test_require(rec.level == pb::diagnostics::severity::error);
    pb_test_require(rec.code == "PB-TEST-001");
    pb_test_require(rec.message == "test diagnostic");
    pb_test_require(rec.file == "diagnostics_contract.cpp");
    pb_test_require(rec.line == 42);
    pb_test_require(rec.column == 7);
    pb_test_require(rec.stage_key == "parse");
    pb_test_require(rec.suggestion == "Check input type");
  }

  // --- collector::add with source_location ---
  pb::diagnostics::diagnostic_collector collector;
  collector.add(pb::diagnostics::severity::warning,
                "PB-WARN-001",
                "something looks suspicious");

  pb_test_require(collector.records.size() == 1);
  const auto& rec = collector.records[0];
  pb_test_require(rec.level == pb::diagnostics::severity::warning);
  pb_test_require(rec.code == "PB-WARN-001");
  pb_test_require(rec.message == "something looks suspicious");

  // source_location should capture this file's name
  pb_test_require(rec.file.find("diagnostics_contract.cpp") != std::string::npos);
  pb_test_require(rec.line > 0);    // line must be populated
  pb_test_require(rec.column > 0);  // column must be populated

  // Empty stage_key and suggestion by default
  pb_test_require(rec.stage_key.empty());
  pb_test_require(rec.suggestion.empty());

  // --- collector::add with stage_key and suggestion ---
  pb::diagnostics::diagnostic_collector collector2;
  {
    auto rec2 = pb::diagnostics::diagnostic_record{
        .level = pb::diagnostics::severity::error,
        .code = "PB-EDGE-001",
        .message = "Pipeline edge mismatch",
        .file = {},
        .line = {},
        .column = {},
        .stage_key = "normalize",
        .suggestion = std::string{pb::diagnostics::suggestions::type_mismatch},
    };
    collector2.records.push_back(std::move(rec2));
  }

  pb_test_require(collector2.records.size() == 1);
  const auto& rec2 = collector2.records[0];
  pb_test_require(rec2.level == pb::diagnostics::severity::error);
  pb_test_require(rec2.code == "PB-EDGE-001");
  pb_test_require(rec2.stage_key == "normalize");
  pb_test_require(rec2.suggestion == pb::diagnostics::suggestions::type_mismatch);

  // --- multiple records ---
  collector.add(pb::diagnostics::severity::info,
                "PB-INFO-001",
                "pipeline constructed successfully");
  collector.add(pb::diagnostics::severity::suggestion,
                "PB-SUGG-001",
                "consider adding a sink stage");

  pb_test_require(collector.records.size() == 3);

  return 0;
}
