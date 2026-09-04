#pragma once

#include <cstdint>

namespace world::chunk {

enum class MaterialID : std::uint8_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Water = 3,
};

} // namespace world::chunk
