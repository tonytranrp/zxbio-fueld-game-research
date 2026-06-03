#include "engine/world/Terrain3D.hpp"

#include <algorithm>
#include <cmath>

namespace biofuel::engine::world {

namespace {

// --- Deterministic value noise -------------------------------------------------

[[nodiscard]] f32 hashToUnit(i32 x, i32 z, u32 seed) noexcept {
    u32 h = static_cast<u32>(x) * 374761393U + static_cast<u32>(z) * 668265263U + seed * 362437U;
    h = (h ^ (h >> 13)) * 1274126177U;
    h ^= h >> 16;
    return static_cast<f32>(h) / static_cast<f32>(0xFFFFFFFFU);
}

[[nodiscard]] f32 smoothstep01(f32 t) noexcept {
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] f32 valueNoise(f32 x, f32 z, u32 seed) noexcept {
    const f32 xf = std::floor(x);
    const f32 zf = std::floor(z);
    const auto xi = static_cast<i32>(xf);
    const auto zi = static_cast<i32>(zf);
    const f32 tx = smoothstep01(x - xf);
    const f32 tz = smoothstep01(z - zf);

    const f32 c00 = hashToUnit(xi, zi, seed);
    const f32 c10 = hashToUnit(xi + 1, zi, seed);
    const f32 c01 = hashToUnit(xi, zi + 1, seed);
    const f32 c11 = hashToUnit(xi + 1, zi + 1, seed);

    const f32 a = c00 + (c10 - c00) * tx;
    const f32 b = c01 + (c11 - c01) * tx;
    return a + (b - a) * tz;
}

[[nodiscard]] u8 lerpChannel(u8 a, u8 b, f32 t) noexcept {
    return static_cast<u8>(static_cast<f32>(a) + (static_cast<f32>(b) - static_cast<f32>(a)) * t);
}

[[nodiscard]] Color lerpColor(Color a, Color b, f32 t) noexcept {
    return Color{
        lerpChannel(a.r, b.r, t),
        lerpChannel(a.g, b.g, t),
        lerpChannel(a.b, b.b, t),
        255U,
    };
}

} // namespace

Terrain3D::~Terrain3D() noexcept {
    unload();
}

f32 Terrain3D::sampleHeight(const i32 col, const i32 row) const noexcept {
    const i32 c = std::clamp(col, 0, m_cols - 1);
    const i32 r = std::clamp(row, 0, m_rows - 1);
    return m_heights[static_cast<usize>(r) * static_cast<usize>(m_cols) + static_cast<usize>(c)];
}

void Terrain3D::generate(const Config& config) {
    unload();
    m_config = config;
    m_cols = config.width + 1;
    m_rows = config.depth + 1;

    // --- Build the height field with fractal value noise ---
    m_heights.assign(static_cast<usize>(m_cols) * static_cast<usize>(m_rows), 0.0f);
    for (i32 r = 0; r < m_rows; ++r) {
        for (i32 c = 0; c < m_cols; ++c) {
            f32 amplitude = 1.0f;
            f32 frequency = config.frequency;
            f32 sum = 0.0f;
            f32 norm = 0.0f;
            for (u32 octave = 0U; octave < config.octaves; ++octave) {
                const f32 n = valueNoise(
                    static_cast<f32>(c) * frequency,
                    static_cast<f32>(r) * frequency,
                    config.seed + octave * 1013U);
                sum += n * amplitude;
                norm += amplitude;
                amplitude *= config.persistence;
                frequency *= 2.0f;
            }
            f32 unit = (norm > 0.0f) ? (sum / norm) : 0.0f;   // [0,1]
            // Bias toward gentle valleys with occasional peaks.
            unit = unit * unit * (3.0f - 2.0f * unit);
            m_heights[static_cast<usize>(r) * static_cast<usize>(m_cols) + static_cast<usize>(c)] =
                unit * config.maxHeight;
        }
    }

    // --- Build the GPU mesh (positions, normals, colors, uvs, indices) ---
    const i32 vertexCount = m_cols * m_rows;
    const i32 quadCols = m_cols - 1;
    const i32 quadRows = m_rows - 1;
    const i32 triangleCount = quadCols * quadRows * 2;

    Mesh mesh{};
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = static_cast<f32*>(MemAlloc(static_cast<u32>(vertexCount) * 3U * sizeof(f32)));
    mesh.normals = static_cast<f32*>(MemAlloc(static_cast<u32>(vertexCount) * 3U * sizeof(f32)));
    mesh.texcoords = static_cast<f32*>(MemAlloc(static_cast<u32>(vertexCount) * 2U * sizeof(f32)));
    mesh.colors = static_cast<u8*>(MemAlloc(static_cast<u32>(vertexCount) * 4U * sizeof(u8)));
    mesh.indices = static_cast<u16*>(MemAlloc(static_cast<u32>(triangleCount) * 3U * sizeof(u16)));

    const f32 invMax = (config.maxHeight > 0.0f) ? (1.0f / config.maxHeight) : 0.0f;

    for (i32 r = 0; r < m_rows; ++r) {
        for (i32 c = 0; c < m_cols; ++c) {
            const i32 v = r * m_cols + c;
            const f32 h = sampleHeight(c, r);

            mesh.vertices[v * 3 + 0] = static_cast<f32>(c);
            mesh.vertices[v * 3 + 1] = h;
            mesh.vertices[v * 3 + 2] = static_cast<f32>(r);

            // Normal from central differences of neighboring heights.
            const f32 hL = sampleHeight(c - 1, r);
            const f32 hR = sampleHeight(c + 1, r);
            const f32 hU = sampleHeight(c, r - 1);
            const f32 hD = sampleHeight(c, r + 1);
            Vector3 normal{hL - hR, 2.0f, hU - hD};
            const f32 len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (len > 0.0001f) {
                normal.x /= len;
                normal.y /= len;
                normal.z /= len;
            }
            mesh.normals[v * 3 + 0] = normal.x;
            mesh.normals[v * 3 + 1] = normal.y;
            mesh.normals[v * 3 + 2] = normal.z;

            mesh.texcoords[v * 2 + 0] = static_cast<f32>(c);
            mesh.texcoords[v * 2 + 1] = static_cast<f32>(r);

            // Height + slope based coloring so the surface reads as 3D unlit.
            const f32 t = std::clamp(h * invMax, 0.0f, 1.0f);
            Color base;
            if (t < 0.4f) {
                base = lerpColor(config.lowColor, config.midColor, t / 0.4f);
            } else if (t < 0.72f) {
                base = lerpColor(config.midColor, config.highColor, (t - 0.4f) / 0.32f);
            } else {
                base = lerpColor(config.highColor, config.peakColor, (t - 0.72f) / 0.28f);
            }
            // Fake directional shade: flatter ground brighter, slopes darker.
            const f32 shade = 0.72f + 0.28f * std::clamp(normal.y, 0.0f, 1.0f);
            mesh.colors[v * 4 + 0] = static_cast<u8>(static_cast<f32>(base.r) * shade);
            mesh.colors[v * 4 + 1] = static_cast<u8>(static_cast<f32>(base.g) * shade);
            mesh.colors[v * 4 + 2] = static_cast<u8>(static_cast<f32>(base.b) * shade);
            mesh.colors[v * 4 + 3] = 255U;
        }
    }

    i32 index = 0;
    for (i32 r = 0; r < quadRows; ++r) {
        for (i32 c = 0; c < quadCols; ++c) {
            const u16 topLeft = static_cast<u16>(r * m_cols + c);
            const u16 topRight = static_cast<u16>(r * m_cols + c + 1);
            const u16 bottomLeft = static_cast<u16>((r + 1) * m_cols + c);
            const u16 bottomRight = static_cast<u16>((r + 1) * m_cols + c + 1);

            mesh.indices[index++] = topLeft;
            mesh.indices[index++] = bottomLeft;
            mesh.indices[index++] = topRight;

            mesh.indices[index++] = topRight;
            mesh.indices[index++] = bottomLeft;
            mesh.indices[index++] = bottomRight;
        }
    }

    UploadMesh(&mesh, false);
    m_model = LoadModelFromMesh(mesh);
    m_loaded = true;
}

void Terrain3D::unload() noexcept {
    if (m_loaded) {
        UnloadModel(m_model);
        m_model = Model{};
        m_loaded = false;
    }
    m_heights.clear();
    m_cols = 0;
    m_rows = 0;
}

f32 Terrain3D::heightAt(const f32 worldX, const f32 worldZ) const noexcept {
    if (!m_loaded || m_cols < 2 || m_rows < 2) {
        return 0.0f;
    }
    const f32 x = std::clamp(worldX, 0.0f, static_cast<f32>(m_cols - 1) - 0.001f);
    const f32 z = std::clamp(worldZ, 0.0f, static_cast<f32>(m_rows - 1) - 0.001f);
    const auto c0 = static_cast<i32>(std::floor(x));
    const auto r0 = static_cast<i32>(std::floor(z));
    const f32 fx = x - static_cast<f32>(c0);
    const f32 fz = z - static_cast<f32>(r0);

    const f32 h00 = sampleHeight(c0, r0);
    const f32 h10 = sampleHeight(c0 + 1, r0);
    const f32 h01 = sampleHeight(c0, r0 + 1);
    const f32 h11 = sampleHeight(c0 + 1, r0 + 1);

    const f32 a = h00 + (h10 - h00) * fx;
    const f32 b = h01 + (h11 - h01) * fx;
    return a + (b - a) * fz;
}

void Terrain3D::render() const noexcept {
    if (!m_loaded) {
        return;
    }
    DrawModel(m_model, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
}

} // namespace biofuel::engine::world
