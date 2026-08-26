// pb.error.v1 JSON serialization regression.
//
// Pins the JSON shape produced by pb::runtime::to_json(const error&)
// and pb::runtime::to_json(const error_record&) against the schema
// documented in include/pb/runtime/error.hpp.
//
// Covers:
//   * Schema-version identifier embedded as the first field
//   * Field order: schema_version, stage, category, message
//   * All four documented error_category enumerator names
//   * JSON escape behavior: quotes, backslash, control characters,
//     named escapes (\b \f \n \r \t)
//   * to_json(error) == to_json(to_record(error)) parity
//   * Empty stage / empty message edge cases

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <string>
#include <string_view>

namespace error_json {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

} // namespace error_json

int main() {
  using namespace error_json;

  // ── 1. Schema-version literal and field order on a populated error. ──
  {
    const pb::runtime::error e{
        .stage = {.key = "stage_key", .name = "stage_name"},
        .category = pb::runtime::error_category::stage_failure,
        .message = "boom"};
    const auto json = pb::runtime::to_json(e);

    pb_test_require(json.rfind("{\"schema_version\":\"pb.error.v1\"", 0) == 0);

    const auto sv      = json.find("\"schema_version\"");
    const auto stage   = json.find("\"stage\"");
    const auto cat     = json.find("\"category\"");
    const auto message = json.find("\"message\"");

    pb_test_require(sv      != std::string::npos);
    pb_test_require(stage   != std::string::npos);
    pb_test_require(cat     != std::string::npos);
    pb_test_require(message != std::string::npos);
    pb_test_require(sv      < stage);
    pb_test_require(stage   < cat);
    pb_test_require(cat     < message);

    // Embedded stage shape: { "key": "...", "name": "..." }
    pb_test_require(contains(json, "\"stage\":{\"key\":\"stage_key\",\"name\":\"stage_name\"}"));
    pb_test_require(contains(json, "\"category\":\"stage_failure\""));
    pb_test_require(contains(json, "\"message\":\"boom\""));
  }

  // ── 2. All four error_category enumerator names round-trip via to_json. ──
  {
    using Cat = pb::runtime::error_category;
    const struct {
      Cat category;
      std::string_view literal;
    } cases[] = {
        {Cat::stage_failure,      "\"category\":\"stage_failure\""},
        {Cat::expected_error,     "\"category\":\"expected_error\""},
        {Cat::exception,          "\"category\":\"exception\""},
        {Cat::contract_violation, "\"category\":\"contract_violation\""},
    };

    for (const auto& tc : cases) {
      pb::runtime::error e{};
      e.category = tc.category;
      const auto json = pb::runtime::to_json(e);
      pb_test_require(contains(json, tc.literal));
    }
  }

  // ── 3. JSON escaping: quotes, backslash, named control escapes. ──
  {
    const pb::runtime::error e{
        .stage = {.key = "key", .name = "name"},
        .category = pb::runtime::error_category::stage_failure,
        .message = std::string{"quote:\"backslash:\\newline:\nreturn:\rtab:\tbell:\x07"}};
    const auto json = pb::runtime::to_json(e);

    pb_test_require(contains(json, "quote:\\\""));     // quote escaped
    pb_test_require(contains(json, "backslash:\\\\")); // backslash escaped
    pb_test_require(contains(json, "newline:\\n"));    // LF named escape
    pb_test_require(contains(json, "return:\\r"));     // CR named escape
    pb_test_require(contains(json, "tab:\\t"));        // tab named escape
    pb_test_require(contains(json, "bell:\\u0007"));   // BEL as \u00xx
  }

  // ── 4. to_json(error) and to_json(to_record(error)) MUST be identical. ──
  {
    const pb::runtime::error e{
        .stage = {.key = "rec_key", .name = "rec_name"},
        .category = pb::runtime::error_category::exception,
        .message = "oops"};
    const auto via_error  = pb::runtime::to_json(e);
    const auto via_record = pb::runtime::to_json(pb::runtime::to_record(e));
    pb_test_require(via_error == via_record);
  }

  // ── 5. Escaped stage identity still round-trips through to_record/to_json. ──
  {
    const pb::runtime::error e{
        .stage = {.key = "key:\"slash\\line\n", .name = "name\t\"quoted\""},
        .category = pb::runtime::error_category::contract_violation,
        .message = "msg:\n\t\"\\end"};
    const auto record = pb::runtime::to_record(e);
    pb_test_require(record.stage_key == "key:\"slash\\line\n");
    pb_test_require(record.stage_name == "name\t\"quoted\"");
    pb_test_require(record.category == "contract_violation");
    pb_test_require(record.message == "msg:\n\t\"\\end");

    const auto via_error  = pb::runtime::to_json(e);
    const auto via_record = pb::runtime::to_json(record);
    pb_test_require(via_error == via_record);
    pb_test_require(contains(via_error, R"("stage":{"key":"key:\"slash\\line\n","name":"name\t\"quoted\""})"));
    pb_test_require(contains(via_error, R"("category":"contract_violation")"));
    pb_test_require(contains(via_error, R"("message":"msg:\n\t\"\\end")"));
  }

  // ── 6. Empty stage + empty message still produce well-formed JSON. ──
  {
    const pb::runtime::error e{};  // all fields default-empty, category=stage_failure
    const auto json = pb::runtime::to_json(e);
    pb_test_require(json.rfind("{\"schema_version\":\"pb.error.v1\"", 0) == 0);
    pb_test_require(contains(json, "\"stage\":{\"key\":\"\",\"name\":\"\"}"));
    pb_test_require(contains(json, "\"category\":\"stage_failure\""));
    pb_test_require(contains(json, "\"message\":\"\""));
    // Last character must be the closing brace.
    pb_test_require(!json.empty() && json.back() == '}');
  }

  // ── 7. Direct error_record serialization preserves edge fields. ──
  {
    const pb::runtime::error_record record{
        .stage_key = "key\\with\"quote",
        .stage_name = std::string{"name\nline"},
        .category = "custom_category",
        .message = std::string{"msg\tvalue"}};
    const auto json = pb::runtime::to_json(record);

    pb_test_require(
        contains(json, "\"stage\":{\"key\":\"key\\\\with\\\"quote\",\"name\":\"name\\nline\"}"));
    pb_test_require(contains(json, "\"category\":\"custom_category\""));
    pb_test_require(contains(json, "\"message\":\"msg\\tvalue\""));
  }

  // ── 8. Default error_record still emits the stable v1 object shape. ──
  {
    const auto json = pb::runtime::to_json(pb::runtime::error_record{});
    pb_test_require(json ==
                    "{\"schema_version\":\"pb.error.v1\",\"stage\":{\"key\":\"\",\"name\":\"\"},"
                    "\"category\":\"\",\"message\":\"\"}");
  }

  // ── 9. Schema-version constant is reachable through pb:: alias. ──
  {
    static_assert(pb::error_schema_version == std::string_view{"pb.error.v1"},
                  "pb::error_schema_version MUST match the v1 contract");
  }

  return 0;
}
