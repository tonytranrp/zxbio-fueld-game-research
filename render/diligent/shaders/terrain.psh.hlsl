// Terrain pixel stage: material-palette lookup + one warm directional sun + two-color hemisphere
// ambient + baked per-vertex AO + low-frequency albedo variation (goals.md Stage 1, goals 13-16)
// + exponential-squared distance fog with height falloff tied to the shared sky palette
// (goals 33/34/91). No textures -- the palette is the slot a texture array upgrades into later.
// g_MaterialColors is sized/ordered by world::chunk::MaterialID and mirrored by kMaterialColors
// in render/diligent/src/pso_terrain.cpp -- update both together.

#include "sky_common.fxh"

cbuffer MaterialPalette
{
    float4 g_MaterialColors[8];
};

cbuffer FogConstants
{
    float4 g_CameraPosWorld; // xyz camera position; w = elapsed seconds (water ripple phase)
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float3 Normal   : NORMAL0;
    float3 WorldPos : TEXCOORD1;
    float  AO       : TEXCOORD2;
    nointerpolation uint Material : TEXCOORD0;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

// Cheap hash-based value noise (goal 16): two smoothed octaves over world XZ. Sin-dot hashing is
// deliberate -- portable across every Diligent shader-conversion path (no bit intrinsics), and a
// debug-visual feature has no precision requirements worth more machinery.
float Hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float ValueNoise(float2 p)
{
    const float2 cell = floor(p);
    const float2 f    = frac(p);
    const float2 u    = f * f * (3.0 - 2.0 * f); // smoothstep fade
    const float a = Hash2(cell);
    const float b = Hash2(cell + float2(1.0, 0.0));
    const float c = Hash2(cell + float2(0.0, 1.0));
    const float d = Hash2(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// Water path (goals 28/30/31/32, design in research/water-foliage-design.md): opaque geometry
// with analytic depth tint standing in for transparency (nothing is meshed beneath water to blend
// against -- the goal-29 decision), Schlick fresnel against the shared sky model (sun glints fall
// out of the reflection vector for free), a two-wave scrolling ripple normal, and an HDR Blinn
// glint that feeds Stage 2's bloom. PSIn.AO carries water-column depth here, not occlusion.
float3 ShadeWater(PSInput PSIn, float3 viewDir, float timeSeconds)
{
    const float2 p = PSIn.WorldPos.xz;
    const float2 d1 = normalize(float2(1.0, 0.35));
    const float2 d2 = normalize(float2(-0.42, 1.0));
    float3 n = float3(0.0, 1.0, 0.0);
    n.xz += 0.040 * cos(dot(p, d1) * 0.90 + timeSeconds * 1.7) * d1;
    n.xz += 0.025 * cos(dot(p, d2) * 1.70 + timeSeconds * 2.6) * d2;
    const float3 rippleN = normalize(n);

    const float depth = PSIn.AO; // 0 = shore, 1 = >=8 voxels deep (baked at mesh time)
    const float3 shallowTint = float3(0.22, 0.46, 0.48); // "seeing the sandy bottom"
    const float3 deepTint    = float3(0.03, 0.14, 0.30);
    const float3 body = lerp(shallowTint, deepTint, sqrt(depth));

    const float cosTheta = saturate(dot(viewDir, rippleN));
    const float fresnel = 0.02 + 0.98 * pow(1.0 - cosTheta, 5.0);
    const float3 reflection = SkyRadiance(reflect(-viewDir, rippleN));

    const float3 halfVec = normalize(viewDir - kSunDirection);
    const float3 glint = kSunColor * (2.5 * pow(saturate(dot(rippleN, halfVec)), 256.0));

    return lerp(body, reflection, fresnel) + glint;
}

void main(in PSInput PSIn, out PSOutput PSOut)
{
    const float3 lightDir = normalize(float3(0.4, -1.0, 0.25)); // sun, slightly off-axis so all face orientations shade distinctly
    const float3 normal   = normalize(PSIn.Normal);
    const float  diffuse  = saturate(dot(normal, -lightDir));

    // Goal 15: an actual sun COLOR, warm/golden, instead of the old colorless scalar.
    const float3 sunColor = float3(1.05, 0.95, 0.78);

    // Goal 14: two-color hemisphere ambient replacing the flat 0.25 floor -- warm sky tint for
    // up-facing normals, cooler/darker ground bounce for down-facing, lerped by normal.y. This is
    // the single biggest step away from flat Lambert and composes with (not replaces) baked AO.
    const float3 skyAmbient    = float3(0.34, 0.33, 0.30);
    const float3 groundAmbient = float3(0.14, 0.15, 0.19);
    const float3 ambient = lerp(groundAmbient, skyAmbient, normal.y * 0.5 + 0.5);

    // Goal 16: low-frequency world-position albedo mottling so a material is not one flat color
    // across a whole chunk. Two octaves, ±10% brightness, at scales (~24 and ~7 voxels) chosen to
    // read as natural variation rather than a visible noise grid.
    const float n1 = ValueNoise(PSIn.WorldPos.xz * (1.0 / 24.0));
    const float n2 = ValueNoise(PSIn.WorldPos.xz * (1.0 / 7.0) + 17.31);
    const float mottle = 0.90 + 0.20 * (0.65 * n1 + 0.35 * n2);

    const float3 albedo = g_MaterialColors[min(PSIn.Material, 7u)].rgb * mottle;

    // Goal 13: AO multiplies the final lit color (both ambient and direct -- the cheap-pipeline
    // convention the reference scheme also uses).
    float3 lit = albedo * (ambient + sunColor * diffuse) * PSIn.AO;

    // Material 3 is Water: full replacement path (fresnel/ripple/depth-tint/glint). Same
    // CPU/GPU-boundary note as terrain.vsh.hlsl's material==5 branch: kBlockTable.is_liquid
    // drives CPU-side logic (mesh_extractor.cpp, block_type.hpp), this literal drives the
    // shader-side one -- two honest dispatches either side of a boundary HLSL can't cross.
    if (PSIn.Material == 3u)
    {
        const float3 viewDir = normalize(g_CameraPosWorld.xyz - PSIn.WorldPos);
        lit = ShadeWater(PSIn, viewDir, g_CameraPosWorld.w);
    }

    // Goals 33/34: exponential-squared distance fog (f = exp(-(d*density)^2), the EXP2
    // formulation) with height-based density falloff (denser near sea level, Quilez's
    // a*exp(-b*y) form at the fragment's height for cheapness). The fog TARGET is the sky
    // gradient along this fragment's own view direction (goal 91 by construction) -- see the
    // note on SkyGradient in sky_common.fxh for the floating-tree artifact a flat fog color
    // caused. The *1.12 saturating lift makes the far band reach a TRUE 100% (dark tree pixels
    // otherwise out-contrast terrain through heavy haze and linger as dashes).
    // Density tuned against this world's actual draw distance by viewed captures: ~25% haze at
    // 200 units, ~80% at 350, hard 100% by ~420.
    const float3 fromCamera = PSIn.WorldPos - g_CameraPosWorld.xyz;
    const float dist = length(fromCamera);
    const float heightFactor = exp2(-max(PSIn.WorldPos.y, 0.0) * 0.012);
    const float density = 0.0030 * (0.80 + 0.20 * heightFactor);
    const float rawFog = 1.0 - exp2(-(dist * density) * (dist * density) * 1.442695);
    const float fogAmount = saturate(rawFog * 1.12);
    const float3 fogged = lerp(lit, SkyGradient(fromCamera / max(dist, 1.0e-3)), fogAmount);
    PSOut.Color = float4(fogged, 1.0);
}
