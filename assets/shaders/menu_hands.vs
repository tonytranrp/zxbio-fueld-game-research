#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec3 fragPosition;
out float fragFingerMask;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

uniform float uTime;
uniform float uPortalStrength;
uniform float uFingerStartY;
uniform float uFingerEndY;

void main() {
    vec3 localPos = vertexPosition;

    float fingerMask = smoothstep(uFingerStartY, uFingerEndY, localPos.y);
    float fingerBand = floor((abs(localPos.x) + 0.08) * 7.0);
    float fingerWave = sin(uTime * 4.2 + fingerBand * 0.95 + localPos.z * 3.5) * 0.5 + 0.5;
    float curl = fingerMask * (0.025 + 0.050 * fingerWave) * (0.35 + 0.65 * uPortalStrength);

    localPos.y -= curl * 0.85;
    localPos.z += curl * 0.55;
    localPos.x -= sign(localPos.x + 0.0001) * curl * 0.18;

    float palmMask = smoothstep(uFingerStartY - 0.18, uFingerStartY + 0.06, localPos.y);
    localPos.z += palmMask * sin(uTime * 1.8 + localPos.x * 2.4) * 0.02 * uPortalStrength;

    vec4 worldPos = matModel * vec4(localPos, 1.0);

    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = worldPos.xyz;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragFingerMask = fingerMask;

    gl_Position = mvp * vec4(localPos, 1.0);
}
