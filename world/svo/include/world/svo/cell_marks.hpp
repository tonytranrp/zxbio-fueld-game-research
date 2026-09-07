#pragma once

// Prompt 004 goal 261: what the marcher records about the cells it touches.
//
// Research §1.4 describes this verbatim -- during traversal each ray "activates the usage stamps of
// the elements that are visited" and "we also set a flag that indicates whether or not a refinement
// or data upload is needed" -- and adds that "it is possible to employ a strategy that allows us to
// avoid any atomic operations in this step". The goal asks me to find that strategy and to check the
// reasoning rather than take it on faith, so:
//
// WHY NO ATOMIC IS NEEDED, argued rather than asserted. Thousands of rays touch the same cell in one
// frame, and every one of them writes the SAME 32-bit word:
//
//   * the frame index is a per-frame constant, identical for every ray in the dispatch;
//   * the request bit depends only on whether that cell is RESIDENT, which is a property of the
//     structure and not of the ray.
//
// So the marking is a set of concurrent stores of one identical value to one address. Whatever order
// they land in, and however they tear -- they cannot tear, since every byte is the same in every
// writer -- the result is that value. A read-modify-write would need an atomic; a write of a
// constant does not. That is the whole trick, and it is why this encoding deliberately has NO
// counter, NO accumulation and NO "how many rays wanted it" field: any of those would be a
// read-modify-write and would put the atomic back.
//
// The cost of getting this wrong is a race that produces plausible-looking numbers, so the tests
// assert idempotence directly.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace world::svo {

/// Bit 31 of a mark word: a ray wanted this cell and it was not resident.
inline constexpr std::uint32_t kCellRequested = 0x80000000u;
/// The frame index occupies the low 31 bits, so it wraps after ~2.1 billion frames -- 137 days at
/// 165 fps. Written down rather than guarded: a wrap would make one frame's stamps look ancient and
/// evict a little too eagerly, once, which is not worth a branch in the hot path.
inline constexpr std::uint32_t kCellFrameMask = 0x7FFFFFFFu;

[[nodiscard]] constexpr std::uint32_t make_cell_mark(std::uint32_t frame, bool requested) noexcept {
    return (frame & kCellFrameMask) | (requested ? kCellRequested : 0u);
}
[[nodiscard]] constexpr std::uint32_t cell_mark_frame(std::uint32_t mark) noexcept {
    return mark & kCellFrameMask;
}
[[nodiscard]] constexpr bool cell_mark_requested(std::uint32_t mark) noexcept {
    return (mark & kCellRequested) != 0u;
}

/// One mark word per cell -- the CPU mirror of the GPU's `g_CellUsage` buffer.
class CellMarks {
public:
    CellMarks() = default;
    explicit CellMarks(std::size_t cells) : marks_(cells, 0u) {}

    [[nodiscard]] std::size_t size() const noexcept { return marks_.size(); }
    [[nodiscard]] const std::vector<std::uint32_t>& words() const noexcept { return marks_; }
    [[nodiscard]] std::uint32_t at(std::size_t index) const noexcept {
        return index < marks_.size() ? marks_[index] : 0u;
    }

    /// The one operation the marcher performs. A plain store, for the reason argued above.
    void mark(std::size_t index, std::uint32_t frame, bool requested) noexcept {
        if (index < marks_.size()) {
            marks_[index] = make_cell_mark(frame, requested);
        }
    }

    /// Cells marked on `frame` -- what the LRU keeps.
    [[nodiscard]] std::vector<std::uint32_t> used_on(std::uint32_t frame) const;
    /// Cells a ray wanted on `frame` and did not get -- what the producer builds next, nearest
    /// first is the CALLER's ordering job since this class knows nothing about where the camera is.
    [[nodiscard]] std::vector<std::uint32_t> requests_on(std::uint32_t frame) const;

    void clear() noexcept { std::fill(marks_.begin(), marks_.end(), 0u); }

    /// Goal 262: the compaction, as GigaVoxels §1.4 describes it -- "two stream reductions, to
    /// separate all elements in the usage list that were used in the current frame from the
    /// others", concatenated so the least recently used land at the FRONT.
    ///
    /// The result is one compact list: `[evictable, oldest first ... | used this frame ...]`, with
    /// `first_used_this_frame` marking the boundary. A caller evicts from index 0 and stops when it
    /// reaches that boundary, which is what makes "never evict something this frame is marching
    /// through" structural rather than a rule someone has to remember.
    struct Compaction {
        std::vector<std::uint32_t> order;          ///< cell indices, LRU first
        std::size_t first_used_this_frame = 0;     ///< everything before this is evictable
        std::size_t requested = 0;                 ///< of the used ones, how many were absent
    };
    [[nodiscard]] Compaction compact(std::uint32_t frame) const;

private:
    std::vector<std::uint32_t> marks_;
};

} // namespace world::svo
