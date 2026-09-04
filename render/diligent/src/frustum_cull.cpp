#include "render/diligent/frustum.hpp"

namespace render::diligent {

Frustum extract_frustum(const glm::mat4& viewProj) noexcept {
    // glm stores column-major (m[column][row]); Gribb–Hartmann works on the matrix's rows.
    const auto row = [&viewProj](int i) noexcept {
        return glm::vec4(viewProj[0][i], viewProj[1][i], viewProj[2][i], viewProj[3][i]);
    };
    const glm::vec4 r0 = row(0);
    const glm::vec4 r1 = row(1);
    const glm::vec4 r2 = row(2);
    const glm::vec4 r3 = row(3);
    return Frustum{{
        r3 + r0, // left:   x_clip >= -w
        r3 - r0, // right:  x_clip <=  w
        r3 + r1, // bottom: y_clip >= -w
        r3 - r1, // top:    y_clip <=  w
        r2,      // near:   z_clip >=  0 -- the [0,1]-depth form, NOT OpenGL's r3 + r2
        r3 - r2, // far:    z_clip <=  w
    }};
}

bool intersects(const Frustum& frustum, const Aabb& box) noexcept {
    for (const glm::vec4& plane : frustum.planes) {
        // Positive vertex: the box corner farthest along the plane normal. If even that corner is
        // outside this plane, the whole box is.
        const glm::vec3 positive{
            plane.x >= 0.0f ? box.max.x : box.min.x,
            plane.y >= 0.0f ? box.max.y : box.min.y,
            plane.z >= 0.0f ? box.max.z : box.min.z,
        };
        if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace render::diligent
