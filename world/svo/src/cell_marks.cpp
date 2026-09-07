#include "world/svo/cell_marks.hpp"

namespace world::svo {

std::vector<std::uint32_t> CellMarks::used_on(std::uint32_t frame) const {
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        if (marks_[i] != 0u && cell_mark_frame(marks_[i]) == (frame & kCellFrameMask)) {
            out.push_back(static_cast<std::uint32_t>(i));
        }
    }
    return out;
}

std::vector<std::uint32_t> CellMarks::requests_on(std::uint32_t frame) const {
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        if (cell_mark_requested(marks_[i]) && cell_mark_frame(marks_[i]) == (frame & kCellFrameMask)) {
            out.push_back(static_cast<std::uint32_t>(i));
        }
    }
    return out;
}

CellMarks::Compaction CellMarks::compact(std::uint32_t frame) const {
    // Two passes over the stamps, exactly as the paper's two stream reductions -- the first
    // gathering everything NOT used this frame, the second everything that was. Concatenating them
    // in that order puts the least recently used at the front by construction.
    //
    // The first pass is sorted by stamp so "oldest first" is a real ordering rather than an
    // accident of index order; a GPU implementation gets that ordering from the reduction itself.
    Compaction out;
    const std::uint32_t current = frame & kCellFrameMask;
    out.order.reserve(marks_.size());
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        if (cell_mark_frame(marks_[i]) != current) {
            out.order.push_back(static_cast<std::uint32_t>(i));
        }
    }
    std::sort(out.order.begin(), out.order.end(), [this](std::uint32_t a, std::uint32_t b) {
        const std::uint32_t fa = cell_mark_frame(marks_[a]);
        const std::uint32_t fb = cell_mark_frame(marks_[b]);
        return fa != fb ? fa < fb : a < b;
    });
    out.first_used_this_frame = out.order.size();
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        if (cell_mark_frame(marks_[i]) == current) {
            out.order.push_back(static_cast<std::uint32_t>(i));
            out.requested += cell_mark_requested(marks_[i]) ? 1u : 0u;
        }
    }
    return out;
}

} // namespace world::svo
