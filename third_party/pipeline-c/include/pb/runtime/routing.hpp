#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "pb/runtime/error.hpp"
#include "pb/runtime/result.hpp"

namespace pb::runtime {

inline constexpr std::string_view route_descriptor_schema_version = "pb.runtime.route.v1";

struct route_case_record {
  std::size_t index{};
  std::string_view key{};
  std::string_view name{};
};

template <std::size_t CaseCount>
struct route_descriptor {
  static constexpr auto schema_version = route_descriptor_schema_version;
  static constexpr std::size_t case_count = CaseCount;

  std::array<route_case_record, CaseCount> cases{};

  [[nodiscard]] constexpr auto empty() const noexcept -> bool { return CaseCount == 0; }

  [[nodiscard]] constexpr auto case_records() const noexcept -> const std::array<route_case_record, CaseCount>& {
    return cases;
  }
};

struct route_selection {
  bool matched{false};
  route_case_record selected{};
};

template <std::size_t CaseCount>
[[nodiscard]] constexpr auto make_route_descriptor(std::array<route_case_record, CaseCount> cases) noexcept
    -> route_descriptor<CaseCount> {
  return route_descriptor<CaseCount>{.cases = cases};
}

template <std::size_t CaseCount>
[[nodiscard]] auto select_route(const route_descriptor<CaseCount>& descriptor, std::span<const bool> matches)
    -> result<route_selection> {
  if (matches.size() != CaseCount) {
    return error{.category = error_category::contract_violation,
                 .message = "branch route predicate count mismatch"};
  }

  for (std::size_t index = 0; index < CaseCount; ++index) {
    if (matches[index]) {
      return route_selection{.matched = true, .selected = descriptor.cases[index]};
    }
  }

  return error{.category = error_category::contract_violation,
               .message = "no branch route matched"};
}

} // namespace pb::runtime
