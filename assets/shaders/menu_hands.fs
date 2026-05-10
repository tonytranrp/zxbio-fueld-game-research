#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragPosition;
in float fragFingerMask;

out vec4 finalColor;

uniform vec4 colDiffuse;
uniform float uTime;
uniform float uPortalStrength;
uniform vec3 uColorA;
uniform vec3 uColorB;
uniform vec3 uRimColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(-fragPosition);
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.7);

    float energy = sin(fragPosition.y * 6.0 - uTime * 1.2 + fragPosition.x * 2.4) * 0.5 + 0.5;
    float fingerGlow = mix(0.0, 1.0, fragFingerMask) * (0.12 + 0.28 * uPortalStrength);
    float sheen = pow(max(dot(normalize(vec3(0.4, 0.8, 0.5)), normal), 0.0), 10.0);
    vec3 base = mix(uColorA, uColorB, energy * 0.22 + fingerGlow * 0.08);
    vec3 body = mix(base, vec3(0.28, 0.78, 1.00), 0.04 + fragFingerMask * 0.02);
    vec3 rim = uRimColor * fresnel * (0.06 + 0.14 * uPortalStrength);
    vec3 color = body * (0.96 + sheen * 0.10) + rim;

    finalColor = vec4(color, colDiffuse.a);
}
