#pragma once

#include <string_view>

namespace biofuel::engine::runtime::typed {

template<typename TPayload>
struct EventTag {
    using Payload = TPayload;
};

template<typename TEvent>
struct EventSpec;

} // namespace biofuel::engine::runtime::typed

