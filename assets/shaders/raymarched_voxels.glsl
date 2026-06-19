#version 330

// ============================================================================
// Voxel DDA raymarcher (John Lin "Crystal Islands" style).
//
// Renders a bounded voxel volume (uploaded as a 2D-flattened R8 texture) from
// the first-person camera. Flat-colored voxels lit by a sun + sky ambient;
// soft shadows / AO / GI are layered on in later passes. Fullscreen quad;
// uses gl_FragCoord (independent of quad texcoords).
// ============================================================================

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform vec2 uResolution;     // render-target pixel size
uniform vec3 uCamPos;         // camera eye in VOLUME-LOCAL coords
uniform vec3 uCamFwd;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform float uTanHalfFov;    // tan(fovY/2)
uniform sampler2D uVolume;    // R8 volume, texel (x, y + z*H)
uniform vec3 uVolDim;         // (W, H, D)
uniform vec3 uSunDir;         // normalized direction TO the sun
uniform vec3 uPalette[8];     // flat albedo per block id (0 = air, unused)
uniform float uFlipY;         // 1.0 to flip vertical (render-texture orientation)

int VW = int(uVolDim.x);
int VH = int(uVolDim.y);
int VD = int(uVolDim.z);

bool inBounds(ivec3 c) {
    return c.x >= 0 && c.y >= 0 && c.z >= 0 && c.x < VW && c.y < VH && c.z < VD;
}

int blockId(ivec3 c) {
    if (!inBounds(c)) return 0;
    float r = texelFetch(uVolume, ivec2(c.x, c.y + c.z * VH), 0).r;
    return int(r * 255.0 + 0.5);
}

// Amanatides-Woo DDA. Returns the hit block id (0 = miss), with the face normal
// and entry distance.
int march(vec3 ro, vec3 rd, int maxSteps, out vec3 normal, out float tEnter) {
    vec3 pos = floor(ro);
    vec3 rstep = sign(rd);
    vec3 deltaDist = abs(1.0 / rd);
    vec3 sideDist = (rstep * (pos - ro) + rstep * 0.5 + 0.5) * deltaDist;
    bvec3 mask = bvec3(false);
    normal = vec3(0.0);
    tEnter = 0.0;

    for (int i = 0; i < maxSteps; ++i) {
        int id = blockId(ivec3(pos));
        if (id > 0) {
            normal = -rstep * vec3(mask);
            tEnter = dot(vec3(mask), sideDist - deltaDist);
            return id;
        }
        // Step to the next voxel boundary along the nearest axis.
        mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
        sideDist += vec3(mask) * deltaDist;
        pos += vec3(mask) * rstep;
        // Leave the bounded volume -> sky.
        if (pos.x < -1.0 || pos.y < -1.0 || pos.z < -1.0 ||
            pos.x > uVolDim.x || pos.y > uVolDim.y || pos.z > uVolDim.z) {
            return 0;
        }
    }
    return 0;
}

vec3 skyColor(vec3 rd) {
    float t = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 horizon = vec3(0.80, 0.86, 0.92);
    vec3 zenith  = vec3(0.35, 0.55, 0.85);
    vec3 sky = mix(horizon, zenith, t);
    // soft sun disk
    float s = max(dot(rd, uSunDir), 0.0);
    sky += vec3(1.0, 0.95, 0.8) * pow(s, 256.0) * 0.9;
    return sky;
}

void main() {
    vec2 frag = gl_FragCoord.xy;
    vec2 uv = (frag / uResolution) * 2.0 - 1.0;
    uv.x *= uResolution.x / uResolution.y;
    uv.y *= uFlipY;

    vec3 rd = normalize(uCamFwd + uCamRight * uv.x * uTanHalfFov + uCamUp * uv.y * uTanHalfFov);
    vec3 ro = uCamPos;

    vec3 normal;
    float tEnter;
    int id = march(ro, rd, 256, normal, tEnter);

    vec3 color;
    if (id > 0) {
        vec3 albedo = uPalette[id];
        float ndl = max(dot(normal, uSunDir), 0.0);
        // ambient (sky) + sun diffuse — soft shadows/AO/GI come in the next pass.
        vec3 lit = albedo * (vec3(0.34, 0.38, 0.46) + vec3(1.05, 1.0, 0.9) * ndl);
        color = lit;
    } else {
        color = skyColor(rd);
    }

    // simple tonemap + gamma
    color = color / (color + vec3(0.7));
    color = pow(color, vec3(1.0 / 2.2));
    finalColor = vec4(color, 1.0);
}
