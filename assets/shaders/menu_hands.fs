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

    float energy = sin(fragPosition.y * 10.0 - uTime * 3.4 + fragPosition.x * 4.5) * 0.5 + 0.5;
    float fingerGlow = mix(0.0, 1.0, fragFingerMask) * (0.35 + 0.65 * uPortalStrength);
    vec3 base = mix(uColorA, uColorB, energy * 0.68 + fingerGlow * 0.22);
    vec3 rim = uRimColor * fresnel * (0.4 + 0.9 * uPortalStrength);
    vec3 color = base * (0.82 + energy * 0.26) + rim;

    finalColor = vec4(color, colDiffuse.a);
}
