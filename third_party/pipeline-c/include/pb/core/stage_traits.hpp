#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

namespace pb {
struct no_error {};
}

namespace pb::core {

namespace detail {
template <class Name>
[[nodiscard]] constexpr std::string_view name_to_string_view(const Name& name) noexcept {
  if constexpr (std::convertible_to<Name, std::string_view>) {
    return std::string_view{name};
  } else if constexpr (requires { name.view(); }) {
    return name.view();
  } else if constexpr (requires { name.c_str(); name.size(); }) {
    return std::string_view{name.c_str(), name.size()};
  } else {
    return std::string_view{"<unnamed>"};
  }
}

template <class T, class = void>
struct error_type_or {
  using type = pb::no_error;
};

template <class T>
struct error_type_or<T, std::void_t<typename T::error_type>> {
  using type = typename T::error_type;
};

template <class T, class = void>
struct has_declared_stage_input_type : std::false_type {};

template <class T>
struct has_declared_stage_input_type<T, std::void_t<typename T::input_type>> : std::true_type {};

template <class T>
inline constexpr bool has_declared_stage_input_type_v = has_declared_stage_input_type<T>::value;

template <class T, class = void>
struct has_declared_stage_output_type : std::false_type {};

template <class T>
struct has_declared_stage_output_type<T, std::void_t<typename T::output_type>> : std::true_type {};

template <class T>
inline constexpr bool has_declared_stage_output_type_v = has_declared_stage_output_type<T>::value;
} // namespace detail

template <class T, class = void>
struct stage_traits {
  static constexpr bool is_valid_stage = false;
};

template <class T>
struct stage_traits<T, std::void_t<typename T::input_type, typename T::output_type>> {
  using input_type = typename T::input_type;
  using output_type = typename T::output_type;
  using error_type = typename detail::error_type_or<T>::type;

  static constexpr bool is_valid_stage = true;

  [[nodiscard]] static constexpr std::string_view name() noexcept {
    if constexpr (requires { T::name; }) {
      return detail::name_to_string_view(T::name);
    } else if constexpr (requires { T::stage_name(); }) {
      return std::string_view{T::stage_name()};
    } else {
      return std::string_view{"<unnamed>"};
    }
  }

  [[nodiscard]] static constexpr std::string_view key() noexcept {
    if constexpr (requires { T::key; }) {
      return detail::name_to_string_view(T::key);
    } else if constexpr (requires { T::stage_key(); }) {
      return std::string_view{T::stage_key()};
    } else {
      return name();
    }
  }
};

namespace detail {
template <class T, class = void>
struct has_trait_stage_input_type : std::false_type {};

template <class T>
struct has_trait_stage_input_type<T, std::void_t<typename stage_traits<T>::input_type>> : std::true_type {};

template <class T>
inline constexpr bool has_stage_input_type_v =
    has_declared_stage_input_type_v<T> || has_trait_stage_input_type<T>::value;

template <class T, class = void>
struct has_trait_stage_output_type : std::false_type {};

template <class T>
struct has_trait_stage_output_type<T, std::void_t<typename stage_traits<T>::output_type>> : std::true_type {};

template <class T>
inline constexpr bool has_stage_output_type_v =
    has_declared_stage_output_type_v<T> || has_trait_stage_output_type<T>::value;

template <class T, bool IsStage = stage_traits<T>::is_valid_stage>
struct stage_input_type_or_diagnostic {
  static_assert(IsStage,
                "pb::stage_input_t requires a valid stage type with an input_type member");
  using type = void;
};

template <class T>
struct stage_input_type_or_diagnostic<T, true> {
  using type = typename stage_traits<T>::input_type;
};

template <class T, bool IsStage = stage_traits<T>::is_valid_stage>
struct stage_output_type_or_diagnostic {
  static_assert(IsStage,
                "pb::stage_output_t requires a valid stage type with an output_type member");
  using type = void;
};

template <class T>
struct stage_output_type_or_diagnostic<T, true> {
  using type = typename stage_traits<T>::output_type;
};

template <class T, bool IsStage = stage_traits<T>::is_valid_stage>
struct stage_error_type_or_diagnostic {
  static_assert(IsStage,
                "pb::stage_error_t requires a valid stage type with input_type and output_type members");
  using type = void;
};

template <class T>
struct stage_error_type_or_diagnostic<T, true> {
  using type = typename stage_traits<T>::error_type;
};
} // namespace detail

template <class T>
inline constexpr bool is_stage_v = stage_traits<T>::is_valid_stage;

template <class T, class = void>
struct stage_info {
  using stage_type = T;

  static constexpr bool valid = false;

  [[nodiscard]] static constexpr std::string_view name() noexcept {
    return std::string_view{"<invalid>"};
  }

  [[nodiscard]] static constexpr std::string_view key() noexcept {
    return std::string_view{"<invalid>"};
  }
};

template <class T>
struct stage_info<T, std::enable_if_t<stage_traits<T>::is_valid_stage>> {
  using stage_type = T;
  using input_type = typename stage_traits<T>::input_type;
  using output_type = typename stage_traits<T>::output_type;
  using error_type = typename stage_traits<T>::error_type;

  static constexpr bool valid = true;

  [[nodiscard]] static constexpr std::string_view name() noexcept {
    return stage_traits<T>::name();
  }

  [[nodiscard]] static constexpr std::string_view key() noexcept {
    return stage_traits<T>::key();
  }
};

template <class T>
using stage_info_t = stage_info<T>;

template <class T>
using stage_input_t = typename detail::stage_input_type_or_diagnostic<T>::type;

template <class T>
using stage_output_t = typename detail::stage_output_type_or_diagnostic<T>::type;

template <class T>
using stage_error_t = typename detail::stage_error_type_or_diagnostic<T>::type;

template <class T>
[[nodiscard]] constexpr std::string_view stage_name() noexcept {
  return stage_info<T>::name();
}

template <class T>
[[nodiscard]] constexpr std::string_view stage_key() noexcept {
  return stage_info<T>::key();
}

} // namespace pb::core
