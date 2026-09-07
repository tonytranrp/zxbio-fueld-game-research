# Procedural Terrain Generation: Algorithms, Shipped Systems, and the Statistics of Real Earth — Part 7 (Synthesis)

Provenance: written by the algorithms-side research agent for the merged Earth-terrain document. Method: local context (question bank §G, `research/micro-voxel-creators-research.md`, `research/grass-rendering-research.md`, house style from `research/water-physics-and-wave-simulation.md`, the engine reality in `research/terrain-fixes-log.md`) absorbed first; then 27 web queries across 7 batches (Exa; game/GDC/vendor-doc sources dominate, geomorphology literature where the physics lives). Every "?"-marked parent anchor was treated as unverified and checked; disagreements are reported inline and tallied in the Provenance section — five this time, one a 25× arithmetic error worth fixing before the merge.

This is the synthesis layer. Parts 1–6 own the geoscience (Part 1 tectonics/mountains, Part 2 fluvial/landscape evolution, Part 3 glacial/coastal/periglacial, Part 4 caves/karst, Part 5 deserts/wind/climate, Part 6 vegetation); this part owns the **algorithms** — what noise is statistically, what erosion solvers compute, what shipped engines do, and how to check any of it against reality. Cross-references point at "Part N" per that mapping.

---

## 1. Noise-based terrain and its statistical failures

### 1.1 The fBm sum

Every procedural-terrain conversation starts from fractional Brownian motion approximated as a sum of band-limited noise octaves — "rescale and add" in Saupe's phrasing, as documented in Musgrave's dissertation [1][2]:

$$
\boxed{\;h(\mathbf{x}) = \sum_{i=0}^{O-1} A\, g^{\,i}\; N\!\left(f_0\, \lambda^{\,i}\, \mathbf{x}\right)\;}
$$

with octave count $O$, gain $g$ (amplitude falloff per octave, classically $0.5$), lacunarity $\lambda$ (frequency ratio, classically $2.0$), base frequency $f_0$, and $N$ a smooth coherent-noise basis (Perlin, simplex, value noise). In practice $O = 3\text{–}8$: Musgrave notes that more octaves are "unnecessary-to-detrimental" — frequencies below the viewport act only as a slope bias, frequencies above half the sampling resolution alias into stochastic noise [1][2]. The engine's own generator sits exactly in this band: 4 octaves, lacunarity 2, gain 0.5, amplitude 64, FastNoise2 pinned to SSE2, analytic `height_at` with a permanent smoothness regression test (`research/terrain-fixes-log.md` §Group R).

The spectral connection: an fBm with Hurst exponent $H$ has a 1D power spectrum $E(k) \propto k^{-\beta}$ with $\beta = 2H + 1$ (Turcotte's relation, equivalently $H_a = (\beta-1)/2$) [4]. So the classic $g = 0.5$, $\lambda = 2$ fBm is *Brownian motion*: $H = 0.5$, $\beta = 2$.

### 1.2 What real terrain's spectrum actually is

The parent anchor "$E(k) \propto k^{-2}$-ish" is **confirmed for second-order statistics, with two mandatory caveats**:- **Confirmed:** Turcotte's 1987 spherical-harmonic analysis found Earth topography "a well-defined fractal with D = 1.5 … Brown noise" — $\beta = 2$ along 1D angle-integrated transects [1], reproduced in his 2007 review [4]. Bathymetric studies span $\beta \approx 1.6\text{–}2.5$ by region and method (Berkson & Matthews 1.6–1.8; Fox & Hayes ~2.5; Gibert & Courtillot 2.1–2.3; Balmino ~2) [3].
- **Caveat 1 (dimensions):** those are 1D/angle-integrated exponents; the angle-*averaged* 2D spectrum carries $\beta + 1$ [3]. Validating a generator's 2D FFT against Turcotte's number without this conversion concludes your terrain is an octave too rough.
- **Caveat 2 (the monofractal lie):** Lovejoy, Schertzer, and Gagnon's analyses of four DEMs spanning 20,000 km down to 50 cm ($>2\times10^8$ pixels) show the $\beta \approx 2$ line holds for *second-order* moments from planetary scales down to ~40 m — but "the multifractal FIF is easily compatible with the data, while the monofractal fBm and fLm are not" [3][5]. Universal multifractal parameters: $\alpha \approx 1.79$ (0 = monofractal), $C_1 \approx 0.12$, and smoothing exponent $H$ differing by setting: $H = 0.46$ (bathymetry), $0.66$ (continents), $0.77$ (continental margins) [3]. Real terrain matches fBm in variance-per-scale and breaks it in the tails — extreme relief (Himalaya vs Indo-Gangetic plain in one tile) is far more common than a Gaussian cascade produces. This is exactly the failure Musgrave chased with his multifractal constructions [2][6][7].

### 1.3 The self-similarity failure

Real terrain is not self-similar, in two documented directions:

- **Vertical anisotropy (peaks vs valleys).** In rugged alpine terrain the peaks are more jagged than the valleys (valleys fill with detritus and get smoothed by glaciers); in diffusion-dominated sub-alpine terrain the hilltops are rounder than the valleys. Musgrave states this as the motivating asymmetry of his whole program — "fBm is by design homogeneous and isotropic, while real terrains are neither" [1][2]. A single-$H$ fBm has one roughness everywhere, up-slope and down-slope alike.
- **Horizontal anisotropy (direction-dependent $H$).** Mountain ranges are elongated; the scaling exponent along a range axis differs from across it. Lovejoy's analyses require an anisotropic scaling operator $\mathbf{G}$ beyond scalar $H$, and note that isotropic analysis "washes out different geomorphologies" [3][5]. Games notice this as the "everything looks the same from any direction" property of naive noise.
- **Scale-boundedness.** The $\beta \approx 2$ line breaks below ~40 m (trees) [3][5], and extended-self-similar analyses find the *local* Hurst exponent varies with scale — Lewis's critique, cited in the ESS work: "landscapes are fractal for only a few scales" [8]. Mandelbrot's quip, relayed by Musgrave: the fractal dimension of the Himalayas is approximately that of the JFK runway — only the crossover scale differs (kilometers vs millimeters) [1]. Crossover-scale modulation (where the fractal band sits), not dimension modulation, is the stronger artistic control — Musgrave's empirical conclusion [1][9].

### 1.4 The hypsometric failure

The parent anchor suggested pure fBm has "too little land at mid elevations." **Disagreement — the failure is the opposite shape.** Earth's hypsometric curve is *bimodal*: two primary elevation groupings — continents a few hundred meters above sea level, abyssal plains near −4,300 m — with ~29% of the surface above sea level [10][11]. Pedersen's 2024 analysis adds that the largest concentration of *land* area sits within a few meters of sea level (maximum at +2–5 m), with the curve steepened at mountains and trenches by active tectonics [11]. An fBm heightfield cut by a sea-level plane yields a *unimodal, roughly Gaussian* area-elevation distribution centered mid-range: far too much terrain at mid elevations, no double peak, and a land fraction that is an accident of the plane's placement rather than a consequence of two different crusts (Part 1's oceanic/continental dichotomy — the real cause of the bimodality [10]). The "blobby average everywhere" quality of fBm islands is this statistic, seen with the naked eye.

### 1.5 The drainage failure — the core one

fBm has no flow routing, therefore no coherent rivers, no valley networks, no ridges-between-valleys structure. Musgrave, 1989: "Fractal terrains in general have no global erosion features inherently due to isotropy and stationarity, and practically due to the difficulty in implementation and computation of such global processes, which require global communication" [1]. Every later development — his own hydraulic/thermal passes, World Machine's and Gaea's erosion nodes (§5), the hydrology-correctness industry (§4), and FastScape-style LEMs (§3) — exists to answer this one sentence. The community-facing version (with USGS elevation maps as the visual): real terrain's fractal shapes "are driven by erosion … the fractal pattern emerges from smaller streams merging into larger streams and rivers as they flow downhill" [12].

This is *the* #1 tell of fake terrain, ahead of every spectral subtlety in §1.2: drainage networks are the globally coherent structure that point-evaluated noise cannot produce, because a height sample at $\mathbf{x}$ in pure fBm is statistically independent of everything more than ~one feature-scale away.

### 1.6 When fBm is enough

- **Micro-relief** (centimeter-to-meter roughness on an already-correct macro surface): below ~40 m the multifractal analysis itself gives out into vegetation-dominated noise [3]; low-octave noise displacement is the standard, defensible choice.
- **Distant LOD.** At a few pixels per feature, second-order statistics dominate perception; the tail failures are invisible. Musgrave's Nyquist argument [1] is the formal version.
- **Base continental shape** — the low-frequency *skeleton* that later passes (tectonic stamps, SPIM carving, §3–4) reorganize. fBm as a *first draft* is fine; fBm as the deliverable is the failure.
- **Non-eroded settings:** recent volcanic terrain, dune fields (Part 5), badlands — Musgrave: "All natural terrains, except perhaps recent volcanic ones, bear the scars of erosion" [7].

---

## 2. Domain warping and noise variants

### 2.1 Domain warping = folded / sheared geology (confirmed anchor)

Warping evaluates $f(\mathbf{p} + \mathbf{h}(\mathbf{p}))$ instead of $f(\mathbf{p})$, with $\mathbf{h}$ itself noise — the canonical treatment is Inigo Quilez's, with the double-warp form $f(\mathbf{p} + 4\,\mathbf{r}(\mathbf{p} + 4\,\mathbf{q}(\mathbf{p})))$ [13]. The parent's claim that this imitates *folded/sheared geology* is **confirmed by the research literature**: Michel et al.'s Eurographics 2015 "Generation of Folded Terrains from Simple Vector Maps" builds exactly that correspondence — infer continental plates from sketched peaks, build a smoothed plate-velocity map $\mathbf{V}(\mathbf{p})$, and warp fBm noise by $w(\mathbf{p}) = \mathbf{p} + \mathbf{V}(\mathbf{p})$; the rapid variation of the translation vector across plate boundaries "compresses the noise in a single direction, perpendicular to the plate boundaries … this process generates what we intuitively interpret as folds" [14]. So the *principled* version of domain warp is not arbitrary swirl — it's a velocity/shear field with geologic meaning, attenuated away from plate boundaries. The unprincipled version (any fBm warp) just makes noise look less grid-aligned; community usage confirms it "resembles terrain deformed by tectonic movement," and No Man's Sky's custom noise ("uber noise") uses domain warping per the community reconstruction citing the GDC talk [12][15].

