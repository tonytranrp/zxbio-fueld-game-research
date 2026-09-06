# The Lin-look, collision & lag pass (2026-09-05) — decision log

The user came back from the first micro-voxel build (`research/micro-voxel-pivot-log.md`) with
three screenshots and five complaints, quoted from the request: shadows that "showed up when I stay
still" and "shadow circles" after a rebuild; "I can't go through blocks or the mountains" (wanted:
collision); lag that "isn't from the chunks or the rendering" but appears "when I go close to
mountains"; a "swirly" artifact on every slope; and the look itself — "John Lin's ... fine grains and
smooth ... instead of our micro voxel looking so blocky". Plus a structural ask (materials as
self-contained component files instead of parallel tables with hardcoded offsets), handled in its
own group.

This file records what each complaint actually was (measured, not assumed), what was changed, and
the number that shows it. Backlog: `docs/goals.md` groups Z–AC (goals 164–175).

## 0. Method

Two research agents ran first: a read-only survey of every hardcoded material site (the input to
the materials group) and a live-web pass on John Lin's renderer, voxel anti-aliasing, and LOD-
dependent self-shadowing (sources in §6). Every rendering conclusion below was then reached by one
of three instruments, in this order of preference:

1. **`tools/svo_render` at the same pose** — the CPU reference marcher renders the same tree with
   the same shading; `--lod-center x,y,z` (new) builds the LOD around a point OTHER than the camera,
   which reproduces "the camera moved away from the last build center" deterministically.
2. **Debug views** (`--debug-view lit|ao|normal|facenormal|level|steps|coverage|cubepx|smooth|lodcube|material|distance`,
   the same names as `svo_render --view`): one shading term per frame, so a wrong frame names its
   cause instead of being stared at.
3. **Bisection by editing the shader** (shaders load at runtime, no rebuild) and **sampling one
   pixel row** with a five-line Python script — the water bug (§4) fell to this in four runs after
   every debug view had come back uniform.

## 1. The shadow rings (goal 164)

**What it was.** Secondary rays judged their LOD by distance from the EYE (`t + t_offset` in the
early-out test, where `t_offset` was the primary hit distance). The early-out fires on any internal
node whose child edge is under the threshold — including the node CONTAINING the ray's own origin.
The shadow ray's threshold (4x the primary's) exceeds a surface's local brick edge exactly when
`t_eye > 2.4 * d_center` (brick edge at distance `d` from the build center ≈ `8 * finest * d / lod_radius`;
threshold = `4 * t * pixel_angle`): every surface point that is more than ~2.4x farther from the
camera than from the tree's build center self-shadows and self-occludes (AO went to 0.4 too). At
rest `t ≈ d` and nothing happens; move away from the build center and a dark disc grows around it;
the rebuild lands centered where the camera WAS when it started, so the disc appears "when it's
done rebuilding" and slides around as the camera keeps moving — the user's exact description.
Reproduced on the CPU with `--lod-center` 20 m off the camera: the nearest slope went dark; the `lit`
view showed the disc.

**Fix.** Secondary rays judge LOD by distance from THEIR OWN origin (`t_offset = 0`, multipliers
4x shadow / 8x AO). Provably self-hit-free: the threshold is 0 at the origin, so the descent reaches
the real leaf, and the DDA starts in the air voxel the primary ray just crossed. Cost is bounded by
the tree's own LOD (voxel ≈ pixel footprint at rest), so the shadow ray's "full resolution near the
origin" is the resolution the tree has there anyway. CPU tool, same pose: shadowed hits 3.3% → 0.6%
after the staircase lift below; secondary steps/pixel 21.8 → 22.1 (unchanged cost).

**Two follow-on rules found while looking at the `lit` view** (goal 166, 171):
- *Staircase self-shadowing is scale-free.* A slope of tangent `s` built from steps of ANY size puts
  `s / tan(sun elevation)` of every tread in its own riser's shadow — 47% of a 45° slope under this
  sun. It is invisible as terracing at coarse LOD and becomes moiré at pixel-sized steps. The
  shadow origin is now lifted one local cube along the averaged surface normal (AO: half a cube),
  Gustafsson's "offset the ray origin a safe distance based on the normal" (§6). Solid leaves get
  no lift — a water body's top face IS its surface, and lifting it along a parent's average normal
  put origins inside the shore (a checkerboard on the water in an intermediate build, caught by the
  D3D12 capture).
- *An LOD node that is 5% solid is not a wall.* The `lit` view at the spawn pose showed blocky black
  patches on sun-facing slopes near ridges: the shadow ray, coarse far from its origin, hit nodes
  treated as solid that were mostly air. Every internal node now stores its volume coverage (§2);
  secondary rays descend nodes under 35% coverage instead of hitting them. Primary rays keep 0
  (closed silhouettes).

## 2. Layout v2: a normal and a coverage per node (goals 165, 167)

