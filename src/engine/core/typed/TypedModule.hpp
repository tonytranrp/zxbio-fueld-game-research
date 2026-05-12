#pragma once

#include "engine/core/typed/Meta.hpp"
#include "engine/core/typed/ModuleExport.hpp"

#define BIOFUEL_TYPED_REGISTRY_MODULE(MODULE_NAME, ...) \
    struct MODULE_NAME { \
        using Registry = ::biofuel::typed::Registry<__VA_ARGS__>; \
    };
