// The wind field, GPU side (Prompt 001 Group B).
//
// MIRROR of world/wind/src/wind_field.cpp, statement for statement, under the same rule the
// marcher lives by: the CPU version is the reference, this follows it. What makes the mirror safe
// here is that it contains no numbers of its own -- every WIND_* constant below is compiled in
// from world/wind's own header by render/diligent/detail/wind_macros.hpp, so the two sides cannot
// disagree about the wave shape even if someone edits one and forgets the other. Only the shape of
// the arithmetic is duplicated, and it is nine lines of sin and dot.
//
// The per-run tuning arrives in a WindConstants block the including shader must declare:
//   float4 WindBaseDirSpeed;  // xy = (cos a, sin a) horizontal direction, z = base speed, w = time
//   float4 WindGustFlutter;   // x = gust amplitude, y = gust frequency, z = gust scroll,
//                             // w = flutter Hz
//   float  WindFlutterFrequency;

#ifndef WIND_FXH
#define WIND_FXH

// A gust is a patch of fast air TRAVELLING downwind, so the sampling frame scrolls along the wind
// direction rather than pulsing in place.
float WindGust(float3 position, float2 windDir, float time, float gustFrequency, float gustScroll) {
    float scroll = gustScroll * time;
    float px = (position.x - windDir.x * scroll) * gustFrequency;
    float pz = (position.z - windDir.y * scroll) * gustFrequency;
    return WIND_GUST_W0 * sin(px * WIND_GUST_K0X + pz * WIND_GUST_K0Z) +
           WIND_GUST_W1 * sin(px * WIND_GUST_K1X + pz * WIND_GUST_K1Z + WIND_GUST_P1) +
           WIND_GUST_W2 * sin(px * WIND_GUST_K2X + pz * WIND_GUST_K2Z + WIND_GUST_P2);
}

// The fast surface shimmer that reads as individual leaves catching the light. Separate from the
// gust on purpose: the gust moves whole canopies, this only modulates their surface.
float WindFlutter(float3 position, float time, float flutterHz, float flutterFrequency) {
    if (flutterHz <= 0.0) {
        return 0.0;
    }
    float t = time * flutterHz * WIND_TWO_PI;
    float a = position.x * WIND_FLUTTER_F0X + position.y * WIND_FLUTTER_F0Y + position.z * WIND_FLUTTER_F0Z;
    float b = position.x * WIND_FLUTTER_F1X + position.y * WIND_FLUTTER_F1Y + position.z * WIND_FLUTTER_F1Z;
    return WIND_FLUTTER_W0 * sin(a * flutterFrequency + t) +
           WIND_FLUTTER_W1 * sin(b * flutterFrequency - t * WIND_FLUTTER_RATIO);
}

// Speed at a point. A gust can lull the wind but never reverse it -- a negative speed would flip
// every consumer's bend direction, which reads as a glitch rather than as calm.
float WindSpeed(float3 position, float2 windDir, float time, float baseSpeed, float gustAmplitude,
                float gustFrequency, float gustScroll) {
    float g = WindGust(position, windDir, time, gustFrequency, gustScroll);
    return max(0.0, baseSpeed * (1.0 + gustAmplitude * g));
}

#endif // WIND_FXH
