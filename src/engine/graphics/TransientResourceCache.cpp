#include "TransientResourceCache.hpp"

#include <raylib.h>

namespace biofuel::engine::graphics {

TransientResourceCache& TransientResourceCache::instance() noexcept {
    static TransientResourceCache cache;
    return cache;
}

std::shared_ptr<RenderSurface> TransientResourceCache::acquireSurface(
    const std::string_view key,
    const i32 width,
    const i32 height)
{
    auto& entry = m_surfaces[std::string{key}];
    if (!entry.surface) {
        entry.surface = std::make_shared<RenderSurface>();
    }

    entry.leased = true;
    entry.expiresAt = 0.0;
    entry.surface->ensureSize(width, height);
    return entry.surface;
}

void TransientResourceCache::releaseSurface(const std::string_view key, const f32 ttlSeconds) noexcept {
    const auto it = m_surfaces.find(key);
    if (it == m_surfaces.end()) {
        return;
    }

    it->second.leased = false;
    it->second.expiresAt = GetTime() + static_cast<f64>(ttlSeconds);
}

void TransientResourceCache::update(const f64 nowSeconds) noexcept {
    for (auto it = m_surfaces.begin(); it != m_surfaces.end(); ) {
        const SurfaceEntry& entry = it->second;
        if (!entry.leased && entry.expiresAt > 0.0 && nowSeconds >= entry.expiresAt) {
            it = m_surfaces.erase(it);
        } else {
            ++it;
        }
    }
}

void TransientResourceCache::releaseAll() noexcept {
    m_surfaces.clear();
}

} // namespace biofuel::engine::graphics
