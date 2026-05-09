#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float uTime;
uniform vec2 uCenter;
uniform vec2 uHalfSize;
uniform float uSelectionStrength;
uniform float uHoverStrength;

out vec4 finalColor;

float saturate(float value) {
    return clamp(value, 0.0, 1.0);
}

void main() {
    vec4 sampleColor = texture(texture0, fragTexCoord) * colDiffuse;
    if (sampleColor.a <= 0.001) {
        discard;
    }

    vec2 halfSize = max(uHalfSize, vec2(1.0));
    vec2 itemUv = (gl_FragCoord.xy - uCenter) / halfSize;
    float selected = saturate(uSelectionStrength);
    float hovered = saturate(uHoverStrength);

    float sweepPhase = fract(uTime * 0.28);
    float sweepPos = mix(-1.15, 1.15, sweepPhase);
    float sweep = exp(-18.0 * abs(itemUv.x - sweepPos));
    float silhouette = saturate(1.0 - dot(itemUv * vec2(0.78, 1.22), itemUv * vec2(0.78, 1.22)));
    float edgeLift = pow(saturate(1.0 - abs(itemUv.x)), 2.0) * pow(saturate(1.0 - abs(itemUv.y)), 0.7);

    float highlight = selected * (0.28 + 0.72 * sweep) + hovered * 0.22;
    vec3 coolGlow = mix(vec3(0.22, 0.34, 0.48), vec3(0.86, 0.93, 1.0), sweep);
    vec3 glow = coolGlow * (0.22 * selected + 0.12 * hovered) * (0.45 + 0.55 * silhouette);
    glow += vec3(0.65, 0.78, 0.94) * edgeLift * highlight * 0.35;

    vec3 color = sampleColor.rgb * (1.0 + highlight * 0.65);
    color += glow;

    float alpha = sampleColor.a * saturate(selected * 0.95 + hovered * 0.7);
    finalColor = vec4(color, alpha);
}
