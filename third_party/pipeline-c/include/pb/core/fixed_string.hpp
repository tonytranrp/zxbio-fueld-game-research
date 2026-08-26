#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace pb::core {

template <std::size_t N>
struct fixed_string {
  char value[N]{};

  constexpr fixed_string() = default;

  constexpr fixed_string(const char (&text)[N]) {
    std::copy_n(text, N, value);
  }

  [[nodiscard]] constexpr const char* c_str() const noexcept { return value; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return N == 0 ? 0 : N - 1; }
  [[nodiscard]] constexpr std::string_view view() const noexcept { return {value, size()}; }

  constexpr auto operator<=>(const fixed_string&) const = default;
};

template <std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

} // namespace pb::core
