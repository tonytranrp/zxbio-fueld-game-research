#pragma once

#include "engine/core/Types.hpp"
#include "engine/custom/procedural/core/ProceduralTypes.hpp"
#include "engine/custom/procedural/materials/ProceduralTextureCache.hpp"
#include <array>
#include <raylib.h>

namespace biofuel::engine::custom::procedural::mesh {

struct ProceduralMeshTag {};
using MeshHandle = ProceduralHandle<ProceduralMeshTag>;

enum class MeshKind : u8 {
    Cube,
    Cylinder,
    Sphere,
};

class ProceduralMeshCache final {
public:
    ProceduralMeshCache() = default;
    ProceduralMeshCache(const ProceduralMeshCache&) = delete;
    ProceduralMeshCache& operator=(const ProceduralMeshCache&) = delete;
    ProceduralMeshCache(ProceduralMeshCache&&) = delete;
    ProceduralMeshCache& operator=(ProceduralMeshCache&&) = delete;
    ~ProceduralMeshCache();

    [[nodiscard]] MeshHandle cube();
    [[nodiscard]] MeshHandle cylinder(i32 slices = 8);
    [[nodiscard]] MeshHandle sphere(i32 rings = 8, i32 slices = 8);

    void draw(
        MeshHandle handle,
        Vector3 position,
        Vector3 rotationAxis,
        f32 rotationDegrees,
        Vector3 scale,
        Color tint,
        Texture2D* texture = nullptr) noexcept;

    void clear() noexcept;

private:
    struct MeshRecord {
        MeshKind kind = MeshKind::Cube;
        i32 rings = 0;
        i32 slices = 0;
        Model model{};
        bool loaded = false;
    };

    [[nodiscard]] MeshHandle getOrCreate(MeshKind kind, i32 rings, i32 slices);
    [[nodiscard]] MeshHandle addRecord(MeshRecord record);
    [[nodiscard]] Model* model(MeshHandle handle) noexcept;

    std::array<MeshRecord, 32> m_records{};
    u32 m_count = 0U;
    u32 m_generation = 1U;
};

[[nodiscard]] Vector3 rotationAxisForSegment(Vector3 start, Vector3 end) noexcept;
[[nodiscard]] f32 rotationDegreesForSegment(Vector3 start, Vector3 end) noexcept;
[[nodiscard]] Vector3 midpoint(Vector3 start, Vector3 end) noexcept;

} // namespace biofuel::engine::custom::procedural::mesh
