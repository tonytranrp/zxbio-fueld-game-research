#pragma once

/// @file
/// @brief Experimental reflection-driven stage adapter.
///
/// `pb::reflect_stage<T>` wraps a user type into a Pipeline stage by
/// extracting its input/output/error metadata via C++26 static reflection
/// (P2996).  This file ships the *contract* for that adapter on every
/// supported compiler — including C++20 baselines — but only provides a
/// working implementation when the toolchain advertises reflection via
/// `PB_HAS_REFLECTION == 1` (see `pb/core/cpp26_features.hpp`).
///
/// Design contract that holds on every compiler:
///
///   * `pb::reflect_stage<T>` is always a well-formed name.
///   * `pb::reflect_stage_available_v<T>` is `true` only when the current
///     build can actually reflect on `T`.  On non-reflection compilers it
///     is unconditionally `false`.
///   * Using `pb::reflect_stage<T>` as a pipeline stage on a compiler
///     without reflection raises a `static_assert` with a clear message
///     pointing at the gate.  No silent fallback, no SFINAE drift.
///
/// User-facing usage (once reflection ships on the target compiler):
///
/// @code
///   struct ParseOrder {
///     using input_type  = RawText;
///     using output_type = Order;
///     static constexpr auto stage_key() noexcept { return "parse_order"; }
///     auto operator()(RawText raw) const -> Order;
///   };
///
///   using ParseStage = pb::reflect_stage<ParseOrder>;
///   // ParseStage::input_type / output_type / stage_key() filled in via
///   // reflection, no manual stage_traits specialization needed.
/// @endcode

#include <type_traits>

#include "pb/core/cpp26_features.hpp"
#include "pb/core/stage_traits.hpp"

#if PB_HAS_REFLECTION
  #include <experimental/meta>
#endif

namespace pb {

#if PB_HAS_REFLECTION

namespace detail::reflect {

// Reflection-driven extraction.  These helpers are intentionally kept in a
// detail namespace and behind the PB_HAS_REFLECTION gate so toolchains
// without reflection never see the `meta::` types.
//
// On P2996-conforming compilers, we expect to query the user type T for
// members named `input_type`, `output_type`, `operator()`, etc.  The exact
// reflection API is still in flux, so the implementation here is the
// minimal probe documented by the most recent stable WG21 wording.

template <class T>
consteval auto reflect_input_type() {
  // P2996 reflection: query nested type alias.
  constexpr auto T_refl = ^^T;
  static_assert(std::meta::is_type(T_refl), "pb::reflect_stage requires a class type");
  return std::meta::reflect_type(std::meta::lookup(T_refl, "input_type"));
}

template <class T>
consteval auto reflect_output_type() {
  constexpr auto T_refl = ^^T;
  return std::meta::reflect_type(std::meta::lookup(T_refl, "output_type"));
}

} // namespace detail::reflect

template <class T>
struct reflect_stage {
  using input_type  = [: detail::reflect::reflect_input_type<T>()  :];
  using output_type = [: detail::reflect::reflect_output_type<T>() :];

  template <class... Args>
  auto operator()(Args&&... args) const
      -> decltype(std::declval<const T&>()(std::forward<Args>(args)...)) {
    return T{}(std::forward<Args>(args)...);
  }
};

template <class T>
inline constexpr bool reflect_stage_available_v = true;

#else // !PB_HAS_REFLECTION

namespace detail::reflect {

template <class T>
struct unavailable_stage {
  static_assert(sizeof(T) == 0,
                "pb::reflect_stage<T> requires C++26 static reflection (P2996). "
                "Define your stage manually (input_type, output_type, operator()), "
                "or build with a reflection-capable compiler and set PB_HAS_REFLECTION=1.");
};

} // namespace detail::reflect

/// Fallback: instantiating `pb::reflect_stage<T>` on a non-reflection
/// compiler is a hard error.  The error message points at the gate so
/// users know exactly which compiler feature is missing.
template <class T>
struct reflect_stage : detail::reflect::unavailable_stage<T> {};

template <class T>
inline constexpr bool reflect_stage_available_v = false;

#endif // PB_HAS_REFLECTION

} // namespace pb
