#pragma once

namespace world::chunk {

class Chunk;

enum class ChunkLoadState {
    Requested,
    Generating,
    Generated,
    Meshing,
    Ready,
};

// ECS component (project brief §2.4, M1.2 brief §5): a chunk's *pipeline state* lives on an
// EnTT entity as this small, homogeneous component. The chunk's actual voxel data (ChunkVoxels)
// stays plain-owned in ChunkStore -- it is never EnTT component data; EnTT's sparse-set storage
// is for many small, cache-friendly components, not one variable-sized paletted blob per entity.
struct ChunkPipelineState {
    ChunkLoadState state = ChunkLoadState::Requested;
    Chunk* chunk = nullptr; // non-owning; ChunkStore owns the real Chunk
};

} // namespace world::chunk