### 2.2 Ridged multifractal — the alpine look and its artifacts

Musgrave's ridged construction, verbatim in the surviving code and in Blender's OSL port [7][9][16]:

```
signal = offset - |noise(p)|;  signal *= signal;   // invert + square
result = signal; weight = 1;
for each octave:
    weight = clamp(signal * gain, 0, 1);           // multiplicative cascade
    signal = offset - |noise(p * lacunarity)|;  signal *= signal;  signal *= weight;
    result += signal * pow(lacunarity, -H·i);
```

Starting parameters from the author: $H \approx 0.25\text{–}1.0$, offset $\approx 1.0$, gain $\approx 2.0$ [7][9]. The $1-|N|$ crease turns the zero-crossings of the basis into sharp ridgelines at *every* scale (a "razorback at all scales" look [7]); the squaring sharpens further; the weight cascade makes it multifractal (roughness follows the ridges — high ridges get more octaves' worth of detail, which is qualitatively the right direction per §1.3's peak-jaggedness asymmetry, and is why this 1993-era hack still reads as "alpine").

**Artifacts, documented:** (a) ridges are *symmetric* — each side of a $1-|N|$ crease is a mirror, whereas real ridge lines are asymmetric (gentle windward dip vs steeper leeward scarp, glacially-gutted cirques on one side; Part 1/Part 3); (b) *ridge confluence wrongness* — creases of a scalar noise field meet in Y-junctions with statistically wrong angles and no notion of which ridge is the primary divide (divide migration and stream piracy, Part 2 §21, are meaningless to it); (c) it needs adaptive-LOD rendering to not alias — Musgrave: nearby ridges take "a saw-toothed appearance, as undersampled elevation values would generally lie on alternating sides of the ridgeline" [7]. Hybrid additive/multiplicative variants (his "hybrid multifractal," Bryce's "ridges"/"Mordor"/"shattered hills" presets) trade some sharpness for better-behaved scaling [7][9].

### 2.3 Billowed, terraced, and the rest

- **Billowed** ($|N|$ or $1-|N|$ without the weight cascade): rounded lumps — clouds, dune swells, cotton-ball foothills. The multiplicative multifractal version gives heterogeneous plains-foothills-mountains in one patch [7][9].
- **Terraced / quantized.** Gardner's 1980s terrain quantized altitude to yield "terraced land, such as mesas" [1]. **What real stair-step terrain is (confirmed):** resistant-layer stratigraphy plus differential erosion. The Grand Staircase is the type example: ~6,000 vertical feet of alternating cliffs and plateaus over ~150 miles; "each 'riser' is a cliff … as much as 2,000 feet high and each 'tread' is a plateau, terrace, or flat … as much as 15 miles wide"; hard sandstones/limestones form cliffs and terraces, soft shales/siltstones the slopes between [17][18][19]. Cosmogenic-erosion work adds the mechanism that matters for generation: strong-over-weak contacts get *undermined* (amplified erosion), weak-over-strong contacts grow protective benches — the stair-step is an erosion-rate pattern in layered rock, not a height quantization [20]. Procedurally: terrace the heightfield where a stratigraphy mask says so, then erode; Houdini's explicit `HeightField Terrace` SOP exists for exactly this [21]. Mesa outlines additionally want cap-rock logic (resistant layer above softer rock, Part 1).
- **Plane/rigid/warped combinations per landform.** Musgrave's parameter-table doctrine: modulate crossover scale with altitude (foothills→peaks), square-and-weight for ridges, offset for valleys [1][7][14]. Shipped-game/tutorial parameter tables vs terrain-science measurements: the former are aesthetic (gain 0.5, lacunarity 2, 3–8 octaves — universal across Musgrave, Blender, FastNoise2 defaults); the latter say the *bulk* spectrum matches $\beta \approx 2$ but $H$ should vary $0.46\text{–}0.77$ by setting [3] and crossover scale — not $D$ — is the artistic knob [1]. A generator honest about this exposes per-biome $H$ and crossover, not one global "roughness."

---

## 3. Erosion algorithms

### 3.1 Thermal erosion (the talus pass)

Musgrave's formulation is still the canonical one: a low-pass filter whose fixed point is a slope, not a height — "exactly like a standard low-pass filter, except that the value to which it converges is not a DC level but rather, for instance, a slope of 45 degrees. Slopes less than the angle of repose are unaffected" [9]. Iterate: for each pair of neighbors, if the height difference exceeds `talus` (a slope threshold in height-units per cell), move material downhill. O(n) per pass, trivially parallel, converges to angle-of-repose hillslopes — which is *genuinely correct physics* for soil-mantled and scree slopes (Part 2 §20: real threshold hillslopes; Montgomery's Olympic/Coast-Range slope histograms cluster at threshold values [22]).

