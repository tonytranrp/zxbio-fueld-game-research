#include "world/wind/wind_field.hpp"

#include <algorithm>
#include <cmath>

namespace world::wind {

using namespace detail; // the wave constants; see the header for why they live there

glm::vec3 wind_direction(const WindParams& params) noexcept {
    return glm::vec3{std::cos(params.base_angle_radians), 0.0f, std::sin(params.base_angle_radians)};
}

float wind_gust(const WindParams& params, const glm::vec3& position, float timeSeconds) noexcept {
    // Sample in a frame that SCROLLS along the wind: a gust is a patch of fast air travelling
    // downwind, so the pattern has to move, not just pulse in place.
    const float dx = std::cos(params.base_angle_radians);
    const float dz = std::sin(params.base_angle_radians);
    const float scroll = params.gust_scroll * timeSeconds;
    const float px = (position.x - dx * scroll) * params.gust_frequency;
    const float pz = (position.z - dz * scroll) * params.gust_frequency;

    return kGustW0 * std::sin(px * kGustK0x + pz * kGustK0z) +
           kGustW1 * std::sin(px * kGustK1x + pz * kGustK1z + kGustP1) +
           kGustW2 * std::sin(px * kGustK2x + pz * kGustK2z + kGustP2);
}

float wind_flutter(const WindParams& params, const glm::vec3& position, float timeSeconds) noexcept {
    if (params.flutter_hz <= 0.0f) {
        return 0.0f;
    }
    const float t = timeSeconds * params.flutter_hz * kTwoPi;
    const float f = params.flutter_frequency;
    const float a = position.x * kFlutterF0x + position.y * kFlutterF0y + position.z * kFlutterF0z;
    const float b = position.x * kFlutterF1x + position.y * kFlutterF1y + position.z * kFlutterF1z;
    return kFlutterW0 * std::sin(a * f + t) + kFlutterW1 * std::sin(b * f - t * kFlutterRatio);
}

float wind_buffet(const WindParams& params, const glm::vec3& position, float timeSeconds) noexcept {
    if (params.turbulence_intensity <= 0.0f) {
        return 0.0f;
    }
    const float px = position.x * params.buffet_frequency;
    const float pz = position.z * params.buffet_frequency;
    const float t = timeSeconds * kTwoPi;
    return kBuffetW0 * std::sin(px * kBuffetK0x + pz * kBuffetK0z + t * kBuffetR0) +
           kBuffetW1 * std::sin(px * kBuffetK1x + pz * kBuffetK1z - t * kBuffetR1) +
           kBuffetW2 * std::sin(px * kBuffetK2x + pz * kBuffetK2z + t * kBuffetR2);
}

WindSample sample_wind(const WindParams& params, const glm::vec3& position, float timeSeconds) noexcept {
    WindSample out;
    out.direction = wind_direction(params);
    out.gust = wind_gust(params, position, timeSeconds);
    // A gust can lull the wind but never reverse it: a negative speed would flip every consumer's
    // bend direction, which reads as a glitch rather than as calm.
    out.buffet = wind_buffet(params, position, timeSeconds);
    out.speed = std::max(0.0f, params.base_speed * (1.0f + params.gust_amplitude * out.gust +
                                                    params.turbulence_intensity * out.buffet));
    return out;
}

} // namespace world::wind
