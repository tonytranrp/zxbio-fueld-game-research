#include "game/presentation/hands/HandPreviewTexture.hpp"

#include <limits>
#include <utility>

namespace biofuel::game::presentation::hands {

void HandPreviewTexture::update(
    ::biofuel::engine::vision::hand_tracking::HandTrackingService& tracking) noexcept
{
    const auto status = tracking.status();
    if (!status.previewEnabled) {
        return;
    }

    const auto preview = tracking.latestPreviewFrameAfter(m_sequence);
    if (!preview) {
        return;
    }

    const bool hasRgbaPreview = !(*preview)->rgbaBytes.empty() && (*preview)->width > 0U && (*preview)->height > 0U;
    const bool hasJpegPreview = !(*preview)->jpegBytes.empty();
    if (!hasRgbaPreview && !hasJpegPreview) {
        return;
    }

    if (hasRgbaPreview) {
        const i32 width = static_cast<i32>((*preview)->width);
        const i32 height = static_cast<i32>((*preview)->height);
        const usize requiredBytes = static_cast<usize>(width) * static_cast<usize>(height) * 4U;
        if ((*preview)->rgbaBytes.size() < requiredBytes) {
            return;
        }

        if (m_texture.valid() && m_texture.width() == width && m_texture.height() == height) {
            m_texture.update((*preview)->rgbaBytes.data());
            m_sequence = (*preview)->sequence;
            return;
        }

        Image image{
            .data = const_cast<u8*>((*preview)->rgbaBytes.data()),
            .width = width,
            .height = height,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        };
        auto texture = ::biofuel::engine::graphics::TextureResource::fromImage(image);
        if (!texture.valid()) {
            return;
        }

        m_texture = std::move(texture);
        m_sequence = (*preview)->sequence;
        return;
    }

    if ((*preview)->jpegBytes.size() > static_cast<usize>(std::numeric_limits<i32>::max())) {
        return;
    }

    auto image = ::biofuel::engine::graphics::ImageResource::loadFromMemory(
        ".jpg",
        (*preview)->jpegBytes.data(),
        static_cast<i32>((*preview)->jpegBytes.size()));
    if (!image.valid()) {
        return;
    }
    ImageFormat(&image.get(), PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    if (m_texture.valid() && m_texture.width() == image.width() && m_texture.height() == image.height()) {
        m_texture.update(image.get().data);
        m_sequence = (*preview)->sequence;
        return;
    }

    auto texture = ::biofuel::engine::graphics::TextureResource::fromImage(image.get());
    if (!texture.valid()) {
        return;
    }

    m_texture = std::move(texture);
    m_sequence = (*preview)->sequence;
}

void HandPreviewTexture::release() noexcept {
    m_texture.reset();
    m_sequence = std::numeric_limits<u64>::max();
}

} // namespace biofuel::game::presentation::hands
