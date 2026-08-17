#pragma once

#include "engine/core/Types.hpp"
#include "engine/graphics/RenderSurface.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace biofuel::engine::graphics {

class TransientResourceCache final {
public:
    [[nodiscard]] static TransientResourceCache& instance() noexcept;

    // Returns a shared handle: expiring the cache entry does not destroy the
    // RenderSurface while a caller still holds it.
    [[nodiscard]] std::shared_ptr<RenderSurface> acquireSurface(std::string_view key, i32 width, i32 height);
    void releaseSurface(std::string_view key, f32 ttlSeconds) noexcept;
    void update(f64 nowSeconds) noexcept;
    void releaseAll() noexcept;

    [[nodiscard]] bool hasSurface(std::string_view key) const noexcept;

    TransientResourceCache(const TransientResourceCache&) = delete;
    TransientResourceCache& operator=(const TransientResourceCache&) = delete;
    TransientResourceCache(TransientResourceCache&&) = delete;
    TransientResourceCache& operator=(TransientResourceCache&&) = delete;

private:
    TransientResourceCache() = default;
    ~TransientResourceCache() noexcept = default;

    struct SurfaceEntry {
        std::shared_ptr<RenderSurface> surface;
        f64 expiresAt = 0.0;
        bool leased = false;
    };

    std::unordered_map<std::string, SurfaceEntry, TransparentHash, std::equal_to<>> m_surfaces;
};

} // namespace biofuel::engine::graphics
