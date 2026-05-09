#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec3 iResolution;
uniform float iTime;
uniform float uBrightness = 0.0;
uniform float uRevealProgress = 0.0;

out vec4 finalColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) +
           (c - a) * u.y * (1.0 - u.x) +
           (d - b) * u.x * u.y;
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;

    for (int i = 0; i < 6; ++i) {
        value += noise(p) * amplitude;
        p = p * 2.02 + vec2(19.0, 13.0);
        amplitude *= 0.52;
    }

    return value;
}

float starField(vec2 uv, float depth) {
    vec2 gridUv = uv * depth;
    vec2 cell = floor(gridUv);
    vec2 local = fract(gridUv) - 0.5;
    float sparkle = hash(cell);
    float radius = mix(0.22, 0.05, sparkle);
    float star = smoothstep(radius, 0.0, length(local));
    star *= smoothstep(0.82, 1.0, sparkle);
    return star;
}

void main() {
    vec2 resolution = iResolution.xy;
    vec2 screenUv = (2.0 * gl_FragCoord.xy - resolution) / resolution.y;

    float reveal = clamp(uRevealProgress, 0.0, 1.0);
    float revealEase = reveal * reveal * (3.0 - 2.0 * reveal);

    vec2 uv = screenUv * mix(1.18, 1.0, revealEase);
    uv.y += mix(0.08, 0.0, revealEase);

    float t = iTime * 0.14;
    vec2 flowUv = uv * vec2(0.9, 0.7);
    float mistA = fbm(flowUv * 1.45 + vec2(0.0, t));
    float mistB = fbm(flowUv * 2.6 + vec2(t * 0.18, -t * 0.52));
    float mistC = fbm(flowUv * 4.1 + vec2(-t * 0.26, t * 0.34));

    float auroraBand = sin(uv.x * 2.4 + mistB * 3.4 - t * 3.0) * 0.5 + 0.5;
    float auroraMask = smoothstep(-0.5, 0.45, uv.y + mistA * 0.42);
    auroraMask *= smoothstep(1.25, -0.1, uv.y);
    float aurora = pow(auroraBand, 2.3) * auroraMask;

    float fogMask = smoothstep(1.15, -0.2, uv.y);
    float starA = starField(uv + vec2(t * 0.18, 0.0), 18.0);
    float starB = starField(uv * 1.35 - vec2(t * 0.08, 0.0), 28.0);
    float stars = (starA * 0.8 + starB * 0.5) * smoothstep(0.15, 1.1, fogMask);

    vec3 skyLow = vec3(0.035, 0.05, 0.11);
    vec3 skyHigh = vec3(0.12, 0.19, 0.31);
    vec3 sky = mix(skyLow, skyHigh, clamp(uv.y * 0.5 + 0.5, 0.0, 1.0));

    vec3 fog = mix(vec3(0.08, 0.12, 0.18), vec3(0.25, 0.34, 0.48), mistA);
    vec3 auroraColor = mix(vec3(0.14, 0.45, 0.72), vec3(0.55, 0.86, 0.94), mistC);
    vec3 warmBridge = vec3(0.42, 0.26, 0.14) * smoothstep(0.6, -0.25, uv.y) * (0.25 + mistB * 0.25);

    vec3 color = sky;
    color = mix(color, fog, fogMask * 0.68);
    color += auroraColor * aurora * 0.72;
    color += vec3(0.7, 0.82, 1.0) * stars * 0.7;
    color += warmBridge * (0.28 + 0.32 * (1.0 - revealEase));

    float bloom = max(0.0, 1.0 - length(screenUv * vec2(0.9, 1.08)) * 0.75);
    color += vec3(0.12, 0.24, 0.36) * bloom * (1.0 - revealEase) * 0.65;

    float vignette = smoothstep(1.45, 0.25, length(screenUv * vec2(0.94, 1.16)));
    float revealMask = smoothstep(-0.22, 0.96, revealEase - length(screenUv) * 0.26);

    color *= mix(0.7, 1.0, vignette);
    finalColor = vec4(color * revealMask * uBrightness, 1.0);
}
