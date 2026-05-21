#version 330

// "Opening Gates" — Cinematic double-door loading screen
// Two industrial blast-door panels are visible the entire loading duration.
// When uRevealProgress approaches 1.0, the doors dramatically slide apart,
// revealing the void behind just before the crossfade to the next screen.
//
// uBrightness  — controls the overall fade-in (panels appear from darkness)
// uRevealProgress — controls the door OPENING (0 = closed, 1 = fully open)
//   Configured with a long delay by startup screens so doors stay closed
//   during loading and only open near the end.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec3 iResolution;
uniform float iTime;
uniform float uBrightness = 0.0;
uniform float uRevealProgress = 0.0;

out vec4 finalColor;

// ---- Utility ----

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float hash11(float n) {
    return fract(sin(n * 127.1) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(i), hash21(i + vec2(1, 0)), f.x),
        mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x),
        f.y
    );
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p = p * 2.1 + vec2(1.7, 3.2);
        a *= 0.48;
    }
    return v;
}

// ---- Door Panel Surface ----

vec3 panelSurface(vec2 localUv, vec2 globalUv, float edgeDist, float time, float doorOpen) {
    // Base: dark brushed metal
    vec3 metalDark = vec3(0.050, 0.045, 0.060);
    vec3 metalMid  = vec3(0.090, 0.080, 0.100);

    // Brushed metal: horizontal streaks
    float brushed  = noise(vec2(localUv.x * 3.0, globalUv.y * 35.0));
    float brushed2 = noise(vec2(localUv.x * 2.0, globalUv.y * 65.0));
    float metalTex = mix(brushed, brushed2, 0.4);
    vec3 base = mix(metalDark, metalMid, metalTex * 0.65);

    // Horizontal panel bands (blast-door segments)
    float bandY   = fract(globalUv.y * 5.0 + 0.5);
    float bandGap = smoothstep(0.0, 0.007, bandY) * smoothstep(0.0, 0.007, 1.0 - bandY);
    base *= mix(0.50, 1.0, bandGap);
    // Red accent in the panel gaps
    base += vec3(0.10, 0.025, 0.01) * (1.0 - bandGap) * 0.35;

    // Vertical seam lines
    float seamX    = fract(localUv.x * 2.5);
    float seamLine = smoothstep(0.0, 0.005, seamX) * smoothstep(0.0, 0.005, 1.0 - seamX);
    base *= mix(0.70, 1.0, seamLine);

    // Faint circuit traces with animated pulse
    float circuit      = noise(vec2(localUv.x * 12.0, globalUv.y * 8.0));
    float circuitLine  = smoothstep(0.48, 0.50, circuit) * smoothstep(0.52, 0.50, circuit);
    float circuitPulse = sin(time * 2.0 + localUv.x * 5.0 + globalUv.y * 3.0) * 0.5 + 0.5;
    base += vec3(0.30, 0.07, 0.02) * circuitLine * circuitPulse * 0.35;

    // Edge glow (warm light from center seam) — gets brighter as doors start opening
    float edgeLight = exp(-edgeDist * 4.5);
    float glowBoost = mix(1.0, 2.5, doorOpen);
    base += vec3(0.80, 0.22, 0.04) * edgeLight * 0.45 * glowBoost;

    // Accent trim line near the edge
    float trimDist = abs(edgeDist - 0.04);
    float trim     = smoothstep(0.004, 0.0, trimDist);
    base += vec3(0.65, 0.14, 0.03) * trim * 0.7;

    // Animated scan line sweeping down the panel
    float scanY = fract(globalUv.y * 0.5 - time * 0.12);
    float scan  = smoothstep(0.012, 0.0, abs(scanY - 0.5));
    base += vec3(0.25, 0.06, 0.02) * scan * 0.35;

    // Second scan line (slower, opposite direction)
    float scanY2 = fract(globalUv.y * 0.3 + time * 0.08);
    float scan2  = smoothstep(0.008, 0.0, abs(scanY2 - 0.5));
    base += vec3(0.15, 0.08, 0.04) * scan2 * 0.2;

    // Subtle breathing pulse
    float pulse = sin(time * 1.2) * 0.5 + 0.5;
    base += vec3(0.06, 0.025, 0.01) * pulse * 0.12 * max(0.0, 1.0 - edgeDist * 0.5);

    // Rivet dots at panel intersections
    vec2 rivetUv = vec2(localUv.x * 2.5, globalUv.y * 5.0);
    vec2 rivetCell = fract(rivetUv) - 0.5;
    float rivet = smoothstep(0.06, 0.04, length(rivetCell));
    base += vec3(0.12, 0.10, 0.13) * rivet * 0.4;

    return base;
}

// ---- Spark Particles ----

float sparkLayer(vec2 uv, float doorEdge, float time, float seed) {
    float totalSparks = 0.0;
    for (float i = 0.0; i < 12.0; i++) {
        float h  = hash11(i + seed * 37.0);
        float h2 = hash11(i + seed * 71.0 + 5.0);

        float sparkX = doorEdge + (h - 0.5) * 0.06;
        float sparkY = (h2 - 0.5) * 2.0;

        float life = fract(time * (0.4 + h * 0.6) + h * 10.0);
        sparkX += (h - 0.5) * life * 0.25;
        sparkY += life * 0.12 - life * life * 0.35;

        float dist  = length(uv - vec2(sparkX, sparkY));
        float spark = smoothstep(0.012, 0.0, dist);
        float fade  = (1.0 - life) * (1.0 - life);
        totalSparks += spark * fade;
    }
    return totalSparks;
}

