#pragma once

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

#include "world/chunk/chunk_coord.hpp"

namespace world::chunk {

// THE coordinate-keyed container aliases (engine-hardening brief Group H tasks 9/11): every
// map/set keyed by ChunkCoord goes through these, so re-evaluating the container is a change
// HERE, not another migration. Backed by boost::unordered_flat_map/_flat_set, chosen on this
// machine's own MSVC benchmark data (benchmarks/bench_chunk_map.cpp; decision + numbers in
// research/engine-hardening-log.md) plus two structural properties:
//   - custom iterators, so MSVC _ITERATOR_DEBUG_LEVEL=2 checked-iterator locking (the measured
//     Group D Debug-collapse class) cannot attach to lookups;
//   - Boost.Unordered post-mixes any hash not marked avalanching, so the engine's std::hash
//     specialization (avalanche-tested healthy in its own right) is safe by two layers.
// Reference-stability note: flat containers invalidate references on rehash. ChunkStore is safe
// because its mapped value is unique_ptr (pointees never move -- task 6's audit); do NOT hold
// references into any other CoordMap across an insert, or use boost::unordered_node_map here
// instead (the documented stability sibling, a one-line change by design).
template <typename T>
using CoordMap = boost::unordered_flat_map<ChunkCoord, T, std::hash<ChunkCoord>>;

using CoordSet = boost::unordered_flat_set<ChunkCoord, std::hash<ChunkCoord>>;

} // namespace world::chunk
