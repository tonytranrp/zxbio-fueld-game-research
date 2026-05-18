#include "game/presentation/hands/HandPreviewTexture.hpp"

#include <limits>

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

        if (m_texture.id != 0U && m_texture.width == width && m_texture.height == height) {
            UpdateTexture(m_texture, (*preview)->rgbaBytes.data());
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
        Texture2D texture = LoadTextureFromImage(image);
        if (texture.id == 0U) {
            return;
        }

        release();
        m_texture = texture;
        m_sequence = (*preview)->sequence;
        return;
    }

    if ((*preview)->jpegBytes.size() > static_cast<usize>(std::numeric_limits<i32>::max())) {
        return;
    }

    Image image = LoadImageFromMemory(".jpg", (*preview)->jpegBytes.data(), static_cast<i32>((*preview)->jpegBytes.size()));
    if (image.data == nullptr) {
        return;
    }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    if (m_texture.id != 0U && m_texture.width == image.width && m_texture.height == image.height) {
        UpdateTexture(m_texture, image.data);
        UnloadImage(image);
        m_sequence = (*preview)->sequence;
        return;
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id == 0U) {
        return;
    }

    release();
    m_texture = texture;
    m_sequence = (*preview)->sequence;
}

void HandPreviewTexture::release() noexcept {
    if (m_texture.id == 0U) {
        return;
    }
    UnloadTexture(m_texture);
    m_texture = Texture2D{};
    m_sequence = std::numeric_limits<u64>::max();
}

} // namespace biofuel::game::presentation::hands