The tree-side half of the look. Internal nodes and brick leaves carry one ATTRIBUTE word after the
header: the area-weighted average normal of the node's exposed faces (three int8) and its volume
coverage (uint8). Built bottom-up in the same pass that builds the tree:

- A brick's exposed faces are counted with row bit tricks (`Brick::exposed_face_sum`, ~400 ops per
  brick — no measurable build-time change: 0.57–0.78 s per tree, same as before), weighted by the
  brick's voxel face area so coarse and fine children mix by real surface area.
- A solid child against an ABSENT sibling contributes one whole child face at the parent (the
  coarse staircase a brick cannot see).
- Coverage: brick = occupied/512, solid = 1, absent = 0, internal = mean of eight.

Tests: the sphere tree's surface bricks report normals within 35° of radial (all of them), its root
reports coverage 0.128 = 33,510/262,144 within 1%, and a summed closed-surface normal near zero;
the brute-force traversal oracle still passes with 0 mismatches over 7,000 rays (layout v2 changes
child-slot arithmetic everywhere). Memory: +1 word per internal node and brick leaf ≈ 3 MB on a
350 MB tree.

**Shading uses it as a blend.** `cubePixels` = the hit cube's projected size. Face normal weight =
`saturate((cubePixels − 1.5) / 3)`: cubes wider than 4.5 px shade as cubes (the John Lin close-up),
cubes near a pixel shade with the averaged normal of the ancestor spanning ~6 px (`--smooth-pixels`).
The `smooth` debug view shows the averaged field as soft facets; the `facenormal` view shows the raw
staircase — that view IS the swirl. Per-cube brightness grain (`--grain 0.10`, Binks' recipe, §6)
fades to zero by 1.5 px: a per-cube hash at pixel frequency is structured noise against the pixel
grid, its own moiré (seen in the first CPU render at ±5%).

## 3. Temporal anti-aliasing (goal 168, was 159)

`svo_taa.psh.hlsl`: the march jitters every primary ray by a Halton(2,3) sub-pixel offset and writes
hit distances to a second target; the resolve rebuilds each pixel's world position from that
distance, reprojects it through the previous frame's view-projection (the world is static, so camera
motion is the only motion — no motion-vector buffer), rejects history when the previous frame saw a
different distance there (2% + 5 cm), clamps it to the current 3x3 neighborhood and blends at 1/8.
Two render targets from the resolve: the post chain's scene target and the next history. Costs one
fullscreen pass; GPU march+resolve measured 3.2–6.3 ms (§5). `--no-taa` for A/B; debug views run
without it. `--verify-frame` reads 34.2% on both backends (the pre-pass 48% counted the moiré as
"contrast"; 34% is the terrain's real texture, still 5x the 6% threshold).

## 4. The water checkerboard (goal 169)

Every debug view on the water came back uniform (lit = 1, material = Water, face normal = +y,
level and distance smooth), yet the frame had white and light-blue cells ~4 m across near the
shore — and so did the user's second screenshot, from before this pass. Bisected by swapping
`ShadeWater`'s return line and sampling pixel row y = 690 across the cells:

| return | water pixels (sRGB) |
|---|---|
| `body` | 105,145,171 flat |
| `lerp(body, reflection, fresnel)` | 107,146,172 flat |
| `body + glint` | 134 → 208 → 170 |

The sun glint alone. With the sun high and the camera looking down, the half-vector is nearly
vertical, so a `pow(., 256)` highlight (2.5x sun color in the original) fired wherever the two
3–7 m ripple trains tilted the normal through the peak — one bright cell per lattice cell. Now a
broad, dim highlight (`0.18 * pow(., 24)`) plus a half-meter noise term: the same row reads
122 → 146 → 140, a smooth glitter band. (The first attempt — noise on the trains, sky-gradient
instead of full-sky reflection — was right about the lattice and wrong about the term; the
bisection is what settled it.)

## 5. The lag (goal 170) — measured, then fixed

The user was right that it was not the rendering: GPU march+resolve (new timestamp query in the
overlay, `gpu march+resolve: N ms`) is 3.2–6.3 ms everywhere measured. A slow-frame attributor was
added to the svo loop: every frame over 20 ms logs its phase breakdown (upload / camera+collision /
render / post / overlay / present) and what was happening (uploading, tree swapped, building). A
900-frame `--autofly --walk` A/B (ground level, trees growing from 205 to 540 MB as the fine ring
fills with surface):

| configuration | slow frames (>20 ms) | attribution | worst |
|---|---|---|---|
| 16 build threads, 32 MB slices (as shipped) | 13 | 12 `present` stalls of 20–30 ms while building, 1 swap | 33.6 ms |
| 8 MB slices | 24 | more upload frames to coincide with the build | 111.6 ms |
| **12 build threads** | 1 | the swap frame (buffer creation, 29 ms) | 30.1 ms |
| 12 threads, 8 MB | 1 | the swap frame | 39.7 ms |

So: a build on every hardware thread starved the render thread — `present` blocked waiting for CPU
time — and that is the stutter the user felt "when the smaller voxels come in". The pool now defaults
to three quarters of the hardware threads. The remaining hitch was the swap frame creating two
~250 MB GPU buffers; the previous tree's buffers are now kept as a spare pair and reused (25%
headroom on growth). Final: **fly `--autofly`, 900 frames: 0 frames over 20 ms, worst 16.9 ms**
(38 ms before this pass); walk: 3 slow frames, all on swaps where the tree outgrew its buffers
(one-time growth, 43 ms). Also moved off the frame: the collider's 6 ms height-cache refresh
(background thread; the first one is paid before the loop).

