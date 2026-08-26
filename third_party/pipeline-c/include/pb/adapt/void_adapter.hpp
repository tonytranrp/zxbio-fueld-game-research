#pragma once

#include <concepts>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "pb/core/concepts.hpp"

namespace pb {

namespace adapt_detail {

template <class Fn, class Input>
concept void_invocable = std::invocable<Fn, Input> &&
                         std::same_as<std::invoke_result_t<Fn, Input>, void>;

template <class Fn, class Input>
concept const_void_invocable = std::invocable<const Fn&, Input> &&
                               std::same_as<std::invoke_result_t<const Fn&, Input>, void>;

template <class Fn, class Input>
concept any_void_invocable = void_invocable<Fn, Input> || const_void_invocable<Fn, Input>;

template <class Fn, class Input>
inline constexpr bool is_noexcept_void_call_v =
    (void_invocable<Fn, Input> && noexcept(std::invoke(std::declval<Fn>(), std::declval<Input>()))) ||
    (const_void_invocable<Fn, Input> && noexcept(std::invoke(std::declval<const Fn&>(), std::declval<Input>())));

} // namespace adapt_detail

// ---------------------------------------------------------------------------
// void_adapter<Fn, Input>
//
// Wraps a void-returning callable as a pass-through pipeline stage.
// The callable is invoked for its side effect and the input is forwarded
// unchanged as the output.  This allows logging, telemetry, and other
// side-effect-only stages to be inserted into a pipeline without altering
// the data flow.
//
// Requirements:
//   Fn must satisfy adapt_detail::any_void_invocable<Fn, Input> and be
//   default-constructible.  The default-constructed instance must be safely
//   callable with an Input — this is required by the runtime engine's
//   construct_stages_per_run policy (the default).
//
//   For raw function pointers, prefer pb::adapt_fn<&func, Input, void>
//   (from <pb/adapt/fn.hpp>) which embeds the function address in the type
//   and therefore survives default construction.
// ---------------------------------------------------------------------------
template <class Fn, class Input>
struct void_adapter {
  static_assert(std::default_initializable<Fn>,
                "pb::void_adapter: Fn must be default-constructible so the runtime engine "
                "can construct the stage with its default policy. "
                "For free functions, use pb::adapt_fn<&func, Input, void> instead.");
  static_assert(adapt_detail::any_void_invocable<Fn, Input>,
                "pb::void_adapter: Fn must be callable with Input and return void. "
                "Use pb::adapt_functor for value-returning callables or adjust the declared Input type.");

  using input_type = Input;
  using output_type = Input; // Pass-through: side-effect-only stages propagate their input

  [[no_unique_address]] Fn fn_;

  static constexpr std::string_view stage_name() noexcept { return "void_adapter"; }

  [[nodiscard]] constexpr output_type operator()(input_type input) const
      noexcept(adapt_detail::is_noexcept_void_call_v<Fn, input_type>)
    requires adapt_detail::any_void_invocable<Fn, input_type>
  {
    if constexpr (adapt_detail::const_void_invocable<Fn, input_type>) {
      std::invoke(static_cast<const Fn&>(fn_), input);
    } else {
      std::invoke(fn_, input);
    }
    return input;
  }
};

} // namespace pb
