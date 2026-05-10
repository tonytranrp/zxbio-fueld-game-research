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
    float sideSign = (abs(localPos.x) < 0.0001) ? 1.0 : sign(localPos.x);

    float fingerMask = smoothstep(uFingerStartY, uFingerEndY, localPos.y);
    float fingerBand = floor((abs(localPos.x) + 0.08) * 7.0);
    float energy = sin(uTime * 2.8 + fingerBand * 0.8 + localPos.y * 2.2) * 0.5 + 0.5;
    float fingertipLift = fingerMask * (0.002 + 0.004 * energy) * uPortalStrength;
    float palmMask = smoothstep(uFingerStartY - 0.22, uFingerStartY + 0.10, localPos.y);
    float palmSway = sin(uTime * 1.7 + localPos.x * 2.0 + sideSign * 0.7) * 0.003 * palmMask * uPortalStrength;

    localPos.z += fingertipLift + palmSway;
    localPos.x += fingerMask * sin(uTime * 2.0 + localPos.y * 3.2) * 0.0015 * uPortalStrength * sideSign;

    vec4 worldPos = matModel * vec4(localPos, 1.0);

    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = worldPos.xyz;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragFingerMask = fingerMask;

    gl_Position = mvp * vec4(localPos, 1.0);
}
