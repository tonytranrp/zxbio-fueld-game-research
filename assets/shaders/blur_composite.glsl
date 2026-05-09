#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float uDesaturation;
uniform float uVignetteStrength;
uniform float uDimStrength;

out vec4 finalColor;

void main() {
    vec4 color = texture(texture0, fragTexCoord) * colDiffuse;

    float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    color.rgb = mix(color.rgb, vec3(luminance), clamp(uDesaturation, 0.0, 1.0));

    vec2 centeredUv = fragTexCoord * 2.0 - 1.0;
    float vignette = smoothstep(1.2, 0.15, dot(centeredUv, centeredUv));
    float dimming = mix(1.0 - clamp(uDimStrength, 0.0, 1.0), 1.0, vignette);
    float edge = mix(1.0 - clamp(uVignetteStrength, 0.0, 1.0), 1.0, vignette);

    color.rgb *= dimming * edge;
    finalColor = color;
}
