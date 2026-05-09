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

inline constexpr std::string_view blur_composite_source = R"shader(#version 330

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
)shader";

inline constexpr std::string_view crossfade_source = R"shader(#version 330

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
)shader";

inline constexpr std::string_view loading_prelude_source = R"shader(#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec3 iResolution;
uniform float iTime;
uniform float uBrightness = 0.0;
uniform float uRevealProgress = 0.0;

out vec4 finalColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) +
           (c - a) * u.y * (1.0 - u.x) +
           (d - b) * u.x * u.y;
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;

    for (int i = 0; i < 5; ++i) {
        value += noise(p) * amplitude;
        p = p * 2.03 + vec2(17.0, 11.0);
        amplitude *= 0.52;
    }

    return value;
}

void main() {
    vec2 resolution = iResolution.xy;
    vec2 uv = (2.0 * gl_FragCoord.xy - resolution) / resolution.y;

    float reveal = clamp(uRevealProgress, 0.0, 1.0);
    float revealEase = reveal * reveal * (3.0 - 2.0 * reveal);

    uv *= mix(1.14, 1.0, revealEase);
    uv.y += mix(0.05, 0.0, revealEase);

    float t = iTime * 0.22;
    vec2 driftUv = uv * vec2(1.1, 0.78);

    float fogField = fbm(driftUv * 2.0 + vec2(0.0, t * 0.9));
    float emberField = fbm(driftUv * 4.6 + vec2(t * 1.2, -t * 0.4));
    float ridgeField = fbm(vec2(driftUv.x * 1.35, driftUv.y * 2.8 - t * 0.3));

    vec3 bg = mix(vec3(0.055, 0.06, 0.09), vec3(0.12, 0.14, 0.19), uv.y * 0.5 + 0.5);
    vec3 fog = mix(vec3(0.2, 0.23, 0.29), vec3(0.46, 0.49, 0.58), fogField);
    vec3 ember = mix(vec3(0.28, 0.05, 0.01), vec3(0.88, 0.18, 0.02), pow(emberField, 3.0));

    float horizon = smoothstep(-0.55, 0.35, ridgeField - uv.y * 0.8);
    float glow = smoothstep(0.05, 0.95, emberField + fogField * 0.25);
    float centerMask = max(0.0, 1.0 - length(uv * vec2(0.95, 1.2)) * 0.82);

    vec3 color = bg;
    color = mix(color, fog, 0.65);
    color += ember * glow * 0.6;
    color += vec3(0.23, 0.08, 0.015) * centerMask * (1.0 - revealEase) * 0.5;
    color = mix(color, color + ember * 0.28, horizon * 0.55);

    float vignette = smoothstep(1.4, 0.2, length(uv * vec2(0.95, 1.18)));
    float revealMask = smoothstep(-0.25, 0.92, revealEase - length(uv) * 0.32);

    color *= mix(0.72, 1.0, vignette);
    finalColor = vec4(color * revealMask * uBrightness, 1.0);
}
)shader";

inline constexpr std::string_view mainmenu_bg_source = R"shader(﻿#version 330

// Voxel DDA Raymarcher — adapted from ShaderToy @xor
// https://www.shadertoy.com/view/XctSz8
// Compatible with raylib''s default vertex shader

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec3 iResolution;
uniform float iTime;
uniform float uBrightness = 0.0;
uniform float uRevealProgress = 0.0;

out vec4 finalColor;

#define SCALE 1.
#define MAX 100.
#define SPEED 7.

const float xA = -20.;
const float yA = -2.;

bool map(vec3 p)
{
    return dot(sin(p * .13), cos(p.yzx*.4536))+p.y*0.0561 > .9;
}

float map2(vec3 p)
{
    return dot(sin(p * .13), cos(p.yzx*.4536))+p.y*.061;
}

float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 411.7))) * 43758.5453);
}

vec3 hash33(vec3 p){ 
    float n = sin(dot(p, vec3(7, 157, 113)));    
    return fract(vec3(2097152, 262144, 32768)*n); 
}

