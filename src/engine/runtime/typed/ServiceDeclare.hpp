#pragma once

#include "engine/core/typed/TypedModule.hpp"
#include "engine/runtime/typed/ServiceBase.hpp"
#include <string_view>

#define BIOFUEL_SERVICE_TAG(TAG_NAME) \
    struct TAG_NAME {}

#define BIOFUEL_SERVICE_SPEC(TAG_TYPE, LABEL) \
    template<> struct ServiceSpec<TAG_TYPE> { \
        using Tag = TAG_TYPE; \
        static constexpr std::string_view Name = LABEL; \
    }

#define BIOFUEL_RUNTIME_SERVICE(TAG_TYPE, LABEL, BACKEND_TYPE, EXPR) \
    BIOFUEL_SERVICE_SPEC(TAG_TYPE, LABEL); \
    template<> struct ServiceModule<TAG_TYPE> { \
        using Service = TAG_TYPE; \
        using Backend = BACKEND_TYPE; \
        static Backend& get() { return (EXPR); } \
    }

#define BIOFUEL_STATIC_SERVICE(TAG_TYPE, LABEL, BACKEND_TYPE) \
    BIOFUEL_SERVICE_SPEC(TAG_TYPE, LABEL); \
    template<> struct ServiceModule<TAG_TYPE> { \
        using Service = TAG_TYPE; \
        using Backend = BACKEND_TYPE; \
        static Backend& get() { \
            static Backend backend{}; \
            return backend; \
        } \
    }

#define BIOFUEL_SERVICE_MODULE(MODULE_NAME, ...) \
    namespace services { \
    BIOFUEL_TYPED_REGISTRY_MODULE(MODULE_NAME, __VA_ARGS__); \
    } \
    BIOFUEL_TYPED_MODULE(service, AppServiceRegistry, ::biofuel::engine::runtime::typed::services::MODULE_NAME)

