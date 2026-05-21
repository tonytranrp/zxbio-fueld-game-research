#pragma once

#include "engine/core/Types.hpp"
#include "engine/graphics/RaylibResource.hpp"
#include "engine/vision/hand_tracking/HandTrackingService.hpp"
#include <limits>
#include <raylib.h>

namespace biofuel::game::presentation::hands {

class HandPreviewTexture final {
public:
    HandPreviewTexture() = default;
    ~HandPreviewTexture() noexcept { release(); }

    HandPreviewTexture(const HandPreviewTexture&) = delete;
    HandPreviewTexture& operator=(const HandPreviewTexture&) = delete;
    HandPreviewTexture(HandPreviewTexture&&) = delete;
    HandPreviewTexture& operator=(HandPreviewTexture&&) = delete;

    void update(::biofuel::engine::vision::hand_tracking::HandTrackingService& tracking) noexcept;
    void release() noexcept;

    [[nodiscard]] Texture2D texture() const noexcept { return m_texture.get(); }
    [[nodiscard]] bool valid() const noexcept { return m_texture.valid(); }
    [[nodiscard]] u64 sequence() const noexcept { return m_sequence; }

private:
    ::biofuel::engine::graphics::TextureResource m_texture;
    u64 m_sequence = std::numeric_limits<u64>::max();
};

} // namespace biofuel::game::presentation::hands
