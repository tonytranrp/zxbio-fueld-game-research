#pragma once

#include "engine/runtime/typed/Events.hpp"
#include "engine/runtime/typed/ServiceTags.hpp"

namespace biofuel::engine::runtime::typed {

template<typename TService>
class ServiceRef {
public:
    using Service = TService;
    using Module = ServiceModule<TService>;
    using Backend = typename Module::Backend;

    explicit ServiceRef(Backend& backend) noexcept : m_backend(&backend) {}

    [[nodiscard]] Backend& get() const noexcept { return *m_backend; }
    [[nodiscard]] Backend* operator->() const noexcept { return m_backend; }
    [[nodiscard]] operator Backend&() const noexcept { return *m_backend; }

private:
    Backend* m_backend = nullptr;
};

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

    template<typename TService>
    [[nodiscard]] ServiceRef<TService> ref() const {
        return ServiceRef<TService>{get<TService>()};
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