## 6. Collision (goal 172)

`world/collision`: `SolidQuery` concept (one `overlaps_solid(Aabb)`), `move_and_slide` (y, then x,
then z, bisected against the query, sub-stepped at 0.25 m so a 1.1 m boost-speed frame cannot jump a
0.5 m trunk — the first version tested end positions only and a 4 m test move passed through a 1 m
wall; ledge step-up to 0.55 m in walk mode; a body that starts inside solid moves unblocked, never
trapped), and `TerrainCollider`: the sparse tree's own voxelization rule applied to the same height
function over a cached 16 m / 3.1 cm grid (513² samples, one SIMD call, ~6 ms on a background
thread) plus the deterministic tree placements' trunk boxes. It never reads the rendered tree, so it
does not depend on the renderer's LOD or rebuild lag; the rendered surface and the collision
surface are the same function. Camera body 0.6 x 1.75 m, eye 5 cm under the top; collision in fly
AND walk modes (`--noclip` restores the old spectator). Tests (9): floors, walls, sliding, low vs
tall ledges, thin-wall tunneling at 200 small steps, a body dropped at 40 random places settles
within 11 cm of the footprint's highest column and never below the surface, a 5 m cliff walk never
enters the hill, a trunk stops the body at both 5 cm and 1.1 m steps and a canopy does not.
`--autofly --walk` still reports 0 frames below the ground surface.

## 7. What was decided against, with the reason

- **Per-voxel stored normals** (Rundlett/Dwyer, §6): 3–4x the brick bytes; the per-NODE average
  blended by projected size gives the same distant smoothness and keeps the close-up cubes, which
  the user asked for ("still a voxel but blends").
- **Coverage-weighted (cone-traced) shadows**: the 35% threshold gets the thick-wall cases; soft
  shadows are a look change the user did not ask for.
- **Colliding against the octree**: correct today only near the build center; the analytic query
  agrees with what is drawn everywhere and costs nothing per tree. The concept boundary keeps the
  swap cheap when editing (goal 160) makes the analytic world stale.
- **Diligent's own TAA** (DiligentFX): needs motion vectors and its PostFXContext machinery for a
  static world; our resolve is 80 lines and uses the distance we already write.
- **Palette-compressed bricks for the swap hitch**: the fix was measured to be thread starvation
  and buffer creation, not bytes; goal 157 stays open with its number.

## 8. Sources

Research agent report (live, 2026-09-05): Lin's own video descriptions (path-traced, temporal-only
denoiser, per-voxel attributes as a stated requirement) and blog; Dwyer devlog 17 and Rundlett
devlog 18 (per-voxel normals as "the secret sauce", TSR from kajiya); Laine & Karras ESVO source
(`Raycast.inl`: terminate when `tc_max * dir_sz + orig_sz >= scale_exp2`, k = 1) and paper
(post-filtered shading attributes); GigaVoxels cone tracing (prefiltered attributes, quadrilinear
LOD); Fessler's Derived Surface Shading whitepaper (density-gradient / occupancy-centroid normals,
"shading snaps between the six face directions"); Gustafsson, *From screen space to voxel space*
(offset shadow origins, soft shadows hide the grid); Teknologicus's Vorxel devlog (the same LOD
shadow-origin bug, and averaging only surface voxels into mips); Binks, enkisoftware devlog
2014-10-22 (sub-voxel pattern faded toward its mean at pixel frequency); Playdead TRAA (Halton
jitter, neighborhood clamping); Tardif's TAA notes; Bikker's voxel series part 5 (reprojection as
"through which pixel was P visible last frame"). Every number above is from a run on this machine
(RTX 4070 Laptop, 1280x720, Release preset); the captures are in `research/captures/lin_*.png`:
`lin_final_vk.png` / `lin_final_d3d12.png` (the pass's final frame on both backends, TAA on),
`lin_view_facenormal.png` (the raw staircase normal — the swirl made visible) against
`lin_view_smooth.png` (the averaged per-node normal field), `lin_shadow_rings_before_cpu_lit.png`
(`svo_render --lod-center` 20 m off the camera, `--view lit`, before §1's fix: the disc and the
contour rings) against `lin_shadow_rings_after_cpu.png` (same pose, after), and
`lin_water_checkerboard_before.png` / `lin_water_checkerboard_after.png` (§4, crops of the shore).
