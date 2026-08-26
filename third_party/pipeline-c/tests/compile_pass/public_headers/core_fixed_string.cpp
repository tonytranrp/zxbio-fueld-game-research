#include <pb/core/fixed_string.hpp>

#include <string_view>
#include <type_traits>

static constexpr pb::core::fixed_string fixed_name{"parse"};
static_assert(fixed_name.size() == 5);
static_assert(fixed_name.view() == std::string_view{"parse"});
static_assert(std::string_view{fixed_name.c_str(), fixed_name.size()} == fixed_name.view());
static_assert(std::same_as<decltype(pb::core::fixed_string{"parse"}), pb::core::fixed_string<6>>);

int pb_public_header_core_fixed_string() { return fixed_name.view() == std::string_view{"parse"} ? 0 : 1; }
