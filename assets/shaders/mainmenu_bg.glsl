#version 330

// Voxel DDA Raymarcher — adapted from ShaderToy @xor
// https://www.shadertoy.com/view/XctSz8
// Compatible with raylib''s default vertex shader

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec3 iResolution;
uniform float iTime;
uniform float uBrightness = 0.0;
uniform float uRevealProgress = 0.0;
uniform float uDimensionShift = 0.0;
uniform float uCameraOffsetX = 0.0;
uniform float uCameraOffsetY = 0.0;
uniform float uCameraYaw = 0.0;

out vec4 finalColor;

#define SCALE 1.
#define MAX 100.
#define SPEED 7.

const float xA = -20.;
const float yA = -2.;

// ---- Voxel density — threshold rises with shift to thin out blocks ----
bool map(vec3 p, float sparsity)
{
    return dot(sin(p * .13), cos(p.yzx * .4536)) + p.y * 0.0561 > (.9 + sparsity);
}

float map2(vec3 p)
{
    return dot(sin(p * .13), cos(p.yzx * .4536)) + p.y * .061;
}

float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 411.7))) * 43758.5453);
}

vec3 hash33(vec3 p){
    float n = sin(dot(p, vec3(7, 157, 113)));
    return fract(vec3(2097152, 262144, 32768) * n);
}

