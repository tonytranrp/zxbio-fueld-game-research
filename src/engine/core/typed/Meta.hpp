#pragma once

#include "engine/core/Types.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace biofuel::typed {

template<typename... Ts>
struct TypeList {
    static constexpr std::size_t size = sizeof...(Ts);
};

template<typename T, typename TList>
struct Contains;

template<typename T, typename... Ts>
struct Contains<T, TypeList<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template<typename T, typename TList>
inline constexpr bool ContainsV = Contains<T, TList>::value;

template<typename T, typename TList>
struct TypeIndex;

template<typename T, typename... Ts>
struct TypeIndex<T, TypeList<Ts...>> {
private:
    static consteval std::size_t compute() {
        std::size_t index = 0;
        bool found = false;
        ((std::is_same_v<T, Ts> ? (found = true, false) : (!found ? (++index, false) : false)), ...);
        return found ? index : static_cast<std::size_t>(-1);
    }

public:
    static constexpr std::size_t value = compute();
    static_assert(value != static_cast<std::size_t>(-1), "Type is not registered in this TypeList.");
};

template<typename T, typename TList>
inline constexpr std::size_t TypeIndexV = TypeIndex<T, TList>::value;

template<typename... Ts>
struct UniqueTypes : std::true_type {};

template<typename T, typename... Rest>
struct UniqueTypes<T, Rest...>
    : std::bool_constant<(!(std::is_same_v<T, Rest> || ...)) && UniqueTypes<Rest...>::value> {};

template<typename... Ts>
consteval bool uniqueTypes(TypeList<Ts...>) {
    return UniqueTypes<Ts...>::value;
}

template<typename... Ts>
struct Registry {
    using Types = TypeList<Ts...>;
    static constexpr std::size_t size = sizeof...(Ts);

    template<typename T>
    static constexpr bool contains = ContainsV<T, Types>;

    template<typename T>
    static constexpr std::size_t index = TypeIndexV<T, Types>;

    static consteval bool valid() {
        return uniqueTypes(Types{});
    }
};

template<typename... TRegistries>
struct RegistryConcat;

template<>
struct RegistryConcat<> {
    using Type = Registry<>;
};

template<typename... Ts>
struct RegistryConcat<Registry<Ts...>> {
    using Type = Registry<Ts...>;
};

template<typename... Left, typename... Right, typename... Rest>
struct RegistryConcat<Registry<Left...>, Registry<Right...>, Rest...> {
    using Type = typename RegistryConcat<Registry<Left..., Right...>, Rest...>::Type;
};

template<typename... TRegistries>
using RegistryConcatT = typename RegistryConcat<TRegistries...>::Type;

template<typename T>
struct TypedId {
    using Type = T;
};

template<typename T>
struct NameOf {
    static constexpr std::string_view value = T::Name;
};

template<typename TModule>
struct ModuleTraits {
    using Module = TModule;
    static constexpr bool valid = requires {
        typename TModule::Tag;
        { TModule::Name } -> std::convertible_to<std::string_view>;
    };
};

} // namespace biofuel::typed
