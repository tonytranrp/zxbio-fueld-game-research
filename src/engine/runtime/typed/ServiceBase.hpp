#pragma once

#include "engine/core/typed/Meta.hpp"
#include <concepts>
#include <string_view>

namespace biofuel::engine::runtime::typed {

template<typename TService>
struct ServiceSpec;

template<typename TService>
struct ServiceModule;

template<typename TRegistry>
class GameServices;

template<typename TService>
concept RegisteredService = requires {
    typename ServiceSpec<TService>::Tag;
    { ServiceSpec<TService>::Name } -> std::convertible_to<std::string_view>;
    typename ServiceModule<TService>::Backend;
    { ServiceModule<TService>::get() } -> std::same_as<typename ServiceModule<TService>::Backend&>;
};

} // namespace biofuel::engine::runtime::typed

