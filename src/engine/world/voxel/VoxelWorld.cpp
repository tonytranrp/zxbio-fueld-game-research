#include "engine/world/voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <rlgl.h>
#include <vector>

namespace biofuel::engine::world::voxel {

namespace {

// --- Deterministic value-noise fBm -------------------------------------------

[[nodiscard]] f32 hashToUnit(i32 x, i32 z, u32 seed) noexcept {
    u32 h = static_cast<u32>(x) * 374761393U + static_cast<u32>(z) * 668265263U + seed * 362437U;
    h = (h ^ (h >> 13)) * 1274126177U;
    h ^= h >> 16;
    return static_cast<f32>(h) / static_cast<f32>(0xFFFFFFFFU);
}

[[nodiscard]] f32 smooth01(f32 t) noexcept { return t * t * (3.0f - 2.0f * t); }

[[nodiscard]] f32 valueNoise(f32 x, f32 z, u32 seed) noexcept {
    const f32 xf = std::floor(x);
    const f32 zf = std::floor(z);
    const auto xi = static_cast<i32>(xf);
    const auto zi = static_cast<i32>(zf);
    const f32 tx = smooth01(x - xf);
    const f32 tz = smooth01(z - zf);
    const f32 c00 = hashToUnit(xi, zi, seed);
    const f32 c10 = hashToUnit(xi + 1, zi, seed);
    const f32 c01 = hashToUnit(xi, zi + 1, seed);
    const f32 c11 = hashToUnit(xi + 1, zi + 1, seed);
    const f32 a = c00 + (c10 - c00) * tx;
    const f32 b = c01 + (c11 - c01) * tx;
    return a + (b - a) * tz;
}

// --- Deterministic 3D value-noise fBm (cave carving) -------------------------

[[nodiscard]] f32 hashToUnit3(i32 x, i32 y, i32 z, u32 seed) noexcept {
    u32 h = static_cast<u32>(x) * 374761393U + static_cast<u32>(y) * 1325264821U
          + static_cast<u32>(z) * 668265263U + seed * 362437U;
    h = (h ^ (h >> 13)) * 1274126177U;
    h ^= h >> 16;
    return static_cast<f32>(h) / static_cast<f32>(0xFFFFFFFFU);
}

[[nodiscard]] f32 valueNoise3(f32 x, f32 y, f32 z, u32 seed) noexcept {
    const f32 xf = std::floor(x), yf = std::floor(y), zf = std::floor(z);
    const auto xi = static_cast<i32>(xf);
    const auto yi = static_cast<i32>(yf);
    const auto zi = static_cast<i32>(zf);
    const f32 tx = smooth01(x - xf), ty = smooth01(y - yf), tz = smooth01(z - zf);
    auto lp = [](f32 a, f32 b, f32 t) noexcept { return a + (b - a) * t; };
    const f32 c000 = hashToUnit3(xi, yi, zi, seed),     c100 = hashToUnit3(xi + 1, yi, zi, seed);
    const f32 c010 = hashToUnit3(xi, yi + 1, zi, seed), c110 = hashToUnit3(xi + 1, yi + 1, zi, seed);
    const f32 c001 = hashToUnit3(xi, yi, zi + 1, seed), c101 = hashToUnit3(xi + 1, yi, zi + 1, seed);
    const f32 c011 = hashToUnit3(xi, yi + 1, zi + 1, seed), c111 = hashToUnit3(xi + 1, yi + 1, zi + 1, seed);
    const f32 x00 = lp(c000, c100, tx), x10 = lp(c010, c110, tx);
    const f32 x01 = lp(c001, c101, tx), x11 = lp(c011, c111, tx);
    return lp(lp(x00, x10, ty), lp(x01, x11, ty), tz);
}

constexpr f32 kCaveFreq = 0.055f;
constexpr i32 kCaveTopGap = 3;     // carve below the surface crust -> occasional overhangs, no swiss-cheese
constexpr i32 kCaveFloorY = 2;     // lowest carveable Y (keeps a base)
constexpr f32 kCaveThreshold = 0.64f;

// Is (wx,wy,wz) hollowed into a cave/tunnel below the surface crust?
[[nodiscard]] inline bool isCarvedAir(i32 wx, i32 wy, i32 wz, i32 surfaceH, u32 seed) noexcept {
    if (wy < kCaveFloorY) return false;
    if (wy > surfaceH - kCaveTopGap) return false;
    const f32 fx = static_cast<f32>(wx) * kCaveFreq;
    const f32 fy = static_cast<f32>(wy) * kCaveFreq * 1.7f;   // squash vertically -> wider tunnels
    const f32 fz = static_cast<f32>(wz) * kCaveFreq;
    f32 d = valueNoise3(fx, fy, fz, seed ^ 0x9E3779B9U);
    d += 0.5f * valueNoise3(fx * 2.03f, fy * 2.03f, fz * 2.03f, seed ^ 0x85EBCA6BU);
    d *= (1.0f / 1.5f);
    const f32 depth = static_cast<f32>(surfaceH - kCaveTopGap - wy);
    const f32 fade = std::clamp(depth * 0.18f, 0.0f, 1.0f);   // smooth cave roofs
    return (d * fade) > kCaveThreshold;
}

// --- Procedural voxel trees --------------------------------------------------

constexpr i32 kTreeCellSize = 5;
constexpr i32 kTrunkMinH = 4;
constexpr i32 kCanopyRadius = 2;
constexpr i32 kTreeScanCells = 1;

[[nodiscard]] u32 treeHash(i32 cx, i32 cz, u32 seed) noexcept {
    u32 h = static_cast<u32>(cx) * 2654435761U ^ static_cast<u32>(cz) * 2246822519U ^ (seed * 3266489917U);
    h ^= h >> 15; h *= 2246822519U; h ^= h >> 13; h *= 3266489917U; h ^= h >> 16;
    return h;
}

struct TreeInfo { bool present = false; i32 rootX = 0; i32 rootZ = 0; i32 trunkH = 0; };

[[nodiscard]] TreeInfo treeInCell(i32 cx, i32 cz, u32 seed) noexcept {
    const u32 h = treeHash(cx, cz, seed);
    TreeInfo t;
    t.present = (h & 0xFFU) < 76U;   // ~30% of cells host a tree
    t.rootX = cx * kTreeCellSize + static_cast<i32>((h >> 8) & 3U);
    t.rootZ = cz * kTreeCellSize + static_cast<i32>((h >> 10) & 3U);
    t.trunkH = kTrunkMinH + static_cast<i32>((h >> 12) % static_cast<u32>(VoxelWorld::kTrunkMaxH - kTrunkMinH + 1));
    return t;
}

// Returns Wood/Leaves for any world position covered by a nearby tree, else Air.
template <class SurfaceFn, class IsGrassFn>
[[nodiscard]] Block treeBlockAt(i32 wx, i32 wy, i32 wz, u32 seed, SurfaceFn&& surfaceOf, IsGrassFn&& rootIsGrass) noexcept {
    const i32 bcx = static_cast<i32>(std::floor(static_cast<f32>(wx) / static_cast<f32>(kTreeCellSize)));
    const i32 bcz = static_cast<i32>(std::floor(static_cast<f32>(wz) / static_cast<f32>(kTreeCellSize)));
    bool hitLeaf = false;
    for (i32 dz = -kTreeScanCells; dz <= kTreeScanCells; ++dz) {
        for (i32 dx = -kTreeScanCells; dx <= kTreeScanCells; ++dx) {
            const TreeInfo t = treeInCell(bcx + dx, bcz + dz, seed);
            if (!t.present) continue;
            const i32 ddx = wx - t.rootX, ddz = wz - t.rootZ;       // cheap reject before noise
            if (std::abs(ddx) > kCanopyRadius || std::abs(ddz) > kCanopyRadius) continue;
            if (!rootIsGrass(t.rootX, t.rootZ)) continue;
            const i32 ground = surfaceOf(t.rootX, t.rootZ) + 1;
            const i32 trunkTop = ground + t.trunkH - 1;
            if (wx == t.rootX && wz == t.rootZ && wy >= ground && wy <= trunkTop) return Block::Wood;
            const i32 canopyBase = trunkTop - 1, canopyTop = trunkTop + 2;
            if (wy >= canopyBase && wy <= canopyTop) {
                const i32 ddy = wy - (trunkTop + 1);
                const i32 manh = std::abs(ddx) + std::abs(ddz) + std::abs(ddy);
                const i32 rad = (wy >= trunkTop + 1) ? (kCanopyRadius - 1) : kCanopyRadius;
                if (std::abs(ddx) <= rad && std::abs(ddz) <= rad && manh <= kCanopyRadius + 1) {
                    if (!(ddx == 0 && ddz == 0 && wy <= trunkTop)) hitLeaf = true;
                }
            }
        }
    }
    return hitLeaf ? Block::Leaves : Block::Air;
}

// --- Per-face shading + block palette ----------------------------------------

enum Face : i32 { FacePosX = 0, FaceNegX, FacePosY, FaceNegY, FacePosZ, FaceNegZ };

[[nodiscard]] f32 faceShade(i32 face) noexcept {
    switch (face) {
    case FacePosY: return 1.0f;     // top — brightest
    case FaceNegY: return 0.5f;     // bottom — darkest
    case FacePosX:
    case FaceNegX: return 0.82f;    // east/west
    default:       return 0.68f;    // north/south
    }
}

struct Rgb { u8 r, g, b; };

[[nodiscard]] Rgb shadeRgb(Rgb rgb, i32 face) noexcept {
    const f32 s = faceShade(face);
    return Rgb{
        static_cast<u8>(static_cast<f32>(rgb.r) * s),
        static_cast<u8>(static_cast<f32>(rgb.g) * s),
        static_cast<u8>(static_cast<f32>(rgb.b) * s),
    };
}

[[nodiscard]] Rgb blockColor(Block block, i32 face) noexcept {
    switch (block) {
    case Block::Grass:
        return (face == FacePosY) ? Rgb{82, 148, 58} : Rgb{122, 92, 58};
    case Block::Dirt:  return Rgb{122, 92, 58};
    case Block::Stone: return Rgb{122, 122, 128};
    case Block::Sand:  return Rgb{210, 198, 142};
    case Block::Snow:  return Rgb{236, 240, 244};
    case Block::Wood:  return (face == FacePosY || face == FaceNegY) ? Rgb{150, 110, 66} : Rgb{110, 80, 48};
    case Block::Leaves: return Rgb{56, 124, 52};
    case Block::Air:   break;
    }
    return Rgb{255, 0, 255};
}

// --- Pixel-art texture atlas -------------------------------------------------
// One 16x16 tile per material, laid out in a horizontal strip. Tiles:
//   0 grass-top, 1 grass-side, 2 dirt, 3 stone, 4 sand, 5 snow, 6 wood, 7 leaves.
constexpr i32 kTilePx = 16;
constexpr i32 kNumTiles = 8;

[[nodiscard]] i32 tileFor(Block block, i32 face) noexcept {
    switch (block) {
    case Block::Grass: return (face == FacePosY) ? 0 : (face == FaceNegY) ? 2 : 1;
    case Block::Dirt:  return 2;
    case Block::Stone: return 3;
    case Block::Sand:  return 4;
    case Block::Snow:  return 5;
    case Block::Wood:  return 6;
    case Block::Leaves: return 7;
    case Block::Air:   break;
    }
    return 2;
}

// Procedural per-texel colour for tile `t` at local pixel (px, py).
[[nodiscard]] Color tilePixel(i32 t, i32 px, i32 py) noexcept {
    const f32 n = hashToUnit(px + t * 37, py + t * 101, 7919U);   // [0,1] speckle
    const f32 jitter = (n - 0.5f) * 2.0f;                          // [-1,1]
    auto mk = [](i32 r, i32 g, i32 b) noexcept {
        return Color{
            static_cast<u8>(std::clamp(r, 0, 255)),
            static_cast<u8>(std::clamp(g, 0, 255)),
            static_cast<u8>(std::clamp(b, 0, 255)),
            255U};
    };
    switch (t) {
    case 0: { // grass top
        const i32 d = static_cast<i32>(jitter * 26.0f) - (n < 0.12f ? 34 : 0);
        return mk(92 + d / 2, 152 + d, 64 + d / 2);
    }
    case 1: { // grass side: jagged green lip over dirt
        const i32 lip = 3 + (static_cast<i32>(hashToUnit(px, 71, 31U) * 2.99f));
        if (py < lip) {
            const i32 d = static_cast<i32>(jitter * 24.0f);
            return mk(92 + d / 2, 150 + d, 64 + d / 2);
        }
        const i32 d = static_cast<i32>(jitter * 22.0f) - (n < 0.12f ? 30 : 0);
        return mk(132 + d, 96 + d, 60 + d);
    }
    case 2: { // dirt
        const i32 d = static_cast<i32>(jitter * 24.0f) - (n < 0.12f ? 32 : 0);
        return mk(132 + d, 96 + d, 60 + d);
    }
    case 3: { // stone
        const i32 d = static_cast<i32>(jitter * 16.0f) - (n < 0.08f ? 30 : 0);
        return mk(128 + d, 128 + d, 134 + d);
    }
    case 4: { // sand
        const i32 d = static_cast<i32>(jitter * 14.0f);
        return mk(214 + d, 202 + d, 150 + d);
    }
    case 6: { // wood — vertical grain streaks
        const f32 streak = hashToUnit(px, 3, 53U);
        const i32 d = static_cast<i32>(jitter * 14.0f) + static_cast<i32>((streak - 0.5f) * 22.0f);
        return mk(120 + d, 86 + d, 52 + d);
    }
    case 7: { // leaves — mottled green with dark gaps
        const i32 dark = (n < 0.22f) ? 40 : 0;
        const i32 d = static_cast<i32>(jitter * 30.0f) - dark;
        return mk(58 + d / 2, 126 + d, 52 + d / 2);
    }
    default: { // snow (tile 5)
        const i32 d = static_cast<i32>(jitter * 8.0f);
        return mk(236 + d, 240 + d, 246 + d);
    }
    }
}

// Face geometry: base corner + two tangents U,V with U x V == outward normal.
struct FaceDef {
    Vector3 base;   // offset from the block's min corner
    Vector3 u;
    Vector3 v;
    Vector3 normal;
};

constexpr FaceDef kFaces[6] = {
    /*+X*/ {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}},
    /*-X*/ {{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {-1, 0, 0}},
    /*+Y*/ {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
    /*-Y*/ {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}},
    /*+Z*/ {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
    /*-Z*/ {{0, 0, 0}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
};

// --- Ambient occlusion -------------------------------------------------------
// Per-vertex brightness from the 3 occluders touching each face corner.
constexpr f32 kAoMul[4] = {0.45f, 0.62f, 0.80f, 1.0f};
struct AoCorner { i32 su; i32 sv; };
// Corner positions in (u,v) face coords for p0,p1=base+u,p2=base+u+v,p3=base+v.
constexpr AoCorner kAoCorners[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
[[nodiscard]] i32 aoLevel(bool s1, bool s2, bool cd) noexcept {
    if (s1 && s2) return 0;
    return 3 - ((s1 ? 1 : 0) + (s2 ? 1 : 0) + (cd ? 1 : 0));
}

// Builds a Raylib Model from CPU-side attribute buffers, copying each array
// into GPU-owned MemAlloc'd memory. Returns a default-constructed Model{}
// when the vertex buffer is empty (caller derives its has-mesh flag from
// model.meshCount > 0).
[[nodiscard]] Model makeModelFromBuffers(
    const std::vector<f32>& pos,
    const std::vector<f32>& nrm,
    const std::vector<f32>& uv,
    const std::vector<u8>& col,
    const Texture2D atlas,
    const bool atlasReady) {
    const i32 vertexCount = static_cast<i32>(pos.size() / 3);
    if (vertexCount == 0) {
        return Model{};
    }
    Mesh mesh{};
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = vertexCount / 3;
    mesh.vertices = static_cast<f32*>(MemAlloc(static_cast<u32>(pos.size()) * sizeof(f32)));
    mesh.normals = static_cast<f32*>(MemAlloc(static_cast<u32>(nrm.size()) * sizeof(f32)));
    mesh.texcoords = static_cast<f32*>(MemAlloc(static_cast<u32>(uv.size()) * sizeof(f32)));
    mesh.colors = static_cast<u8*>(MemAlloc(static_cast<u32>(col.size()) * sizeof(u8)));
    std::copy(pos.begin(), pos.end(), mesh.vertices);
    std::copy(nrm.begin(), nrm.end(), mesh.normals);
    std::copy(uv.begin(), uv.end(), mesh.texcoords);
    std::copy(col.begin(), col.end(), mesh.colors);
    UploadMesh(&mesh, false);
    Model model = LoadModelFromMesh(mesh);
    if (atlasReady && model.materialCount > 0) {
        model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = atlas;
    }
    return model;
}

} // namespace

VoxelWorld::~VoxelWorld() noexcept {
    unloadAll();
}

i32 VoxelWorld::surfaceHeight(const i32 worldX, const i32 worldZ) const noexcept {
    f32 amplitude = 1.0f;
    f32 frequency = m_config.frequency;
    f32 sum = 0.0f;
    f32 norm = 0.0f;
    for (i32 octave = 0; octave < m_config.octaves; ++octave) {
        const f32 n = valueNoise(
            static_cast<f32>(worldX) * frequency,
            static_cast<f32>(worldZ) * frequency,
            m_config.seed + static_cast<u32>(octave) * 1013U);
        sum += n * amplitude;
        norm += amplitude;
        amplitude *= m_config.persistence;
        frequency *= 2.0f;
    }
    f32 unit = (norm > 0.0f) ? (sum / norm) : 0.0f;
    unit = unit * unit * (3.0f - 2.0f * unit);   // bias toward valleys + peaks
    return m_config.baseHeight + static_cast<i32>(unit * static_cast<f32>(m_config.amplitude));
}

Block VoxelWorld::blockAt(const i32 worldX, const i32 worldY, const i32 worldZ) const noexcept {
    return blockAt(worldX, worldY, worldZ, surfaceHeight(worldX, worldZ));
}

Block VoxelWorld::blockAt(const i32 worldX, const i32 worldY, const i32 worldZ, const i32 knownSurfaceHeight) const noexcept {
    if (worldY < 0) {
        return Block::Stone;            // bedrock fill (culls the world floor)
    }
    const i32 h = knownSurfaceHeight;
    const i32 snowLevel = m_config.baseHeight + m_config.amplitude - 7;
    const i32 sandLevel = m_config.seaLevel + 1;    // beach band at the waterline

    if (worldY > h) {
        // Above the surface: only tree voxels exist here.
        const u32 seed = m_config.seed;
        const auto surfaceOf = [this](i32 sx, i32 sz) noexcept { return surfaceHeight(sx, sz); };
        const auto rootIsGrass = [this, snowLevel, sandLevel](i32 sx, i32 sz) noexcept {
            const i32 sh = surfaceHeight(sx, sz);
            return sh < snowLevel && sh > sandLevel;    // trees only on grass, not snow/beach
        };
        return treeBlockAt(worldX, worldY, worldZ, seed, surfaceOf, rootIsGrass);
    }

    if (isCarvedAir(worldX, worldY, worldZ, h, m_config.seed)) {
        return Block::Air;              // caves carve at/below the crust
    }
    if (worldY == h) {
        if (h >= snowLevel) return Block::Snow;
        if (h <= sandLevel) return Block::Sand;
        return Block::Grass;
    }
    if (worldY >= h - 3) {
        return (h <= sandLevel) ? Block::Sand : Block::Dirt;
    }
    return Block::Stone;
}

void VoxelWorld::buildChunkMesh(Chunk& chunk) const {
    const i32 originX = chunk.cx * kChunkSize;
    const i32 originZ = chunk.cz * kChunkSize;
    constexpr i32 pad = kChunkSize + 2;

    // Pass 1: padded surface heights -> Y bounds.
    std::vector<i32> heights(static_cast<usize>(pad) * static_cast<usize>(pad), 0);
    i32 maxH = 0;
    for (i32 pz = 0; pz < pad; ++pz) {
        for (i32 px = 0; px < pad; ++px) {
            const i32 hh = surfaceHeight(originX + px - 1, originZ + pz - 1);
            heights[static_cast<usize>(pz) * static_cast<usize>(pad) + static_cast<usize>(px)] = hh;
            maxH = std::max(maxH, hh);
        }
    }
    auto heightAtPad = [&](i32 px, i32 pz) noexcept {
        return heights[static_cast<usize>(pz + 1) * static_cast<usize>(pad) + static_cast<usize>(px + 1)];
    };

    // Y window: cave floor below, tree canopy ceiling above.
    const i32 yLo = std::max(0, kCaveFloorY - 1);
    const i32 yHi = maxH + kTrunkMaxH + kCanopyPad;
    const i32 ySpan = (yHi >= yLo) ? (yHi - yLo + 1) : 1;

    // Pass 2: fill a padded 3D Block volume (one blockAt per cell).
    auto packBlocks = [&](i32 px, i32 y, i32 pz) noexcept -> usize {
        return (static_cast<usize>(pz) * static_cast<usize>(ySpan) + static_cast<usize>(y - yLo))
             * static_cast<usize>(pad) + static_cast<usize>(px);
    };
    std::vector<Block> blocks(static_cast<usize>(pad) * static_cast<usize>(pad) * static_cast<usize>(ySpan), Block::Air);
    for (i32 pz = 0; pz < pad; ++pz) {
        for (i32 px = 0; px < pad; ++px) {
            const i32 colH = heights[static_cast<usize>(pz) * static_cast<usize>(pad) + static_cast<usize>(px)];
            const i32 wx = originX + px - 1, wz = originZ + pz - 1;
            const i32 colTop = std::min(yHi, colH + kTrunkMaxH + kCanopyPad);
            for (i32 y = yLo; y <= colTop; ++y) {
                blocks[packBlocks(px, y, pz)] = blockAt(wx, y, wz, colH);
            }
        }
    }
    auto solidAt = [&](i32 px, i32 y, i32 pz) noexcept -> bool {
        if (y < yLo) return true;       // below window: solid (no spurious floor faces)
        if (y > yHi) return false;      // above window: air
        return blocks[packBlocks(px, y, pz)] != Block::Air;
    };
    auto blockAtPad = [&](i32 px, i32 y, i32 pz) noexcept -> Block {
        if (y < yLo || y > yHi) return Block::Air;
        return blocks[packBlocks(px, y, pz)];
    };

    std::vector<f32> positions;
    std::vector<f32> normals;
    std::vector<u8> colors;
    std::vector<f32> texcoords;
    positions.reserve(8192);
    const f32 tileSpan = 1.0f / static_cast<f32>(kNumTiles);

    // emitFace with per-vertex ambient occlusion. px/y/pz are PADDED volume coords.
    auto emitFace = [&](Vector3 blockMin, const FaceDef& f, Rgb rgb, i32 tile, i32 px, i32 y, i32 pz) {
        const Vector3 p0{blockMin.x + f.base.x, blockMin.y + f.base.y, blockMin.z + f.base.z};
        const Vector3 p1{p0.x + f.u.x, p0.y + f.u.y, p0.z + f.u.z};
        const Vector3 p2{p1.x + f.v.x, p1.y + f.v.y, p1.z + f.v.z};
        const Vector3 p3{p0.x + f.v.x, p0.y + f.v.y, p0.z + f.v.z};
        const Vector3 corner[4] = {p0, p1, p2, p3};
        const Vector3 nrm = f.normal, ui = f.u, vi = f.v;

        auto S = [&](f32 ox, f32 oy, f32 oz) noexcept {
            return solidAt(px + static_cast<i32>(std::lround(ox)),
                           y + static_cast<i32>(std::lround(oy)),
                           pz + static_cast<i32>(std::lround(oz)));
        };
        i32 ao[4];
        for (i32 c = 0; c < 4; ++c) {
            const f32 du = static_cast<f32>(kAoCorners[c].su * 2 - 1);   // -1 / +1
            const f32 dv = static_cast<f32>(kAoCorners[c].sv * 2 - 1);
            const bool s1 = S(nrm.x + du * ui.x, nrm.y + du * ui.y, nrm.z + du * ui.z);
            const bool s2 = S(nrm.x + dv * vi.x, nrm.y + dv * vi.y, nrm.z + dv * vi.z);
            const bool sd = S(nrm.x + du * ui.x + dv * vi.x, nrm.y + du * ui.y + dv * vi.y, nrm.z + du * ui.z + dv * vi.z);
            ao[c] = aoLevel(s1, s2, sd);
        }
        auto cornerRgb = [&](i32 c) noexcept {
            const f32 m = kAoMul[ao[c]];
            return Rgb{static_cast<u8>(static_cast<f32>(rgb.r) * m),
                       static_cast<u8>(static_cast<f32>(rgb.g) * m),
                       static_cast<u8>(static_cast<f32>(rgb.b) * m)};
        };

        constexpr std::array<i32, 6> kSplitNormal{0, 1, 2, 0, 2, 3};
        constexpr std::array<i32, 6> kSplitFlipped{1, 2, 3, 1, 3, 0};
        const bool flip = (ao[0] + ao[2]) < (ao[1] + ao[3]);   // anisotropy fix
        const std::array<i32, 6>& idx = flip ? kSplitFlipped : kSplitNormal;

        const f32 u0 = static_cast<f32>(tile) * tileSpan, u1 = u0 + tileSpan;
        const f32 cornerUv[4][2] = {{u0, 1.0f}, {u1, 1.0f}, {u1, 0.0f}, {u0, 0.0f}};
        for (i32 i = 0; i < 6; ++i) {
            const i32 c = idx[static_cast<usize>(i)];
            positions.push_back(corner[c].x);
            positions.push_back(corner[c].y);
            positions.push_back(corner[c].z);
            normals.push_back(f.normal.x);
            normals.push_back(f.normal.y);
            normals.push_back(f.normal.z);
            const Rgb cr = cornerRgb(c);
            colors.push_back(cr.r);
            colors.push_back(cr.g);
            colors.push_back(cr.b);
            colors.push_back(255U);
            texcoords.push_back(cornerUv[c][0]);
            texcoords.push_back(cornerUv[c][1]);
        }
    };

    // Pass 3: 6-neighbour cull (offsets MATCH the Face enum order).
    static constexpr i32 nbx[6] = {1, -1, 0, 0, 0, 0};
    static constexpr i32 nby[6] = {0, 0, 1, -1, 0, 0};
    static constexpr i32 nbz[6] = {0, 0, 0, 0, 1, -1};
    for (i32 lz = 0; lz < kChunkSize; ++lz) {
        for (i32 lx = 0; lx < kChunkSize; ++lx) {
            const i32 px = lx + 1, pz = lz + 1;
            const i32 colTop = std::min(yHi, heightAtPad(lx, lz) + kTrunkMaxH + kCanopyPad);
            for (i32 y = yLo; y <= colTop; ++y) {
                const Block block = blockAtPad(px, y, pz);
                if (block == Block::Air) continue;
                const Vector3 blockMin{static_cast<f32>(lx), static_cast<f32>(y), static_cast<f32>(lz)};
                for (i32 face = 0; face < 6; ++face) {
                    if (!solidAt(px + nbx[face], y + nby[face], pz + nbz[face])) {
                        emitFace(blockMin, kFaces[face], shadeRgb(blockColor(block, face), face),
                                 tileFor(block, face), px, y, pz);
                    }
                }
            }
        }
    }

    chunk.model = makeModelFromBuffers(positions, normals, texcoords, colors, m_atlas, m_atlasReady);
    chunk.hasMesh = chunk.model.meshCount > 0;
}

void VoxelWorld::buildChunkWater(Chunk& chunk) const {
    const i32 originX = chunk.cx * kChunkSize, originZ = chunk.cz * kChunkSize;
    const auto seaY = static_cast<f32>(m_config.seaLevel);
    std::vector<f32> positions, normals, texcoords;
    std::vector<u8> colors;
    positions.reserve(1024);
    constexpr u8 kR = 40, kG = 110, kB = 190, kA = 150;
    const f32 uvSpan = 1.0f / static_cast<f32>(kNumTiles);
    const f32 u0 = 4.0f * uvSpan;   // sand tile region (texture barely shows through alpha)
    auto cell = [&](f32 lx, f32 lz) {
        const Vector3 q[6] = {{lx, seaY, lz}, {lx + 1, seaY, lz}, {lx + 1, seaY, lz + 1},
                              {lx, seaY, lz}, {lx + 1, seaY, lz + 1}, {lx, seaY, lz + 1}};
        const f32 uv[6][2] = {{u0, 1}, {u0 + uvSpan, 1}, {u0 + uvSpan, 0}, {u0, 1}, {u0 + uvSpan, 0}, {u0, 0}};
        for (i32 i = 0; i < 6; ++i) {
            positions.push_back(q[i].x); positions.push_back(q[i].y); positions.push_back(q[i].z);
            normals.push_back(0); normals.push_back(1); normals.push_back(0);
            colors.push_back(kR); colors.push_back(kG); colors.push_back(kB); colors.push_back(kA);
            texcoords.push_back(uv[i][0]); texcoords.push_back(uv[i][1]);
        }
    };
    for (i32 lz = 0; lz < kChunkSize; ++lz) {
        for (i32 lx = 0; lx < kChunkSize; ++lx) {
            if (surfaceHeight(originX + lx, originZ + lz) < m_config.seaLevel) {
                cell(static_cast<f32>(lx), static_cast<f32>(lz));
            }
        }
    }
    chunk.waterModel = makeModelFromBuffers(positions, normals, texcoords, colors, m_atlas, m_atlasReady);
    chunk.hasWater = chunk.waterModel.meshCount > 0;
}

void VoxelWorld::ensureAtlas() {
    if (m_atlasReady) {
        return;
    }
    Image image = GenImageColor(kNumTiles * kTilePx, kTilePx, BLANK);
    for (i32 t = 0; t < kNumTiles; ++t) {
        for (i32 py = 0; py < kTilePx; ++py) {
            for (i32 px = 0; px < kTilePx; ++px) {
                ImageDrawPixel(&image, t * kTilePx + px, py, tilePixel(t, px, py));
            }
        }
    }
    m_atlas = LoadTextureFromImage(image);
    SetTextureFilter(m_atlas, TEXTURE_FILTER_POINT);   // crisp pixels, no blur
    UnloadImage(image);
    m_atlasReady = true;
}

void VoxelWorld::destroyAtlas() noexcept {
    if (m_atlasReady) {
        UnloadTexture(m_atlas);
        m_atlas = Texture2D{};
        m_atlasReady = false;
    }
}

void VoxelWorld::destroyChunk(Chunk& chunk) noexcept {
    if (chunk.hasMesh) {
        UnloadModel(chunk.model);
        chunk.model = Model{};
        chunk.hasMesh = false;
    }
    if (chunk.hasWater) {
        UnloadModel(chunk.waterModel);
        chunk.waterModel = Model{};
        chunk.hasWater = false;
    }
}

void VoxelWorld::update(const Vector3 playerPosition) {
    ensureAtlas();
    m_builtThisFrame = 0;
    const i32 pcx = static_cast<i32>(std::floor(playerPosition.x / static_cast<f32>(kChunkSize)));
    const i32 pcz = static_cast<i32>(std::floor(playerPosition.z / static_cast<f32>(kChunkSize)));
    const i32 radius = m_config.viewRadiusChunks;

    // --- Unload chunks beyond the keep radius ---
    const i32 keep = radius + 1;
    for (auto it = m_chunks.begin(); it != m_chunks.end();) {
        if (std::abs(it->second.cx - pcx) > keep || std::abs(it->second.cz - pcz) > keep) {
            destroyChunk(it->second);
            it = m_chunks.erase(it);
        } else {
            ++it;
        }
    }

    // --- Build the nearest missing chunks, bounded per frame ---
    i32 bestCx = 0;
    i32 bestCz = 0;
    for (i32 builds = 0; builds < m_config.maxBuildsPerFrame; ++builds) {
        i64 bestDist = std::numeric_limits<i64>::max();
        bool found = false;
        for (i32 dz = -radius; dz <= radius; ++dz) {
            for (i32 dx = -radius; dx <= radius; ++dx) {
                const i32 cx = pcx + dx;
                const i32 cz = pcz + dz;
                if (m_chunks.find(keyOf(cx, cz)) != m_chunks.end()) {
                    continue;
                }
                const i64 dist = static_cast<i64>(dx) * dx + static_cast<i64>(dz) * dz;
                if (dist < bestDist) {
                    bestDist = dist;
                    bestCx = cx;
                    bestCz = cz;
                    found = true;
                }
            }
        }
        if (!found) {
            break;
        }
        Chunk chunk;
        chunk.cx = bestCx;
        chunk.cz = bestCz;
        buildChunkMesh(chunk);
        buildChunkWater(chunk);
        m_chunks.emplace(keyOf(bestCx, bestCz), std::move(chunk));
        ++m_builtThisFrame;
    }
}

void VoxelWorld::render() const noexcept {
    for (const auto& [_, chunk] : m_chunks) {
        if (chunk.hasMesh) {
            DrawModel(chunk.model,
                      Vector3{static_cast<f32>(chunk.cx * kChunkSize), 0.0f, static_cast<f32>(chunk.cz * kChunkSize)},
                      1.0f, WHITE);
        }
    }
}

void VoxelWorld::renderWater(const f32 timeSeconds) const noexcept {
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    for (const auto& [_, chunk] : m_chunks) {
        if (!chunk.hasWater) continue;
        const f32 phase = static_cast<f32>(chunk.cx) * 0.7f + static_cast<f32>(chunk.cz) * 1.3f;
        const f32 bob = std::sin(timeSeconds * m_config.waveSpeed + phase) * m_config.waveAmplitude;
        DrawModel(chunk.waterModel,
                  Vector3{static_cast<f32>(chunk.cx * kChunkSize), bob, static_cast<f32>(chunk.cz * kChunkSize)},
                  1.0f, WHITE);
    }
    rlEnableBackfaceCulling();
    EndBlendMode();
}

void VoxelWorld::unloadAll() noexcept {
    for (auto& [_, chunk] : m_chunks) {
        destroyChunk(chunk);
    }
    m_chunks.clear();
    destroyAtlas();
}

f32 VoxelWorld::groundHeight(const f32 worldX, const f32 worldZ) const noexcept {
    const i32 bx = static_cast<i32>(std::floor(worldX));
    const i32 bz = static_cast<i32>(std::floor(worldZ));
    const i32 surf = surfaceHeight(bx, bz);
    for (i32 y = surf; y >= 0; --y) {
        if (blockAt(bx, y, bz, surf) != Block::Air) {
            return static_cast<f32>(y + 1);
        }
    }
    return static_cast<f32>(surf + 1);
}

} // namespace biofuel::engine::world::voxel
