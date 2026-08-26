#pragma once

#include <concepts>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "pb/core/concepts.hpp"

namespace pb {

namespace adapt_detail {
struct unnamed_stage {
  static constexpr std::string_view value{"unnamed"};
};

template <class Tag>
concept named_tag = requires {
  { Tag::value } -> std::convertible_to<std::string_view>;
};

template <class Tag>
constexpr std::string_view tag_value() noexcept {
  if constexpr (named_tag<Tag>) {
    return std::string_view{Tag::value};
  } else {
    return std::string_view{"unnamed"};
  }
}

template <class T>
struct member_object;

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...)> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) const> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) const noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) const&> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) const& noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) const&&> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) const&& noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) &> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) & noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) &&> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) && noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) volatile> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) volatile noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) volatile&> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) volatile& noexcept> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) volatile&&> {
  using type = C;
};

template <class C, class R, class... Args>
struct member_object<R (C::*)(Args...) volatile&& noexcept> {
  using type = C;
};

template <class T>
using member_object_t = typename member_object<T>::type;

template <class T>
concept expected_like_result = requires(T value) {
  typename std::remove_cvref_t<T>::value_type;
  typename std::remove_cvref_t<T>::error_type;
  { value.has_value() } -> std::convertible_to<bool>;
  value.value();
  value.error();
};

template <class Result, class Output>
concept returns_declared_output = std::same_as<std::remove_cvref_t<Result>, Output> ||
                                  (expected_like_result<Result> &&
                                   std::same_as<typename std::remove_cvref_t<Result>::value_type, Output>);

template <class Callable, class Input, class Output>
concept invocable_as_output = std::invocable<Callable, Input> &&
                              returns_declared_output<std::invoke_result_t<Callable, Input>, Output>;

template <class Callable, class ObjectExpr, class Input, class Output>
concept member_invocable_expr_as_output = std::invocable<Callable, ObjectExpr, Input> &&
                                          returns_declared_output<std::invoke_result_t<Callable, ObjectExpr, Input>, Output>;

template <class Callable, class Object, class Input, class Output>
concept member_invocable_as_output =
    member_invocable_expr_as_output<Callable, Object&, Input, Output> ||
    member_invocable_expr_as_output<Callable, const Object&, Input, Output> ||
    member_invocable_expr_as_output<Callable, volatile Object&, Input, Output> ||
    member_invocable_expr_as_output<Callable, const volatile Object&, Input, Output> ||
    member_invocable_expr_as_output<Callable, Object, Input, Output>;

template <auto Function, class Object, class Input>
constexpr bool member_invoke_is_noexcept() {
  using Callable = decltype(Function);
  if constexpr (std::invocable<Callable, Object&, Input>) {
    return noexcept(std::invoke(Function, std::declval<Object&>(), std::declval<Input>()));
  } else if constexpr (std::invocable<Callable, const Object&, Input>) {
    return noexcept(std::invoke(Function, std::declval<const Object&>(), std::declval<Input>()));
  } else if constexpr (std::invocable<Callable, volatile Object&, Input>) {
    return noexcept(std::invoke(Function, std::declval<volatile Object&>(), std::declval<Input>()));
  } else if constexpr (std::invocable<Callable, const volatile Object&, Input>) {
    return noexcept(std::invoke(Function, std::declval<const volatile Object&>(), std::declval<Input>()));
  } else {
    return noexcept(std::invoke(Function, std::declval<Object>(), std::declval<Input>()));
  }
}

template <auto Function, class Object, class Input>
constexpr decltype(auto) invoke_member(Object& object, Input&& input)
    noexcept(member_invoke_is_noexcept<Function, Object, Input>()) {
  using Callable = decltype(Function);
  if constexpr (std::invocable<Callable, Object&, Input>) {
    return std::invoke(Function, object, std::forward<Input>(input));
  } else if constexpr (std::invocable<Callable, const Object&, Input>) {
    return std::invoke(Function, static_cast<const Object&>(object), std::forward<Input>(input));
  } else if constexpr (std::invocable<Callable, volatile Object&, Input>) {
    return std::invoke(Function, static_cast<volatile Object&>(object), std::forward<Input>(input));
  } else if constexpr (std::invocable<Callable, const volatile Object&, Input>) {
    return std::invoke(Function, static_cast<const volatile Object&>(object), std::forward<Input>(input));
  } else {
    return std::invoke(Function, std::move(object), std::forward<Input>(input));
  }
}
} // namespace adapt_detail

template <class Tag>
struct name {
  using tag_type = Tag;
};

template <auto Function>
struct fn {
  static constexpr auto value = Function;
};

template <auto Function>
struct member {
  static constexpr auto value = Function;
};

template <class Functor>
struct functor {
  using type = Functor;
};

template <class T>
struct in {
  using type = T;
};

template <class T>
struct out {
  using type = T;
};

template <class T = no_error>
struct err {
  using type = T;
};

template <class Name, class Callable, class Input, class Output, class Error = no_error>
struct callable_stage {
  using name_tag = Name;
  using input_type = Input;
  using output_type = Output;
  using error_type = Error;
  using callable_type = Callable;

  static constexpr std::string_view name = adapt_detail::tag_value<name_tag>();

  static constexpr std::string_view stage_name() noexcept { return name; }

