#pragma once

#include "engine/core/Types.hpp"
#include <string_view>

namespace biofuel::engine::custom::procedural {

template<typename TTag>
struct ProceduralTypedId {
    using Tag = TTag;
    u32 value = 0U;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return value != 0U;
    }

    [[nodiscard]] friend constexpr bool operator==(ProceduralTypedId, ProceduralTypedId) noexcept = default;
};

template<typename TResource>
struct ProceduralHandle {
    u32 index = 0U;
    u32 generation = 0U;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return generation != 0U;
    }

    [[nodiscard]] friend constexpr bool operator==(ProceduralHandle, ProceduralHandle) noexcept = default;
};

template<typename TModule>
struct ProceduralModuleTraits {
    static constexpr std::string_view name = "unnamed-procedural-module";
};

struct ProceduralDirtyFlag {
    bool dirty = true;

    void mark() noexcept {
        dirty = true;
    }

    [[nodiscard]] bool consume() noexcept {
        const bool wasDirty = dirty;
        dirty = false;
        return wasDirty;
    }
};

} // namespace biofuel::engine::custom::procedural
