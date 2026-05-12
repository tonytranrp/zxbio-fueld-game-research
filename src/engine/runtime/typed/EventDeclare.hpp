#pragma once

#include "engine/core/typed/TypedModule.hpp"
#include "engine/runtime/typed/EventBase.hpp"
#include <string_view>

#define BIOFUEL_EVENT_TAG(TAG_NAME, PAYLOAD_TYPE) \
    struct TAG_NAME : ::biofuel::engine::runtime::typed::EventTag<PAYLOAD_TYPE> {}

#define BIOFUEL_EVENT_SPEC(TAG_TYPE, LABEL) \
    template<> struct EventSpec<TAG_TYPE> { \
        using Payload = typename TAG_TYPE::Payload; \
        static constexpr std::string_view Name = LABEL; \
    }

#define BIOFUEL_EVENT_MODULE(MODULE_NAME, REGISTRY_ALIAS, ...) \
    namespace events { \
    BIOFUEL_TYPED_REGISTRY_MODULE(MODULE_NAME, __VA_ARGS__); \
    } \
    BIOFUEL_TYPED_MODULE(event, REGISTRY_ALIAS, ::biofuel::engine::runtime::typed::events::MODULE_NAME)

