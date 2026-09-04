// Shared sky/sun palette (goals.md goals 88-91): ONE place for the colors the sky pass, the
// terrain pass's fog, and any water reflection tint all read, so atmosphere stays consistent by
// construction (goal 89/91) instead of three separately-tuned color sets.

// Sun direction TOWARD surfaces -- must stay identical to terrain.psh.hlsl's lightDir.
static const float3 kSunDirection = normalize(float3(0.4, -1.0, 0.25));
// Warm/golden sun, same family as the terrain pass's direct-light color.
static const float3 kSunColor = float3(1.05, 0.95, 0.78);

static const float3 kSkyZenith = float3(0.30, 0.52, 0.80);
static const float3 kSkyHorizon = float3(0.78, 0.83, 0.88); // warm haze; also the fog color
static const float3 kSkyBelowHorizon = float3(0.50, 0.56, 0.63);

// Sky radiance for a world-space view direction. Horizon-to-zenith lerp with a soft power curve;
// below the horizon fades toward a neutral ground-haze so downward glances don't show a hard line.
float3 SkyRadiance(float3 dir)
{
    const float upness = dir.y;
    const float t = pow(saturate(upness), 0.45);
    float3 sky = lerp(kSkyHorizon, kSkyZenith, t);
    sky = lerp(sky, kSkyBelowHorizon, saturate(-upness * 3.0));

    // Analytic sun disc + halo (goal 90). HDR-bright on purpose: this is the scene's first
    // genuinely >1.0 emitter, which is exactly what Stage 2's bloom threshold was waiting for;
    // the composite soft-knee rolls the disc off instead of clipping.
    const float toSun = saturate(dot(dir, -kSunDirection));
    sky += kSunColor * (3.0 * pow(toSun, 4096.0) + 0.25 * pow(toSun, 64.0));
    return sky;
}
