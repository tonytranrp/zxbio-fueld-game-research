// mesh_dump: extracts one chunk's mesh (terrain + deterministic tree decoration) from the real
// generator and writes a Wavefront .obj -- the export deferred since Phase 0's scaffold stub,
// implemented per goals.md goal 74 now that the vertex shape has settled (position/normal/
// material/ao). Materials become .obj object groups so viewers color them distinctly; AO isn't
// representable in bare .obj and is deliberately dropped (documented, not forgotten).
//
// Usage: mesh_dump [cx cy cz] [seed] [out.obj]
//   defaults:      0  0  0     1337   chunk_<cx>_<cy>_<cz>.obj

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "world/chunk/chunk_store.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/generation/terrain_fill.hpp"
#include "world/meshing/mesh_extractor.hpp"

using world::chunk::ChunkCoord;
using world::chunk::MaterialID;

namespace {

const char* material_group(MaterialID m) {
    switch (m) {
    case MaterialID::Stone:
        return "Stone";
    case MaterialID::Dirt:
        return "Dirt";
    case MaterialID::Water:
        return "Water";
    case MaterialID::Wood:
        return "Wood";
    case MaterialID::Leaves:
        return "Leaves";
    case MaterialID::Sand:
        return "Sand";
    case MaterialID::Grass:
        return "Grass";
    case MaterialID::Air:
        break;
    }
    return "Air";
}

} // namespace

int main(int argc, char** argv) try {
    const auto argInt = [&](int i, int fallback) {
        // strtol over atoi (clang-tidy bugprone-unchecked-string-to-number-conversion): a garbage
        // argument falls back instead of silently becoming 0.
        if (argc <= i) {
            return fallback;
        }
        char* end = nullptr;
        const long v = std::strtol(argv[i], &end, 10);
        return end != argv[i] ? static_cast<int>(v) : fallback;
    };
    const ChunkCoord coord{argInt(1, 0), argInt(2, 0), argInt(3, 0)};
    const int seed = argInt(4, 1337);
    const std::string outPath = argc > 5
                                    ? argv[5]
                                    : "chunk_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) +
                                          "_" + std::to_string(coord.z) + ".obj";

    const world::generation::HeightmapGenerator generator(seed);
    world::chunk::ChunkStore store;
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                world::generation::fill_terrain(
                    store.get_or_create({coord.x + dx, coord.y + dy, coord.z + dz}), generator);
            }
        }
    }
    const world::meshing::MeshData mesh = world::meshing::extract_mesh(store, coord);
    if (mesh.vertices.empty()) {
        std::printf("mesh_dump: chunk [%d,%d,%d] has no surface geometry (all air or all solid)\n", coord.x,
                    coord.y, coord.z);
        return EXIT_SUCCESS;
    }

    std::FILE* f = nullptr;
#if defined(_MSC_VER)
    (void)fopen_s(&f, outPath.c_str(), "w");
#else
    f = std::fopen(outPath.c_str(), "w");
#endif
    if (f == nullptr) {
        std::fprintf(stderr, "mesh_dump: cannot open %s for writing\n", outPath.c_str());
        return EXIT_FAILURE;
    }

    std::fprintf(f, "# voxel_app chunk [%d,%d,%d], seed %d: %zu vertices, %zu triangles\n", coord.x, coord.y,
                 coord.z, seed, mesh.vertices.size(), mesh.indices.size() / 3);
    for (const auto& v : mesh.vertices) {
        std::fprintf(f, "v %.6f %.6f %.6f\n", static_cast<double>(v.position.x),
                     static_cast<double>(v.position.y), static_cast<double>(v.position.z));
    }
    for (const auto& v : mesh.vertices) {
        std::fprintf(f, "vn %.6f %.6f %.6f\n", static_cast<double>(v.normal.x),
                     static_cast<double>(v.normal.y), static_cast<double>(v.normal.z));
    }
    // Group triangles by their first vertex's material so viewers can color per material. .obj
    // is 1-indexed; v//vn (no texcoords).
    MaterialID currentGroup = MaterialID::Air;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const MaterialID m = mesh.vertices[mesh.indices[i]].material;
        if (m != currentGroup) {
            std::fprintf(f, "o %s\n", material_group(m));
            currentGroup = m;
        }
        std::fprintf(f, "f %u//%u %u//%u %u//%u\n", mesh.indices[i] + 1, mesh.indices[i] + 1,
                     mesh.indices[i + 1] + 1, mesh.indices[i + 1] + 1, mesh.indices[i + 2] + 1,
                     mesh.indices[i + 2] + 1);
    }
    std::fclose(f);
    std::printf("mesh_dump: wrote %s (%zu vertices, %zu triangles)\n", outPath.c_str(), mesh.vertices.size(),
                mesh.indices.size() / 3);
    return EXIT_SUCCESS;
} catch (const std::exception& e) { // generation/meshing throw on real failures; report, don't crash
    std::fprintf(stderr, "mesh_dump: %s\n", e.what());
    return EXIT_FAILURE;
}
