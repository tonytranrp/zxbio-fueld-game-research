#include <pb/tooling/cli_registry.hpp>

#include <string>
#include <string_view>
#include <type_traits>

namespace {
static_assert(std::is_default_constructible_v<pb::tooling::pipeline_registry>);
static_assert(std::is_default_constructible_v<pb::tooling::pipeline_entry>);
static_assert(std::is_same_v<decltype(pb::tooling::pipeline_entry{}.to_text()), std::string>);
static_assert(std::is_same_v<decltype(pb::tooling::pipeline_entry{}.to_json()), std::string>);
static_assert(std::is_same_v<decltype(pb::tooling::pipeline_entry{}.to_dot(std::string_view{})), std::string>);
} // namespace

int pb_public_header_tooling_cli_registry() {
  pb::tooling::pipeline_registry registry;
  return registry.size() == 0 && !registry.contains("missing") ? 0 : 1;
}
