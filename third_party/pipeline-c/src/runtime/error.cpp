#include "pb/runtime/error.hpp"

#include <cstdio>
#include <ostream>
#include <string>

namespace pb::runtime {

namespace detail {

inline void append_json_string(std::string& out, std::string_view value) {
  out.push_back('"');
  for (const char ch : value) {
    switch (ch) {
    case '"':
      out.append("\\\"");
      break;
    case '\\':
      out.append("\\\\");
      break;
    case '\b':
      out.append("\\b");
      break;
    case '\f':
      out.append("\\f");
      break;
    case '\n':
      out.append("\\n");
      break;
    case '\r':
      out.append("\\r");
      break;
    case '\t':
      out.append("\\t");
      break;
    default:
      if (static_cast<unsigned char>(ch) < 0x20U) {
        char buf[7];
        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(ch)));
        out.append(buf);
      } else {
        out.push_back(ch);
      }
      break;
    }
  }
  out.push_back('"');
}

} // namespace detail

auto category_name(error_category category) noexcept -> std::string_view {
  switch (category) {
  case error_category::stage_failure:
    return "stage_failure";
  case error_category::expected_error:
    return "expected_error";
  case error_category::exception:
    return "exception";
  case error_category::contract_violation:
    return "contract_violation";
  }
  return "unknown";
}

auto has_category(const error& value, error_category category) noexcept -> bool {
  return value.category == category;
}

auto has_stage(const stage_id& stage) noexcept -> bool {
  return !stage.key.empty() || !stage.name.empty();
}

auto has_stage(const error& value) noexcept -> bool {
  return has_stage(value.stage);
}

auto has_message(const error& value) noexcept -> bool {
  return !value.message.empty();
}

auto to_record(const error& value) -> error_record {
  return error_record{.stage_key = value.stage.key,
                      .stage_name = value.stage.name,
                      .category = std::string{category_name(value.category)},
                      .message = value.message};
}

auto describe(const stage_id& stage) -> std::string {
  if (stage.name.empty()) {
    return has_stage(stage) ? stage.key : std::string{"<unknown stage>"};
  }
  if (stage.key.empty()) {
    return stage.name;
  }
  if (stage.key == stage.name) {
    return stage.name;
  }
  return stage.name + " (" + stage.key + ")";
}

auto describe(const error& value) -> std::string {
  auto text = std::string{category_name(value.category)};
  text += " at ";
  text += describe(value.stage);
  if (has_message(value)) {
    text += ": ";
    text += value.message;
  }
  return text;
}

auto operator<<(std::ostream& stream, error_category category) -> std::ostream& {
  return stream << category_name(category);
}

auto operator<<(std::ostream& stream, const stage_id& stage) -> std::ostream& {
  return stream << describe(stage);
}

auto operator<<(std::ostream& stream, const error& value) -> std::ostream& {
  return stream << describe(value);
}

auto to_json(const error_record& value) -> std::string {
  std::string out;
  out.append("{\"schema_version\":");
  detail::append_json_string(out, error_schema_version);
  out.append(",\"stage\":{\"key\":");
  detail::append_json_string(out, value.stage_key);
  out.append(",\"name\":");
  detail::append_json_string(out, value.stage_name);
  out.append("},\"category\":");
  detail::append_json_string(out, value.category);
  out.append(",\"message\":");
  detail::append_json_string(out, value.message);
  out.push_back('}');
  return out;
}

auto to_json(const error& value) -> std::string {
  return to_json(to_record(value));
}

} // namespace pb::runtime