void main()
{
    vec2 R = iResolution.xy;
    float aspect = R.x / R.y;

    // UV: y in [-1, 1], x in [-aspect, aspect]
    vec2 uv = (2.0 * gl_FragCoord.xy - R) / R.y;
    vec2 screenUv = gl_FragCoord.xy / R;

    float reveal     = clamp(uRevealProgress, 0.0, 1.0);
    float revealEase = reveal * reveal * (3.0 - 2.0 * reveal);

    // === DOOR OPENING (controlled by uRevealProgress) ===
    float doorTravel = aspect + 0.2;
    float doorOffset = revealEase * doorTravel;

    float leftEdge  = -doorOffset;
    float rightEdge =  doorOffset;

    bool isLeftDoor  = (uv.x < leftEdge);
    bool isRightDoor = (uv.x > rightEdge);
    bool isGap       = !isLeftDoor && !isRightDoor;

    vec3 col = vec3(0.0);

    // === CENTER SEAM GLOW (visible when doors are closed / nearly closed) ===
    float seamGlow  = exp(-abs(uv.x) * 14.0) * (1.0 - revealEase);
    float seamPulse = sin(iTime * 2.5) * 0.3 + 0.7;
    // Seam brightens as doors are about to open
    float preOpenWarning = smoothstep(0.0, 0.15, revealEase) * (1.0 - revealEase);

    // === GAP (visible between the doors as they open) ===
    if (isGap && revealEase > 0.001) {
        vec3 voidCol = vec3(0.018, 0.014, 0.024);

        float gapHalfWidth = max(doorOffset, 0.001);
        float normX        = abs(uv.x) / gapHalfWidth;
        float gapNarrow    = 1.0 - revealEase;

        // Bright core beam — intense when gap is narrow
        float beam     = exp(-normX * mix(0.8, 6.0, gapNarrow));
        vec3 beamColor = vec3(0.95, 0.40, 0.08) * beam * gapNarrow * 2.5;

        // Vertical light rays drifting through
        float rays = 0.0;
        for (float i = 0.0; i < 4.0; i++) {
            float yOff = sin(iTime * 0.4 + i * 1.8) * 0.25;
            float ray  = exp(-abs(uv.y - yOff) * 2.5) * exp(-abs(uv.x) * 1.8);
            rays += ray * 0.12;
        }
        vec3 rayColor = vec3(0.75, 0.18, 0.04) * rays * gapNarrow;

        // Swirling fog in the void
        float fog = fbm(uv * 2.5 + vec2(iTime * 0.06, iTime * 0.03)) * 0.1;
        vec3 fogCol = vec3(0.06, 0.04, 0.05) * fog;

        col = voidCol + beamColor + rayColor + fogCol;
        col *= mix(1.0, 0.3, smoothstep(0.6, 1.0, revealEase));
    }

    // === DOOR PANELS ===
    // When doors are closed (revealEase ≈ 0), both panels fill the screen.
    // The "edge" at uv.x = 0 is the center seam.
    if (isLeftDoor || (isGap && revealEase < 0.001 && uv.x <= 0.0)) {
        float edgeDist = max(0.0, leftEdge - uv.x);
        if (revealEase < 0.001) {
            edgeDist = abs(uv.x); // distance from center seam when closed
        }
        vec2 localUv = vec2(edgeDist, uv.y);
        col = panelSurface(localUv, uv, edgeDist, iTime, revealEase);
    }

    if (isRightDoor || (isGap && revealEase < 0.001 && uv.x > 0.0)) {
        float edgeDist = max(0.0, uv.x - rightEdge);
        if (revealEase < 0.001) {
            edgeDist = abs(uv.x); // distance from center seam when closed
        }
        vec2 localUv = vec2(edgeDist, uv.y);
        col = panelSurface(localUv, uv, edgeDist, iTime, revealEase);
    }

    // === SPARKS along door edges (only when doors are actively moving) ===
    float sparkIntensity = revealEase * (1.0 - revealEase) * 4.0;
    if (sparkIntensity > 0.01) {
        float sparksL = sparkLayer(uv, leftEdge,  iTime, 1.0);
        float sparksR = sparkLayer(uv, rightEdge, iTime, 2.0);
        vec3 sparkColor = vec3(1.0, 0.55, 0.15);
        col += sparkColor * (sparksL + sparksR) * sparkIntensity * 0.8;
    }

    // === LIGHT SPILL from gap onto door surfaces ===
    if (revealEase > 0.01) {
        float distFromGap = 0.0;
        if (isLeftDoor)  distFromGap = leftEdge - uv.x;
        if (isRightDoor) distFromGap = uv.x - rightEdge;
        if (distFromGap > 0.0) {
            float spill = exp(-distFromGap * 5.0) * (1.0 - revealEase);
            col += vec3(0.55, 0.13, 0.03) * spill * 0.35;
        }
    }

    // === CENTER SEAM (applied on top when doors are closed) ===
    col += vec3(0.70, 0.18, 0.04) * seamGlow * seamPulse * 0.45;
    col += vec3(0.90, 0.30, 0.06) * preOpenWarning * seamGlow * 0.6;

    // === VIGNETTE ===
    float vig = 1.0 - length(uv / vec2(aspect, 1.0)) * 0.28;
    col *= max(vig, 0.0);

    // === FINAL OUTPUT ===
    finalColor = vec4(col * uBrightness, 1.0);
}