// ---- Filmic tone mapping — compresses bright values without clipping ----
vec3 tonemapACES(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// ---- Subtle cosmic sparkles — very faint pinpoints only ----
float cosmicSparkles(vec2 uv, float shift) {
    if (shift < 0.05) return 0.0;

    float sparkles = 0.0;
    for (int i = 0; i < 6; i++) {
        vec2 pos = vec2(
            sin(float(i) * 1.73 + iTime * 0.10) * 0.65,
            cos(float(i) * 2.41 + iTime * 0.09) * 0.45
        );
        float d = length(uv - pos);
        float twinkle = sin(iTime * (1.5 + float(i) * 0.5) + float(i) * 3.7) * 0.5 + 0.5;
        sparkles += smoothstep(0.006, 0.0, d) * twinkle;
    }
    // Very faint — sparkles are accents, not light sources
    return sparkles * shift * 0.25;
}

void main()
{
    vec2 R = iResolution.xy;
    float reveal = clamp(uRevealProgress, 0.0, 1.0);
    float revealEase = reveal * reveal * (3.0 - 2.0 * reveal);
    float shift = clamp(uDimensionShift, 0.0, 1.0);
    float shiftEase = shift * shift * (3.0 - 2.0 * shift);

    vec2 screenUv = (2. * gl_FragCoord.xy - R) / -R.y;

    // Dimension shift: subtle FOV expansion
    float fovScale = mix(1.0, 1.18, shiftEase);
    vec2 uv = screenUv * mix(1.16, 1.0, revealEase) * fovScale;
    uv.y += mix(0.08, 0.0, revealEase);

    // ---- Stable camera: additive offset instead of speed multiply ----
    float extraTravel = shiftEase * shiftEase * 120.0;
    vec3 ro = vec3(xA, yA, iTime * SPEED + extraTravel);

    // Gentle cosmic drift
    float driftX = sin(iTime * 0.3) * 0.06 * shiftEase;
    float driftY = cos(iTime * 0.2) * 0.04 * shiftEase;
    vec3 rd = normalize(vec3(uv.x + driftX, uv.y + driftY, 1));

    // ---- Camera yaw: rotate ray direction horizontally ----
    if (abs(uCameraYaw) > 0.0001) {
        float cy = cos(uCameraYaw);
        float sy = sin(uCameraYaw);
        rd.xz = mat2(cy, -sy, sy, cy) * rd.xz;
    }

    // Epsilon fix (must be AFTER yaw rotation to avoid center-line artifact)
    rd += vec3(rd.x==0.0, rd.y==0.0, rd.z==0.0) * 1e-5;

    vec3 col = vec3(0);

    // ---- Color palette: warm → cosmic ----
    // Warm reds/oranges (normal mode)
    vec3 warmArr[7] = vec3[7](
        vec3(0.839,0.071,0.),
        vec3(0.651,0.055,0.),
        vec3(0.439,0.035,0.),
        vec3(0.349,0.027,0.),
        vec3(0.383, 0.782, 1.0),
        vec3(0.542, 0.549, 0.625),
        vec3(0.277, 0.133, 0.137)
    );

    // Cosmic palette — deliberately darker/more muted so lighting doesn't overexpose
    // Values kept below 0.55 so lightAcc multiplication stays in range
    vec3 cosmicArr[7] = vec3[7](
        vec3(0.28, 0.30, 0.42),    // dark slate blue
        vec3(0.20, 0.26, 0.40),    // deep navy
        vec3(0.18, 0.20, 0.35),    // dark indigo
        vec3(0.14, 0.15, 0.28),    // very dark violet
        vec3(0.22, 0.35, 0.50),    // muted teal-blue
        vec3(0.25, 0.27, 0.38),    // dark platinum
        vec3(0.16, 0.14, 0.28)     // deep cosmic violet
    );

    vec3 colArr[7];
    for (int i = 0; i < 7; i++) {
        colArr[i] = mix(warmArr[i], cosmicArr[i], shiftEase);
    }

    vec3 axisDir = sign(rd);
    vec3 stepDir = (1. / abs(rd)) * 1.;

    vec3 vox = floor(ro);

    vec3 xyCrossing = ((vox-ro + .0)
    * axisDir + 0.6)
    * stepDir;

    vec3 axis, p, vox2;

    vec3 L = vec3(xA, yA, iTime * SPEED + extraTravel + 15.);

    // Voxel sparsity: thin out blocks during cosmic shift
    float sparsity = shiftEase * 0.55;

    float accum = 0.0, att, voxDt, steps = 0.0;
    float transmittance = 1., stepL, henyey, newXyCros;
    vec3 lightAcc = vec3(0.0);

    for(float i = 1.0; i<MAX; i++)
    {
        if(map(vox, sparsity)) break;

        xyCrossing *= 1.0 + hash33(xyCrossing)*.005;
        xyCrossing += hash33(vox + ro) * stepDir * 0.005;

        voxDt = length((vox+.5) - L)  * .2;

        att = 1./ (10. + voxDt * voxDt );

        accum += att;

        steps++;

        axis =  xyCrossing.x<xyCrossing.z?
             ( xyCrossing.x<xyCrossing.y? vec3(1,0,0) : vec3(0,1,0) ):
             ( xyCrossing.z<xyCrossing.y? vec3(0,0,1) : vec3(0,1,0) );

        p = ro + dot(xyCrossing, axis) * rd;

        float angleL = dot(rd, normalize(p-L));

        float g = 0.45;
        henyey = (1. - g) / (4.*3.1416 * pow((1. + g*g - 2.*g*cos(angleL)),2.5));

        vox2 = vox;

        vox += axis * axisDir;

        newXyCros = dot(xyCrossing, axis);

        xyCrossing += axis * stepDir;

        stepL = dot(xyCrossing, axis) - newXyCros * 0.7;

        transmittance *= exp(-0.00712 * stepL);

        lightAcc += max(map2(p), 0.0) * henyey * transmittance * stepL * att;
    }

    vec3 grid = max(vec3(.7) - 2. * abs(fract(p+.5)-0.) * SCALE, 0.0);
    vec3 newGrid = mix(vec3(0.061),vec3(0),grid.x + grid.y + grid.z);

    float rand = hash(floor(vox)) * 2. + sin(iTime * .103 + .530) * 1.5 + .5;

    col =  colArr[int(clamp(rand, 0.0, 6.0))] * 1.;

    col = lightAcc * col;

    col -= newGrid * revealEase;

    float fog = steps/MAX;
    float fogAmount = clamp(fog * fog * mix(1.65, 1.0, revealEase), 0.0, 1.0);

    // Dimension shift: thin fog, shift color to deep space
    fogAmount *= mix(1.0, 0.40, shiftEase);
    vec3 fogColor = mix(vec3(0.2, .24, .3), vec3(0.05, 0.04, 0.10), shiftEase);
    col = mix(col, fogColor, fogAmount);

    float centerGlow = max(0.0, 1.0 - length(screenUv) * 0.65);
    col += vec3(0.14, 0.05, 0.01) * centerGlow * (1.0 - revealEase) * 0.55;

    // ---- Cosmic dimension shift effects ----
    if (shiftEase > 0.01) {
        // Apply ACES tone mapping FIRST to prevent blown-out highlights
        // before adding any additive cosmic effects
        col = mix(col, tonemapACES(col * 2.2), shiftEase * 0.85);

        // Very subtle nebula-like luminosity variation across blocks
        float nebulaWave = sin(p.x * 0.8 + iTime * 0.12) * sin(p.z * 0.6 + iTime * 0.08) * 0.5 + 0.5;
        vec3 nebulaColor = mix(
            vec3(0.08, 0.06, 0.18),  // deep violet
            vec3(0.06, 0.10, 0.20),  // deep navy blue
            nebulaWave
        );
        col = mix(col, col + nebulaColor * 0.12, shiftEase * 0.6);

        // Subtle sparkle pinpoints in void (very faint)
        float sparkles = cosmicSparkles(screenUv, shiftEase);
        col += vec3(0.55, 0.65, 0.85) * sparkles;

        // Very soft radial darkening at edges (not full vignette — just depth)
        float edgeDim = smoothstep(0.5, 1.4, length(screenUv));
        col *= mix(1.0, 1.0 - edgeDim * 0.45, shiftEase * 0.5);
    }

    float revealMask = smoothstep(-0.2, 0.95, revealEase - length(screenUv) * 0.42);
    revealMask = mix(revealMask, 1.0, shiftEase);

    finalColor = vec4(col * revealMask * uBrightness, 1.0);
}
