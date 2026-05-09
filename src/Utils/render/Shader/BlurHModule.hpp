#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"

namespace biofuel::utils::render::shader {

class BlurHModule {
public:
    static constexpr std::string_view NAME = "blur_h";
    static constexpr std::string_view FRAGMENT_SOURCE = R"(#version 330

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
)";
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };
    static constexpr std::string_view UNIFORM_TEXEL_SIZE = "texelSize";
    static constexpr std::string_view UNIFORM_BLUR_RADIUS = "blurRadius";
};

} // namespace biofuel::utils::render::shader
