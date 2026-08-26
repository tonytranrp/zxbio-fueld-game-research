#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pb::meta {

template <class... Ts>
struct type_list {};

template <class List>
struct size;

template <class... Ts>
struct size<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template <class List>
inline constexpr std::size_t size_v = size<List>::value;

template <class List, class T>
struct push_back;

template <class... Ts, class T>
struct push_back<type_list<Ts...>, T> { using type = type_list<Ts..., T>; };

template <class List, class T>
using push_back_t = typename push_back<List, T>::type;

template <class T, class List>
struct push_front;

template <class T, class... Ts>
struct push_front<T, type_list<Ts...>> { using type = type_list<T, Ts...>; };

template <class T, class List>
using push_front_t = typename push_front<T, List>::type;

namespace detail {

template <class List, class T, class IndexSequence>
struct replace_back_impl;

template <class... Ts, class T, std::size_t... Is>
struct replace_back_impl<type_list<Ts...>, T, std::index_sequence<Is...>> {
  using tuple_t = std::tuple<Ts...>;
  static constexpr std::size_t last = sizeof...(Ts) - 1;
  using type = type_list<std::conditional_t<Is < last, std::tuple_element_t<Is, tuple_t>, T>...>;
};

} // namespace detail

template <class List, class T>
struct replace_back;

template <class T, class U>
struct replace_back<type_list<T>, U> { using type = type_list<U>; };

template <class... Ts, class T>
struct replace_back<type_list<Ts...>, T> {
  using type = typename detail::replace_back_impl<
      type_list<Ts...>, T, std::make_index_sequence<sizeof...(Ts)>>::type;
};

template <class List, class T>
using replace_back_t = typename replace_back<List, T>::type;

template <class List, class T>
struct contains;

template <class... Ts, class T>
struct contains<type_list<Ts...>, T> : std::bool_constant<(std::is_same_v<Ts, T> || ...)> {};

template <class List, template <class> class F>
struct transform;

template <class... Ts, template <class> class F>
struct transform<type_list<Ts...>, F> { using type = type_list<typename F<Ts>::type...>; };

/// Alias for `transform<List, F>::type` — completes the `_t` convention shared
/// by every other type-mapping trait in this header.
template <class List, template <class> class F>
using transform_t = typename transform<List, F>::type;

template <class List, template <class> class Pred>
struct all_of;

template <class... Ts, template <class> class Pred>
struct all_of<type_list<Ts...>, Pred> : std::bool_constant<(Pred<Ts>::value && ...)> {};

template <class List, std::size_t Index>
struct at;

template <class... Ts, std::size_t Index>
struct at<type_list<Ts...>, Index> {
  static_assert(Index < sizeof...(Ts), "pb::meta::at index out of range");
  using type = std::tuple_element_t<Index, std::tuple<Ts...>>;
};

template <class List, std::size_t Index>
using at_t = typename at<List, Index>::type;

template <class List>
using front_t = at_t<List, 0>;

template <class List>
using back_t = at_t<List, size_v<List> - 1>;

template <class List, class T>
inline constexpr bool contains_v = contains<List, T>::value;

template <class L1, class L2>
struct concat;

template <class... Ts, class... Us>
struct concat<type_list<Ts...>, type_list<Us...>> {
  using type = type_list<Ts..., Us...>;
};

template <class L1, class L2>
using concat_t = typename concat<L1, L2>::type;

template <class List, template <class> class Pred>
struct any_of;

template <class... Ts, template <class> class Pred>
struct any_of<type_list<Ts...>, Pred> : std::bool_constant<(Pred<Ts>::value || ...)> {};

template <class List, template <class> class Pred>
inline constexpr bool any_of_v = any_of<List, Pred>::value;

template <class List, template <class> class Pred>
struct none_of;

template <class... Ts, template <class> class Pred>
struct none_of<type_list<Ts...>, Pred> : std::bool_constant<(!Pred<Ts>::value && ...)> {};

template <class List, template <class> class Pred>
inline constexpr bool none_of_v = none_of<List, Pred>::value;

template <class List, template <class> class Pred>
struct count;

template <class... Ts, template <class> class Pred>
struct count<type_list<Ts...>, Pred> : std::integral_constant<std::size_t, ((Pred<Ts>::value ? 1 : 0) + ...)> {};

template <class List, template <class> class Pred>
inline constexpr std::size_t count_v = count<List, Pred>::value;

template <class List, template <class> class Pred, class Default = void>
struct find_if;

template <template <class> class Pred, class Default>
struct find_if<type_list<>, Pred, Default> {
  using type = Default;
};

template <class Head, class... Tail, template <class> class Pred, class Default>
struct find_if<type_list<Head, Tail...>, Pred, Default> {
  using type = std::conditional_t<
    Pred<Head>::value,
    Head,
    typename find_if<type_list<Tail...>, Pred, Default>::type
  >;
};

template <class List, template <class> class Pred, class Default = void>
using find_if_t = typename find_if<List, Pred, Default>::type;

template <class List, class Result = type_list<>>
struct unique;

template <class Result>
struct unique<type_list<>, Result> {
  using type = Result;
};

template <class Head, class... Tail, class Result>
struct unique<type_list<Head, Tail...>, Result> {
  using type = std::conditional_t<
    contains_v<Result, Head>,
    typename unique<type_list<Tail...>, Result>::type,
    typename unique<type_list<Tail...>, push_back_t<Result, Head>>::type
  >;
};

template <class List>
using unique_t = typename unique<List>::type;

template <class List, template <class> class Pred, class Result = type_list<>>
struct filter;

template <template <class> class Pred, class Result>
struct filter<type_list<>, Pred, Result> {
  using type = Result;
};

template <class Head, class... Tail, template <class> class Pred, class Result>
struct filter<type_list<Head, Tail...>, Pred, Result> {
  using type = std::conditional_t<
    Pred<Head>::value,
    typename filter<type_list<Tail...>, Pred, push_back_t<Result, Head>>::type,
    typename filter<type_list<Tail...>, Pred, Result>::type
  >;
};

template <class List, template <class> class Pred>
using filter_t = typename filter<List, Pred>::type;

} // namespace pb::meta

namespace pb::core::meta {
using ::pb::meta::all_of;
using ::pb::meta::any_of;
using ::pb::meta::any_of_v;
using ::pb::meta::at;
using ::pb::meta::at_t;
using ::pb::meta::back_t;
using ::pb::meta::concat;
using ::pb::meta::concat_t;
using ::pb::meta::contains;
using ::pb::meta::contains_v;
using ::pb::meta::count;
using ::pb::meta::count_v;
using ::pb::meta::filter;
using ::pb::meta::filter_t;
using ::pb::meta::find_if;
using ::pb::meta::find_if_t;
using ::pb::meta::front_t;
using ::pb::meta::none_of;
using ::pb::meta::none_of_v;
using ::pb::meta::push_back;
using ::pb::meta::push_back_t;
using ::pb::meta::push_front;
using ::pb::meta::push_front_t;
using ::pb::meta::replace_back;
using ::pb::meta::replace_back_t;
using ::pb::meta::size;
using ::pb::meta::size_v;
using ::pb::meta::transform;
using ::pb::meta::transform_t;
using ::pb::meta::type_list;
using ::pb::meta::unique;
using ::pb::meta::unique_t;
} // namespace pb::core::meta
