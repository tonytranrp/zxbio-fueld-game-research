#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pb::core::detail {

class string_sink {
public:
  string_sink() = default;

  explicit string_sink(std::size_t reserve_hint) {
    output_.reserve(reserve_hint);
  }

  auto operator<<(char value) -> string_sink& {
    output_.push_back(value);
    return *this;
  }

  auto operator<<(const char* value) -> string_sink& {
    output_ += value;
    return *this;
  }

  auto operator<<(std::string_view value) -> string_sink& {
    output_.append(value.data(), value.size());
    return *this;
  }

  auto operator<<(const std::string& value) -> string_sink& {
    output_ += value;
    return *this;
  }

  template <class Integer>
    requires(std::is_integral_v<Integer> && !std::is_same_v<std::remove_cv_t<Integer>, char> &&
             !std::is_same_v<std::remove_cv_t<Integer>, signed char> &&
             !std::is_same_v<std::remove_cv_t<Integer>, unsigned char> &&
             !std::is_same_v<std::remove_cv_t<Integer>, bool>)
  auto operator<<(Integer value) -> string_sink& {
    output_ += std::to_string(value);
    return *this;
  }

  [[nodiscard]] auto str() const& -> std::string {
    return output_;
  }

  [[nodiscard]] auto str() && -> std::string {
    return std::move(output_);
  }

private:
  std::string output_;
};

} // namespace pb::core::detail
