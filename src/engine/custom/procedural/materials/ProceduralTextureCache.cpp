#include "engine/custom/procedural/materials/ProceduralTextureCache.hpp"

#include <spdlog/spdlog.h>

namespace biofuel::engine::custom::procedural::materials {

ProceduralTextureCache::~ProceduralTextureCache() {
    clear();
}

TextureHandle ProceduralTextureCache::generated(const TexturePattern pattern, const Color a, const Color b) {
    for (u32 index = 0U; index < m_count; ++index) {
        const TextureRecord& record = m_records[index];
        if (record.sourcePath.empty() && record.pattern == pattern && sameColor(record.a, a) && sameColor(record.b, b)) {
            return TextureHandle{.index = index + 1U, .generation = m_generation};
        }
    }

    Image image = buildImage(pattern, a, b);
    TextureRecord record{
        .pattern = pattern,
        .a = a,
        .b = b,
        .sourcePath = {},
        .texture = LoadTextureFromImage(image),
        .loaded = true,
    };
    UnloadImage(image);
    return addRecord(record);
}

TextureHandle ProceduralTextureCache::fromPngOrGenerated(
    const std::filesystem::path& path,
    const TexturePattern fallbackPattern,
    const Color a,
    const Color b)
{
    if (!path.empty() && std::filesystem::exists(path)) {
        const std::string normalized = path.generic_string();
        for (u32 index = 0U; index < m_count; ++index) {
            const TextureRecord& record = m_records[index];
            if (record.sourcePath == normalized) {
                return TextureHandle{.index = index + 1U, .generation = m_generation};
            }
        }

        Texture2D loaded = LoadTexture(path.string().c_str());
        if (loaded.id != 0U) {
            SetTextureFilter(loaded, TEXTURE_FILTER_POINT);
            return addRecord(TextureRecord{
                .pattern = fallbackPattern,
                .a = a,
                .b = b,
                .sourcePath = normalized,
                .texture = loaded,
                .loaded = true,
            });
        }
        spdlog::warn("ProceduralTextureCache: failed to load '{}', using generated fallback", normalized);
    }

    return generated(fallbackPattern, a, b);
}

Texture2D* ProceduralTextureCache::texture(const TextureHandle handle) noexcept {
    if (!handle.valid() || handle.generation != m_generation || handle.index == 0U || handle.index > m_count) {
        return nullptr;
    }
    TextureRecord& record = m_records[handle.index - 1U];
    return record.loaded ? &record.texture : nullptr;
}

void ProceduralTextureCache::clear() noexcept {
    for (u32 index = 0U; index < m_count; ++index) {
        TextureRecord& record = m_records[index];
        if (record.loaded && record.texture.id != 0U) {
            UnloadTexture(record.texture);
        }
        record = {};
    }
    m_count = 0U;
    ++m_generation;
    if (m_generation == 0U) {
        m_generation = 1U;
    }
}

TextureHandle ProceduralTextureCache::addRecord(TextureRecord record) {
    if (m_count >= m_records.size()) {
        spdlog::warn("ProceduralTextureCache: cache full, returning empty texture handle");
        if (record.loaded && record.texture.id != 0U) {
            UnloadTexture(record.texture);
        }
        return {};
    }

    const u32 index = m_count++;
    m_records[index] = record;
    return TextureHandle{.index = index + 1U, .generation = m_generation};
}

bool ProceduralTextureCache::sameColor(const Color a, const Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

Image ProceduralTextureCache::buildImage(const TexturePattern pattern, const Color a, const Color b) {
    switch (pattern) {
    case TexturePattern::Solid:
        return GenImageColor(16, 16, a);
    case TexturePattern::Checked:
        return GenImageChecked(32, 32, 8, 8, a, b);
    case TexturePattern::Stripes:
        {
            Image image = GenImageColor(32, 32, a);
            for (i32 y = 0; y < 32; ++y) {
                if ((y / 4) % 2 == 0) {
                    ImageDrawRectangle(&image, 0, y, 32, 1, b);
                }
            }
            return image;
        }
    }
    return GenImageColor(16, 16, a);
}

} // namespace biofuel::engine::custom::procedural::materials
