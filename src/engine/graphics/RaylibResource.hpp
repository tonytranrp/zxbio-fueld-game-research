#pragma once

#include <raylib.h>
#include <utility>

namespace biofuel::engine::graphics {

class TextureResource final {
public:
    TextureResource() = default;
    explicit TextureResource(Texture2D texture) noexcept : m_texture(texture) {}
    ~TextureResource() noexcept { reset(); }

    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;

    TextureResource(TextureResource&& other) noexcept
        : m_texture(std::exchange(other.m_texture, Texture2D{})) {}

    TextureResource& operator=(TextureResource&& other) noexcept {
        if (this != &other) {
            reset();
            m_texture = std::exchange(other.m_texture, Texture2D{});
        }
        return *this;
    }

    void reset(Texture2D texture = {}) noexcept {
        if (m_texture.id != 0U) {
            UnloadTexture(m_texture);
        }
        m_texture = texture;
    }

    [[nodiscard]] Texture2D& get() noexcept { return m_texture; }
    [[nodiscard]] const Texture2D& get() const noexcept { return m_texture; }
    [[nodiscard]] bool valid() const noexcept { return m_texture.id != 0U; }
    [[nodiscard]] i32 width() const noexcept { return m_texture.width; }
    [[nodiscard]] i32 height() const noexcept { return m_texture.height; }

    void update(const void* pixels) noexcept {
        if (valid()) {
            UpdateTexture(m_texture, pixels);
        }
    }

private:
    Texture2D m_texture{};
};

class ImageResource final {
public:
    ImageResource() = default;
    explicit ImageResource(Image image) noexcept : m_image(image) {}
    ~ImageResource() noexcept { reset(); }

    ImageResource(const ImageResource&) = delete;
    ImageResource& operator=(const ImageResource&) = delete;

    ImageResource(ImageResource&& other) noexcept
        : m_image(std::exchange(other.m_image, Image{})) {}

    ImageResource& operator=(ImageResource&& other) noexcept {
        if (this != &other) {
            reset();
            m_image = std::exchange(other.m_image, Image{});
        }
        return *this;
    }

    void reset(Image image = {}) noexcept {
        if (m_image.data != nullptr) {
            UnloadImage(m_image);
        }
        m_image = image;
    }

    [[nodiscard]] Image& get() noexcept { return m_image; }
    [[nodiscard]] const Image& get() const noexcept { return m_image; }
    [[nodiscard]] bool valid() const noexcept { return m_image.data != nullptr; }
    [[nodiscard]] i32 width() const noexcept { return m_image.width; }
    [[nodiscard]] i32 height() const noexcept { return m_image.height; }

private:
    Image m_image{};
};

} // namespace biofuel::engine::graphics
