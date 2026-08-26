#pragma once

#include <concepts>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "pb/runtime/error.hpp"

namespace pb::runtime {

template <class T, class E = error>
class result {
public:
  using value_type = T;
  using error_type = E;

  result(const T& value) : storage_(value) {}
  result(T&& value) : storage_(std::move(value)) {}
  result(const E& error_value) : storage_(error_value) {}
  result(E&& error_value) : storage_(std::move(error_value)) {}

  [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
  [[nodiscard]] bool has_error() const noexcept { return !has_value(); }
  explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] T& value() & { return std::get<T>(storage_); }
  [[nodiscard]] const T& value() const& { return std::get<T>(storage_); }
  [[nodiscard]] T&& value() && { return std::move(std::get<T>(storage_)); }

  template <class U>
    requires std::copy_constructible<T> && std::constructible_from<T, U>
  [[nodiscard]] T value_or(U&& fallback) const& {
    return has_value() ? value() : T{std::forward<U>(fallback)};
  }

  template <class U>
    requires std::move_constructible<T> && std::constructible_from<T, U>
  [[nodiscard]] T value_or(U&& fallback) && {
    return has_value() ? std::move(*this).value() : T{std::forward<U>(fallback)};
  }

  [[nodiscard]] E& error() & { return std::get<E>(storage_); }
  [[nodiscard]] const E& error() const& { return std::get<E>(storage_); }
  [[nodiscard]] E&& error() && { return std::move(std::get<E>(storage_)); }

  template <class G>
    requires std::copy_constructible<E> && std::constructible_from<E, G>
  [[nodiscard]] E error_or(G&& fallback) const& {
    return has_value() ? E{std::forward<G>(fallback)} : error();
  }

  template <class G>
    requires std::move_constructible<E> && std::constructible_from<E, G>
  [[nodiscard]] E error_or(G&& fallback) && {
    return has_value() ? E{std::forward<G>(fallback)} : std::move(*this).error();
  }

private:
  static_assert(!std::same_as<T, E>, "pb::runtime::result value_type and error_type must differ");
  std::variant<T, E> storage_;
};

template <class E>
class result<void, E> {
public:
  using value_type = void;
  using error_type = E;

  result() = default;
  result(const E& error_value) : storage_(error_value) {}
  result(E&& error_value) : storage_(std::move(error_value)) {}

  [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<std::monostate>(storage_); }
  [[nodiscard]] bool has_error() const noexcept { return !has_value(); }
  explicit operator bool() const noexcept { return has_value(); }
  [[nodiscard]] E& error() & { return std::get<E>(storage_); }
  [[nodiscard]] const E& error() const& { return std::get<E>(storage_); }
  [[nodiscard]] E&& error() && { return std::move(std::get<E>(storage_)); }

  template <class G>
    requires std::copy_constructible<E> && std::constructible_from<E, G>
  [[nodiscard]] E error_or(G&& fallback) const& {
    return has_value() ? E{std::forward<G>(fallback)} : error();
  }

  template <class G>
    requires std::move_constructible<E> && std::constructible_from<E, G>
  [[nodiscard]] E error_or(G&& fallback) && {
    return has_value() ? E{std::forward<G>(fallback)} : std::move(*this).error();
  }

private:
  std::variant<std::monostate, E> storage_{std::monostate{}};
};

namespace detail {
template <class T>
struct is_result : std::false_type {};

template <class T, class E>
struct is_result<result<T, E>> : std::true_type {};

template <class T>
concept streamable_error = requires(std::ostringstream stream, const T& value) {
  { stream << value } -> std::same_as<std::ostringstream&>;
};

inline auto error_message_from(const std::string& message) -> std::string { return message; }
inline auto error_message_from(const char* message) -> std::string { return message == nullptr ? std::string{} : std::string{message}; }
inline auto error_message_from(const error& value) -> std::string { return value.message; }

template <class E>
auto error_message_from(const E& value) -> std::string {
  if constexpr (streamable_error<E>) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
  } else {
    return "expected-like object reported an error";
  }
}

template <class E>
auto error_message_from(const E& value) -> std::string
  requires requires(const E& source) {
    { source.diagnostic } -> std::convertible_to<error>;
  }
{
  auto as_error = error{value.diagnostic};
  return as_error.message;
}

template <class E>
auto normalize_expected_error(E&& value) -> error {
  if constexpr (std::same_as<std::remove_cvref_t<E>, error>) {
    return std::forward<E>(value);
  } else {
    return error{.category = error_category::expected_error,
                 .message = error_message_from(std::forward<E>(value))};
  }
}
} // namespace detail

template <class T>
inline constexpr bool is_result_v = detail::is_result<std::remove_cvref_t<T>>::value;

template <class T>
concept expected_like = is_result_v<T> || requires(T value) {
  typename std::remove_cvref_t<T>::value_type;
  typename std::remove_cvref_t<T>::error_type;
  { value.has_value() } -> std::convertible_to<bool>;
  value.value();
  value.error();
};

template <class T>
[[nodiscard]] auto to_result(T&& value) {
  using Value = std::remove_cvref_t<T>;
  if constexpr (is_result_v<Value>) {
    return std::forward<T>(value);
  } else if constexpr (expected_like<Value>) {
    using Output = typename Value::value_type;
    if (value.has_value()) {
      if constexpr (std::same_as<Output, void>) {
        std::forward<T>(value).value();
        return result<void>{};
      } else {
        return result<Output>{std::forward<T>(value).value()};
      }
    }
    return result<Output>{detail::normalize_expected_error(std::forward<T>(value).error())};
  } else {
    using Output = std::remove_cvref_t<T>;
    return result<Output>{std::forward<T>(value)};
  }
}

template <class T>
[[nodiscard]] auto make_result(T&& value) {
  return to_result(std::forward<T>(value));
}

[[nodiscard]] inline auto make_result() -> result<void> {
  return {};
}

} // namespace pb::runtime