void main()
{
    vec2 R = iResolution.xy;
    float reveal = clamp(uRevealProgress, 0.0, 1.0);
    float revealEase = reveal * reveal * (3.0 - 2.0 * reveal);

    vec2 screenUv = (2. * gl_FragCoord.xy - R) / -R.y;
    vec2 uv = screenUv * mix(1.16, 1.0, revealEase);
    uv.y += mix(0.08, 0.0, revealEase);
    
    vec3 ro = vec3(xA, yA, iTime * SPEED);
    vec3 rd = normalize(vec3(uv * 1., 1));
    rd += vec3(rd.x==0.0, rd.y==0.0, rd.z==0.0) * 1e-5;
    
    vec3 col = vec3(0);
    
    vec3 colArr[7] = vec3[7](
        vec3(0.839,0.071,0.),
        vec3(0.651,0.055,0.),
        vec3(0.439,0.035,0.),
        vec3(0.349,0.027,0.),
        vec3(0.383, 0.782, 1.0),
        vec3(0.542, 0.549, 0.625), 
        vec3(0.277, 0.133, 0.137)
    );
    
    vec3 axisDir = sign(rd);
    vec3 stepDir = (1. / abs(rd)) * 1.;
    
    vec3 vox = floor(ro);
    
    vec3 xyCrossing = ((vox-ro + .0)
    * axisDir + 0.6)
    * stepDir;
     
    vec3 axis, p, vox2;
    
    vec3 L = vec3(xA, yA, iTime*SPEED + 15.);
    
float accum = 0.0, att, voxDt, steps = 0.0;
    float transmittance = 1., stepL,henyey, newXyCros;
    vec3 lightAcc = vec3(0.0);
    
    for(float i = 1.0; i<MAX; i++)
    {
        if(map(vox)) break;
        
        xyCrossing *= 1.0 + hash33(xyCrossing)*.005;
        xyCrossing += hash33(vox + ro) * stepDir * 0.005;

        voxDt = length((vox+.5) - L)  * .2;
        
        att = 1./ (10. + voxDt * voxDt );
        
        accum += att;

        steps++;
        
        axis =  xyCrossing.x<xyCrossing.z? 
             ( xyCrossing.x<xyCrossing.y? vec3(1,0,0) : vec3(0,1,0) ):
             ( xyCrossing.z<xyCrossing.y? vec3(0,0,1) : vec3(0,1,0) );

        p = ro + dot(xyCrossing, axis) * rd;
        
        float angleL = dot(rd, normalize(p-L));
        
        float g = 0.45;
        henyey = (1. - g) / (4.*3.1416 * pow((1. + g*g - 2.*g*cos(angleL)),2.5)); 
        
        vox2 = vox;
        
        vox += axis * axisDir;
        
        newXyCros = dot(xyCrossing, axis);
        
        xyCrossing += axis * stepDir;
        
        stepL = dot(xyCrossing, axis) - newXyCros * 0.7;
        
        transmittance *= exp(-0.00712 * stepL);
        
        lightAcc += max(map2(p), 0.0) * henyey * transmittance * stepL * att;
    }
    
    vec3 grid = max(vec3(.7) - 2. * abs(fract(p+.5)-0.) * SCALE, 0.0);
    vec3 newGrid = mix(vec3(0.061),vec3(0),grid.x + grid.y + grid.z);
    
    float rand = hash(floor(vox)) * 2. + sin(iTime * .103 + .530) * 1.5 + .5;
    
    col =  colArr[int(clamp(rand, 0.0, 6.0))] * 1.;
    
    vec3 shade = mix(vec3(0.243,0.243,0.259), vec3(1.1), dot(axis, vec3(0,1,.5)));
    
    col = lightAcc * col;
    
    col -= newGrid * revealEase;
    
    float fog = steps/MAX;
    float fogAmount = clamp(fog * fog * mix(1.65, 1.0, revealEase), 0.0, 1.0);
    col = mix(col, vec3(0.2,.24,.3), fogAmount);

    float centerGlow = max(0.0, 1.0 - length(screenUv) * 0.65);
    col += vec3(0.14, 0.05, 0.01) * centerGlow * (1.0 - revealEase) * 0.55;

    float revealMask = smoothstep(-0.2, 0.95, revealEase - length(screenUv) * 0.42);
    finalColor = vec4(col * revealMask * uBrightness, 1.0);
}
)shader";

} // namespace biofuel::shader_source
