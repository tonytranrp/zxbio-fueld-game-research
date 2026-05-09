#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Custom uniforms
uniform vec2 texelSize;
uniform float blurRadius;

// Output fragment color
out vec4 finalColor;

// Gaussian weights for 9-tap kernel
float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    // Texel color fetching from texture sampler
    vec3 texelColor = texture(texture0, fragTexCoord).rgb * weights[0];

    for (int i = 1; i < 5; i++)
    {
        float offset = float(i) * blurRadius * texelSize.y;
        texelColor += texture(texture0, fragTexCoord + vec2(0.0, offset)).rgb * weights[i];
        texelColor += texture(texture0, fragTexCoord - vec2(0.0, offset)).rgb * weights[i];
    }

    finalColor = vec4(texelColor, 1.0) * colDiffuse;
}