**The soap-bubble artifact:** iterated to convergence, thermal erosion minimizes total height variance subject to the slope constraint — every convexity gets planed toward the talus angle, and the terrain ends up a network of planar facets meeting at ridges, the signature of a *minimum-energy soap film*, not of rock-mass-strength contrasts. Real talus fields are patchy (lithology, aspect, vegetation; Part 6 §66); the algorithm is lithology-blind and homogeneous. Production mitigation: mask the pass by rock-softness (Gaea's Selective Processing exposes exactly a Rock Softness bias mask [23]).

### 3.2 Droplet / hydraulic particle erosion

The Musgrave-1989 ancestor drops water on each vertex and tracks sediment capacity/erosion/deposition [1]. The modern hobby-standard is Sebastian Lague's implementation (itself following the firespark.de "Implementation of a method for hydraulic erosion" paper and the ranmantaru writeups [24][25][26][27]): bilinear height/gradient sampling, inertia, sediment capacity $\propto -\Delta h \cdot \text{speed} \cdot \text{water}$, erosion clamped to $\min(\text{capacity}-\text{sediment}, -\Delta h)$ — that clamp being the firespark paper's key insight ("erosion shouldn't exceed the height difference between points," which is what prevents the spike/pit instability Lague hit first [25]) — a radial erosion brush, point deposition, evaporation. Measured cost: **70,000 droplets on a 255×255 map in ~0.75 s** on a dev machine (2019) [25].

**What it gets right:** dendritic-looking channel networks emerge from pure point-dynamics — the visual reason it conquered the tutorial space. Crucially the *cause* is right in a way noise never is: concentrated flow carves, and tributaries join (§1.5's missing physics, in miniature).

**The stochastic-streak artifact:** droplets spawn at random points and follow the gradient field; early droplets deepen their own paths, which attract later droplets (positive feedback), and the result is a map scored by a finite sample of trajectories — streaky, seed-dependent channels, over-deepened along single-pixel paths, with un-eroded noise ridges between. Lague's own video shows the signature look: "these nice crisp ridges and … grooves down the side" [25] — aesthetic at game scale, statistically wrong (real channel networks have Horton-geometry branching, Part 2 §16, not $n$ independent random walks). Musgrave's 1989 verdict stands for the physically-serious version: the full transport PDEs are "nasty," need small timesteps, and were "not fast enough for our purposes in image synthesis" [9].

### 3.3 Grid-based stream-power solvers — FastScape / Braun–Willett

The stream-power incision model (Part 2 §15 owns the derivation; the algorithm is ours):

$$
\boxed{\;\frac{\partial h}{\partial t} = U - K\, A^{m} S^{n}\;}
$$

with uplift $U$, drainage area $A$ (proxy for discharge), slope $S$, erodibility $K$, and the concavity ratio $\theta = m/n \approx 0.5$ from real river profiles (observed slope–area exponents −0.35 to −0.6 [28]).

The Braun–Willett (2013) algorithm solves this **O(n)** and fully implicit, and the parent anchor is **confirmed as stated** [29][30]:

1. Route each node to its steepest-descent receiver; order all nodes into a single **stack** (topological order of the flow DAG) — this is the O(n) step, replacing the O(n·p²) brute force and the O(n log n) priority-queue methods that preceded it [29].
2. Accumulate discharge/area by sweeping the stack once, back-to-front — trivial, and it accepts *spatially varying precipitation* (an orographic field from §6 plugs straight in) [29].
3. March time implicitly node-by-node down the stack: because each node's new height depends only on its receiver's already-updated height, large time steps remain stable [29].

The stack-ordering idea is the single most stealable algorithm in this document for a game engine: it converts "global hydrology" from an iterative, convergence-sensitive simulation into two linear sweeps. The FastScape library family (fastscapelib-fortran and its C++ successor) packages SPL + sediment transport + hillslope diffusion + marine deposition, all implicit and O(n), plus O(n) depression-resolving flow routing and implicit O(n) glacial erosion; it couples to flexural isostasy and has run $10^8$-node problems on a laptop [30][31][32]. Flexural-isostasy coupling (erosion unloads the crust, the crust rebounds; Part 1 §4) is best treated in a game as a cheap low-pass rebound kernel, not a physical plate solver.

### 3.4 Cost/quality ranking and what each gets RIGHT

| Method | Cost | Gets right vs real physics | Structurally blind to |
|---|---|---|---|
| Thermal/talus pass | O(n)/iter, ms-scale | Angle-of-repose hillslopes (real threshold slopes [22]) | Lithology, everything fluvial; soap-facet artifact |
| Droplet/hydraulic particles | ~0.1–1 s per 256² map for $10^5$ droplets [25] | Flow concentration → dendritic channels; deposition fans | Network geometry (Horton's laws), sediment budgets, determinism |
| Grid SPIM (Braun–Willett) | O(n) per implicit step; $10^8$ nodes feasible [29][30] | Concave-up profiles, slope–area law, drainage reorganization, response to uplift/climate fields (Part 2's actual laws) | Threshold hillslopes (add diffusion term), landslides, glacial/braided channels |
| Full LEM (Child, Landlab, FastScape full stack) | minutes–hours at research scale | Everything above + deposition, stratigraphy | — (but see §10: planet-scale is out) |

Blunt call: for a real-time engine, thermal pass = cheap garnish; droplets = tutorials and small hero areas; SPIM-at-generation-time = the only option that buys *structural* correctness (coherent rivers, right concavity, divides in the right places) at a cost you pay once per world region, not per frame.

---

## 4. Hydrologically-correct terrain

The "rivers that end nowhere" bug class — every pseudorandom heightfield has internal basins whose outflow doesn't exist; fill them wrong and your river either lakes forever or teleports. The GIS/geomorphology literature solved this decades ago, and the algorithms are all game-adoptable.

### 4.1 Depression filling: Priority-Flood (Barnes et al.)

Flood the DEM inward from its edges using a priority queue keyed on elevation; pop the lowest queued cell, raise any unvisited neighbor to at least that cell's height, enqueue. The result "has no depressions or digital dams: every cell is guaranteed to drain" [33][34]. Complexity, **parent anchor confirmed** [33][34]:

$$
\boxed{\;T_{\text{int}} = O(n)\ \ (\text{integer DEMs, O(1) bucket queues});\qquad T_{\text{float}} = O(n \log_2 n)\ \ \text{(heap queue)}\;}
$$

with the improved variant — plain FIFO queue once inside a found depression — at $O(m \log_2 m)$, $m \le n$, the lowest known complexity for floating-point data, up to 37% faster in practice; the older Planchon–Darboux algorithm it dominates is ≥ $O(n^{1.2})$, and a *parallel* Planchon–Darboux needed six cores to match single-core improved Priority-Flood [33][34]. Pseudocode is 20 lines; the C++ reference is under 100 [33]. Variants fill-with-ε (Barnes' Algorithm 3, the standard "give flats a drainage gradient" fix), carve, and watershed-label — all in the same framework. For a game engine: quantize heights to integers (or fine buckets) and you get the O(n) version, which is essentially free next to any noise evaluation.

### 4.2 Flow routing: D8 vs D∞ vs MFD

- **D8** (O'Callaghan & Mark 1984): each cell drains to its steepest of 8 neighbors. **Parent anchor confirmed:** flow paths "are unrealistically restricted to multiples of 45°" — the diagonal-bias artifact, producing straight sawtooth channels along cardinal/diagonal directions [28][35][36].
- **D∞** (Tarboton 1997): steepest *triangular facet*; flow partitions between the two adjacent cells — removes the 45° quantization but has its own newly-requantified bias: on analytic test surfaces, D∞ concentrates ~25% too much area along cardinal and ordinal directions [35][37].
- **MFD** (Freeman 1991, Quinn 1991): partition flow to *all* downhill neighbors by slope. Best match to analytic solutions on cones and planes (errors ~10× lower than D∞ on a test cone), but disperses flow on convergent terrain where a single channel is physically right, and its slope-exponent parameter $p$ reintroduces grid-orientation dependence at high $p$ [35][38].
- **Path-based D8-LAD/LTD** (Orlandini): keep single-direction routing but carry cumulative deviation from the theoretical aspect — nondispersive *and* non-biased over long paths [39].

Practical game guidance: **D8 + Priority-Flood-ε for the SPIM stack** (Braun–Willett assumes single receivers anyway [29]); **D∞ or MFD only for pretty flow-accumulation masks** (moisture maps for §6/§8), never for carving. Landlab's comparison tutorial is the best visual catalogue of the artifacts [40]. Also mind grid-resolution dependence: MFD/D∞ contributing areas on hillslopes change by 1.2–2× under 2× refinement — hillslope area is a grid artifact, channel area is not [41].

### 4.3 Flow accumulation and stream network extraction

Accumulation is one more stack sweep (§3.3). Where to put rivers — **parent anchor confirmed**: the constant-drop law (Broscoe 1959; Tarboton et al. 1991) — mean elevation drop along Strahler streams is approximately *constant across orders*; therefore the right channel-initiation area threshold is the *smallest* threshold at which the constant-drop property (and slope–area power-law scaling) still holds, i.e., the highest-resolution network that still behaves like a channel network [42][43][44]. TauDEM automates this as drop analysis: sweep thresholds, t-test first-order mean drop vs higher-order mean drop, pick the smallest threshold with $|t| < 2$ [43]. Typical drainage densities that fall out: **2–5 km/km² generally, 2–12 km/km² across semi-arid→humid** (climate- and lithology-controlled; the arid/semi-arid peak and vegetation-suppressed humid trough are both real, per Abrahams' synthesis and Tucker–Bras' theory) [45][46][47][48]; LiDAR-resolved first-order networks push 6–41 km/km² [48]. A generator's acceptance band (§9) should sit in the 2–12 range at 30 m-equivalent resolution.

### 4.4 Enforcing drainage coherence when combining noise layers

The failure mode: warp your fBm (§2.1), multiply in a ridge mask (§2.2), stamp a mesa (§2.3) — and the drainage field that your river pass computed on the *previous* mix no longer matches the final heights. The discipline that fixes it, in order:

1. Build the *final* macro heightfield from all coherent layers (continents, folds, belts) **before** any hydrology.
2. Priority-Flood (fill or ε-fill) → D8 → stack.
3. Carve/erode on that graph (SPIM sweep, §3.3) — erosion *reorganizes* drainage as it goes, which is the physics [29].
4. Add fine noise **only as detail on slopes' flanks**, amplitude-bounded so it cannot create new internal basins (the engine already has the exact discipline needed: the analytic slope-bound regression test in `terrain-fixes-log.md` §Group R is this constraint, stated for fBm).
5. Rivers, lakes, and any water table are *outputs* of the graph (positions = cells exceeding the channel threshold; lakes = filled depressions with real volumes from the fill deltas), never painted inputs.

MishMash's confessed pipeline is the cautionary case: "generally a height field, however with several stamping and carving passes … a few extra carving passes to erode the heightmap for canyons and rivers" — carving passes *after* the fact, with no flow graph, is the fake-terrain shortcut; his water sim then fights "cave cracks draining the pond" — the same incoherence surfacing downstream (`research/micro-voxel-creators-research.md` §2.2, §2.4).

---

## 5. Shipped systems and tools

### 5.1 World Machine & Gaea (heightfield-first, node-graph)

**World Machine:** a world file "doesn't define a terrain, but the steps to create a terrain" — devices wired in a flowchart, continuous previews, build to high resolution on export; the same graph with a new seed gives a sibling terrain [49]. Basic flow: primitive/noise devices → erosion devices (hydraulic + thermal, the flagship) → coastal/masking/export. **Gaea:** same shape — one node per heightfield operation, graphs strictly left-to-right, Portals/Chokepoints for organization [50]. Gaea's Erosion node is the reference commercial hydraulic-erosion implementation: Feature Scale in meters, Real Scale driven by the terrain definition, selective processing by Rock Softness / Erosion Strength / Precipitation masks (slope/altitude bias or custom), and — the two properties a game should copy — **resolution-parity** (a 512² preview preserves major erosion features of a 4K/8K build) and an explicit **Deterministic** toggle (parallel erosion is otherwise nondeterministic in the small; single-core for reproducibility) [23][51]. Data outputs — Wear, Deposits, Flow — are the mask set every downstream system consumes [23]. Gaea's docs also carry the sharpest practitioner warnings in the field: flow-line textures make terrains "extremely conspicuous"; real terrains "rarely have clean flow lines" [51].

*Architecture implication:* heightfield-first with erosion as the central transform, everything else as masks — the proven pipeline for *artists*, and the graph structure a voxel engine's generation passes should mirror internally.

### 5.2 Unreal Landscape (heightmap-consumer)

Landscape is a GPU heightfield with non-destructive Edit Layers and splines [52], imported from external tools (World Machine explicitly called out); world composition streams level tiles, with a **Tiled Landscape Import** consuming World Machine's tiled heightmap/weightmap export and requiring adjacent tiles to share border vertices [53][54]. Section size 63×63 quads recommended; component/section LOD structure and origin shifting for large worlds are in the docs [53][55]. *Implication:* Unreal does not generate — it consumes; the generation architecture lives upstream, which is exactly the seam where a game with its own procedural pipeline slots in.

### 5.3 Houdini terrain (heightfields as 2D volumes)

Heightfields are 2D volume primitives (`height` + `mask` layers, default 1000×1000 m at 2 m spacing = 500×500 samples) — "it is not possible to work on a terrain's vertical areas" without converting to polygons; masks as second inputs on nearly every node; erosion via `HeightField Erode` (rewritten in Houdini 21) producing `sediment`, `debris`, `flow`, `flowdir` layers, with hydro/thermal sub-node control and the documented stacking workflow: **Massing → Seeding → Lobing → Remapping (elevation passes) → Upsampling → Shaping (Terrace/Clip) → Re-seeding → Erosion**, iterating erode→distort→erode chains [21][56][57][58]. Seeding — "the less smooth the surfaces, the more realistic erosion will be later … obstacles that water and soil must move around" — is the practitioner's version of our §1.3 scale-boundedness point. *Implication:* DCC-grade iteration on the same heightfield-first substrate; the LOD of truth is the 2D grid, 3D only by conversion.

### 5.4 No Man's Sky (density-first on a cube-sphere)

From the two GDC talks (Sean Murray 2017, "Building Worlds Using Math(s)"; Innes McKendrick 2017, "Continuous World Generation in No Man's Sky") and Polygon's report [15][59][60][61]:

- **Space:** planet surfaces stored on a **cube, projected to the sphere**; work happens in a limited shell ~128 m thick around the (noise-varied) sphere radius. The trick that buys mountains and oceans: the sphere's base radius varies with noise by ~600 m–1 km, so the 128 m voxel shell only holds *local* relief [60][61].
- **Voxels:** regions of 32³ voxels at 1 m (nearest LOD), polygonized over a 36³ overlap to prevent seams; ~6 bytes/voxel (2 bytes density × 2 materials + blend data); 6 LOD levels by repeated subdivision (densities reduced, especially in Y); an octree per solar system plus ultra-cheap 2-voxel-high "voxel spheres" for distant planets [60].
- **Generation:** "we generate a whole bunch of noise which is like the terrain and a bunch of other stuff — clouds and populations … that all goes into some voxels, then we polygonize it" (Murray, deliberately simplified, crediting McKendrick's talk for the real pipeline) [15][61]. Positive space (overhangs, "winding, worm-like stone structures," floating islands) and negative space (caverns, crevasses) come from injected noise algorithms folded into the cubic data [61]. Domain warping is used in their custom "uber noise" per the community reconstruction [12].
- **The diversity claim — parent anchor partially unverified:** Murray attributes player hours to "the diversity of the worlds," and the game shipped ~300 MB of generator-side content for everything seen on screen [15]. But the specific phrase "weird terrain libraries" as a named internal system could **not** be verified from the talks or press; what is verified is layered custom noise + domain warp + per-planet parameter variation + hand-injected structure algorithms. Treat "weird terrain libraries" as an unverified paraphrase and drop it from the merged doc.

*Implication:* density-first with heightfield-scale noise driving the base surface — the shipped proof that a voxel engine's macro shape can stay 2D-ish (cheap, LOD-able) while 3D noise supplies only the shell.

### 5.5 Minecraft (density-first, data-driven)

The 1.18+ architecture, from the wiki's noise-router documentation and the custom-worldgen tutorial [62][63][64]:

- **Noise settings** carry a **noise router**: a collection of **density functions** — composable JSON operators (`add`, `mul`, `clamp`, `range_choice`, `y_clamped_gradient`, `noise`, …) evaluated per block position — with named channels: `final_density` (where solid), aquifer channels (`barrier`, `fluid_level_floodedness`, `fluid_level_spread`, `lava`), ore-vein channels, and — separately — biome channels `temperature`, `vegetation` (humidity), `continents`, `erosion`, `depth`, `ridges` (weirdness) that "do not affect terrain shape" [62].
- **Terrain shape** = `sloped_cheese` (base 3D density from `depth` × `factor` — roughly $h(x,z)-y$ plus 3D noise) with a `range_choice` split: above the 1.5625 threshold, the surface regime; below, caves. A `jaggedness` noise adds sharp peaks in high mountains [63].
- **Caves — parent anchor confirmed and extended:** Minecraft carves with **three** noise-cave types, not two — **cheese caves** (3D `cave_cheese` noise blobs: "the black part of the noise image becomes stone, white becomes air … resembling cheese with many holes" — large open pockets), **spaghetti caves** (2D-ish noise pair whose *intersection* is air — long tunnels), and **noodle caves** (thinner, squigglier, 1–5 block wide variants), plus `cave_entrances` noise connecting surface to underground, noise pillars, and aquifers governing cave water/lava with per-aquifer fluid levels [62][63][64]. Pre-1.18 "carver caves" (worm-like feature carvers) still exist as a separate feature pass [64].
- **Biomes:** a multi-noise parameter list — each biome is a point in (temperature, humidity, continentalness, erosion, weirdness, depth) space; nearest-neighbor wins. Terrain and biome *share* some noise inputs (the same continents/erosion/ridges fields feed both), creating the implicit link between shape and surface without biome-determines-terrain coupling [62][64].
- **Surface rules:** a separate declarative pass decides surface blocks (grass/sand/etc. by biome + slope + depth + water) after density [62][64].

*Implication:* the most-shipped density-first architecture on Earth, and entirely data-driven — worth copying structurally (declarative density graph; biome as climate-space nearest-neighbor; surface rules as a separate pass), whatever one thinks of blocky output.

### 5.6 Dwarf Fortress (simulation-first)

From Tarn Adams' own descriptions (Gamasutra 2008 interview; GameAIPro ch. 41; PRACTICE 2016) [65][66][67]:

1. **Elevation** by midpoint displacement (its axis-alignment artifacts are explicitly why the erosion phase exists next).
2. **Climate fields:** temperature (biased by elevation and latitude), **rainfall later biased with orographic precipitation / rain shadows**, drainage as another fractal, plus salinity, vegetation, and fantasy fields (savagery, good/evil).
3. **Biomes as derived lookup, never laid down directly:** "rainfall ≥ 66/100 and drainage < 50 → swamp" — "the nice thing about having the fractally-generated basic fields is that the biome boundaries all look natural" [65].
4. **Erosion phase:** temporary river paths run out from mountain bases, "digging away at a square if it can't find a lower one"; then real rivers, *forced* to the ocean if they fail; lakes bulged; loop-erasure; flow amounts and tributary structure computed; rivers named [65]. A hand-rolled priority-flood-plus-carve — §4's algorithm class, invented independently in 2006-era hobby code.
5. Then vegetation/animal populations, civilizations, ~500 years of history [65][67]. Adams' design principles: simulate basic fields and let biomes *arise*; "base your model on real-world analogs … the world maps improved greatly when rain shadows were taken into consideration … drainage was another nonobvious consideration" [66].

*Implication:* simulation-first is the only shipped architecture whose *rainfall is a function of its own mountains* — the payoff this document's §6 argues for — and it runs at world-map resolution (coarse grids, seconds-to-minutes), not voxel resolution. The scale separation is the lesson.

### 5.7 Architecture summary

| System | Architecture | Macro shape | Hydrology | Biomes |
|---|---|---|---|---|
| World Machine/Gaea | heightfield-first, node graph | noise+primitives | hydraulic erosion node (nondeterministic w/o toggle) | masks |
| Unreal Landscape | heightmap consumer | imported | none (upstream) | painted layers |
| Houdini | heightfield-as-2D-volume DCC | noise+projection | erode stack (hydro+thermal) | masks/scatter |
| No Man's Sky | density-first (cube-sphere) | noise-varied sphere + 128 m shell | none structural | per-planet parameters |
| Minecraft | density-first, data-driven density graph | sloped_cheese 3D shell | none (cave aquifers only) | multi-noise climate-space NN |
| Dwarf Fortress | simulation-first (world-map scale) | midpoint displacement | forced-river carve + rain shadow | derived from climate fields |

For a voxel engine: **heightfield-first for the hydrological macro layer, density-first for the voxel shell, simulation-first (Dwarf-Fortress-style, coarse) for climate** — each architecture where it is strong.

---

## 6. Biome maps

### 6.1 Whittaker placement

The Whittaker diagram classifies ~9 terrestrial biomes on axes of mean annual precipitation vs mean annual temperature (tropical rainforest top-left/wet-hot, tundra cold-dry, subtropical desert hot-dry) [68][69][70]. It is the default game biome lookup because it is 2D, monotone-ish, and cheap: sample T and P, find the cell. What it gets right: biomes really are first-order a climate function (Köppen's zones were *defined* by vegetation correspondence [69]). What it misses: fire, herbivory, soil, and history — the tropical forest–savanna system is the canonical counterexample (below).

### 6.2 Climate fields from terrain

- **Temperature:** latitude gradient + elevation lapse. The standard environmental lapse rate is ~6.5 °C/km (standard-atmosphere value; flagged here as a textbook constant I did not re-verify against a fetched source this session — the parent's "~6.5 °C/km environmental" anchor matches the standard figure; treat as confirmed-by-consensus, not by citation).
- **Continentality:** temperature range and moisture decline with distance from ocean along prevailing wind — a simple distance-to-coast field is the standard cheap proxy.
- **Orographic precipitation — parent anchor confirmed:** the *simple linear upslope model* is the classical baseline: $S(x,y) = C_w\, \mathbf{U}\cdot\nabla h + S_\infty$ — condensation proportional to wind-speed-times-terrain-slope, background rate added, precipitation assumed to fall where it condenses [71]. Smith & Barstad's 2004 Linear Theory upgrades it with airborne dynamics, cloud conversion/fallout time delays, and downslope evaporation — all via one FFT: transform terrain, multiply by a wavenumber transfer function, inverse transform, apply the positive-part cutoff; ~8 s for a 1024² 1 km grid on a 2004 workstation, ~1 s at 256² [71]. Open implementations exist (fastscape-lem's Python LT model; a QGIS plugin) [72][73], and Roe & Baker's companion model gives the analytic intuition (drift distances 5–25 km, reverse rain shadows possible) [74]. A game needs exactly the four-step FFT version — rain shadows fall out for free, correctly displaced downwind, which no hand-painted shadow mask is.

### 6.3 Altitude belts

Standard sequence up a tropical mountain (Holdridge-style life zones): lowland rainforest → premontane → lower montane → montane (cloud) forest → subalpine/páramo → alpine → snow/nival, driven by the lapse rate plus the moisture profile (cloud condensation belt typically makes the lower-montane/montane transition the wettest zone). (Qualitative consensus of the biome literature above; the Holdridge quantization itself was not fetched this session — Part 6 should own the per-belt numbers.) Dwarf Fortress and Minecraft both use elevation as a biome axis via `depth`/elevation channels [62][65]; the trap is forgetting that temperature-elevation and *moisture*-elevation are different curves (wet middle, not monotone).

### 6.4 Ecotones: sharper than games make them

**Parent anchor confirmed, with numbers.** The forest–savanna boundary — the most widespread tropical ecotone — is "frequently quite abrupt … only a narrow ecotone averaging 10 m in width separating the two states," with bimodal tree-cover distributions; fire–grass and shade–fire-suppression feedbacks maintain the two as alternative stable states [75][76][77][78]. Threshold behavior is field-verified: functional traits, soils, and fire regimes shift *coincidentally* at a breakpoint along the closure gradient (Cerrado; 98 plots), with the fire-suppression threshold around ~45–50% tree cover [76][77]. The Maxwell-point/coexistence literature refines this: boundaries sit where the two states are equally stable, coexistence patches concentrate near those rainfall bands (~1,580–1,760 mm MAP for Africa/South America), and topographic roughness permits local coexistence in dry climates [78][79]. Temperate forest edges against agriculture are likewise sharp — abrupt, maintained edges vs gradual succession edges are a *management* distinction, with gradual transition zones only ~5 m wide (shrubs strips 1.1–7.4 m) [80][81].

**Design rule for games:** biome blending widths should be *landcover-class-dependent*: climate-graded transitions (boreal→temperate forest) can be kilometers; disturbance-maintained boundaries (forest↔savanna, forest↔agriculture, treeline-adjacent krummholz) should be a few to a few tens of meters — i.e., most game engines' default "blend everything over 100 m" is wrong in the direction of too soft, exactly as the parent suspected. A cheap implementation: use a nonlinear (sigmoid, even hysteretic) response of landcover to the climate index rather than a linear blend, then add small-scale patch noise *inside* each state rather than across the boundary.

---

## 7. Voxel-specific terrain

### 7.1 3D density fields

The standard Minecraft-style surface: $d(x,y,z) = h(x,z) - y + N_3(x,y,z)$ — a sloped shell (their `sloped_cheese` [63]) plus 3D noise for overhangs and floating islands; `final_density > 0` ⇒ solid [62]. NMS is the same idea with the shell wrapped on a cube-sphere and the elevation variation folded into the radius [60][61].

**Its documented limitation — parent anchor confirmed:** the structure *below* the surface is whatever the 3D noise term does; there is no true 3D geology — no coherent stratigraphy, no structural control, no per-layer erodibility (which §2.3 says is what mesas actually are). Houdini states the heightfield version ("not possible to work on a terrain's vertical areas" [56]); the density-field version is subtler: any 3D shape is expressible, but the macro surface still dominates and the interior is statistically homogeneous noise. Minecraft's answer is to make the interior *intentional* (cave noise + aquifers + surface rules), not geological.

### 7.2 Cave carving: cheese + spaghetti + noodle (verified)

Minecraft 1.18's actual scheme (§5.5): large blob **cheese** caves from a clamped 3D noise (`cave_cheese`, y-anisotropic scale ~0.67 vs 1.0, suppressed near the surface by a `sloped_cheese`-dependent term); tunnel **spaghetti** caves from the intersection of two ridged-style 2D noises; **noodle** caves as thin 1–5-block squiggles; cave-entrance noise linking surface to deep; noise pillars; aquifers as per-region fluid levels with flood/spread/barrier channels deciding water vs air vs lava (lava threshold 0.3) [62][63][64]. Frequency/hollowness/thickness parameters per type give "extremely diverse" caves [64].

**Cellular-automata smoothing** is the complementary pass where it matters: Minecraft carvers historically applied post-carve smoothing, and CA is standard in falling-sand/tunnel generators (Dwarf Fortress's fluid engine is "a specialized cellular automata … water falls down if it can, over if it can" [65]; Rijsdijk's voxel playground runs CA water, per the micro-voxel doc §3.7). CA passes buy wall-eroded, pocket-rounded tunnels from raw noise intersections at O(n) per iteration — cheap at chunk scale.

### 7.3 Arches and overhangs

Overhangs come from the $N_3$ term's amplitude and y-scale (Minecraft's `y_scale: 1` vs `0` toggles overhangs on/off in the wiki's own minimal example [62]). **Arches** specifically want *ridged* 3D noise: the $1-|N_3|$ crease surfaces (§2.2) become thin curved sheets in 3D; intersect a ridged shell with the terrain shell and the positive region is exactly arch/fin geometry. NMS's "winding, worm-like stone structures" and floating islands are the shipped examples of the same trick [61]. Failure modes mirror the 2D ones: symmetric sheets, wrong junction geometry — and at voxel resolution, thin sheets alias into staircases unless the density function is Lipschitz-bounded per voxel step (the engine's `heightmap_profile.csv` discipline, generalized to 3D).

### 7.4 Cliff stratigraphy

Layered materials along Y — Minecraft's surface rules + deepslate banding; Houdini's Terrace SOP + layer stacks [21][62]. To make it *geological* rather than cosmetic: drive layer boundaries by a gently warped 2D field (a low-frequency plane + warp = folded strata, §2.1), assign per-layer hardness, and let the erosion stencil (§3, §10) erode soft layers faster — that is the actual mesa/canyon mechanism [17][20], and it is the difference between painted stripes and stair-steps that survive an erosion pass.

### 7.5 The heightfield–voxel hybrid — argued for, emphatically

Generate a hydrologically-correct heightfield (§4), then voxelize with 3D detail only where needed. **For:** (a) rivers, drainage, and biome fields all need 2D graphs anyway — no one has shipped coherent river networks from pure 3D density fields; (b) NMS — the most voxel-native shipped game — does exactly this (noise-varied sphere radius + a thin 3D shell) [60]; Minecraft's `sloped_cheese` is the flat-world version [63]; MishMash's micro-voxel engine is "generally a height field, however with several stamping and carving passes to create 3d detail" (micro-voxel doc §2.2); (c) collision/vegetation/LOD all want an authoritative analytic surface (the engine's own `height_at` pattern — micro-voxel doc §4.2 argues the same). **Against:** 3D detail near the surface can contradict the hydrology (a cave breaching a riverbed drains the river; MishMash's documented pond-draining bug). The discipline: treat the 3D shell as *subordinate* — carve caves with a density budget that goes to zero below the water table, and make the water table a property of the flow graph, not local geometry. Verdict: hybrid, with the heightfield as the constitution and 3D noise as statute.

---

## 8. Vegetation placement

### 8.1 Density maps and the arithmetic that kills naive plans

Density = f(slope, moisture, biome) is the standard map stack (Gaea's flow/moisture data maps; Houdini scatter-by-mask; every shipped engine's variant). The units must come from Part 6. Real temperate old-growth canopy densities: **398 stems/ha (deciduous), 500 (mixed), 556 (coniferous)**; all-ages values 496–721 stems/ha [82]; global moist-temperate reviews concur [83]; dry Isoberlinia woodlands run 255–443 stems/ha [84].

**Parent anchor disagreement (arithmetic, 25×):** "a temperate forest at 400 stems/ha = 1 tree per 25×25 m cell" is wrong. 1 ha = 10,000 m², so 400 stems/ha = one stem per **25 m² = a 5×5 m cell**. A 25×25 m cell would be 16 stems/ha — a savanna, not a forest. The corrected walk-through, in this engine's units:

- 400 stems/ha ⇒ 5 m mean spacing ⇒ within any 32×32 m voxel region (NMS-scale), ~**41 trees**; per 64×64 m Minecraft-style superchunk, ~**164**; per the engine's default SVO root region (512×512 m ≈ 26 ha), **~10,500 trees** at full temperate density.
- At 7.8 mm voxels a 15 m canopy tree is ~1,900 voxels tall — full voxelization of even the ~41 trees in one 32 m region is a *large* brick budget (and Laine & Karras's warning that vegetation is the pathological SVO content class applies; grass-rendering doc §1.6). This is the quantitative case for the two-tier rule the micro-voxel doc already extracted from MishMash: **deterministic-ID voxel/prop trees near, card/instanced trees far**, with grass as clustered coarse-resolution scatter (micro-voxel doc §2.3, §5.6). It is also why John Lin ray-traces placement queries (sunlight, cave walls, openness) rather than scattering blindly (micro-voxel doc §1.4) — at real densities you cannot afford to place first and cull later.

### 8.2 Point distributions: Poisson-disk vs blue noise vs jittered grid

The verified artifact catalogue (Red Blob Games' systematic comparison [85]; Muratori's Nebraska Problem [86]; the comparison literature [87]):

- **Jittered grid:** trivially cheap and tileable, but *no jitter value works*: isotropy of angles needs jitter ≥ 0.9 cell; avoiding close-pairs and gaps needs ≤ 0.6 — the two requirements don't intersect [85]. Visible grid diagonals at the wrong camera angle.
- **Poisson disk** (Bridson-style): min-distance guaranteed, good angle distribution; artifacts: long-distance gaps, and Muratori's "Nebraska problem" — near-colinear point alignments that read as cornfield rows from certain viewpoints; his fix (staggered concentric intersection packing, fully deterministic) beat blue noise on visual coverage *with fewer points* [86].
- **Precomputed blue noise textures:** the density-varying champion — threshold a blue-noise bitmap to get spatially-varying density with stable growth ordering [85].
- **What real forests actually are (Part 6 §61–67):** *clustered* (Thomas/Neyman–Scott processes fit field data), not min-distance-regular. Poisson disk is *too regular* for a natural stand. The user-study evidence is refreshingly pragmatic: a plant-competition model looked most believable from the air, but *random uniform* placement rated highest for playability and photorealism from first person [88]; a 2025 Unity comparison found Poisson-disk the best efficiency/fidelity balance among noise/Poisson/FON [89]. Blunt synthesis: use jittered-hex or Poisson-with-slack for the *visible* near ring (nobody can tell at 5 m spacing), add cluster structure (patches, gaps, nurse-plant clumps) via a second-scale noise on top of the base density, and spend the determinism budget on stable per-position hashing so LOD transitions don't reshuffle trees (the grass-rendering doc's "probabilistic distance falloff with stable per-blade position hash" is the same fix at grass scale).

### 8.3 Patchiness and edge rule set

- **Inside-stand:** gap dynamics and microsite preference make real stands patchy at 5–50 m scales (Part 6 §63, §67). One extra octave of density noise (multiplied, not added) captures it.
- **Forest edges:** sharp where cut (agriculture/management: abrupt edges, ~5 m transition, overhanging canopy [80][81]), diffuse where climate grades (succession shrub belts, §6.4's ecotone widths). Rule: edge sharpness = f(cause). Human-caused boundaries sharp; climate boundaries graded *except* where disturbance feedbacks (fire) sharpen them — those are the 10 m forest/savanna walls [75][79].
- **Ecotone-placement rule set** (compiled): (1) climate index → biome via Whittaker; (2) landcover response to the index is sigmoid/hysteretic near feedback-maintained boundaries (savanna/forest, treeline), linear-blended along pure climate gradients; (3) patch noise inside states, not across boundaries; (4) topographic roughness permits climate-contrary patches (valley forest in dry country — the Central-Africa coexistence mechanism [79]); (5) treeline follows the temperature belt with a wind/exposure modulator (Part 6 §65).

---

## 9. Validation: acceptance tests against real-Earth statistics

A generator is a statistical hypothesis about terrain; test it like one. Each test is one histogram or one curve, computed on a 512²-or-larger patch of final (post-pipeline) heights, with the measured reference band and source.

1. **Slope-distribution histogram.** Real distributions are unimodal; skewness runs positive at low mean slope → negative at high mean slope across >10,000 sampled US landscapes (Wolinsky & Pratson) [90]; uplift-zone terrain ~normal, depositional ~exponential (Montgomery, Olympics/Coast Range) [22]; tails decay as a power law whose exponent steepens with landscape age (Andes $q \approx -6.0$ → Appalachians $q \approx -10.2$; oldest landscapes approach Rayleigh = isotropic Gaussian gradients) [91]. *Test:* assert unimodality; assert the skew-vs-mean-slope trend across your biome set; flag any fat tail not attributable to cliffs you stamped on purpose.
2. **Power spectrum.** 1D angle-integrated slope $\beta \in [1.6, 2.5]$, target ≈ 2 [1][3][4]; assert no spurious peaks (periodicity) and roll-off consistent with your octave cutoff.
3. **Hypsometric curve.** Bimodal for a full world map (land peak within a few hundred m of sea level; abyssal peak ~−4 km; land fraction ~29%) [10][11]; for a single catchment, compare against the classic basin hypsometric integral (Part 2 §30). *Test:* land-area-vs-elevation distribution within tolerance of Earth's shape, not Gaussian.
4. **Drainage density.** Extract the network by constant-drop analysis (§4.3); assert 2–12 km/km² at 30 m-equivalent resolution [45][46][47][48]. This one test catches both "no rivers" (≈0) and "noise gullies everywhere" (≫12).
5. **Valley cross-sections.** Fit $y = a x^{b}$ to ridge-to-ridge transects along extracted channels: fluvial terrain ⇒ $b \approx 1$ (V), glacial ⇒ $b \to 1.5\text{–}2$ (parabolic U; Graf's Beartooth values 1.5–2.0; Svensson's Lapporten ≈ 2.0–2.2) [92][93][94]; or use the V-index ($A_x/A_v - 1$: 0 = perfect V, >0 = U), validated on 27,331 Sierra Nevada sections [95]. Montgomery's Olympics data adds a *magnitude* check: glaciated valleys >50 km² reach 2–4× the cross-sectional area of fluvial neighbors [94]. *Test:* per-process b-value bands.
6. **Constant-drop property.** The same t-test TauDEM uses on real DEMs should pass on generated ones: mean first-order Strahler drop vs higher-order drop, $|t| < 2$ [43][44]. Cheap, and it ties network extraction to physics.
7. **Slope–area scaling.** Log-log channel slope vs drainage area: exponent in $[-0.6, -0.35]$ [28]; the hillslope-side plateau (slope independent of area below the channelization length) must exist — its absence means your diffusion term is off or missing [28][96].
8. **Fractal dimension / variogram.** Variogram log-log linearity over your intended scale band, with local $H$ varying by landform (0.46–0.77 by setting [3]); pyTopoComplexity packages wavelet/fractal-dimension estimators if a reference implementation is wanted [97].
9. **Hydrological coherence (structural, cheap):** zero internal-basin count after generation *by construction* (priority-flood); every river polyline ends at sea level or a lake; every lake has a spill path; aquifer/cave densities don't breach the water table (§7.5's discipline).
10. **Biome/vegetation sanity:** stems/ha per biome inside Part 6's measured bands (§8.1's 400–700 for temperate [82][83]); boundary sharpness distribution per §6.4 (10 m-class for disturbance edges).

Run as a nightly golden-seed suite: fixed seeds, dumped statistics, threshold assertions — the same discipline as the engine's existing `--verify-frame` and slope-bound regression tests, applied to geomorphology. Ten histograms, each with a measured Earth band, is an acceptance suite no shipped game currently publishes and any engine could.

---

## 10. The honest ceiling and the recommended pipeline

### 10.1 Practical vs fake

**Practical (do it for real, at generation time):**

- **Hydrological coherence.** Priority-Flood (O(n) integer, 20 lines [33]) + D8 stack + flow accumulation + SPIM carving (O(n) implicit [29]) on a regional heightfield, once, offline of the frame loop. This is the single highest-value real physics a generator can buy. Dwarf Fortress does a hand-rolled version at world scale in seconds [65]; FastScape does the rigorous version at $10^8$ nodes on a laptop [29][30].
- **Orographic climate.** Smith–Barstad LT = one FFT pair [71]; rain shadows, spillover, drift — all correct by construction.
- **Statistical fidelity.** Match $\beta$, slope histograms, drainage density, valley b-values via the §9 suite. Free once the suite exists.

**Fake convincingly (stencil, don't simulate):**

- **Tectonic history.** Full LEM-at-planet-scale is out: FastScape's own design goal is ensemble simulation on research hardware [31], and Musgrave's parameter-space complaint stands — a somewhat realistic model has "on the order of 10 parameters," and searching a 100-dimensional space per world is not a game-engine activity [9]. Instead: stamp linear orogenic belts (Part 1's range geometry) as macro heightfield features — folded-noise ridges along plate-boundary curves [14] — with *fake roots* (an isostatic-looking low-pass flexure under the belt) and let the SPIM pass carve real drainage through the fake mountains. The rivers will make the stamps credible; nothing else will.
- **Glacial carving.** A stencil pass: select basins above the (climate-field) snowline, trace flow lines downvalley, carve parabolic cross-sections ($b = 1.5\text{–}2$) with width scaled to upstream ice-flux proxy (drainage area), overdeepen below base level at confluences, truncate tributaries into hanging valleys — the U-valley statistics of §9.5 are the acceptance test, and a stencil can pass them; a full glacial LEM is not needed to pass a *cross-section* test. (FastScape's implicit O(n) glacial module exists if a real solver is ever wanted [31].)
- **Karst, coastal, periglacial:** Part 3/Part 4's landform statistics drive stencil passes (doline fields from clustered noise with drainage-sink enforcement; cliff-retreat profiles; patterned-ground textures). The validation suite keeps the stencils honest.
- **Vegetation dynamics:** place at Part-6 densities with two-tier LOD (§8.1); do not simulate succession except as a one-shot age-field that biases species mix (Part 6 §63).

### 10.2 The pipeline, bluntly

For this engine — analytic fBm heightfield today, ray-marched 7.8 mm SVO, static deterministic world, generation-time budget (world-ready ~0.6 s on the svo path, so the macro pipeline can afford hundreds of ms regionally):

1. **Base continents:** low-frequency fBm + plate-velocity domain warp (§2.1 [13][14]); enforce Earth-like bimodal hypsometry by construction (separate ocean/land crust fields, sea level cut at ~29% land [10][11]) — replaces today's single 4-octave field as the *skeleton* only.
2. **Orographic climate:** FFT Smith–Barstad on the continental heights → P(x,z), T(x,z) from latitude + 6.5 °C/km lapse; continentality distance fields [71].
3. **Hydrological heightfield with SPIM carving:** Priority-Flood → D8 → stack → implicit SPIM to steady-ish state with the orographic P as the rain field (all O(n) [29][33]); hillslope diffusion term for the slope plateau; channel network by constant-drop threshold [42][43].
4. **Stencil passes:** glacial U-valley carving above snowline; coastal cliff/shore platforms; karst sinks in carbonate-lithology mask; dune fields from Part 5's wind fields where the climate says desert.
5. **Voxelization with 3D cave density:** heightfield-first hybrid (§7.5, argued for) — $d = h - y + N_3$ with cheese/spaghetti/noodle-style cave channels [62][63], cave density suppressed near the water table, cliff stratigraphy from a warped layer field with per-layer hardness feeding the erosion look.
6. **Biome map:** Whittaker on (T,P) + altitude belts + ecotone sharpness rules (§6.4); landcover response sigmoid at feedback boundaries.
7. **Vegetation at Part-6 densities:** density = slope × moisture × biome; jittered/Poisson-with-slack base + cluster noise; deterministic per-position IDs cascading across LOD bands (MishMash two-tier, micro-voxel doc §5.6); near-ring voxel trees, far-ring instances/cards.

What this pipeline deliberately does *not* include: real-time erosion, per-frame hydrology, tectonic simulation, glacial LEMs, vegetation succession. Every one of those is either generation-time-only or stencil-faked above, and the §9 acceptance suite is the referee that says whether the fakes are good enough. The ceiling is real: hydrology and climate and statistics — yes, cheaply; history — no, but a convincing costume of it is a solved costume.

---

## Provenance

- **Mandatory reads** (`_terrain_question_bank.md` §G + footer; `micro-voxel-creators-research.md`; `grass-rendering-research.md`; `water-physics-and-wave-simulation.md` house style; `terrain-fixes-log.md` lines 30–50) were read in full or to the specified extent before searching. Voxel-creator findings are *cited from the local doc*, not re-researched, as instructed.
- **Search budget:** 7 batches / 27 queries (cap ~14/45) — completed under budget; the last third of context reserved for the write, per instruction.
- **Anchor-disagreement register (5):**
  1. **Tree-density cell size — parent wrong by 25×:** 400 stems/ha = 1 tree per 25 m² (5×5 m cell), not 1 per 25×25 m cell (that would be 16 stems/ha). Sources: Keddy 398–556 stems/ha old-growth [82]; Burrascano global review [83].
  2. **Hypsometric mismatch direction:** parent guessed "too little land at mid elevations"; the actual fBm failure is a *unimodal Gaussian* area-elevation curve vs Earth's *bimodal* (continental + abyssal) distribution with ~29% land [10][11].
  3. **"$k^{-2}$" needs qualification:** true for 1D angle-integrated second-order stats (Turcotte [1]); the 2D angle-averaged exponent is $\beta+1$; and monofractal fBm is formally rejected against multifractal FIF in higher moments (Lovejoy/Schertzer [3][5]).
  4. **NMS "weird terrain libraries" — unverified:** could not confirm any named internal library from the GDC talks or press; verified instead: layered noise + domain warp in "uber noise" + hand-injected positive/negative-space structure algorithms [12][15][60][61]. Recommend dropping the phrase from the merged doc.
  5. **Lapse rate ~6.5 °C/km — flagged, not URL-verified this session** (matches the standard-atmosphere constant; no fetched source). Also a *nuance* rather than disagreement: D8's diagonal bias is confirmed, but the newest re-evaluation shows D∞ carries its own ~25% cardinal/ordinal bias [35] — the merged doc shouldn't present D∞ as bias-free.
- **Unverified-and-omitted:** none beyond #4/#5 (no fabricated URLs; no Zelda-wind-audio-style unverifiable talks were needed here). The firespark.de hydraulic-erosion paper and ranmantaru blog are cited by URL as community-standard references (author metadata not captured in this session's fetches).

## Sources

1. Turcotte (1987), fractal topography/geoid spectra — https://doi.org/10.1029/jb092ib04p0e597
2. Musgrave (1994), *Methods for Realistic Landscape Imaging* — https://www.kenmusgrave.com/dissertation.pdf
3. Lovejoy et al. (2006), multifractal earth topography — https://npg.copernicus.org/articles/13/541/2006/ (PDF: https://hal.science/hal-00331093/file/npg-13-541-2006.pdf)
4. Turcotte (2007), self-organized complexity in geomorphology — https://pdodds.w3.uvm.edu/files/papers/others/2007/turcotte2007a.pdf
5. Gagnon et al. (2006), multifractal topography EPL — http://www.physics.mcgill.ca/~gang/eprints/eprintLovejoy/topoEPL.JS_Gagnon.pdf
6. Musgrave, Kolb, Mace (1989), eroded fractal terrains — https://doi.org/10.1145/74334.74337
7. Musgrave, "Procedural Fractal Terrains" chapter — https://blenderartists.org/uploads/short-url/z1tZXakC8HSoHjytpwiCvqKejSU.pdf
8. Kaplan & Kuo (1995), extended self-similar terrain — https://doi.org/10.1117/12.205974
9. Musgrave terrain course notes — https://www.classes.cs.uchicago.edu/archive/2015/fall/23700-1/final-project/MusgraveTerrain00.pdf
10. NCEI/NOAA, hypsographic curve from ETOPO1 — https://www.ncei.noaa.gov/sites/default/files/2023-01/Hypsographic%20Curve%20of%20Earth%E2%80%99s%20Surface%20from%20ETOPO1.pdf
11. Pedersen et al. (2024), Earth's hypsometry & sea level — https://pure.au.dk/ws/portalfiles/portal/451367801/1-s2.0-S0012821X2400503X-main.pdf
12. terrain-erosion-3-ways (NMS uber noise + erosion motivation) — https://github.com/r2d2meuleu/terrain-erosion-3-ways
13. Quilez, "Domain warping" — https://iquilezles.org/articles/warp/
14. Michel et al. (2015), folded terrains from vector maps — https://portfolio.exppad.com/documents/2015__Michel__Generation_of_Folded_Terrains_from_Simple_Vector_Maps.pdf
15. Murray (GDC 2017), Building Worlds Using Math(s) — https://www.youtube.com/watch?v=C9RyEiEzMiU ; https://www.gdcvault.com/play/1024514/Building-Worlds-Using
16. Blender OSL Musgrave node — https://github.com/jesterKing/blender/blob/master/blender/intern/cycles/kernel/shaders/node_musgrave_texture.osl
17. Utah Geol. Survey, "What is the Grand Staircase?" — https://ugspub.nr.utah.gov/publications/public_information/pi-64.pdf
18. NPS, Grand Staircase — https://www.nps.gov/brca/learn/nature/grandstaircase.htm
19. Wikipedia, Grand Staircase — https://en.wikipedia.org/wiki/Grand_Staircase
20. Darling/Bierman et al. (2018), GSA abstract, erosion rates Grand Staircase — https://www.uvm.edu/cosmolab/papers/Darling_2018_6461.pdf
21. SideFX, Houdini erosion guide — https://www.sidefx.com/docs/houdini/heightfields/erosion.html
22. Montgomery (2001), slope distributions & threshold hillslopes — https://doi.org/10.2475/ajs.301.4-5.432
23. Gaea docs, Erosion node — https://docs.gaea.app/reference/nodes/simulate/erosion.html
24. SebLague/Hydraulic-Erosion — https://www.github.com/SebLague/Hydraulic-Erosion
25. Lague transcript, hydraulic erosion — https://rosetta.to/u/sebastianlague/coding-adventure-hydraulic-erosion (video: https://www.youtube.com/watch?v=eaXk97ujbPQ)
26. firespark.de, hydraulic erosion method — https://www.firespark.de/resources/downloads/implementation%20of%20a%20methode%20for%20hydraulic%20erosion.pdf
27. ranmantaru, water erosion on heightmaps — http://ranmantaru.com/blog/2011/10/08/water-erosion-on-heightmap-terrain/
28. Grid-resolution dependence of flow routing — https://www.sciencedirect.com/science/article/abs/pii/S0169555X10002606
29. Braun & Willett (2013), O(n) implicit SPL solver — https://doi.org/10.1016/j.geomorph.2012.10.008 (https://www.sciencedirect.com/science/article/abs/pii/S0169555X12004618)
30. FastScapeLib docs — https://fastscape.org/fastscapelib-fortran/
31. GFZ, FastScape project page — https://www.gfz.de/en/section/earth-surface-process-modelling/projects/current-projects/fastscape-landscape-evolution-model-development
32. Bovy et al. (2020), FastScape software stack — https://doi.org/10.5194/egusphere-egu2020-9474
33. Barnes, Lehman, Mulla (2014), Priority-Flood — https://richard.science/sci/2014_depressions.pdf ; https://doi.org/10.1016/j.cageo.2013.04.024 ; https://arxiv.org/pdf/1511.04463
34. Barnes et al., ScienceDirect page — https://www.sciencedirect.com/science/article/abs/pii/S0098300413001337
35. Flow-routing algorithm evaluation, *Earth Surf. Dynam.* 13 (2025) — https://esurf.copernicus.org/articles/13/239/2025/esurf-13-239-2025.pdf
36. Eight flow-accumulation algorithms compared — https://www.sciencedirect.com/science/article/abs/pii/S1364815214002497
37. (same as 28)
38. Landlab, FlowDirectors comparison — https://landlab.csdms.io/tutorials/flow_direction_and_accumulation/compare_FlowDirectors.html
39. Orlandini et al., path-based D8-LAD/LTD — http://idrologia.unimore.it/orlandini/web-archive/papers/2002WR001639.pdf
40. (same as 38)
41. (same as 28)
42. Tarboton, Bras, Rodriguez-Iturbe (1991), channel-network extraction — https://hydrology.usu.edu/dtarb/hp91.pdf ; https://doi.org/10.1002/hyp.3360050107
43. TauDEM, Stream Drop Analysis / Stream Definition — https://hydrology.usu.edu/taudem/taudem5/help53/StreamDropAnalysis.html ; https://hydrology.usu.edu/taudem/taudem5/help53/StreamDefinitionWithDropAnalysis.html
44. Tarboton, terrain analysis in hydrology — https://hydrology.usu.edu/dtarb/ESRI_paper_6_03.pdf
45. NetMap, drainage density — https://www.netmaptools.org/Pages/NetMapHelp/drainage_density.htm
46. Tucker & Bras (1998), hillslope processes & drainage density — https://doi.org/10.1029/98wr01474
47. Collins & Bras (2010), drainage density in drylands — https://doi.org/10.1029/2009wr008615
48. Kim, Yoon, Choi (2023), LiDAR drainage density — https://doi.org/10.3390/app13020700
49. World Machine Help, Ch.1 — https://help.world-machine.com/topic/chapter-1-an-introduction-to-world-machine/
50. Gaea docs, Infinity Graph — https://docs.quadspinner.com/Guide/Graph/Graph.html
51. Gaea docs, Understanding Erosion — https://docs.gaea.app/using/using-gaea/understanding-erosion/index.html
52. UE4.27, Landscape Edit Layers — https://docs.unrealengine.com/4.27/en-US/BuildingWorlds/Landscape/Layers/
53. UE4.27, World Composition — https://docs.unrealengine.com/4.27/en-US/BuildingWorlds/LevelStreaming/WorldBrowser/
54. UE, World Composition — https://dev.epicgames.com/documentation/unreal-engine/world-composition-in-unreal-engine
55. UE4.27, custom heightmaps & layers — https://docs.unrealengine.com/4.27/en-US/BuildingWorlds/Landscape/Custom/
56. SideFX, heightfields & terrains — https://www.sidefx.com/docs/houdini/heightfields/index.html
57. SideFX, realistic terrain workflow — http://www.sidefx.com/docs/houdini/model/terrain_workflow.html
58. SideFX, terrain creation — http://www.sidefx.com/docs/houdini/heightfields/creation.html
59. McKendrick (GDC 2017), Continuous World Generation in NMS — https://www.youtube.com/watch?v=sCRzxEEcO2Y ; https://www.gdcvault.com/play/1024265/Continuous-World-Generation-in-No-Man-s-Sky-
60. Polygon (2017), "In the beginning, No Man's Sky was flat" — https://www.polygon.com/2017/3/2/14790028/no-mans-sky-was-flat-procedural-world-generation-maths/
61. (same as 15)
62. Minecraft Wiki, Noise router — https://minecraft.wiki/w/Noise_router
63. Minecraft Wiki, Tutorial:Custom world generation — https://minecraft.wiki/w/Tutorial:Custom_world_generation
64. Minecraft caves & generation order — https://wiki.sasgaming.net/wiki/Minecraft:Cave ; https://minecraftathome.miraheze.org/wiki/World_Generation
65. Adams, Gamasutra interview (2008) — https://www.gamedeveloper.com/design/interview-the-making-of-dwarf-fortress
66. Adams, GameAIPro ch. 41 — http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter41_Simulation_Principles_from_Dwarf_Fortress.pdf
67. Adams, PRACTICE 2016 — https://www.youtube.com/watch?v=yDPb0jqRr3o
68. Whittaker diagram guide — https://gveg.wyobiodiversity.org/application/files/7916/4641/2117/Whittaker_Diagram_Guide.pdf
69. Macmillan/Gervais, Climate and Life: Biomes — https://digfir-published.macmillanusa.com/gervais1e/gervais1e_ch08_2.html
70. Scientific Data, modified Whittaker diagram — https://www.nature.com/articles/s41597-025-04387-0/figures/2
71. Smith & Barstad (2004), linear theory of orographic precipitation — https://journals.ametsoc.org/view/journals/atsc/61/12/1520-0469_2004_061_1377_altoop_2.0.co_2.xml
72. fastscape-lem orographic-precipitation (Python) — https://github.com/fastscape-lem/orographic-precipitation
73. QGIS LT orographic precipitation plugin — https://plugins.qgis.org/plugins/LinearTheoryOrographicPrecipitation/
74. Roe & Baker (2006), orographic precipitation patterns — https://earthweb.ess.washington.edu/roe/Web/GerardWeb/Publications_files/RoeBaker_PrecipPatt_JAS06.pdf
75. Oliveras & Malhi (2015), forest–savannah transitions — https://royalsocietypublishing.org/doi/10.1098/rstb.2015.0308
76. Dantas, Batalha, Pausas (2013), fire-driven savanna–forest thresholds — https://digital.csic.es/bitstream/10261/94686/1/Dantas-2013-Ecology_savanna-forest-threshold.pdf
77. Bernardino et al. (2022), savanna–forest coexistence across fire gradient — https://www.uv.es/jgpausas/papers/Bernardino-2022-Ecosystems_savanna-forest-fire-gradient.pdf
78. Staal et al. (2016), bistability & tropical forest/savanna distribution — https://doi.org/10.1007/s10021-016-0011-1
79. Forest-savanna coexistence in Central Africa — https://beta.iopscience.iop.org/article/10.1088/1748-9326/ad8cef
80. Wuyts et al., gradual vs steep forest edges — https://www.sciencedirect.com/science/article/abs/pii/S0378112708007378
81. Sci. Rep. (2023), forest edge type & snail assemblages — https://www.nature.com/articles/s41598-023-43758-8
82. Keddy, forest structure in E. North America — https://www.eomf.on.ca/media/k2/attachments/structure.pdf
83. Burrascano et al. (2013), temperate old-growth review — https://www.uvm.edu/giee/pubpdfs/Burrascano_2013_Forest_Ecology_and_Management.pdf
84. Frontiers (2026), Isoberlinia stand structure — https://www.frontiersin.org/journals/forests-and-global-change/articles/10.3389/ffgc.2026.1800379/full
85. Red Blob Games, 2D point sets — https://www.redblobgames.com/x/1830-jittered-grid/
86. Muratori, "The Nebraska Problem" — https://caseymuratori.com/blog_0011
87. Lagae & Dutré, Poisson-disk comparison — https://onlinelibrary.wiley.com/doi/10.1111/j.1467-8659.2007.01100.x
88. Williams, Ritsos, Headleand (2020), virtual forestry tree placement — https://mdpi-res.com/d_attachment/computers/computers-09-00020/article_deploy/computers-09-00020.pdf?version=1584100029
89. Åkesson (2025), KTH thesis on PVG methods — http://urn.kb.se/resolve?urn=urn%3Anbn%3Ase%3Akth%3Adiva-367794
90. Wolinsky & Pratson (2005), landscape evolution from slope histograms — https://doi.org/10.1130/g21296.1
91. On the dynamic smoothing of mountains — https://agupubs.onlinelibrary.wiley.com/doi/10.1002/2017GL073095
92. Graf (1970), glacial valley cross-section — https://scholarcommons.sc.edu/cgi/viewcontent.cgi?article=1041&context=geog_facpub
93. Coles (2014), glacial valley cross-sections thesis — https://etheses.whiterose.ac.uk/id/eprint/5452/1/Coles_2014.pdf
94. Montgomery (2002), valley formation fluvial vs glacial — https://glaciers.pdx.edu/fountain/readings/TopicsInGeomorphology/Montgomery2002_ValleyFormationGlaciersRivers.pdf
95. Glacial valley modification assessment (V-index) — https://www.sciencedirect.com/science/article/abs/pii/S0169555X18302526
96. Hergarten & Robl (2018), Flint's law vs hillslope diffusion — https://meetingorganizer.copernicus.org/EGU2018/EGU2018-3093-1.pdf
97. pyTopoComplexity — https://par.nsf.gov/biblio/10494722
98. Local docs: `research/micro-voxel-creators-research.md`, `research/grass-rendering-research.md`, `research/terrain-fixes-log.md`, `research/_terrain_question_bank.md`

---

## Appendix A — Forty further questions

Beyond the parent's 69–80; each answered or explicitly dispositioned.

1. **Is the β=2 spectrum an attractor of erosion physics, or of deposition?** Partially answered: Turcotte's lattice-deposition model produces k⁻² surfaces from pure deposition [4]; fluvial self-similarity under SPIM dynamics has conditions on m, n, uplift variability [not fetched]. Disposition: follow-up in Part 2's terms.
2. **What octave count does β=2 require if lacunarity ≠ 2?** The octave-lacunarity-gain trio sets a finite band; outside it the spectrum rolls off. Answered qualitatively [2]; exact filter response is a one-page derivation — future work.
3. **Can a voxel engine validate its 3D density field (not just heights) against anything?** Open; no equivalent statistics for full 3D terrain exist in the literature surveyed. Proposal: extend §9 with cave-porosity and passage-orientation statistics against Part 4.
4. **What is the visual, not statistical, detection threshold for drainage wrongness?** Unanswered in the literature found; user-study territory ([88][89] are the template).
5. **Does priority-flood filling produce geologically wrong lakes?** Yes as stated in GIS practice — fills are data-conditioning, not hydrology; real lakes need depression *hierarchy* and outflow decisions. Flagged; Barnes' watershed-labeling variant is the starting point [33].
6. **How fast is Priority-Flood in game terms?** O(n) integer, 20 lines [33]; sub-millisecond-scale for a 1024² region on modern CPUs. Answered by complexity, not benchmarked.
7. **Is D8's 45° bias visible at game resolution?** At channel widths ≥3 cells, yes — sawtooth rivers; mitigations: D8-LTD [39] or sub-cell path accumulation. Answered.
8. **What m/n should a game's SPIM use?** θ=m/n≈0.4–0.5 typical, from observed slope–area exponents −0.35…−0.6 with n≤1 [28][29]. Answered.
9. **How many implicit SPIM steps to steady state?** Depends on uplift/erodibility; research codes use hundreds–thousands of steps but the implicit scheme allows large dt [29]. Disposition: needs an experiment at our heightfield scale; not in sources.
10. **Can orographic P and SPIM oscillate (rain shadow chases the ridge)?** Physically yes over geologic time; one LT pass per erosion checkpoint is standard in coupled fastscape work [30][72]. Partially answered.
11. **What does the 128 m NMS shell imply for our 7.8 mm engine?** Our shell must be similarly thin — full-res voxels only near the camera (already the engine's LOD-radius design); macro relief belongs to the analytic layer [60]. Answered by analogy.
12. **Is Minecraft's 1.5625 sloped_cheese threshold meaningful for us?** A tuned constant of their density math, not transferable. Answered (negative).
13. **Do aquifers generalize beyond Minecraft's water/lava?** Yes trivially — per-region fluid level + flood/spread/barrier channels is a clean chunk-scale water table [62]. Answered.
14. **What is the real cave-volume fraction (porosity) by karst maturity?** Part 4's territory; not fetched. Disposition: cross-reference.
15. **Can the constant-drop test run per-biome?** Yes — drainage density varies 2–12 km/km² by climate [45][46]; per-biome t-tests extend TauDEM's global one [43]. Answered.
16. **Does thermal erosion's soap-facet artifact survive under added noise?** Practically masked by re-seeded detail (Houdini's re-seeding step [58]). Answered.
17. **What droplet count converges to a stable channel network?** No convergence theory exists; Lague's 70k/255² is aesthetic [25]. Disposition: open; treat droplets as decoration.
18. **Is there a deterministic (seed-stable) droplet scheme?** Yes — fixed spawn lattice + deterministic PRNG per droplet; Gaea's Deterministic toggle is the commercial precedent [23]. Answered.
19. **Should rivers be carved below the heightfield or the heightfield lowered to them?** Carve: SPIM produces the valley and the channel together; post-hoc lowering breaks the slope–area law [29]. Answered by construction.
20. **How wide should a game river be per catchment area?** Hydraulic geometry: w ∝ A^0.5 within basins [28]. Answered.
21. **Where do waterfalls/knickpoints belong?** As SPIM transients on lithology contrasts (Part 2 §19, §29); a stencil on layer boundaries is the cheap version. Answered in outline.
22. **Can domain warp encode real fold *orientation*?** Yes — Michel et al. drive warp by plate velocity vectors [14]; orientation comes free if the belt skeleton is authored. Answered.
23. **Is a 2D heightfield adequate for anticlinal ridges with breached cores?** Marginally: ridge + carve stencil gets the planform; the water gap through the breach needs flow-graph forcing. Partially answered.
24. **Does anyone ship variogram-based LOD (fractal interpolation between samples)?** Musgrave's QAEB tracing is the historical version [7]; modern engines bake LODs instead. Answered (negative).
25. **What grid resolution should hydrology run at vs the voxel grid?** Decoupled: hydrology at 10–30 m-equivalent (drainage statistics are defined there [42][48]), voxels 7.8 mm near camera. Answered by scale analysis.
26. **Is blue-noise vegetation placement worth it over jittered-hex?** At 5 m tree spacing, no user can tell (angle histograms differ [85]); spend the effort on cluster structure [88]. Answered.
27. **How do you place vegetation on 3D cave walls?** John Lin ray-traces placement queries (sunlight, openness, cave walls) — micro-voxel doc §1.4. Answered by citation.
28. **What's the grass-density equivalent of stems/ha?** Part 6's ground-cover fractions own this; grass-rendering doc §4's shipped blade budgets (83k–100k drawn) are the render-side answer. Cross-reference.
29. **Do ecotone sharpness rules apply underwater?** Unresearched (kelp/seagrass boundaries). Disposition: open; Part 6/Part 3 follow-up.
30. **Can the Whittaker diagram be made hysteretic cheaply?** Yes — order-dependent lookup (last biome biases threshold), implementing alternative stable states; no shipped example found. Partially answered (proposal).
31. **What's the cheapest correct rain shadow?** Upslope model P = Cw·U·∇h [71]; one gradient + dot product. Answered.
32. **When does the LT FFT model beat the upslope model?** When mountain width ~ drift distance (5–25 km): spillover and displaced maxima matter [71][74]. Answered.
33. **Should biome noise fields share octaves with terrain noise (Minecraft-style)?** Yes where correlation is physical (elevation→temperature), no where independence is physical (rainfall vs micro-relief); Minecraft shares continents/erosion/ridges [62]. Answered.
34. **How is the 29% land fraction best enforced?** Two-crust construction (§10.2 step 1) or sea-level quantile matching; the second is cheaper, the first geologically honest (Part 1). Answered with options.
35. **Does the slope-histogram test distinguish SPIM output from stamped terrain?** Yes — stamped cliffs create bimodal/shouldered histograms absent from Wolinsky-Pratson's observed trend [90]. Answered.
36. **What is the memory cost of a flow graph per region?** D8 receivers + stack order + accumulation: ~3 words/cell at hydrology resolution — negligible vs bricks. Answered by arithmetic.
37. **Can the SPIM stack be computed incrementally as chunks stream?** Not locally — flow graphs are global by nature; hydrology at 10–30 m fits whole-region computation at generation time (DF does world-scale in seconds [65]). Answered.
38. **Is there a shipped game with published terrain validation statistics?** None found; the §9 suite would be novel. Answered (negative).
39. **What breaks first in the pipeline at planet scale?** FFT-based orography (needs spherical harmonics/HEALPix) and 32-bit float coordinates (NMS's documented multi-space problem [15][60]). Answered.
40. **Which single §9 test gives the most bug-detection per line of code?** Drainage density (test #4): catches missing rivers, noise gullies, wrong thresholds, and broken flow routing in one number with a wide real-Earth band [45][46][47][48]. Answered — recommendation.
