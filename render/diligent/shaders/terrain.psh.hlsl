// Terrain pixel stage: material-palette lookup + one warm directional sun + two-color hemisphere
// ambient + baked per-vertex AO + low-frequency albedo variation (goals.md Stage 1, goals 13-16).
// No textures -- the palette is the slot a texture array upgrades into later. g_MaterialColors is
// sized/ordered by world::chunk::MaterialID and mirrored by kMaterialColors in
// render/diligent/src/pso_terrain.cpp -- update both together.

cbuffer MaterialPalette
{
    float4 g_MaterialColors[6];
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

    const float3 albedo = g_MaterialColors[min(PSIn.Material, 5u)].rgb * mottle;

    // Goal 13: AO multiplies the final lit color (both ambient and direct -- the cheap-pipeline
    // convention the reference scheme also uses).
    const float3 lit = albedo * (ambient + sunColor * diffuse) * PSIn.AO;
    PSOut.Color = float4(lit, 1.0);
}
