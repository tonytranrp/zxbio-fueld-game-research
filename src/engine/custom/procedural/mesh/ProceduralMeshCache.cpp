#include "engine/custom/procedural/mesh/ProceduralMeshCache.hpp"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::mesh {

namespace {

constexpr f32 RAD_TO_DEG = 57.29577951308232f;

} // namespace

ProceduralMeshCache::~ProceduralMeshCache() {
    clear();
}

MeshHandle ProceduralMeshCache::cube() {
    return getOrCreate(MeshKind::Cube, 0, 0);
}

MeshHandle ProceduralMeshCache::cylinder(const i32 slices) {
    return getOrCreate(MeshKind::Cylinder, 0, std::max(slices, 3));
}

MeshHandle ProceduralMeshCache::sphere(const i32 rings, const i32 slices) {
    return getOrCreate(MeshKind::Sphere, std::max(rings, 3), std::max(slices, 3));
}

void ProceduralMeshCache::draw(
    const MeshHandle handle,
    const Vector3 position,
    const Vector3 rotationAxis,
    const f32 rotationDegrees,
    const Vector3 scale,
    const Color tint,
    Texture2D* texture) noexcept
{
    Model* target = model(handle);
    if (target == nullptr) {
        return;
    }

    Texture2D previous{};
    bool hadTexture = false;
    if (texture != nullptr && target->materialCount > 0 && target->materials != nullptr) {
        previous = target->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
        hadTexture = true;
        target->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *texture;
    }

    DrawModelEx(*target, position, rotationAxis, rotationDegrees, scale, tint);

    if (hadTexture) {
        target->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = previous;
    }
}

void ProceduralMeshCache::clear() noexcept {
    for (u32 index = 0U; index < m_count; ++index) {
        MeshRecord& record = m_records[index];
        if (record.loaded) {
            UnloadModel(record.model);
        }
        record = {};
    }
    m_count = 0U;
    ++m_generation;
    if (m_generation == 0U) {
        m_generation = 1U;
    }
}

MeshHandle ProceduralMeshCache::getOrCreate(const MeshKind kind, const i32 rings, const i32 slices) {
    for (u32 index = 0U; index < m_count; ++index) {
        const MeshRecord& record = m_records[index];
        if (record.loaded && record.kind == kind && record.rings == rings && record.slices == slices) {
            return MeshHandle{.index = index + 1U, .generation = m_generation};
        }
    }

    Mesh generated{};
    switch (kind) {
    case MeshKind::Cube:
        generated = GenMeshCube(1.0f, 1.0f, 1.0f);
        break;
    case MeshKind::Cylinder:
        generated = GenMeshCylinder(1.0f, 1.0f, slices);
        break;
    case MeshKind::Sphere:
        generated = GenMeshSphere(1.0f, rings, slices);
        break;
    default:
        break;
    }

    return addRecord(MeshRecord{
        .kind = kind,
        .rings = rings,
        .slices = slices,
        .model = LoadModelFromMesh(generated),
        .loaded = true,
    });
}

MeshHandle ProceduralMeshCache::addRecord(MeshRecord record) {
    if (m_count >= m_records.size()) {
        spdlog::warn("ProceduralMeshCache: cache full, returning empty mesh handle");
        if (record.loaded) {
            UnloadModel(record.model);
        }
        return {};
    }

    const u32 index = m_count++;
    m_records[index] = record;
    return MeshHandle{.index = index + 1U, .generation = m_generation};
}

Model* ProceduralMeshCache::model(const MeshHandle handle) noexcept {
    if (!handle.valid() || handle.generation != m_generation || handle.index == 0U || handle.index > m_count) {
        return nullptr;
    }
    MeshRecord& record = m_records[handle.index - 1U];
    return record.loaded ? &record.model : nullptr;
}

Vector3 rotationAxisForSegment(const Vector3 start, const Vector3 end) noexcept {
    const Vector3 direction = Vector3Normalize(Vector3Subtract(end, start));
    const Vector3 yAxis{0.0f, 1.0f, 0.0f};
    const Vector3 axis = Vector3CrossProduct(yAxis, direction);
    if (Vector3Length(axis) <= 0.0001f) {
        return Vector3{1.0f, 0.0f, 0.0f};
    }
    return axis;
}

f32 rotationDegreesForSegment(const Vector3 start, const Vector3 end) noexcept {
    const Vector3 direction = Vector3Normalize(Vector3Subtract(end, start));
    const f32 dot = std::clamp(Vector3DotProduct(Vector3{0.0f, 1.0f, 0.0f}, direction), -1.0f, 1.0f);
    return std::acos(dot) * RAD_TO_DEG;
}

Vector3 midpoint(const Vector3 start, const Vector3 end) noexcept {
    return Vector3Scale(Vector3Add(start, end), 0.5f);
}

} // namespace biofuel::engine::custom::procedural::mesh
