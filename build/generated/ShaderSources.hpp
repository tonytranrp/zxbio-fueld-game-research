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

    for (int i = 0; i < 6; ++i) {
        value += noise(p) * amplitude;
        p = p * 2.02 + vec2(19.0, 13.0);
        amplitude *= 0.52;
    }

    return value;
}

float starField(vec2 uv, float depth) {
    vec2 gridUv = uv * depth;
    vec2 cell = floor(gridUv);
    vec2 local = fract(gridUv) - 0.5;
    float sparkle = hash(cell);
    float radius = mix(0.22, 0.05, sparkle);
    float star = smoothstep(radius, 0.0, length(local));
    star *= smoothstep(0.82, 1.0, sparkle);
    return star;
}

void main() {
    vec2 resolution = iResolution.xy;
    vec2 screenUv = (2.0 * gl_FragCoord.xy - resolution) / resolution.y;

    float reveal = clamp(uRevealProgress, 0.0, 1.0);
    float revealEase = reveal * reveal * (3.0 - 2.0 * reveal);

    vec2 uv = screenUv * mix(1.18, 1.0, revealEase);
    uv.y += mix(0.08, 0.0, revealEase);

    float t = iTime * 0.14;
    vec2 flowUv = uv * vec2(0.9, 0.7);
    float mistA = fbm(flowUv * 1.45 + vec2(0.0, t));
    float mistB = fbm(flowUv * 2.6 + vec2(t * 0.18, -t * 0.52));
    float mistC = fbm(flowUv * 4.1 + vec2(-t * 0.26, t * 0.34));

    float auroraBand = sin(uv.x * 2.4 + mistB * 3.4 - t * 3.0) * 0.5 + 0.5;
    float auroraMask = smoothstep(-0.5, 0.45, uv.y + mistA * 0.42);
    auroraMask *= smoothstep(1.25, -0.1, uv.y);
    float aurora = pow(auroraBand, 2.3) * auroraMask;

    float fogMask = smoothstep(1.15, -0.2, uv.y);
    float starA = starField(uv + vec2(t * 0.18, 0.0), 18.0);
    float starB = starField(uv * 1.35 - vec2(t * 0.08, 0.0), 28.0);
    float stars = (starA * 0.8 + starB * 0.5) * smoothstep(0.15, 1.1, fogMask);

    vec3 skyLow = vec3(0.035, 0.05, 0.11);
    vec3 skyHigh = vec3(0.12, 0.19, 0.31);
    vec3 sky = mix(skyLow, skyHigh, clamp(uv.y * 0.5 + 0.5, 0.0, 1.0));

    vec3 fog = mix(vec3(0.08, 0.12, 0.18), vec3(0.25, 0.34, 0.48), mistA);
    vec3 auroraColor = mix(vec3(0.14, 0.45, 0.72), vec3(0.55, 0.86, 0.94), mistC);
    vec3 warmBridge = vec3(0.42, 0.26, 0.14) * smoothstep(0.6, -0.25, uv.y) * (0.25 + mistB * 0.25);

    vec3 color = sky;
    color = mix(color, fog, fogMask * 0.68);
    color += auroraColor * aurora * 0.72;
    color += vec3(0.7, 0.82, 1.0) * stars * 0.7;
    color += warmBridge * (0.28 + 0.32 * (1.0 - revealEase));

    float bloom = max(0.0, 1.0 - length(screenUv * vec2(0.9, 1.08)) * 0.75);
    color += vec3(0.12, 0.24, 0.36) * bloom * (1.0 - revealEase) * 0.65;

    float vignette = smoothstep(1.45, 0.25, length(screenUv * vec2(0.94, 1.16)));
    float revealMask = smoothstep(-0.22, 0.96, revealEase - length(screenUv) * 0.26);

    color *= mix(0.7, 1.0, vignette);
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
