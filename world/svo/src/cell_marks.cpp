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

} // namespace world::svo
