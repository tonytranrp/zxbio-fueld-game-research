#pragma once

// tree_dump's sway flipbook (Prompt 007 goal 335). Goal 190's Check asks for a step response, a
// resonance and a frame budget; this is the addition the prompt makes to it -- "a captured frame
// sequence across one sway period, viewed, so the motion is seen and not only measured."
//
// An orthographic side view, drawn on the CPU with a line rasteriser, because the thing being looked
// at is a SKELETON and the voxelizer that would put it on screen is the next goal (336). Waiting for
// that would mean shipping the dynamics with nobody having watched them move.
//
// Two deliberate choices in the picture itself:
//   * the REST pose is drawn behind the posed one, in grey. A swaying tree photographed once looks
//     exactly like a still tree; against its own rest silhouette the displacement is legible in every
//     single frame, without a flipbook and without a difference image.
//   * segment thickness comes from the pipe-model radius, so the trunk reads as a trunk. A uniform
//     one-pixel wireframe hides precisely the thing the sway model is keyed to.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "../../svo_render/png_writer.hpp"
#include "world/generation/tree_skeleton.hpp"

namespace tools::tree_dump {

class SwayCanvas {
public:
    SwayCanvas(std::uint32_t width, std::uint32_t height)
        : width_(width), height_(height), rgb_(static_cast<std::size_t>(width) * height * 3u, 0u) {}

    void clear(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        for (std::size_t i = 0; i + 2 < rgb_.size(); i += 3) {
            rgb_[i] = r;
            rgb_[i + 1] = g;
            rgb_[i + 2] = b;
        }
    }

    void plot(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        if (x < 0 || y < 0 || x >= static_cast<int>(width_) || y >= static_cast<int>(height_)) {
            return;
        }
        const std::size_t i = (static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(x)) * 3u;
        rgb_[i] = r;
        rgb_[i + 1] = g;
        rgb_[i + 2] = b;
    }

    // A thick line, drawn as a disc swept along the segment. Slower than Bresenham and shorter than
    // a correct thick-line rasteriser; this writes a handful of frames, not a frame budget.
    void line(float x0, float y0, float x1, float y1, float halfWidth, std::uint8_t r, std::uint8_t g,
              std::uint8_t b) {
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        const int steps = std::max(1, static_cast<int>(len * 2.0f));
        const int rad = std::max(0, static_cast<int>(std::floor(halfWidth)));
        for (int s = 0; s <= steps; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(steps);
            const float px = x0 + dx * t;
            const float py = y0 + dy * t;
            for (int oy = -rad; oy <= rad; ++oy) {
                for (int ox = -rad; ox <= rad; ++ox) {
                    if (static_cast<float>(ox * ox + oy * oy) > halfWidth * halfWidth) {
                        continue;
                    }
                    plot(static_cast<int>(px) + ox, static_cast<int>(py) + oy, r, g, b);
                }
            }
        }
    }

    [[nodiscard]] bool write(const std::string& path) const {
        return svo_render::PngWriter::write(path.c_str(), width_, height_, rgb_.data());
    }

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<std::uint8_t> rgb_;
};

// World-to-pixel for an orthographic side view looking along -Z: +X right, +Y up. The frame is
// fitted to the REST bounds with a margin, and then never re-fitted, so a tree that leans out of
// frame reads as leaning rather than as the camera following it.
struct SideView {
    float scale = 1.0f;
    float originX = 0.0f;
    float originY = 0.0f;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] static SideView fit(const world::generation::TreeBounds& b, std::uint32_t width,
                                      std::uint32_t height, float leanMargin) {
        SideView v;
        v.width = width;
        v.height = height;
        // Widen the horizontal extent by the expected lean so a downwind tree stays inside.
        const float spanX = std::max(0.1f, (b.max.x - b.min.x) + 2.0f * leanMargin);
        const float spanY = std::max(0.1f, b.max.y - b.min.y);
        const float pad = 0.94f;
        v.scale = std::min(pad * static_cast<float>(width) / spanX, pad * static_cast<float>(height) / spanY);
        v.originX = 0.5f * (b.min.x + b.max.x);
        v.originY = b.min.y;
        return v;
    }

    [[nodiscard]] float px(float worldX) const {
        return 0.5f * static_cast<float>(width) + (worldX - originX) * scale;
    }
    [[nodiscard]] float py(float worldY) const {
        // Leave a small plinth at the bottom so the base is not on the very last row.
        return static_cast<float>(height) - 8.0f - (worldY - originY) * scale;
    }
};

} // namespace tools::tree_dump