  constexpr decltype(auto) operator()(input_type input) const
      noexcept(noexcept(std::invoke(callable_type{}, std::move(input))))
      requires std::default_initializable<callable_type> &&
               adapt_detail::invocable_as_output<callable_type, input_type, output_type>
  {
    return std::invoke(callable_type{}, std::move(input));
  }
};

template <class Name, auto Function, class Input, class Output, class Error = no_error>
struct function_stage {
  using name_tag = Name;
  using input_type = Input;
  using output_type = Output;
  using error_type = Error;
  using function_type = decltype(Function);

  static constexpr auto function = Function;
  static constexpr std::string_view name = adapt_detail::tag_value<name_tag>();

  static constexpr std::string_view stage_name() noexcept { return name; }

  constexpr decltype(auto) operator()(input_type input) const
      noexcept(noexcept(std::invoke(function, std::move(input))))
      requires adapt_detail::invocable_as_output<function_type, input_type, output_type>
  {
    return std::invoke(function, std::move(input));
  }
};

template <class... Options>
struct adapt;

template <class NameTag, auto Function, class Input, class Output, class Error>
struct adapt<name<NameTag>, fn<Function>, in<Input>, out<Output>, err<Error>>
    : function_stage<NameTag, Function, Input, Output, Error> {};

template <class NameTag, auto Function, class Input, class Output, class Error>
struct adapt<name<NameTag>, member<Function>, in<Input>, out<Output>, err<Error>> {
  using name_tag = NameTag;
  using input_type = Input;
  using output_type = Output;
  using error_type = Error;
  using function_type = decltype(Function);
  using member_type = adapt_detail::member_object_t<function_type>;

  static constexpr std::string_view name = adapt_detail::tag_value<name_tag>();

  static constexpr std::string_view stage_name() noexcept { return name; }

  constexpr decltype(auto) operator()(input_type input) const
      noexcept(adapt_detail::member_invoke_is_noexcept<Function, member_type, input_type>())
      requires std::default_initializable<member_type> &&
               adapt_detail::member_invocable_as_output<function_type, member_type, input_type, output_type>
  {
    member_type object{};
    return adapt_detail::invoke_member<Function, member_type>(object, std::move(input));
  }
};

template <class NameTag, auto Function, class Input, class Output>
struct adapt<name<NameTag>, fn<Function>, in<Input>, out<Output>>
    : function_stage<NameTag, Function, Input, Output> {};

template <class NameTag, auto Function, class Input, class Output>
struct adapt<name<NameTag>, member<Function>, in<Input>, out<Output>>
    : adapt<name<NameTag>, member<Function>, in<Input>, out<Output>, err<no_error>> {};

template <auto Function, class Input, class Output, class Error>
struct adapt<fn<Function>, in<Input>, out<Output>, err<Error>>
    : function_stage<adapt_detail::unnamed_stage, Function, Input, Output, Error> {};

template <auto Function, class Input, class Output, class Error>
struct adapt<member<Function>, in<Input>, out<Output>, err<Error>>
    : adapt<name<adapt_detail::unnamed_stage>, member<Function>, in<Input>, out<Output>, err<Error>> {};

template <auto Function, class Input, class Output>
struct adapt<member<Function>, in<Input>, out<Output>>
    : adapt<name<adapt_detail::unnamed_stage>, member<Function>, in<Input>, out<Output>, err<no_error>> {};

template <auto Function, class Input, class Output>
struct adapt<fn<Function>, in<Input>, out<Output>>
    : function_stage<adapt_detail::unnamed_stage, Function, Input, Output> {};

template <class NameTag, class Functor, class Input, class Output, class Error>
struct adapt<name<NameTag>, functor<Functor>, in<Input>, out<Output>, err<Error>>
    : callable_stage<NameTag, Functor, Input, Output, Error> {};

template <class NameTag, class Functor, class Input, class Output>
struct adapt<name<NameTag>, functor<Functor>, in<Input>, out<Output>>
    : callable_stage<NameTag, Functor, Input, Output> {};

template <class Functor, class Input, class Output, class Error>
struct adapt<functor<Functor>, in<Input>, out<Output>, err<Error>>
    : callable_stage<adapt_detail::unnamed_stage, Functor, Input, Output, Error> {};

template <class Functor, class Input, class Output>
struct adapt<functor<Functor>, in<Input>, out<Output>>
    : callable_stage<adapt_detail::unnamed_stage, Functor, Input, Output> {};

template <auto Function, class Input, class Output, class Error = no_error,
          class NameTag = adapt_detail::unnamed_stage>
using adapt_fn = adapt<name<NameTag>, fn<Function>, in<Input>, out<Output>, err<Error>>;

template <auto Function, class Input, class Output, class Error = no_error,
          class NameTag = adapt_detail::unnamed_stage>
using adapt_member = adapt<name<NameTag>, member<Function>, in<Input>, out<Output>, err<Error>>;

template <class Functor, class Input, class Output, class Error = no_error,
          class NameTag = adapt_detail::unnamed_stage>
using adapt_functor = adapt<name<NameTag>, functor<Functor>, in<Input>, out<Output>, err<Error>>;

template <class Stage>
concept adapted_stage = pb::core::Stage<Stage> && requires(Stage stage, typename Stage::input_type input) {
  { Stage::stage_name() } -> std::convertible_to<std::string_view>;
  stage(std::move(input));
};

template <class Stage, class Input>
inline constexpr bool is_noexcept_stage_v =
    noexcept(std::declval<Stage>()(std::declval<Input>()));

} // namespace pb
