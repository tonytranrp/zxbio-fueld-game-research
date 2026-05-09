#pragma once
#include <string_view>

namespace biofuel::shader_source {
inline constexpr std::string_view blur_h_source = R"shader(#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 texelSize;
uniform float blurRadius;

out vec4 finalColor;

float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec3 texelColor = texture(texture0, fragTexCoord).rgb * weights[0];

    for (int i = 1; i < 5; i++)
    {
        float offset = float(i) * blurRadius * texelSize.x;
        texelColor += texture(texture0, fragTexCoord + vec2(offset, 0.0)).rgb * weights[i];
        texelColor += texture(texture0, fragTexCoord - vec2(offset, 0.0)).rgb * weights[i];
    }

    finalColor = vec4(texelColor, 1.0) * colDiffuse;
}
)shader";

inline constexpr std::string_view blur_v_source = R"shader(#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 texelSize;
uniform float blurRadius;

out vec4 finalColor;

float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec3 texelColor = texture(texture0, fragTexCoord).rgb * weights[0];

    for (int i = 1; i < 5; i++)
    {
        float offset = float(i) * blurRadius * texelSize.y;
        texelColor += texture(texture0, fragTexCoord + vec2(0.0, offset)).rgb * weights[i];
        texelColor += texture(texture0, fragTexCoord - vec2(0.0, offset)).rgb * weights[i];
    }

    finalColor = vec4(texelColor, 1.0) * colDiffuse;
}
)shader";

} // namespace biofuel::shader_source
