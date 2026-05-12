#pragma once

#include "engine/core/Types.hpp"
#include "engine/debug/MemoryTelemetry.hpp"
#include <raylib.h>

namespace biofuel::engine::graphics {

class RenderSurface final {
public:
    RenderSurface() = default;
    ~RenderSurface() noexcept {
        release();
    }

    RenderSurface(const RenderSurface&) = delete;
    RenderSurface& operator=(const RenderSurface&) = delete;
    RenderSurface(RenderSurface&&) = delete;
    RenderSurface& operator=(RenderSurface&&) = delete;

    void ensureSize(i32 width, i32 height) {
        if (width <= 0 || height <= 0) {
            return;
        }

        if (m_target.id > 0 && m_width == width && m_height == height) {
            return;
        }

        release();
        m_target = LoadRenderTexture(width, height);
        if (m_target.id == 0) {
            // GPU allocation failed — leave surface in invalid state
            m_width = 0;
            m_height = 0;
            m_estimatedBytes = 0;
            return;
        }
        m_width = width;
        m_height = height;
        m_estimatedBytes = static_cast<i64>(width) * static_cast<i64>(height) * 4;
        ::biofuel::engine::debug::MemoryTelemetry::add(
            ::biofuel::engine::debug::ResourceKind::RenderSurface,
            1,
            m_estimatedBytes);
    }

    void release() noexcept {
        if (m_target.id > 0) {
            UnloadRenderTexture(m_target);
            m_target = {};
            ::biofuel::engine::debug::MemoryTelemetry::remove(
                ::biofuel::engine::debug::ResourceKind::RenderSurface,
                1,
                m_estimatedBytes);
        }
        m_width = 0;
        m_height = 0;
        m_estimatedBytes = 0;
    }

    [[nodiscard]] bool valid() const noexcept { return m_target.id > 0; }
    [[nodiscard]] i32 width() const noexcept { return m_width; }
    [[nodiscard]] i32 height() const noexcept { return m_height; }
    [[nodiscard]] RenderTexture2D target() const noexcept { return m_target; }
    [[nodiscard]] Texture2D texture() const noexcept { return m_target.texture; }

private:
    RenderTexture2D m_target{};
    i32 m_width = 0;
    i32 m_height = 0;
    i64 m_estimatedBytes = 0;
};

} // namespace biofuel::engine::graphics
