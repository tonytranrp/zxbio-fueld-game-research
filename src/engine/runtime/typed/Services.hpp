#pragma once

#include "engine/runtime/typed/Events.hpp"
#include "engine/runtime/typed/ServiceTags.hpp"

namespace biofuel::engine::runtime::typed {

template<typename TRegistry>
class GameServices {
public:
    using Registry = TRegistry;

    template<typename TService>
    [[nodiscard]] typename ServiceModule<TService>::Backend& get() const {
        static_assert(Registry::template contains<TService>, "Service is not registered in this GameServices registry.");
        static_assert(RegisteredService<TService>, "Every service needs ServiceSpec<T> and ServiceModule<T>.");
        return ServiceModule<TService>::get();
    }
};

template<typename TRegistry>
struct ServiceRegistryValidator;

template<typename... TServices>
struct ServiceRegistryValidator<biofuel::typed::Registry<TServices...>> {
    static consteval bool valid() {
        return (RegisteredService<TServices> && ...);
    }
};

static_assert(ServiceRegistryValidator<AppServiceRegistry>::valid());

} // namespace biofuel::engine::runtime::typed

