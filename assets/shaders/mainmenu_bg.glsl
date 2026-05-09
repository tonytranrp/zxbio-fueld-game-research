#version 330

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
