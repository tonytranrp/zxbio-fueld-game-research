#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

TerrainField::TerrainField(FieldGeometry geometry) : geometry_(geometry) {
    planes_.assign(cell_count() * static_cast<std::size_t>(Plane::Count), 0.0f);
}

std::span<float> TerrainField::plane(Plane p) noexcept {
    const std::size_t n = cell_count();
    const std::size_t base = static_cast<std::size_t>(p) * n;
    return std::span<float>{planes_.data() + base, n};
}

std::span<const float> TerrainField::plane(Plane p) const noexcept {
    const std::size_t n = cell_count();
    const std::size_t base = static_cast<std::size_t>(p) * n;
    return std::span<const float>{planes_.data() + base, n};
}

} // namespace world::generation::field
