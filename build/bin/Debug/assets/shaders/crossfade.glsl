#version 330

// Crossfade fragment shader — blends two screen captures by progress.
// texture0 = outgoing screen (bound to render texture draw call)
// textureIn = incoming screen (set via uniform)
// progress  = 0.0 shows outgoing fully, 1.0 shows incoming fully

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D textureIn;
uniform float progress;

out vec4 finalColor;

void main() {
    vec4 outColor = texture(texture0, fragTexCoord);
    vec4 inColor  = texture(textureIn, fragTexCoord);
    finalColor = mix(outColor, inColor, progress);
}
