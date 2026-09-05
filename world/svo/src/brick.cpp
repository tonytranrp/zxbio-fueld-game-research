#include "world/svo/brick.hpp"

namespace world::svo {

world::chunk::MaterialID Brick::representative() const noexcept {
    std::array<std::uint16_t, 256> counts{};
    for (int z = 0; z < kBrickEdge; ++z) {
        for (int x = 0; x < kBrickEdge; ++x) {
            for (int y = kBrickEdge - 1; y >= 0; --y) {
                if (occupied(x, y, z)) {
                    ++counts[static_cast<std::size_t>(at(x, y, z))];
                    break;
                }
            }
        }
    }
    std::size_t best = 0;
    for (std::size_t m = 1; m < counts.size(); ++m) {
        if (counts[m] > counts[best]) {
            best = m;
        }
    }
    return counts[best] == 0 ? world::chunk::MaterialID::Air : static_cast<world::chunk::MaterialID>(best);
}

} // namespace world::svo
