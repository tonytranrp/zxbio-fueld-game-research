#pragma once

#include "engine/core/Types.hpp"
#include "engine/custom/procedural/core/ProceduralTypes.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <raylib.h>

namespace biofuel::engine::custom::procedural::materials {

struct ProceduralTextureTag {};
using TextureHandle = ProceduralHandle<ProceduralTextureTag>;

enum class TexturePattern : u8 {
    Solid,
    Checked,
    Stripes,
};

class ProceduralTextureCache final {
public:
    ProceduralTextureCache() = default;
    ProceduralTextureCache(const ProceduralTextureCache&) = delete;
    ProceduralTextureCache& operator=(const ProceduralTextureCache&) = delete;
    ProceduralTextureCache(ProceduralTextureCache&&) = delete;
    ProceduralTextureCache& operator=(ProceduralTextureCache&&) = delete;
    ~ProceduralTextureCache();

    [[nodiscard]] TextureHandle generated(TexturePattern pattern, Color a, Color b = Color{255, 255, 255, 255});
    [[nodiscard]] TextureHandle fromPngOrGenerated(
        const std::filesystem::path& path,
        TexturePattern fallbackPattern,
        Color a,
        Color b = Color{255, 255, 255, 255});

    [[nodiscard]] Texture2D* texture(TextureHandle handle) noexcept;
    void clear() noexcept;

private:
    struct TextureRecord {
        TexturePattern pattern = TexturePattern::Solid;
        Color a{};
        Color b{};
        std::string sourcePath{};
        Texture2D texture{};
        bool loaded = false;
    };

    [[nodiscard]] TextureHandle addRecord(TextureRecord record);
    [[nodiscard]] static bool sameColor(Color a, Color b) noexcept;
    [[nodiscard]] static Image buildImage(TexturePattern pattern, Color a, Color b);

    std::array<TextureRecord, 32> m_records{};
    u32 m_count = 0U;
    u32 m_generation = 1U;
};

} // namespace biofuel::engine::custom::procedural::materials
