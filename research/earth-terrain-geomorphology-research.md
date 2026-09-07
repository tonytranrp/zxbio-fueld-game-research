# Earth Terrain and Geomorphology: A Simulation-Grade Reference

**Scope:** how Earth's terrain actually works — the physics and math of mountain building, river erosion, glacial carving, coasts, caves and karst, deserts and wind, the spawn rates and spatial statistics of vegetation, and the algorithms that turn all of it into generated worlds — researched at simulation depth for a voxel game engine. This is the fourth document in the project's natural-world research series, after [Water Dynamics & Wave Physics](water-physics-and-wave-simulation.md), [Human Movement and Perception](human-movement-and-perception-research.md), and [Human Eye and Vision](human-eye-and-vision-research.md). Tree growth, sway, and leaf optics live in [Tree Motion, Growth & Appearance](tree-motion-growth-and-appearance.md); game-side grass rendering practice lives in [Grass Rendering Research](grass-rendering-research.md). Where those documents own a topic, this one cross-references instead of re-deriving.

**How this was built:** one parent question bank of 80 questions (`_terrain_question_bank.md`, retained), seven parallel research agents with live web access, each instructed to verify the parent's pre-computed numeric anchors independently and *report disagreements rather than silently copy them* — that discipline caught 20 substantive corrections, all documented in the text below where they occur and consolidated in each part's Provenance section. Each part additionally generated 40 further questions (280 beyond the parent's 80), answered or explicitly dispositioned in its Appendix A.

**How to read it:** Parts 1–6 are the geoscience, in causal order (build terrain → carve it with water → carve it with ice and waves → dissolve it underground → dry it out and let wind reshape it → grow vegetation on the wreckage). Part 7 is the synthesis layer — what noise actually does statistically, which erosion algorithms are worth implementing, how shipped engines do it, and the recommended layered pipeline for this engine. Every measured quantity is given as a range with its source; where the literature genuinely disagrees, both values are reported. Where something could not be verified, it is flagged as such rather than smoothed away.

**The one-paragraph answer to "how should terrain work":** real terrain is not a heightfield texture — it is a *history*. Continents and mountain belts are placed by tectonics (Part 1: linear belts with a 1:10–1:30 width:length ratio, isostatic roots meaning erosion lowers a summit only 0.18× the rock removed, peak heights clamped near the snowline in glaciated ranges). Water then imposes the single most visible structure in any landscape — a drainage network with Horton-geometry branching and concave-up profiles carved by the stream-power law (Part 2) — and no noise-based terrain reproduces it, which is why hydrologically-correct generation (priority-flood + D8 + implicit stream-power carving, all O(n)) is the highest-value real physics a generator can buy. Ice converts V-valleys to power-law U-troughs and stamps cirques at the snowline (Part 3); limestone dissolves into cave systems organized by the water table, not randomness (Part 4); deserts are placed by latitude, rain shadows, and cold coasts, and their dunes are selected by the wind rose's directional variability, not by a sand texture (Part 5); and vegetation is not a density slider but six orders of magnitude of stems/ha across biome × size-class × successional stage, almost never uniformly spaced (Part 6). Part 7 assembles the pipeline.

---


# Part 1 - Tectonics and Mountain Building: How Terrain Is Constructed

**Scope:** how Earth's crust builds terrain at every scale — plate kinematics, isostasy, orogens, rifts, arcs, volcanoes, hotspots, and the ~4-billion-year arc of terrain evolution. This is Part 1 of the merged Earth-terrain reference. Every formula is derived or cited; every measured quantity is given as a range with its source. Where the literature genuinely disagrees, both values appear. Where I could not verify a number this pass, the text says so explicitly rather than laundering a textbook figure through a fake citation.

**How to read this:** Sections 1–2 are the physics backbone. Sections 3–8 are terrain-type anatomy. Section 9 is deep time. Section 10 is the engineering synthesis — read it last.

---

## 1. Plate tectonics mechanics

### 1.1 The driving forces, and their measured ranking

Three forces drive plates; everything else resists them.

**Slab pull** is the negative buoyancy of cold lithosphere sinking into the mantle. A slab is denser than the surrounding mantle by Δρ ≈ 40–50 kg/m³ (typical value used in analytical subduction models; [Suchoy et al., 2021](https://doi.org/10.5194/se-12-79-2021)). The cold plate behaves as a stress guide: the gravitational body force on the descending slab is transmitted up the plate to its horizontal part ([Forsyth & Uyeda, 1975](https://doi.org/10.1111/j.1365-246x.1975.tb00631.x)).

**Ridge push** is not actually a push at the ridge crest; it is the integrated gravitational sliding force of the cooling, thickening, topographically elevated oceanic lithosphere as it moves away from the ridge — distributed over the whole plate, not applied at the boundary. Its magnitude per unit ridge length is of order $3\times10^{12}$ N/m (~300 bar average lithospheric stress; [Forsyth & Uyeda, 1975](https://doi.org/10.1111/j.1365-246x.1975.tb00631.x)).

**Basal drag** is the viscous shear coupling between plate undersides and mantle flow. It can drive or resist depending on whether asthenospheric flow is faster or slower than the plate. Classic inversions found continental drag ~8× stronger than oceanic drag ([Forsyth & Uyeda, 1975](https://doi.org/10.1111/j.1365-246x.1975.tb00631.x)); modern 2-D models put basal drag at ~3–30 % of slab pull (≈10 % average in reference models; up to ~30 % for the Pacific-sized plate; [Suchoy et al., 2021](https://doi.org/10.5194/se-12-79-2021)).

The relative ranking, from four independent eras of study:

| Study | Finding |
|---|---|
| [Forsyth & Uyeda, 1975](https://doi.org/10.1111/j.1365-246x.1975.tb00631.x) | Slab-related forces an order of magnitude stronger than all others; subducting plates fall at a "terminal velocity" of ~6–9 cm/yr |
| [Lithgow-Bertelloni & Richards, 1995](https://discovery.ucl.ac.uk/id/eprint/94137/1/95GL01325.pdf) | Slab buoyancy (upper + lower mantle) >90 % of driving torque; lithospheric thickening (ridge push) <10 % |
| [Conrad, 2004](https://www.clintconrad.no/papers/Conrad_JGR2004.pdf) | Present split: ~60 % direct slab pull from upper-mantle slabs + ~40 % "slab suction" (lower-mantle slabs driving plates via mantle flow); subducting plates move 3–4× faster than non-subducting ones |
| [Suchoy et al., 2021](https://doi.org/10.5194/se-12-79-2021) | Basal drag 5–30 % of slab pull; ridge push ~10 % of slab pull |

$$\boxed{\text{Slab pull} \gg \text{basal drag} \approx \text{ridge push} \sim 10\%\ \text{of slab pull}}$$

**Simulation consequence:** a plate generator that wants Earth-like speeds should couple each plate's speed to whether it has a subducting (sinking) edge. Plates with big subduction boundaries move fast (5–10 cm/yr); plates with none crawl (1–2 cm/yr).

### 1.2 Plate speeds

Verified ranges from global kinematic models (NUVEL-1A, MORVEL, and GPS-based REVEL): full-size plates span roughly **1–10 cm/yr** of motion ([DeMets et al., 1990](https://doi.org/10.1111/j.1365-246x.1990.tb06579.x); [Sella et al., 2002](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1467&context=geo_facpub); [DeMets et al., 2010](https://academic.oup.com/gji/article/181/1/1/713644)). The parent anchor "~1–10 cm/yr" is **verified**, with the caveat that the fastest *plate boundary* relative motion on Earth is faster: Tonga total convergence including back-arc spreading reaches 225 km/Myr = ~22 cm/yr ([Syracuse & Abers, 2006](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2005GC001045)).

- **Fastest major plates:** Pacific (up to ~7–10 cm/yr toward WNW at the plate's fastest end), Cocos and Nazca in the eastern Pacific (Nazca–South America convergence is 65.5 ± 0.8 mm/yr at ~30°S, tapering to ~50 mm/yr in northern Colombia; [Jarrín et al., 2022](https://doi.org/10.1093/gji/ggac353)). All three are fast *because* they are attached to long subducting slab edges — the slab-pull rule in action.
- **Slowest:** Eurasia, North America, Africa — little or no subducting boundary of their own, large continental area (high drag). REVEL found several of these pairs decelerating, consistent with continental collision absorbing motion ([Sella et al., 2002](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1467&context=geo_facpub)).
- **India** is the cautionary case: pre-collision it moved at ~15–20 cm/yr (fastest known plate episode, on a now-subducted slab); after colliding with Eurasia it slowed to ~4–6 cm/yr, of which the Himalaya currently absorbs about one third (~19–20 mm/yr of shortening; [Long et al., 2012](https://www.sciencedirect.com/science/article/abs/pii/S0012821X13006213)).

### 1.3 The three boundary types, and why convergence makes belts, not blobs

Divergent (ridges, rifts), convergent (subduction zones, collision zones), and transform (strike-slip). The terrain signature is set by *where deformation is localized*. A plate boundary is a 1-D curve on a 2-D surface (a small circle or small-circle chain on the sphere; see §6.1), and the strain is confined to a band a few tens to a few hundred km wide on either side of that curve. Convergence therefore **extrudes relief along the boundary line**: the Himalayan fold-thrust belt is ~150–300 km wide but ~2,400 km long; the Andes are ~200–700 km wide and ~7,000 km long. A blob (a circular uplift) requires a point force — a mantle plume or a big volcano — and those are the exceptions (Hawaii, Yellowstone, Iceland), not the rule. At the simulation level: if your mountain generator produces equant mountain blobs, it is simulating plumes and volcanoes but not plate tectonics.

---

## 2. Isostasy

### 2.1 Airy isostasy — derived

The lithosphere floats on the mantle like ice on water, with low-density crustal "roots" beneath high topography. Consider two crustal columns of density $\rho_c$ over mantle of density $\rho_m$, both extending to a common compensation depth. Reference column: crustal thickness $T$. Mountain column: surface elevated $h$, crustal root $r$ below the reference base. Equal mass per unit area above the compensation depth:

$$\rho_c (T + h + r) + \rho_m(D - r) = \rho_c T + \rho_m D$$

$$\Rightarrow\quad \rho_c h = (\rho_m - \rho_c)\, r \quad\Rightarrow\quad \boxed{r = \frac{\rho_c}{\rho_m-\rho_c}\,h = 4.5\,h \quad (\rho_c = 2700,\ \rho_m = 3300\ \text{kg/m}^3)}$$

**Parent anchor verification: CONFIRMED.** With $\rho_c = 2700$ and $\rho_m = 3300$ kg/m³, the root is exactly 4.5 × the surface height. Everest's 8,848 m implies an Airy root of $4.5 \times 8.848 \approx 39.8$ km — the parent's arithmetic is correct. Two caveats a simulation should know:

1. **Density sensitivity.** Textbooks that use $\rho_c = 2800$ kg/m³ get $r = 2800/500 = 5.6\,h$ — a 25 % deeper root for identical topography. The 4.5× value is as good as the density contrast; real crustal densities vary ±100 kg/m³.
2. **Total crustal thickening** is $h + r = 5.5\,h$. For Everest: ~48.7 km of added crust. But the *measured* Moho under the Himalaya is 70–80 km against a ~35–40 km Indian reference (§4), i.e. ~2× what Everest's own elevation requires — because the whole range rides on crust thickened by hundreds of km of shortening, not just by Everest's peak.

The oceanic version (for submarine mountains): a column under water depth $d$ has its root *removed* by $r = d(\rho_c - \rho_w)/(\rho_m - \rho_c) \approx 0.82\,d$, which is why a 5 km-deep seafloor sits on ~30 km plate and why oceanic plateaux and volcanic edifices get flexurally depressed rather than Airy-rooted.

### 2.2 Pratt isostasy

Pratt compensation assumes the crustal columns have *different densities* but a common base (no root). Then

$$\rho(x)\,[T + h(x)] = \text{const}$$

Elevation differences reflect lateral density: hot, expanded lithosphere (ridges, rift flanks, plume swells) stands high; cold, dense lithosphere sits low. Pratt-type compensation describes the ~2–3 km elevation difference between mid-ocean ridges and abyssal plains (the ridge is hot and buoyant, not rootless crust) and the East African plateau's dynamic support (§5.3). Real Earth uses both: Airy dominates in collisional belts, Pratt/thermal dominates in oceans and rifts.

### 2.3 Flexural isostasy — what it changes

Airy assumes zero lithospheric strength: every column floats independently. Real lithosphere has finite flexural rigidity

$$D = \frac{E\,T_e^3}{12(1-\nu^2)}$$

with $T_e$ the **effective elastic thickness** — not the mechanical plate thickness, but the thickness of an ideal elastic plate with the same long-term strength. The plate deflects under loads according to

$$D\,\frac{d^4 w}{dx^4} + (\rho_m - \rho_c)\,g\,w = q(x)$$

with a flexural length scale $\alpha = \left[4D/((\rho_m-\rho_c)g)\right]^{1/4} \approx 100$–$150$ km for $T_e = 20$–$40$ km. Consequences:

- **Loads are spread out.** A mountain belt does not just push its own local root down; it bends the plate over a region ~$\alpha$ wide. Small loads (a volcano, 20–60 km across) are partly supported by plate strength — the seafloor under Mauna Loa is *depressed ~8 km* by its ~75,000 km³ mass rather than floating on a root ([USGS, Mauna Loa](https://www.usgs.gov/volcanoes/mauna-loa)).
- **Foreland basins exist only because of flexure.** The Ganges basin in front of the Himalaya holds ~6 km of sediment in a flexural trough; the plate then rebounds into a **forebulge** 30–50 m high some distance away (Himalayan forebulge amplitude, 3-D flexural modeling; [flexural modeling of the Himalayan foreland](https://uh-ir.tdl.org/items/a874b0ff-529d-4343-b387-5014ead41d2c)).
- **$T_e$ is strongly bimodal and setting-dependent:** continental values span 5–125 km, clustering at 10–20 km (rifted, young, hot lithosphere — narrow deep foreland basins like the Apennines') and 80–90 km (old cold cratons — wide shallow basins like the Ganges') ([Watts, 1992](https://doi.org/10.1111/j.1365-2117.1992.tb00043.x)). The Himalayan foreland is a high-$T_e$ rigid block ($T_e \approx 40$–$100$ km, up to 125 km in the center) while the Tibetan plateau itself is nearly strengthless ($T_e \approx 0$–$20$ km) ([Jordan & Watts, 2005](https://www.sciencedirect.com/science/article/abs/pii/S0012821X05003390)). Eastern Himalaya (Bhutan) shows the along-strike weakening: flexural rigidity drops from ~$3.8\times10^{24}$ N·m (Nepal, coupled layers) to $\le 7\times10^{22}$ N·m (Bhutan, decoupled; equivalent $T_e$ 94 → 25 km) ([Hetényi et al., 2014](https://agupubs.onlinelibrary.wiley.com/doi/10.1002/grl.50793)).

### 2.4 Post-glacial rebound — the isostatic response you can watch

Glacial isostatic adjustment (GIA) is isostasy running at human-observable speed. Ice sheets 2–3 km thick depressed the crust by hundreds of meters; deglaciation ~11 ka unloaded it and the mantle is still flowing back.

- **Hudson Bay** (thickest Laurentide ice): present uplift ~**10 mm/yr**, decreasing away from the bay and flipping to 1–2 mm/yr subsidence south of the Great Lakes (a "hinge line" between uplift and subsidence; GPS, 360 sites; [Sella et al., 2007](https://doi.org/10.1029/2006gl027081)).
- **Fennoscandia:** maximum uplift **10.3 mm/yr** near Umeå, northern Sweden (9.6 mm/yr relative to the rising geoid), with the zero-line along the German/Polish coast ([NKG2016LU land-uplift model, 2019](https://link.springer.com/article/10.1007/s00190-019-01280-8)); the BIFROST GPS project measured the same ~10–11 mm/yr maximum ([Johansson et al., 2002](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2001JB000400)).

**Parent anchor verification: CONFIRMED** — rebound is mm/yr-scale, peaking at ~10 mm/yr at both classic locales, roughly 10⁴ yr after unloading. Fitting these rates is the classical constraint on upper-mantle viscosity ($\sim10^{21}$ Pa·s). Note the mismatch of timescales: tectonic isostatic response (collision, erosion) takes 10⁴–10⁶ yr because the loads are wide; the *instantaneous* flexural response to a big volcanic emplacement can be metres in years.

---

## 3. The height limit of mountains

Why ~8.8 km and not 20 km? Five competing constraints, all real, all with numbers:

1. **Crustal strength.** A column of rock of height $H$ exerts a basal differential stress $\sim \rho_c g H$. At $H = 8.8$ km that is ~230 MPa — right at the yield strength of hot, wet mid-crustal rocks (100–300 MPa range over geological strain rates). Twice the height, twice the stress, and the lower crust flows out sideways instead of supporting the load. This is the accepted limit on *plateau* elevation: Tibet (~5 km average) and the Altiplano (~3.7 km) sit at the strength ceiling for their crustal compositions ([Egholm et al., 2009](https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf) and refs therein).
2. **The angle of repose and talus limits.** Loose talus rests at 30–37°; bedrock slopes steeper than ~30–35° fail by landslide and are throttled back toward that angle (mean slopes >30° in the glacially steepened Washington Cascades are interpreted as at or near threshold steepness; [Mitchell & Montgomery, 2006](https://doi.org/10.1016/j.yqres.2005.08.018)). A peak with 4–5 km of local relief at ~35° needs only a ~7–12 km basal footprint — consistent with real high-mountain geometry, and a direct constraint on any voxel cliff generator (see the sibling coverage of threshold hillslopes in Part 2, §hillslopes).
3. **The glacial buzzsaw.** Glaciers erode fastest at and just below the equilibrium-line altitude (ELA / snowline). Globally, summit elevations are confined to within ~1,500 m above the local snowline, and hypsometric maxima pile up just below it — independent of tectonic uplift rate, lithology, and setting ([Egholm et al., 2009](https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf); review: [Herman et al., 2021](https://preview-www.nature.com/articles/s43017-021-00165-9)). In the Washington Cascades, peaks rise ≤600 m above a planar zone of 373 cirques ([Mitchell & Montgomery, 2006](https://doi.org/10.1016/j.yqres.2005.08.018)). **Caveat:** the buzzsaw is contested — critics argue some "buzzsaw" surfaces are inherited pre-glacial relief and that the ELA–summit correlation is not always causal (passive-margin ranges break the simple link; ["Glacial and periglacial buzzsaws", Quaternary Research](https://www.cambridge.org/core/journals/quaternary-research/article/abs/glacial-and-periglacial-buzzsaws-fitting-mechanisms-to-metaphors/4828F3C917E238FD0405E6A0B9AF51CF)). Full glacial-erosion mechanics are the sibling agent's Part 3; from this side it suffices that a snowline-indexed ceiling exists in most cold ranges.
4. **Erosion rate vs uplift rate balance.** Erosion increases with slope and discharge; steepen a growing range and erosion accelerates until it matches uplift. The Himalaya absorbs ~20 mm/yr of shortening and its Greater Himalayan rocks exhume at ~1–3 mm/yr (§4.3) — a near-steady state in which extra height buys extra erosion 1:1.
5. **Isostatic amplification of erosion.** Because of the 4.5× root relation, eroding $s$ meters of surface only *lowers* the surface by $e = s(1-\rho_c/\rho_m) \approx 0.18\,s$ (Airy, local): removing 1 km of rock drops the summit only ~180 m. So mountains are eroded away ~5.5× slower than naive volume accounting suggests — the reason Paleozoic orogens still stand 1–2 km high. Conversely this also means uplift rates from erosion-driven rebound (20–200 m in buzzsaw models; [Egholm et al., 2009](https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf)) can be large without any tectonic input.

**The Everest synthesis (verified):** Everest does not stand on normal crust. It stands on the ~5 km Tibetan plateau, on ~70–80 km thick crust (§4.2) — roughly double normal continental thickness — so its 8,848 m summit is ~8.8 km of *local* relief machinery riding ~5 km of *regional* isostatic pedestal. A 20 km mountain would need ~110 km of Airy root under a strength-limited column of >500 MPa basal stress: the crust fails, the lower crust channels away, and glaciers and landslapes attack the top. Earth's tallest possible mountain is the sum of (strength-limited plateau) + (slope-and-glacier-limited local relief), and that sums to ~9–10 km — which is where we find it: Everest 8,848 m, and Mauna Loa, measured from its isostatically depressed seafloor base, ~17 km ([USGS](https://www.usgs.gov/volcanoes/mauna-loa)) — the only way to beat 9 km is to build in water, where gravity is 1 g but erosion is glacial-free.

---

## 4. Collisional orogens — Himalaya/Tibet as the type example

### 4.1 The kinematics

India collided with Eurasia at ~50–55 Ma and ~1,000 km of shortening has since been absorbed between southern Tibet and the Indian shield ([Patriat & Achache 1984; Dewey et al. 1989], cited in [Gao et al., 2019? — see source 11](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2019TC005483)). Of the ongoing India–Eurasia convergence of 5.8 ± 0.4 cm/yr, the Himalayan fold-thrust belt takes up about one third: long-term shortening rate ~20 ± 2 mm/yr, modern geodetic 19 ± 2.5 mm/yr, Quaternary terrace-derived 21.5 ± 2 mm/yr — a rare case where three independent methods agree ([Long et al., 2012](https://www.sciencedirect.com/science/article/abs/pii/S0012821X13006213), compiling DeCelles, Bettinelli, Lavé & Avouac). The Himalayan fold-thrust belt alone has a *minimum* total shortening up to ~670 km, greatest in western Nepal/northern India, and an additional ~700 km-long slab of Indian lower crust may have been inserted beneath Tibet ([DeCelles et al., 2002](https://doi.org/10.1029/2001tc001322)).

### 4.2 The crustal structure — Moho depths verified

Receiver-function and deep-seismic results (Hi-CLIMB, INDEPTH lines): Moho at **40 km** beneath the Ganga basin, dipping north to **50 km** beneath the High Himalaya, **~70 km** beneath the Indus–Yarlung suture, constant at ~70 km under the 200-km-wide Lhasa block, rising to ~65 km under the Qiangtang block ([Nábělek et al. 2009 via Searle, 2011](https://www.ccsp.ox.ac.uk/sites/default/files/ccsp/documents/media/searle_et_al_2011-tibet.pdf)). Crustal thickness under the collision zone at 88°E is 66–81 km — "two times the global average" — from deep reflection profiling ([Gao et al., 2016, nonuniform subduction of Indian crust](https://pmc.ncbi.nlm.nih.gov/articles/PMC5624955/)). Under the Karakoram and far-west Tibet the Moho reaches its global continental maximum: **75–90 km** ([Searle et al., 2011](https://www.ccsp.ox.ac.uk/sites/default/files/ccsp/documents/media/searle_et_al_2011-tibet.pdf), compiling Wittlinger 2004, Rai 2006).

**Parent anchor verification: CONFIRMED** — "~70–80 km Moho under Tibet" is right, with the true range 65–90 km (shallowest under Qiangtang, deepest under Karakoram/far-west).

### 4.3 Uplift history and exhumation rates

Competing models for the plateau: (1) Argand-style underthrusting of Indian lower crust; (2) rigid-block extrusion; (3) distributed continuum shortening; (4) lower-crustal channel flow ([Searle et al., 2011](https://www.ccsp.ox.ac.uk/sites/default/files/ccsp/documents/media/searle_et_al_2011-tibet.pdf) reviews all four). Timing remains genuinely contested: southern Tibet may have been high by early Miocene, with stepwise northward growth; two slab-breakoff/delamination events (~45–35 Ma and ~20–10 Ma) are invoked for pulses of uplift and volcanism ([DeCelles et al., 2002](https://doi.org/10.1029/2001tc001322)). Mesozoic–early Cenozoic shortening had already thickened Tibetan crust to ~45–55 km *before* the main collision — Tibet's elevation is partly pre-paid.

**Exhumation of Greater Himalayan rocks — mm/yr scale: CONFIRMED.** Direct constraints from the Bhutan Himalaya: cooling rates in excess of 75 °C/Myr (peak exhumation 13–9 Ma) and 50–90 °C/Myr (2 Ma–present near the range front) ([Long et al., 2012](https://www.sciencedirect.com/science/article/abs/pii/S0012821X13006213)). At a 30 °C/km geotherm, 50–90 °C/Myr ≈ **1.7–3 mm/yr** of exhumation — the correct order for the world's fastest-exhuming rocks. The channel-flow model exposes a 10–20 km thick mid-crustal slab of Greater Himalayan gneiss along the Main Central Thrust (below) and South Tibetan Detachment (above), synchronously active ~24–15 Ma with ≥100 km of southward extrusion ([Searle et al., 2011](https://www.ccsp.ox.ac.uk/sites/default/files/ccsp/documents/media/searle_et_al_2011-tibet.pdf)).

### 4.4 The structural grammar of collision — what to simulate

- **Fold-and-thrust belts ride a basal décollement** (the Main Himalayan Thrust, MHT — the master fault that soles the whole belt). The MHT is a **ramp-flat geometry**: a low-angle flat at ~10–15 km depth under southern Tibet climbs a mid-crustal ramp of ~20° dip, 50–70 km long, under the High Himalaya, where a ~14 km of structural relief is accommodated (thermo-kinematic inversion, eastern Himalaya; [Cona cross-structure study](https://www.sciencedirect.com/science/article/abs/pii/S0012821X13006213) and refs). Fault-bend folds (ramp anticlines) form where thrust sheets ride up ramps — the classic geometric constructions are Suppe (1983)'s (standard textbook; not re-derived here). **Simulation grammar:** generate a décollement, cut ramps into it at spacing ~50–100 km, and the surface anticlines come out for free.
- **Accretionary wedges** (at subduction trenches, and scaled-down at thrust fronts) obey critical-taper mechanics: the wedge grows until its surface slope α plus basal dip β reaches a critical taper θ_c set by basal and internal friction and pore pressure (Coulomb wedge theory, Davis/Dahlen/Dahlen-Suppe 1983–1990; standard textbook — flagged: not independently re-verified this pass). Real wedges: taper angles of a few degrees (e.g., ~4–8° offshore Japan/Nankai-class prisms); the Himalayan wedge as a whole is often treated as a critical taper orogen above the MHT.

---

## 5. Extensional terrain

### 5.1 Normal faults, grabens and horsts

Extension thins the crust. The brittle upper crust fails on **normal faults** dipping ~50–65°; the hanging wall drops, the footwall rises isostatically. Pairs of opposed normal faults drop **grabens** between **horsts**. Because the dominant fault in any basin is usually one border fault, most rift basins are **half-grabens** — asymmetric, with all the displacement on one side and the other side a tilted, ramp-like margin. In map view rifts are strings of alternating-polarity half-grabens linked by accommodation zones: the border-fault segments of the East African rift are individually 60–120 km long and bound 40–75 km-wide extensional zones ([Ebinger, 1988](https://doi.org/10.1575/1912/4624)).

### 5.2 Basin and Range

The type province for continental extension. Key verified numbers:

- **Magnitude:** on average the province *doubled* in width ([Hamilton, 1987](https://doi.org/10.1144/gsl.sp.1987.028.01.12)); a balanced cross-section at 39°N retrodeforms to **230 ± 42 km of cumulative extension (46 ± 8 %)**, restoring to 54 ± 6 km pre-extensional crustal thickness ([Long, 2019](http://www.seanpatricklong.com/uploads/1/4/5/6/14564072/long_2019_gsab_basin_range_xsec.pdf)); locally >100 % (Grant Range, Nevada: 24 km of extension, 115 %, between 31 and 15 Ma; [Long et al., 2018](https://doi.org/10.1029/2018tc005073); southern Great Basin >100 %, [Rehrig, 1986](https://doi.org/10.1130/spe208-p97)).
- **Timing:** major extension at **17–16 Ma** in the northern and central province, rapid phase 16–10 Ma, waning since — triggered partly by gravitational collapse of a 2.5–3.5 km high "Nevadaplano" and catalyzed by Yellowstone plume arrival and demise of Farallon subduction ([Camp et al., 2015](https://doi.org/10.1130/ges01051.1); [Long, 2019](http://www.seanpatricklong.com/uploads/1/4/5/6/14564072/long_2019_gsab_basin_range_xsec.pdf)). Older Oligo-Miocene core-complex extension (35–16 Ma) was localized ([Rehrig, 1986](https://doi.org/10.1130/spe208-p97); [Long et al., 2018](https://doi.org/10.1029/2018tc005073)).
- **Terrain result:** north-trending ranges 15–25 km wide, 1–2+ km of relief, spaced one basin-width apart, covering ~800,000 km².

**Metamorphic core complexes (MCCs)** are the footwall exhumations of low-angle **detachment faults**: mylonitic mid-crustal rocks (cooled from ~10 km depth) beneath brittlely-rotated upper plates, commonly with a chloritic breccia carapace; displacement rates on core-complex detachments run up to **7–9 km/Myr** (vs 0.5–1 km/Myr on ordinary Basin-and-Range faults; [Long et al., 2018](https://doi.org/10.1029/2018tc005073), compiling many studies). In cross-section an MCC is an antiform of deep crust unroofed by tens of km of extension — a rare place where mid-crustal rocks are the landscape.

### 5.3 East African Rift and why rift flanks stand high

The East African Rift System (EARS) is ~5,000 km long and split into a magmatic eastern branch and a nearly amagmatic western branch that avoid the Archean Tanzanian craton (whose 170–250 km-thick buoyant keel resists extension and deflects the plume head around it; [Koptev et al., 2015](https://doi.org/10.1016/j.gsf.2015.11.002)).

- **Rates:** Nubia–Somalia divergence was ~2 mm/yr from rifting onset (25–30 Ma) until ~4 Ma, then accelerated to ~4 mm/yr ([Koptev et al., 2015](https://doi.org/10.1016/j.gsf.2015.11.002), compiling geodesy). The inner graben of the Northern Kenya Rift shows a mid-Pleistocene-to-Recent brittle extension rate of **1.0–1.6 mm/yr, locally 2.0 mm/yr**, with ≥65 % of the extension concentrated in a 20-km-wide axial zone as the border faults go dormant ([Vetuto/Frischhut? — see source; 2022](https://doi.org/10.1029/2021GC010123)).
- **Flank topography:** the elevated rift flanks of the western branch stand 900–2,500 m above sea level, i.e. **400–2,000 m above the lake floors**, uplifted and tilted *away* from the rift valley ([Ebinger, 1988](https://doi.org/10.1575/1912/4624)). Rift flank uplift is the sum of (a) flexural rebound of the footwall block as the hanging wall drops, (b) lithospheric thinning bringing hot, low-density mantle up (Pratt compensation, §2.2), and (c) dynamic support from plume-driven convection — which maintains the ~1,000 m-average East African plateau at wavelengths longer than the flexural wavelength of the broken plate ([Ebinger, 1988](https://doi.org/10.1575/1912/4624)).
- **Thermal anatomy:** the magmatic eastern branch has ~150 km lithosphere and earthquake hypocenters in the upper ~15 km with heat flow to 110 mW/m²; the amagmatic western branch has ~200 km lithosphere and hypocenters to 30–40 km depth ([Koptev et al., 2015](https://doi.org/10.1016/j.gsf.2015.11.002)).

**Simulation grammar:** a rift generator = an axis of alternating half-grabens, border faults 60–120 km long, basins subsiding (2–7 km of fill in the deep lakes — standard textbook figure, not independently verified this pass), flanks 0.4–2 km high and 50–100 km wide, thermal uplift domes of ~1 km amplitude and ~500 km wavelength over the plume, and — if allowed to run — transition to seafloor spreading in 10–30 Myr.

---

## 6. Subduction arcs and transform boundaries

### 6.1 Why arcs are curved

Three stacked reasons, all geometric/physical:

1. **Sphere geometry (Frank, 1968).** A bending plate is a shell on a sphere; a trench-arc system traces small-circle arcs — the intersection of a plane with the sphere. Most trenches on Earth are arcuate segments of small circles ([Stevenson/... Western Sunda Arc analysis, 2023](https://link.springer.com/article/10.1007/s11600-023-01163-9)).
2. **Shell mechanics (Bevis, 2010).** Arcuate subduction zones are the *free*-subduction case — negatively buoyant lithosphere sinking under its own weight, mean slab dip 49° ± 18°, which produces arc-shaped "dimpled" segments from the competition between bending and stretching in edge-buckling modes of thin spherical shells. Straight subduction segments are the *forced* case — lithosphere pushed under an overriding plate, shallow dip ([Bevis, 2010](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2010TC002720)).
3. **Convergence-rate correlation.** Slowly converging boundaries (v_c < 5 cm/yr) have weakly curved arcs (curvature <10°); fast convergence produces arcs of 10° to >90° curvature. Slab dip and arc curvature are *not* correlated ([Tovish & Schubert, 1978](https://doi.org/10.1029/gl005i005p00329)) — a useful falsifier for any generator that couples them.

**Parent anchor verdict: PARTIALLY CONFIRMED.** "Geometry of a sphere" is correct (Frank 1968; Bevis 2010) but is only one of three mechanisms; slab *rollback* shapes back-arc opening more than arc curvature (contra the parent's parenthetical — rollback matters for trench migration and backarc basins, not primarily for the plan-view curve).

### 6.2 Trench depth, arc spacing, and the anatomy of a subduction zone

- **Trench depth.** Mariana Trench: **10,935 ± 6 m** (2020 dives, pressure-referenced; [Stewart & Jamieson, 2021](https://www.sciencedirect.com/science/article/pii/S0967063721001813)); a 2010 multibeam campaign reported 10,994 ± 40 m ([Challenger Deep compilation](https://en.wikipedia.org/wiki/Challenger_Deep)). **Parent anchor "~11 km Mariana": CONFIRMED** (10.9–11.0 km). Typical trenches are 6–10 km; the deepest occur where old, cold, dense lithosphere subducts (Pacific crust at the Mariana is up to ~170 Ma). The Mariana is also deepest *because* it is nearly trench-parallel extensional/oblique with en-echelon strike-slip deeps along its axis.
- **Arc spacing from the trench — PARENT ANCHOR DISAGREEMENT.** The parent's "~100–150 km" does not match the global compilation. The robust global constant is the **depth to the slab top beneath the arc front: 105 km average** (H averaged over 500-km segments ranges 72–173 km; 99 % of volcano-specific values 58–203 km; [Syracuse & Abers, 2006](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2005GC001045)). The *horizontal* trench-to-arc distance is set by dip: middle 50 % of arcs lie **180–275 km** from the trench; full range **85–90 km** (Scotia, Vanuatu — steep slabs) to **470 km** (Alaska — shallow slab) ([Syracuse & Abers, 2006](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2005GC001045)). The ~100–150 km figure that circulates in textbooks is a *steep-slab* number and/or a conflation of the 105-km slab **depth** with the horizontal distance. **Report both, use 180–275 km as the median.**
- **What controls the position?** Two competing models: (a) slab dehydration at ~100–110 km triggers wet melting in the wedge (classic; [Grove et al., 2009](https://preview-www.nature.com/articles/nature08044) — sub-arc slab depth varies systematically 60–173 km with dip and convergence rate, and melting occurs where the wet solidus overlaps dehydration windows); (b) melting above the *anhydrous* solidus where it makes its closest approach to the trench, with melt then focused along a sub-horizontal permeability barrier in the lithosphere and venting at the barrier's apex ([England & Katz, 2010](https://preview-www.nature.com/articles/nature09417); [Arcay, 2020](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2020GC009253)). The two mechanisms predict similar first-order geometry: **a narrow volcanic front ~100–300 km from the trench, sub-parallel to it** ([Britannica: island arcs](https://www.britannica.com/science/plate-tectonics/Island-arcs)).
- **Cross-strike anatomy:** trench → outer rise (faulted bulge, horsts and grabens on the bending plate) → trench → **accretionary prism** (offscraped sediment stack, critical-taper wedge; e.g. Barbados, Makran; accretionary vs *erosive* margins depending on sediment supply) → **forearc** (the prism's backstop and a forearc basin 1–5 km deep) → **volcanic arc** (chain of stratovolcanoes, §7) → **backarc** (extensional basins where rollback drives spreading — Mariana Trough at several cm/yr — or compressional high plateaus like the Altiplano where the overriding plate advances over the trench).

### 6.3 What strike-slip leaves behind

Transform boundaries slide crust past crust with near-zero convergence: no belt, no root. What they leave is *damage-line* terrain, and it is instantly recognizable:

- A **linear trough** (the fault valley itself, hundreds of m deep, bounded by strike-ridges), along which lakes and bays line up — the classic air-photo signature of the San Andreas ([Recognizing the San Andreas Fault: landforms, troughs, sag ponds](https://digital-desert.baremetal.com/san-andreas-fault/san-andreas-03.html)).
- **Sag ponds** — small undrained depressions where minor fault-parallel sags and pull-aparts pond water.
- **Offset drainage** — streams that jog sharply (right-laterally ~330 km of total offset across the San Andreas since ~15-16 Ma is the classic textbook figure — flagged: not independently verified this pass; Quaternary offsets like Wallace Creek's are the sibling Agent-3/2 domain).
- **Shutter ridges** and **pressure ridges** — blocks shoved along the fault that block side valleys, and compressional uplifts at fault bends and stepovers.
- **Pull-apart basins** at releasing stepovers — the origin of Ridge Basin (CA) and the Dead Sea depression (standard textbook examples; flagged).

---

## 7. Volcanoes as terrain builders

### 7.1 The taxonomy with numbers

| Type | Height | Flank slope | Growth rate | Lifespan |
|---|---|---|---|---|
| Shield (oceanic, basalt) | Mauna Loa: 4,169 m ASL; **~9 km above the ocean floor** ([Smithsonian GVP](https://volcano.si.edu/volcano.cfm?vn=332020)); ~17 km above its flexurally-depressed base ([USGS](https://www.usgs.gov/volcanoes/mauna-loa)) | subaerial 3° at coast → 5° at 2 km → ~9° at 3.3 km; rift-zone axes ~5° *less* steep than flanks; submarine toes >14° ([Rowland & Garbeil, 2000](https://www.soest.hawaii.edu/earthsciences_archive/FACULTY/ROWLAND/pdfs/Rowland_Garbeil_2000.pdf)) | shield stage adds ~95 % of final volume; earliest flows 0.6–1 Ma ago, subaerial emergence ~300 ka → ~0.05–0.08 km³/yr long-term; historical 1843–1984: 4.2 km³/141 yr ≈ 0.03 km³/yr ([USGS Geology & History of Mauna Loa](https://www.usgs.gov/volcanoes/mauna-loa/science/geology-and-history-mauna-loa); [Decker, USGS PP1350](https://pubs.usgs.gov/pp/1987/1350/pdf/chapters/pp1350_ch18.pdf)) | ~0.5–1 Myr to build; ~90 % of surface covered by flows younger than 4 ka (resurfacing is continuous) |
| Stratovolcano (andesite-dacite) | 2–4 km (Fuji 3,776 m) | **30–35°** average, concave-up profile ([BGS](https://www.bgs.ac.uk/discovering-geology/earth-hazards/volcanoes/how-volcanoes-form/)); best-fit analytic form is a log/exponential/polynomial cone, not a straight cone ([Cosburn & Roy, 2020](https://www.sciencedirect.com/science/article/abs/pii/S0377027320303991)) | Fuji: 3.5 km³/kyr DRE during cone-building, 0.8–2.0 km³/kyr later ([Yasuda/Takada et al., 2021, EPS](https://link.springer.com/article/10.1186/s40623-021-01505-1)) | 10⁵–10⁶ yr; summit caldera collapse and flank failures punctuate growth |
| Caldera systems | collapse depressions 10–100 km across (Yellowstone 55×72 km — standard figure, not verified this pass) | low rim, resurgent domes | 10³–10⁴ km³ in single 10⁴–10⁶-yr-recurrence events | the magma systems persist 10⁶–10⁷ yr |
| Cinder cone | 100–400 m | ~30–33° (angle of repose of loose scoria; textbook) | one eruption, months–years | single-event landforms |
| Flood basalt (LIP) | **plateaus ~2 km thick**, not cones | ~1–5° regional | Deccan: individual eruptions 50–250 km³/yr sustained for centuries, separated by 3,000–6,000-yr hiatuses ([Self et al., 2022](https://www.annualreviews.org/content/journals/10.1146/annurev-earth-012721-051416)) | 1–2 Myr total province |

**Parent anchors verified:** Mauna Loa "from ocean floor ~9 km total" — **CONFIRMED** (GVP: "rises almost 9 km from the ocean floor"; USGS gives ~17 km to the depressed base — the more spectacular and equally sourced number). **Etna growth rates: NOT INDEPENDENTLY VERIFIED this pass** — the commonly cited time-averaged supply of ~0.5–1 m³/s (~16–30 km³/kyr) for Etna could not be confirmed against a fetched source in this budget; use Fuji's verified 3.5 km³/kyr (or Mauna Loa's ~50 km³/kyr-equivalent shield rate) as the type numbers and treat Etna as unverified.

### 7.2 Lava composition controls shape

Viscosity spans eight orders of magnitude across melt compositions: basalt ~10–10³ Pa·s, andesite ~10³–10⁵, dacite-rhyolite ~10⁵–10⁸ (standard volcanology ranges; textbook). Low-viscosity basalt flows kilometers from the vent → broad convex **shields** with slopes <10°; viscous andesite-dacite piles at the vent → concave **stratocones** at 30–35°; very viscous rhyolite barely flows → **domes** ([BGS](https://www.bgs.ac.uk/discovering-geology/earth-hazards/volcanoes/how-volcanoes-form/)). Important caveat from morphology studies: composition is a proxy, not the cause — eruptive conditions (effusion rate, viscosity, volume) are the primary determinants, and basaltic stratocones (Fuji, Mayon, Izalco) as well as trachytic/phonolitic shields exist ([Pasin/Greeley, 1988](https://ntrs.nasa.gov/citations/19880021104)). Water content drives explosivity independently of SiO₂ (it was rising water, not changing composition, that flipped Fuji from effusive to explosive; [Takada et al., 2021](https://link.springer.com/article/10.1186/s40623-021-01505-1)).

### 7.3 Flood basalts and the terrain they leave

**Deccan Traps:** erupted 67.5–64.5 Ma (main pulses ~67 and 65.5–65 Ma, mostly within reversed chron 29r, ~800 kyr); present extent ~500,000 km² of stacked flows **>2 km thick**; preserved volume ~750,000 km³ (older estimate ~500,000; Wikipedia's ~1M), original eruptive volume **~1.3 × 10⁶ km³** ([Jay & Widdowson, 2008](https://doi.org/10.1144/0016-76492006-062); [Self, LIP of the Month](http://www.largeigneousprovinces.org/12may)). Individual pāhoehoe sheet-lobe flow fields reached ~1,000 km from vent (the Rajahmundry Traps on India's east coast) with volumes of order 7,000–10,000 km³ per eruption ([Self, LIP of the Month](http://www.largeigneousprovinces.org/12may)). **Terrain left:** a stepped lava plateau — flat flow tops, steep escarpments at flow margins and along the rifted western coast (Western Ghats), decaying by steps as erosion strips flow after flow. **Siberian Traps:** ~252 Ma, end-Permian; volume commonly cited ~1–5 × 10⁶ km³ including intrusive sills — **flagged: exact volume not independently verified this pass** (the [Burgess & Black, 2025 review](https://www.annualreviews.org/content/journals/10.1146/annurev-earth-040722-105544) is the identified source but its volume table was not retrieved in budget).

---

## 8. Hotspots, mid-ocean ridges and ocean-island terrain

### 8.1 Hawaii's age progression and what plate motion it records

The Hawaiian-Emperor chain is the canonical hotspot track: seamounts age from 0 Ma at Hawai'i to ~85 Ma at the north end of the Emperor chain ([Zhu et al., 2024](https://link.springer.com/article/10.1038/s41467-024-51055-9)). Measured propagation: a *constant* 57 ± 2 km/Myr between ~57 and 25 Ma, then faster since ~15 Ma ([O'Connor et al., 2013](https://doi.org/10.1002/ggge.20267)).

**Parent anchor "~10 cm/yr": PARTIALLY CONFIRMED — report both numbers.** For the last ~15 Myr the Hawaiian ridge records ~8–10 cm/yr of effective plate-over-hotspot motion (consistent with the Pacific plate's modern absolute motion of ~7–10 cm/yr); but over 57–25 Ma the rate was 5.7 ± 0.2 cm/yr ([O'Connor et al., 2013](https://doi.org/10.1002/ggge.20267)). And the hotspot itself is *not* fixed: paleomagnetism of Emperor seamounts requires 4–9° of southward plume drift between 80 and 47 Ma (0.2–0.4°/Myr; [Torsvik et al., 2017](https://www.nature.com/articles/ncomms15660)), with the fastest published estimate 47.8 ± 15.3 mm/yr of southward drift during 63–52 Ma ([Tarduno et al., 2003](https://doi.org/10.1126/science.1086442); [Bono et al., 2019](https://doi.org/10.1038/s41467-019-11314-6)). The 60° Hawaiian-Emperor Bend (~47 Ma) is therefore *both* a plate-motion change and hotspot motion — modern consensus weights the plate-motion change as necessary ([Torsvik et al., 2017](https://www.nature.com/articles/ncomms15660)) while the plume-lithosphere interaction school shows shallow effects (ridge attraction, plate drag on the plume) can account for ~50 % of the paleolatitude signal ([Zhu et al., 2024](https://link.springer.com/article/10.1038/s41467-024-51055-9)). **Simulation consequence:** hotspot tracks are *not* perfect plate-speed tape measures; a game can treat them as such, but drift is real.

### 8.2 Seamount → guyot → atoll: Darwin's sequence

A volcano rides the plate off the hotspot and sinks on thermally subsiding lithosphere (depth ≈ 2500 + 350√(age in Myr) m — the standard cooling-plate relation; textbook). In the tropics, coral grows upward at rates that can match subsidence while the volcano is a shallow bank; the result is Darwin's (1842) sequence: **volcanic island → fringing reef → barrier reef (lagoon widens as the island drowns) → atoll** (ring reef, no island). Where coral cannot keep up or the plate is too far poleward, the flat-topped wave-planed seamount remains as a **guyot**. Drilling at Enewetak atoll penetrated ~1,200+ m of carbonate before basalt — the classical confirmation (textbook; not re-verified this pass). Atoll carbonate thicknesses of order 1–1.5 km are therefore the *terrain* hotspot chains leave in the tropics; elsewhere they leave guyot fields and linear ridges (e.g., the Emperor chain, the Louisville trail).

### 8.3 Mid-ocean ridges: fast vs slow spreading

Oceanic lithosphere is created along a ~65,000 km global ridge network (standard figure; textbook). The load-bearing distinction for terrain:

- **Fast-spreading ridges** (full rate >~8–10 cm/yr; East Pacific Rise): an axial *high* — a broad smooth swell a few hundred m above the flanks, no rift.
- **Slow-spreading ridges** (full rate <~4 cm/yr; Mid-Atlantic Ridge ~2–3 cm/yr full rate): a true **axial rift valley**, ~1–2 km deep and 20–40 km wide, with rugged faulted inner walls — because the plate is mechanically strong enough at the slow spreading rate to support axial stretching before magmatic fill (elastic-plate modeling of axial topography; standard — flagged: figures from textbook literature, not independently verified this pass).

At the slowest end (ultraslow, Southwest Indian Ridge, <2 cm/yr) magmatism becomes discontinuous along axis and the terrain is amagmatic fault blocks. The axial rift valley is the ocean's own Basin-and-Range — the same normal-fault grammar as §5, submerged. *Ridge crest elevation* is Pratt isostasy in action: hot thin lithosphere stands 2–3 km high at the axis and subsides symmetrically with √age (§8.2).

### 8.4 Ophiolites — oceanic terrain stranded on continents

Occasionally a slice of oceanic lithosphere is shoved onto a continental margin instead of subducting (**obduction**). The resulting **ophiolite** exposes, bottom to top: mantle peridotite (serpentinized), layered gabbros, sheeted vertical dike swarms, pillow basalt, and ribbon chert/red deep-sea sediment — a complete ~5–7 km oceanic crustal section you can walk through (type examples: Semail/Oman, Troodos/Cyprus, Bay of Islands/Newfoundland — standard textbook). For a voxel game, ophiolite terrain is the rare place where "seafloor rocks in the mountains" is literally correct, with distinctive terrain consequences: serpentinite soils (toxic to many plants → sparse vegetation bands), red jasper and greenstone ridges, and peridotite scarps.

---

## 9. Deep time — ~4 billion years of terrain

### 9.1 First cratons, and why they are flat

- **Oldest preserved crust:** the Acasta Gneiss Complex, Canada, ~4.0 Ga; Hadean detrital zircons to ~4.4 Ga (Jack Hills, Yilgarn) prove still-older felsic crust that was destroyed ([Cawood et al., 2022, Secular Evolution of Continents and the Earth System](https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029%2F2022RG000789); [Petersson et al., 2023? — see crustal-rejuvenation source](https://www.nature.com/articles/s41467-021-23805-6)).
- **First craton formation at scale: ~3.8–3.2 Ga; stabilization 3.2–2.5 Ga** (the "Primitive" and "Juvenile Earth" phases of Cawood et al.'s seven-phase synthesis; [Cawood et al., 2022](https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029%2F2022RG000789)). **Parent anchor "~3.5–4.0 Ga first cratons": CONFIRMED as a defensible range** — with the nuance that individual cratonic nuclei stabilized diachronously over ≥250 Myr (3.85–3.6 Ga switch points in Yilgarn, Slave, Wyoming, Singhbhum), and that experiments on Eoarchaean TTG magmas require deep (>50 km) subduction-like environments by ~4.0–3.6 Ga ([Reimink et al., 2023? — see deep-formation source](https://preview-www.nature.com/articles/s41561-023-01249-5)).
- **Why cratons are flat and persist:** they are underpinned by 150–250 km-thick buoyant, dehydrated, melt-depleted mantle keels — low-density enough to float despite being cold, strong enough to resist convective destruction; cratons comprise **>60 % of the continental landmass** ([Lee et al./Pearson, 2021, Deep continental roots and cratons](https://www.nature.com/articles/s41586-021-03600-5)). Their surfaces have "generally maintained positions at or near sea-level for long periods," evidenced by sub-horizontal unconformable sedimentary veneers as old as late Archean ([Cawood et al., 2022](https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029%2F2022RG000789)). No keel, no craton: some (North China, Wyoming) had their roots removed later and deformed accordingly.

### 9.2 The supercontinent cycle

Assembly and breakup recur on a **~500 Myr** cycle ([Nance & Murphy, retrospective](https://pmc.ncbi.nlm.nih.gov/articles/PMC9796656/)):

| Supercontinent | Assembled | Broke up |
|---|---|---|
| Columbia/Nuna | ~1.8–1.5 Ga | ~1.4–1.3 Ga |
| **Rodinia** | ~1.3–0.9 Ga (Grenville orogens) | ~750–600 Ma |
| **Gondwana** | ~570–530 Ma (Pan-African orogens) | merged into Pangaea |
| **Pangaea** | ~350–250 Ma (Variscan/Alleghanian) | ~200–175 Ma (Central Atlantic opens) |

**What the land surface looked like at each:**

- **Rodinia (~900 Ma):** a single landmass with interior deserts and a supercontinent-scale orogenic spine (Grenville). Its breakup prefaced Snowball Earth glaciations and the Great Unconformity (below). Soils were purely microbial — no plants, no animals; rivers were wide braided sheet-flow systems on barren plains (pre-vegetation fluvial style; standard geological interpretation).
- **Gondwana (~550–300 Ma):** the Pan-African orogens (uplifted belt through what is now Antarctica-India-Australia-Africa-South America) were the "Himalayas of their day," largely erased since. Vegetation arrives here: first land plants ~470 Ma, forests ~385 Ma (Devonian) — from this point soils deepen, meandering rivers appear, and hillslopes stabilize (the sibling agents cover the fluvial and vegetation consequences).
- **Pangaea (~250 Ma):** the extreme case — one landmass, one ocean, continental-interior climates of unmatched severity (megamonsoon), vast interior deserts, and assembly orogens (Appalachians/Variscan/Urals) that have since been eroded to stumps.
- **"Mountains as we know them"** — glacially sculpted, forested, landslide-regulated ranges — require all of: modern plate tectonics (by ~2.5–3 Ga at the latest; transitional before), land vegetation (Devonian, ~390 Ma), and late-Cenozoic cooling with its glaciations (<~35 Ma). The Himalaya as a *landscape* is younger than its collision.

### 9.3 The Great Unconformity

A globally traceable stratigraphic surface separating Precambrian crystalline basement from much younger (mostly Cambrian) shallow-marine sediments — named at the Grand Canyon, where ~525 Ma Tapeats Sandstone sits on 1,740 Ma Vishnu Schist with ~1.2 *billion* years missing ([Peters & Gaines, 2012](https://doi.org/10.1038/nature10969)). Its formation involved a protracted period of widespread continental denudation: Neoproterozoic glacial erosion averaging **3–5 vertical km** globally, per the leading model ([Keller et al., 2019, PNAS](https://www.pnas.org/doi/abs/10.1073/pnas.1804350116)), with thermochronologic support for 3–5 km of Cryogenian unroofing across North America ([Flowers et al., 2022, PNAS](https://www.pnas.org/doi/10.1073/pnas.2118682119)) — though a tectonic (diachronous) origin has serious advocates. Across the boundary, preserved sediment abundance per unit time jumps **fivefold** (Ronov, cited in [Flowers et al., 2022](https://www.pnas.org/doi/10.1073/pnas.2118682119)). The freshly exposed basement then weathered at >3× the rate of soil-mantled rock, fertilizing the oceans and plausibly triggering biomineralization and the Cambrian explosion ([Peters & Gaines, 2012](https://doi.org/10.1038/nature10969)).

### 9.4 How much has erosion erased?

Essentially all of it, repeatedly. The mean age of continental surface is tiny compared to the crust's age: orogens are reduced to subdued relief in 10⁷–10⁸ yr (Part 2 has the landscape-response timescales), and the Great Unconformity alone removed kilometers of crust globally. **What fraction of Earth's continental surface is Precambrian shield vs younger: no clean verified number this pass.** The solid anchors I can cite: cratons (shields + their sediment-veneered platforms) comprise **>60 % of the continental landmass** ([Pearson et al., 2021](https://www.nature.com/articles/s41586-021-03600-5)), and *exposed* Precambrian shield is much less — classic compilations put shields proper at ~10–15 % of land area (textbook; flagged unverified). The honest simulation takeaway: the present surface is a mosaic of a few ancient flat platforms, a residue of eroded Paleozoic orogens, and young belts — with the *dominant* exposed terrain being young, because orogens are where erosion is slow enough to preserve relief and platforms are where it is flat.

---

## 10. Practical simulation synthesis

Opinionated. Ranked by terrain-generation value per engineering effort:

### 10.1 What actually matters

**1. Linear belt generator with isostatic roots (build this first).** 80 % of Earth's dramatic terrain lives on convergent-boundary curves. Implementation: draw boundary curves on the sphere (small-circle chains — which §6.1 says is *literally* the correct geometry), then deform a band around each curve. Parameters with real ranges:
- Belt width: fold-thrust belt 100–300 km; full orogen with plateau 500–1,000 km (Himalaya + Tibet).
- Mean elevation 2–5 km; peaks to 8–9 km *only* where riding a plateau (§3). Do not simulate the root — enforce the Airy *consequence* instead: clamp peak heights to ~1.5 km above the regional snowline in glaciated belts (the buzzsaw envelope; [Egholm et al., 2009](https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf)) and to the plateau-pedestal sum elsewhere.
- Uplift rates: 0.1–1 mm/yr for mature belts, 1–5 mm/yr for active collision, up to ~10 mm/yr short-lived pulses. Convergence-driven shortening: 1–5 cm/yr plate scale, of which 20 mm/yr in the Himalaya is the modern maximum for a continental belt (§4.1).
- Ramp-flat thrust grammar: a décollement with 50–100 km-spaced ramps generates anticline ranges for free (§4.4).

**2. Uplift-vs-erosion balance (couple to Part 2's stream-power solver).** Relief is the integral of (uplift − erosion). With uplift 1–5 mm/yr and erosion responding to slope and drainage area, a belt reaches steady state in a few Myr. The one number from this Part that the erosion solver needs is the isostatic feedback: surface lowering = 0.18 × eroded thickness (§3.5) — without it, mountains vanish ~5× too fast.

**3. Plateau mechanism.** Continental collision that persists >10 Myr should produce a flat 4–5 km plateau (Tibet 5 km, Altiplano 3.7 km) between the belt and the interior — strength-limited, not height-limited. Cheap implementation: when cumulative shortening exceeds ~300–500 km, switch the interior from "fold-thrust" mode to "plateau" mode (flat top, 1–3 km escarpmented margins).

**4. Arc-island generator.** Curved chains (§6.1): volcanic front 180–275 km from the trench (median; 85–470 km full range, set by slab dip; [Syracuse & Abers, 2006](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2005GC001045)), stratovolcanoes at 30–35° slopes spaced ~20–50 km along the front (observed arc-volcano spacing; standard compilation — flagged approximate), heights 1–4 km, plus a 1–5 km-deep forearc basin and an accretionary wedge of a few degrees' taper. Trench 6–11 km deep (10.9–11.0 km floor value for the extreme case; §6.2).

**5. Rift-valley generator.** Half-graben strings (§5.3): border faults 60–120 km long, 40–75 km-wide extensional zones, basins 2–7 km deep (fill), flanks +0.4–2 km tilted away from the axis, extension rates 1–6 mm/yr, and — if the game spans enough time — progression to narrow ocean with passive margins. Basin-and-Range mode = the same grammar with ~50 % total extension producing range-and-basin fabric at 30–50 km wavelength.

### 10.2 What can safely be faked

- **The Moho and the root.** Never simulate 70 km of crustal thickness; use it only as an internal justification for the height clamp.
- **Flexure.** Only if you want foreland basins and forebulges as visible terrain (a 1-D elastic beam is ~50 lines of code; $T_e$ = 10–20 km young/rifted, 40–100 km cratonic foreland, §2.3). Otherwise skip.
- **Plate speeds.** Tie speeds to slab-pull presence (§1.1) and pick any 1–10 cm/yr value; nobody can falsify your plate circuit.
- **Hotspot fixity.** Perfectly fine to make hotspots fixed; just know the real ones drift (§8.1).
- **Deep time.** A game world does not need Rodinia-to-Pangaea history; it needs the *consequences*: old flat cratonic cores, a few young sharp belts, drowned continental margins, and one Great-Unconformity-style basement surface somewhere for flavor.

### 10.3 What NOT to fake

- **Belt linearity and its curvature statistics** — this is the single most conspicuous real-Earth property that naive noise terrain lacks (§1.3, §6.1).
- **The 4.5× root-height relation** as the height clamp logic — it's cheap, correct, and prevents 20 km mountains.
- **The mm/yr-scale rate differential** between tectonic uplift (~1–10 mm/yr), erosion (~0.01–10 mm/yr depending on setting; Part 2's domain), and post-glacial rebound (≤10–11 mm/yr transient; §2.4) — any gameplay timescale that shows terrain change should respect these relative speeds.

---

## Provenance

**Verified against fetched sources (high confidence):** slab-pull dominance ranking (four independent studies, §1.1); plate speed range 1–10 cm/yr and fastest-plate identities (§1.2); Airy 4.5× root derivation and Everest 39.8 km arithmetic (§2.1 — exact match to parent); post-glacial rebound ~10 mm/yr max at both Hudson Bay and Fennoscandia (§2.4); glacial-buzzsaw summit envelope ~1,500 m above snowline (§3); Himalayan shortening (~670 km FTB minimum; ~1,000 km total) and 19–21 mm/yr shortening-rate agreement (§4.1); Tibet Moho 65–90 km, typical 70–80 (§4.2); Greater Himalaya exhumation 50–90 °C/Myr ≈ 1.7–3 mm/yr (§4.3); Basin-and-Range 46 ± 8 % to >100 % extension, 17–16 Ma onset, core-complex rates to 7–9 km/Myr (§5.2); East African rift geometry, 2→4 mm/yr opening, flank elevations (§5.3); arc-curvature mechanisms (§6.1); Mariana 10,935 ± 6 m (§6.2); Mauna Loa ~9 km from seafloor, 75,000 km³, growth rates (§7.1); stratovolcano slopes 30–35° vs shields <10° (§7.2); Deccan 1.3 × 10⁶ km³ original, 2 km thick, 50–250 km³/yr pulses (§7.3); Hawaiian propagation 5.7 cm/yr then ~8–10 cm/yr, hotspot drift 4–9° (§8.1); craton ages and keel thicknesses, >60 % of landmass (§9.1); supercontinent timings (§9.2); Great Unconformity 3–5 km glacial erosion (§9.3).

**Disagreements with parent anchors (reported, not silently adopted):**
1. **Arc-trench gap "~100–150 km" — DISAGREES.** Global compilation: median 180–275 km (middle 50 %), full range 85–470 km ([Syracuse & Abers, 2006](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2005GC001045)). The ~105–110 km figure is the slab *depth* beneath arcs, not the horizontal distance. §6.2 reports both.
2. **Hawaii "~10 cm/yr" — PARTIAL.** Correct for the last ~15 Myr; 5.7 ± 0.2 cm/yr from 57–25 Ma; plus the hotspot itself drifted (§8.1).
3. **Arc curvature "sphere + rollback" — PARTIAL.** Sphere geometry confirmed (Frank 1968; Bevis 2010), but slab rollback controls trench migration/backarc opening rather than plan-view curvature; convergence rate correlates with curvature, slab dip does not (§6.1).
4. **Isostasy anchors 4.5×, 39.8 km — CONFIRMED EXACTLY** (with the density-sensitivity caveat that ρc = 2800 gives 5.6×).
5. **Moho Tibet ~70–80 km, rebound mm/yr, Mariana ~11 km, Mauna Loa ~9 km, exhumation mm/yr, cratons ~3.5–4.0 Ga, supercontinent timings — all CONFIRMED** within stated ranges.

**Could not verify this pass (explicitly flagged in text):** Etna growth-rate figures (§7.1); Siberian Traps exact volume (§7.3 — review identified, table not retrieved); fast/slow ridge rift-valley dimensions (§8.3 — standard textbook figures); Enewetak carbonate thickness (§8.2); Coulomb-wedge taper values (§4.4); San Andreas total offset (§6.3); the exposed-shield fraction of continental area (§9.4 — only the >60 % craton-lithosphere figure is sourced); Yellowstone caldera dimensions (§7.1); young-arc volcano along-strike spacing (§10.1, approximate).

---

## Sources

1. Forsyth, D. & Uyeda, S. (1975). On the Relative Importance of the Driving Forces of Plate Motion. — https://doi.org/10.1111/j.1365-246x.1975.tb00631.x
2. Lithgow-Bertelloni, C. & Richards, M. (1995). Cenozoic Plate Driving Forces. — https://discovery.ucl.ac.uk/id/eprint/94137/1/95GL01325.pdf
3. Conrad, C. (2004). The temporal evolution of plate driving forces. — https://www.clintconrad.no/papers/Conrad_JGR2004.pdf
4. Suchoy, L. et al. (2021). Effects of basal drag on subduction dynamics from 2D numerical models. — https://doi.org/10.5194/se-12-79-2021
5. DeMets, C., Gordon, R., Argus, D. & Stein, S. (1990). Current plate motions (NUVEL-1). — https://doi.org/10.1111/j.1365-246x.1990.tb06579.x
6. Sella, G., Dixon, T. & Mao, A. (2002). REVEL: A Model for Recent Plate Velocities from Space Geodesy. — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1467&context=geo_facpub
7. DeMets, C., Gordon, R. & Argus, D. (2010). Geologically current plate motions (MORVEL). — https://academic.oup.com/gji/article/181/1/1/713644
8. Larson, K., Freymueller, J. & Philipsen, S. (1997). Global plate velocities from the GPS. — https://doi.org/10.1029/97jb00514
9. Jarrín, P. et al. (2022). Current motion and deformation of the Nazca Plate. — https://doi.org/10.1093/gji/ggac353
10. Sella, G. et al. (2007). Observation of glacial isostatic adjustment in "stable" North America with GPS. — https://doi.org/10.1029/2006gl027081
11. Olsson, P.-A. et al. (2019). NKG2016LU: a new land uplift model for Fennoscandia and the Baltic Region. — https://link.springer.com/article/10.1007/s00190-019-01280-8
12. Johansson, J. et al. (2002). Continuous GPS measurements of postglacial adjustment in Fennoscandia (BIFROST). — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2001JB000400
13. Watts, A.B. (1992). The effective elastic thickness of the lithosphere and the evolution of foreland basins. — https://doi.org/10.1111/j.1365-2117.1992.tb00043.x
14. Jordan, T.A. & Watts, A.B. (2005). Gravity anomalies, flexure and the elastic thickness structure of the India–Eurasia collisional system. — https://www.sciencedirect.com/science/article/abs/pii/S0012821X05003390
15. Hetényi, R. et al. (2014). Flexure of the India plate underneath the Bhutan Himalaya. — https://agupubs.onlinelibrary.wiley.com/doi/10.1002/grl.50793
16. Flexural Modeling of the Himalayan Foreland Basin (thesis). — https://uh-ir.tdl.org/items/a874b0ff-529d-4343-b387-5014ead41d2c
17. Egholm, D. et al. (2009). Glacial effects limiting mountain height. — https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf
18. Herman, F. et al. (2021). The impact of glaciers on mountain erosion. Nature Reviews Earth & Environment. — https://preview-www.nature.com/articles/s43017-021-00165-9
19. Mitchell, S.G. & Montgomery, D.R. (2006). Influence of a glacial buzzsaw on the height and morphology of the Cascade Range. — https://doi.org/10.1016/j.yqres.2005.08.018
20. Glacial and periglacial buzzsaws: fitting mechanisms to metaphors. Quaternary Research. — https://www.cambridge.org/core/journals/quaternary-research/article/abs/glacial-and-periglacial-buzzsaws-fitting-mechanisms-to-metaphors/4828F3C917E238FD0405E6A0B9AF51CF
21. DeCelles, P., Robinson, D. & Zandt, G. (2002). Implications of shortening in the Himalayan fold-thrust belt for uplift of the Tibetan Plateau. — https://doi.org/10.1029/2001tc001322
22. Gao, R. et al. (2016). Nonuniform subduction of the Indian crust beneath the Himalayas. — https://pmc.ncbi.nlm.nih.gov/articles/PMC5624955/
23. Searle, M. et al. (2011). The structure and evolution of the Tibetan Plateau (J. Geol. Soc. London). — https://www.ccsp.ox.ac.uk/sites/default/files/ccsp/documents/media/searle_et_al_2011-tibet.pdf
24. Long, S. et al. (2012). Variable exhumation rates and variable displacement rates: western Bhutan. — https://www.sciencedirect.com/science/article/abs/pii/S0012821X13006213
25. Deep Structure of the Eastern Himalayan Collision Zone (2019). — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2019TC005483
26. Ebinger, C. (1988). Thermal and mechanical development of the East African Rift System. — https://doi.org/10.1575/1912/4624
27. Koptev, A. et al. (2015). Contrasted continental rifting via plume-craton interaction. — https://doi.org/10.1016/j.gsf.2015.11.002
28. Mid-Pleistocene to Recent Crustal Extension in the Inner Graben of the Northern Kenya Rift (2022). — https://doi.org/10.1029/2021GC010123
29. Hamilton, W. (1987). Crustal extension in the Basin and Range Province. — https://doi.org/10.1144/gsl.sp.1987.028.01.12
30. Long, S. (2019). Geometry and magnitude of extension in the Basin and Range. — http://www.seanpatricklong.com/uploads/1/4/5/6/14564072/long_2019_gsab_basin_range_xsec.pdf
31. Long, S. et al. (2018). Rapid Oligocene to Early Miocene Extension Along the Grant Range Detachment System. — https://doi.org/10.1029/2018tc005073
32. Rehrig, W. (1986). Processes of regional Tertiary extension in the western Cordillera. — https://doi.org/10.1130/spe208-p97
33. Camp, V., Pierce, K. & Morgan, L. (2015). Yellowstone plume trigger for Basin and Range extension. — https://doi.org/10.1130/ges01051.1
34. Bevis, M. (2010). Why subduction zones are curved. — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2010TC002720
35. Tovish, A. & Schubert, G. (1978). Island arc curvature, velocity of convergence and angle of subduction. — https://doi.org/10.1029/gl005i005p00329
36. Oblique plate convergence along arcuate trenches on a spherical Earth: the Western Sunda Arc (2023). — https://link.springer.com/article/10.1007/s11600-023-01163-9
37. Syracuse, E.M. & Abers, G.A. (2006). Global compilation of variations in slab depth beneath arc volcanoes. — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2005GC001045
38. England, P. & Katz, R. (2010). Melting above the anhydrous solidus controls the location of volcanic arcs. — https://preview-www.nature.com/articles/nature09417
39. Grove, T. et al. (2009). Kinematic variables and water transport control the formation and location of arc volcanoes. — https://preview-www.nature.com/articles/nature08044
40. Arcay, D. (2020). Melt Focusing Along Permeability Barriers at Subduction Zones. — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2020GC009253
41. Ha, G., Montési, L. & Zhu, W. (2023). Geometrical Relations Between Slab Dip and the Location of Volcanic Arcs and Back-Arc Spreading Centers. — https://doi.org/10.1029/2023gc010997
42. Britannica: Plate tectonics — Island arcs. — https://www.britannica.com/science/plate-tectonics/Island-arcs
43. Stewart, H. & Jamieson, A. (2021). Revised depth of the Challenger Deep from submersible transects. — https://www.sciencedirect.com/science/article/pii/S0967063721001813
44. Challenger Deep (depth compilation). — https://en.wikipedia.org/wiki/Challenger_Deep
45. Recognizing the San Andreas Fault: Landforms, Troughs & Sag Ponds. — https://digital-desert.baremetal.com/san-andreas-fault/san-andreas-03.html
46. USGS. Geology and History of Mauna Loa. — https://www.usgs.gov/volcanoes/mauna-loa/science/geology-and-history-mauna-loa
47. USGS. Mauna Loa volcano overview. — https://www.usgs.gov/volcanoes/mauna-loa
48. Smithsonian GVP: Mauna Loa (332020). — https://volcano.si.edu/volcano.cfm?vn=332020
49. Rowland, S. & Garbeil, H. (2000). Slopes of Oceanic Basalt Volcanoes. — https://www.soest.hawaii.edu/earthsciences_archive/FACULTY/ROWLAND/pdfs/Rowland_Garbeil_2000.pdf
50. Decker, R. (ed.) (1987). USGS Professional Paper 1350, ch. 18 (Mauna Loa). — https://pubs.usgs.gov/pp/1987/1350/pdf/chapters/pp1350_ch18.pdf
51. BGS. Types of volcano. — https://www.bgs.ac.uk/discovering-geology/earth-hazards/volcanoes/how-volcanoes-form/
52. Pasin, S. & Greeley, R. (1988). Eruptive viscosity and volcano morphology. — https://ntrs.nasa.gov/citations/19880021104
53. Takada, A. et al. (2021). Temporal variations of magma composition, eruption style and rate at Fuji Volcano. — https://link.springer.com/article/10.1186/s40623-021-01505-1
54. Cosburn, M. & Roy, D. (2020). Analysing the topographic form of stratovolcanoes. — https://www.sciencedirect.com/science/article/abs/pii/S0377027320303991
55. Self, S. (2012). LIP of the Month: Deccan Volcanic Province. — http://www.largeigneousprovinces.org/12may
56. Jay, A. & Widdowson, M. (2008). Stratigraphy, structure and volcanology of the SE Deccan. — https://doi.org/10.1144/0016-76492006-062
57. Self, S. et al. (2022). Toward Understanding Deccan Volcanism. Annual Review of Earth and Planetary Sciences. — https://www.annualreviews.org/content/journals/10.1146/annurev-earth-012721-051416
58. Burgess, S. & Black, B. (2025). The Anatomy and Lethality of the Siberian Traps LIP. Annual Review of Earth and Planetary Sciences. — https://www.annualreviews.org/content/journals/10.1146/annurev-earth-040722-105544
59. Deccan Traps (area/volume summary). — https://en.wikipedia.org/wiki/Deccan_traps
60. Torsvik, T. et al. (2017). Pacific plate motion change caused the Hawaiian-Emperor Bend. — https://www.nature.com/articles/ncomms15660
61. Tarduno, J. et al. (2003). The Emperor Seamounts: Southward Motion of the Hawaiian Hotspot Plume. — https://doi.org/10.1126/science.1086442
62. Bono, R., Tarduno, J. & Bunge, H.-P. (2019). Hotspot motion caused the Hawaiian-Emperor Bend and LLSVPs are not fixed. — https://doi.org/10.1038/s41467-019-11314-6
63. O'Connor, J. et al. (2013). Constraints on past plate and mantle motion from new ages for the Hawaiian-Emperor Seamount Chain. — https://doi.org/10.1002/ggge.20267
64. The role of plume-lithosphere interaction in Hawaii-Emperor chain formation (2024). — https://link.springer.com/article/10.1038/s41467-024-51055-9
65. Pearson, D.G. et al. (2021). Deep continental roots and cratons. — https://www.nature.com/articles/s41586-021-03600-5
66. Crustal rejuvenation stabilised Earth's first cratons (2021). — https://www.nature.com/articles/s41467-021-23805-6
67. Deep formation of Earth's earliest continental crust consistent with subduction (2023). — https://preview-www.nature.com/articles/s41561-023-01249-5
68. Cawood, P. et al. (2022). Secular Evolution of Continents and the Earth System. Reviews of Geophysics. — https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029%2F2022RG000789
69. Nance, R.D. et al. (2022). The supercontinent cycle and Earth's long-term climate. — https://pmc.ncbi.nlm.nih.gov/articles/PMC9796656/
70. Peters, S. & Gaines, R. (2012). Formation of the 'Great Unconformity' as a trigger for the Cambrian explosion. — https://doi.org/10.1038/nature10969
71. Keller, C.B. et al. (2019). Neoproterozoic glacial origin of the Great Unconformity. PNAS. — https://www.pnas.org/doi/abs/10.1073/pnas.1804350116
72. Flowers, R. et al. (2022). Thermochronologic constraints on the origin of the Great Unconformity. PNAS. — https://www.pnas.org/doi/10.1073/pnas.2118682119

---

## Appendix A — Forty further questions

Beyond the parent's 14 (§A of the question bank). Each answered in one line or explicitly dispositioned.

1. **How thick is oceanic crust, and why is it so uniform?** ~7 ± 1 km (6–8), set by the mantle melting column at ridges; textbook figure, not re-verified this pass.
2. **Why are there no Andes-style mountains on the Atlantic margins?** No subduction — passive margins subside and bury; the *ancient* Appalachian belt shows what happens when subduction stops (erosion wins).
3. **What is the average continental crustal thickness?** ~35–40 km (vs ~7 km oceanic); standard.
4. **How fast do accretionary prisms grow?** By offscraping the incoming sediment column (100s of m); rates set by convergence × incoming sediment thickness — per-surface numbers not verified this pass.
5. **What is a suture zone and what terrain does it leave?** The collapsed remnant of an ocean (ophiolite slivers, mélange, often a topographic low); textbook.
6. **Why do some subduction zones erode the overriding plate instead of accreting sediment?** Low sediment supply + rough/hydrated plate → basal and frontal erosion (e.g., Tonga, Marianas); standard.
7. **What are blueschists and why do they matter for terrain?** High-pressure/low-temperature rocks of subducted slabs returned by exhumation — evidence of cold subduction; standard.
8. **How high was the "Nevadaplano"?** 2.5–3.5 km paleoelevation before Basin-and-Range collapse (§5.2, verified).
9. **Why does the Tibetan plateau have N-S grabens at its top?** East-west extension of the collapsing/flowing plateau (verified in DeCelles 2002 discussion).
10. **What sets the width of a collisional plateau?** The extent of underthrust lower crust and the rheological limit of thickening; Tibet ~1,300 km N-S.
11. **Are there mountains on Venus and why are they different?** No plate tectonics; belts are Corona/belt-like deformation belts — outside this document's verified scope; dropped.
12. **Why is most of Iceland above sea level despite being a ridge?** A mantle plume superimposed on the ridge pushes the ridge axis above sea level (hotter, thicker crust ~15–20 km); standard, not independently verified.
13. **What is dynamic topography and how big is it?** Vertical motion from mantle flow, amplitudes ±1–2 km, wavelengths ~10³ km — the East African plateau is partly it (§5.3); numbers approximate.
14. **How do transform margins form passive-margin pairs?** Pull-apart → breach → spreading center steps; standard.
15. **Why is the Western Ghats escarpment where it is?** Rift-flank uplift + erosion of the Deccan edge at India's separation from Seychelles; standard interpretation.
16. **What controls stratovolcano spacing along an arc?** Melt-focusing wavelength and crustal stress state; ~20–50 km typical; approximate.
17. **How often do caldera-forming eruptions recur at a large system?** ~10⁵–10⁶ yr for 10²–10³ km³ events; textbook, flagged.
18. **What is a resurgent dome?** Uplift of the caldera floor after collapse (Yellowstone, Valles); standard.
19. **How do lava tubes change flow reach?** Insulated transport extends flows 10s of km; Part 2/Agent-4 domain (lava tubes are in the caves section).
20. **Why do Hawaiian volcanoes die from NW to SE?** Plate motion off the plume + declining supply, then subsidence (§8.2 verified).
21. **What is the Loihi stage of Hawaiian volcanism?** Deep-submarine pre-shield stage, ~1 km below sea level, youngest SE of Hawaii; standard.
22. **How fast do ocean island volcanoes slump?** Giant flank failures (100s of km³, 80–100 km runout) recur ~10⁵ yr (Alika slides; §7.1 USGS source verified the deposits).
23. **What are seamount guyot flatness tolerances?** Wave-planed at sea level then subsided; flat within 10s of m; standard.
24. **What did Earth's first mountains look like?** Likely small, greenstone-granite collision belts by ~3 Ga — no modern analog verified; genuinely uncertain in the literature (§9.1 sources).
25. **When did continents reach something like modern area?** Progressive; ~2.5 Ga major craton stabilization, ~25–30 % of today's area is a common estimate — unverified, treat as speculative.
26. **How much continental crust is recycled into the mantle?** A minority flux at subduction zones; magnitude debated; dropped as unresolved.
27. **What is a forebulge and can players see one?** The flexural up-bow in front of a loaded plate, 30–50 m amplitude in the Himalayan case (§2.3 verified) — 30 m, so barely.
28. **How does obliquity of convergence change arc terrain?** Partitioned strike-slip + arc-parallel basins (Sumatra fault; §6.1 Sunda source verified the mechanics).
29. **Why does the Altiplano exist between two Cordilleras?** Weak central lithosphere + crustal shortening of the backarc; standard model, numbers not verified.
30. **What happens when a ridge subducts?** Gap in the arc, slab window, uplift (e.g., Mendocino/Cascade gap); standard.
31. **What is a slab window's terrain signature?** Regional uplift + adakitic volcanism + gap in the volcanic front; standard.
32. **How long does an orogen's memory last after collision stops?** 10⁷–10⁸ yr to peneplain (Part 2's timescale domain; Appalachians still ~1–2 km after 300 Myr — the isostatic 5.5× factor of §3 explains why).
33. **Why are the Appalachians still mountains?** Isostatic amplification of erosion + resistant crystalline cores; §3.5 logic.
34. **What is the elevation of mid-plate volcanic swells?** ~500–1,500 m (Hawaii swell, Cape Verde); standard.
35. **How do plates start?** Subduction initiation is an unresolved research problem (spontaneous vs forced; Bevis 2010 touches it); dropped as out of scope.
36. **What would a stalled rift leave behind?** Aulacogens — failed arms filled with sediment (e.g., Reelfoot/New Madrid, Benue); standard.
37. **How do intraplate earthquakes deform terrain?** Mostly no permanent relief (elastic rebound); New Madrid/Bhuj exceptions produce minor scarps; standard.
38. **What are kimberlites and their terrain expression?** Explosive craters through cratons (the diamond pipes); pipes are <1 km across — terrain impact negligible except as game flavor.
39. **How does salt tectonics build terrain?** Flowing evaporite diapirs make ridges and minibasins (Persian Gulf margins, 10s–100s of m relief); standard, Part 2-adjacent.
40. **What is the single biggest number to get right in a tectonic terrain generator?** The belt width-to-length ratio: real orogens are 1:10 to 1:30 (belt width vs along-strike length; Himalaya ~1:8 to 1:16, Andes ~1:10 to 1:35) — noise terrain routinely gets this 1:1, and it is the most visible wrongness in procedural worlds.


# Part 2 - Fluvial Erosion and Landscape Evolution: The Mathematical Core

**Scope:** how water carves terrain — the stream-power incision model and its whole family, drainage-network geometry, hillslope transport, landscape evolution models and their response times, the Grand Canyon as the worked case, meandering, long profiles and base level, deltas and sediment cascades, landscape memory, and the practical synthesis for a game terrain pipeline. This is the most mathematical part of the Earth-terrain document. Every formula is either derived (shown) or cited to a specific paper (URL). Measured quantities are ranges with sources; the numeric anchors from the parent brief are independently checked and disagreements are reported, not silently copied.

**How to read this:** Sections 1–2 are the load-bearing physics. Sections 3–4 give the static anatomy (networks, hillslopes). Sections 5–6 give the dynamics (LEMs, response times, the Grand Canyon). Sections 7–10 cover planform, base level, deposition, and memory. Section 11 is the opinionated implementation guide — read it last.

---

## 1. The stream power incision model (SPIM)

### 1.1 From energy to the stream power

A river of density $\rho = 1000$ kg/m³ falling a vertical distance through a reach does work on the bed. Total stream power (power per unit channel length, W/m) is the rate of potential-energy loss:

$$\Omega = \rho g Q S$$

with $Q$ the discharge (m³/s), $S$ the water-surface slope (dimensionless), $g = 9.81$ m/s². Divide by channel width $W$ to get **unit stream power** (W/m²), the quantity the bed actually feels:

$$\omega = \frac{\Omega}{W} = \rho g q S, \qquad q = Q/W \ \text{(unit discharge, m²/s)}$$

Equivalently, $\omega = \tau_b U$ (boundary shear stress × mean velocity), which is the standard derivation route: wide-channel shear stress $\tau_b = \rho g h S$, Chezy/friction closure $U \sim \sqrt{ghS}$, so $\omega \sim \rho g^{3/2} h^{3/2} S^{3/2}$ — this $S^{3/2}$, $h^{3/2}$ scaling is why incision is so violently nonlinear in slope and depth.

### 1.2 The detachment-limited incision law

Assume (i) the river can transport everything supplied to it (detachment-limited), (ii) discharge scales with drainage area, $Q \propto A$, (iii) width scales with discharge, $W \propto Q^{1/2}$ (Section 1.5). Then erosion rate scales as unit stream power, which scales as $A^{1/2}S$ (the classic $m=1/2$, $n=1$ of specific stream power; [Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120), [Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035)). Generalizing the exponents gives the workhorse of quantitative geomorphology:

$$\boxed{E = K A^m S^n}$$

- $E$ — long-term bedrock incision rate (m/yr)
- $A$ — upstream drainage area (m² or km², must match K's calibration)
- $S$ — channel slope
- $K$ — erodibility coefficient (lumps lithology, climate, channel width, unit weight of water)
- $m$, $n$ — positive exponents; theory gives $0 < m < 2$, $0 < n < 4$ ([Goren et al. 2014](https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf)); mechanistic derivations give $n \in [2/3, 7/3]$ depending on process ([Whipple et al. 2000](https://www.eoas.ubc.ca/~mjelline/453website/eosc453/E_prints/newfer06/2004whippleAREPS.pdf))

**The units problem (parent anchor check):** $K$ is *not* dimensionally fixed — its units are $[\text{m}^{1-2m} \cdot \text{yr}^{-1}]$ for the area-in-m² convention (e.g., m$^{0.5}$/yr when $m=0.25$; m$^{0.2}$/yr only if $m=0.4$... exactly one of the parent's guesses). Quoting "K ~ 1e-6 to 1e-5" without $m$, $n$ and the $A$ units is meaningless; the Harel et al. global compilation reports a *normalized* erodibility $K_{ref} = 2.9\times10^{-10} \pm 1.0\times10^{-9}$ m$^{1-2m_{ref}}$/yr at a reference concavity $m/n = 0.5$ — i.e., **K varies over ~9 orders of magnitude** across lithologies/climates ([Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035); [Goren et al. 2014](https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf) assume up to 9 orders). **Worked example (checked):** with $m=0.5, n=1$, $K = 10^{-5}$ m$^{0.5}$/yr (soft rock, area in m²), a mid-basin point with $A = 10^7$ m² (10 km²) and $S = 0.01$ gives $E = 10^{-5} \times 3162 \times 0.01 \approx 3.2\times10^{-4}$ m/yr = 320 m/Myr — a fast, active-orogen rate. With $K=10^{-6}$: ~32 m/Myr, a normal mountain-belt rate. With $K = 10^{-6}$ and a hard-rock $A=10^6$ m², $S=0.05$: 50 m/Myr. So "K~1e-6 to 1e-5" gives incision rates of roughly **10–500 m/Myr across typical mountain $A, S$ — parent's framing is right in magnitude but only once $m=0.5, n=1$, area-in-m² is fixed.**

### 1.3 The steady-state (equilibrium) profile — full derivation

At steady state with spatially uniform uplift $U$, incision balances uplift: $U = K A^m S^n$. Solving for slope:

$$S = \left(\frac{U}{K}\right)^{1/n} A^{-m/n}$$

This is **Flint's law** $S = k_s A^{-\theta}$ with the **concavity index** $\theta = m/n$ and **steepness index** $k_s = (U/K)^{1/n}$. Substitute $S = -dz/dx$ and Hack's law (Section 3) $A = (x/c)^{1/h}$ with $h \approx 0.6$:

$$-\frac{dz}{dx} = \left(\frac{U}{K}\right)^{1/n} \left(\frac{x}{c}\right)^{-\frac{m}{nh}}$$

Integrating from the outlet ($x = x_b$, $z = z_b$) upstream:

$$\boxed{z(x) = z_b + \frac{k_s\, c^{-\theta/h}}{1 - \theta/h} \left[ \left(\frac{x_b}{c}\right)^{1-\theta/h} - \left(\frac{x}{c}\right)^{1-\theta/h} \right]}$$

For typical $\theta/h \approx 0.5/0.6 = 0.83 < 1$ the profile is **concave-up** (slope decreases downstream, i.e. as $x$ grows). In the special case $m/n = h$ the exponent is 1 and the profile is exponential in $x$: $z = z_b + k_s' \ln(x_b/x)$ — the "logarithmic profile" special case. Note this derivation requires uniform $U$ and $K$; spatial gradients in either break the log-linear form and are the basis of the $\chi$ methods in Section 10 ([Willett et al. 2014](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf); [Smith & Fox 2024](https://eprints.bbk.ac.uk/id/eprint/54285/1/Smith_and_Fox_2024_Concavity.pdf)).

### 1.4 Concavity: verified values

- Theory predicts $0.4 < m/n < 0.6$ ([Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120); range 0.35–0.6 in [Goren et al. 2014](https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf)).
- Global compilation, N=1457 basins: **median $\theta = 0.51 \pm 0.14$** — the parent's "~0.4–0.6" is verified ([Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035)).
- But the same compilation finds a **global median slope exponent $n = 2.43 \pm 0.15$ (mean 2.6)**, well above the traditional $n=1$: incision is predominantly threshold-controlled and nonlinear. The common practice of assuming $n=1$ is questionable ([Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035); [Attal 2013](https://onlinelibrary.wiley.com/doi/10.1002/esp.3462) concludes the standard SPIM has "a narrow range of validity").

### 1.5 Channel width hydraulic geometry

Downstream hydraulic geometry ([Leopold & Maddock 1953](https://doi.org/10.5194/esurf-9-379-2021) gives $b \approx 0.5$, $k \approx 0.4$, $m \approx 0.1$, $z \approx -0.4$ for $W \propto Q^b$ etc.): the modern global dataset of Dunne & Jerolmack gives width exponent **$b = 0.512 \pm 0.007$** and depth $0.402 \pm 0.006$ ($R^2 = 0.89$, 0.86) ([Pelletier 2021](https://doi.org/10.5194/esurf-9-379-2021)). So $W \propto Q^{0.5}$ is verified.

**Parent anchor check — $W \approx 3.83\sqrt{Q}$:** This is REAL but is a *regional North American gravel-bed river* relation: **Bray (1973, 1982), Alberta single-thread gravel rivers: $W = 3.83\, Q_b^{0.53}$ (SI units, m and m³/s)**, from the classic regime-equation compilations ([NRCS Part 654 Ch. 9, Table 9-1](https://irrigationtoolbox.com/NEH/Part%20654/CHAPTERS/Chapter-09.pdf); also quoted in [Julien & Wargadalam 1995](https://doi.org/10.1061/(asce)0733-9429(1995)121:4(312))). The scatter and regional dependence are large: the same table gives **Nixon (1959, UK): $W = 2.99 Q^{0.5}$**; generalized North American gravel-bed rivers: **$W = 3.68 Q^{0.5}$** (Soar & Thorne 2001, per the NRCS chapter); Hey & Thorne (1986) UK types give 2.3–4.33 with $b = 0.5$. Emmett (1975, Salmon River ID): 2.8 $Q^{0.49}$. So **the coefficient varies 2.3–4.3 (~factor 2) between regions** — sediment load, bank vegetation, and flashiness (the NRCS chapter notes US rivers are systematically wider than UK ones at equal $Q$, possibly due to flashier flow or higher loads; [Parker et al. 2007](https://doi.org/10.1029/2006jf000549) provides the physics-based "quasi-universal" gravel version). **Verdict: parent's 3.83 is a legitimate bankfull relation (Bray), but it is one of a family spanning 2.3–4.3; use 3.0–4.0 as the plausible band, and expect ±50% site scatter.**

### 1.6 Threshold form and the tools-vs-cover effect

Real bedrock channels erode only when boundary shear stress exceeds a critical value. The threshold SPIM ([Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120); [Snyder et al. 2003](https://doi.org/10.1029/2001jb001655)):

$$E = K (A^m S^n - A_c^m S_c^n) \quad \text{or} \quad E = \max(0,\ K A^m S^n - \tau_c \cdot \text{(geometry factor)})$$

The LandLab implementation is literally $E = K A^m S^n - \text{threshold}$, floored at zero ([LandLab FastScape component docs](https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html)). Thresholds combined with stochastic flood distributions push the *effective* long-term $n$ above 1 ([Snyder et al. 2003](https://doi.org/10.1029/2001jb001655); [Lague 2014 per Harel](https://doi.org/10.1016/j.geomorph.2016.05.035)) — big floods do disproportionate work.

**Critical shear stress, verified:** for *coarse gravel motion* the critical Shields number is $\tau_c^* \approx 0.045$ on rough beds (gravel-bed rivers, low gradient) but ranges up to ~0.2 on very steep channels and down to ~0.007–0.01 for isolated grains on smooth bedrock ([Lamb et al. 2015](https://lamb.caltech.edu/documents/19653/Lamb_etal_Geomorphology_2015.pdf)). Converting to dimensional stress: $\tau_c = \tau_c^* (\rho_s - \rho) g D$. For $D = 50$ mm gravel, $\tau_c^* = 0.045$: $\tau_c \approx 0.045 \times 1650 \times 9.81 \times 0.05 \approx 36$ Pa. For $D = 100$ mm: ~73 Pa. For cobble/boulder $D = 0.5$ m on steep beds ($\tau_c^*=0.1$): ~800 Pa. **Parent's "~10–100 Pa for coarse gravel" is verified for the grain sizes 10–100 mm**; the full natural range is wider at both ends (smooth-bedrock incision thresholds can be a few Pa; field measurements at the Erlenbach gave a *motion* threshold around 1.9 N/m² but an effective incision threshold up to ~407 W/m² of unit stream power for the virtual bedload threshold; [Turowski et al. 2015 per Beer et al.](https://esurf.copernicus.org/articles/3/291/2015/esurf-3-291-2015.pdf)).

**Tools vs cover (Gilbert 1877; [Sklar & Dietrich 2004](https://doi.org/10.1029/2003wr002496)):** sediment is both the abrasive tool (more load → more impacts → more erosion) and the protective cover (more load → bed armored → less erosion). The saltation-abrasion model writes incision as

$$E = V_i \, I_r \, R_a$$

— volume eroded per impact × impact rate per unit area × fraction of exposed bedrock. Sklar & Dietrich used $R_a = 1 - Q_s/Q_t$ (linear cover); Turowski et al. (2007) derived an **exponential cover** $R_a = e^{-\gamma Q_s/Q_t}$, which fits the flume data better at high supply and predicts maximum erosion at $Q_s = Q_t$ rather than $Q_t/2$ ([Turowski, Lague & Hovius 2007](https://doi.org/10.1029/2006jf000697)). The generic shape: **erosion peaks at intermediate sediment supply** and →0 both at zero supply (no tools) and at capacity (full cover). Any LEM that ignores this overpredicts incision in supply-rich reaches.

### 1.7 When the SPIM is valid vs transport-limited

The SPIM is **detachment-limited**: erosion is rate-limited by the bed's ability to detach material, sediment exits the reach immediately. It is **transport-limited** when sediment supply exceeds capacity and the bed alluviates: then incision rate = (transport capacity out − supply in)/reach length, and profile form is set by the sediment-transport law, not the detachment law. Transport-limited behavior dominates: low-gradient rivers, sediment-rich orogens, downstream alluviated reaches; detachment-limited dominates steep bedrock channels with thin cover. Attal (2013)'s verdict is the honest one: the SPIM in its simple form has a narrow range of validity, and most natural datasets are threshold-dominated ([Attal 2013](https://onlinelibrary.wiley.com/doi/10.1002/esp.3462)); the FastScape lineage now solves a sediment-transport-enriched SPL precisely to cover this gap ([Braun & Willett 2013](https://doi.org/10.1016/j.geomorph.2012.10.008); Yuan et al. 2019 per [FastScapeLib docs](https://fastscape.org/fastscapelib-fortran/)).

### 1.8 Worked numeric anchors — unit stream power (parent check)

$\omega = \rho g q S$ with $\rho g = 9810$ N/m³:

| Parent anchor | Computation | Verdict |
|---|---|---|
| $\omega = 9.8$ W/m² at $q=1$ m²/s, $S=0.001$ | $9810 \times 1 \times 0.001 = 9.81$ W/m² | ✓ agrees (9.8) |
| $\omega = 981$ W/m² at $q=10$, $S=0.01$ | $9810 \times 10 \times 0.01 = 981$ W/m² | ✓ exact |
| $\omega = 490.5$ W/m² at $q=100$, $S=0.0005$ | $9810 \times 100 \times 0.0005 = 490.5$ W/m² | ✓ exact |

All three unit-stream-power anchors check out exactly (they're just $9810 \cdot qS$). For calibration: measured mean unit stream power at a sediment-starved bedrock chute (Erlenbach) was **5.9 W/m²** at flood stage with transport stage ~75× threshold ([Beer et al. 2015](https://esurf.copernicus.org/articles/3/291/2015/esurf-3-291-2015.pdf)) — i.e., useful incision happens at single-digit to tens of W/m² in steep headwaters; 100–1000 W/m² is big-river/large-flood territory.

---

## 2. Parent anchors consolidated — verdicts

| # | Anchor | Verdict |
|---|---|---|
| 1 | $\omega(1, 0.001) = 9.8$ | ✓ (9.81) |
| 2 | $\omega(10, 0.01) = 981$ | ✓ |
| 3 | $\omega(100, 0.0005) = 490.5$ | ✓ |
| 4 | $W \approx 3.83\sqrt{Q}$ bankfull | ✓ real (Bray 1973/82 Alberta gravel rivers) but regional: coefficient spans 2.3–4.3; scatter ±50% |
| 5 | $K \sim 10^{-6}$–$10^{-5}$ "m^0.2?" | **Units flagged**: K's units depend on m; 10⁻⁶–10⁻⁵ m^0.5/yr (m=0.5, area in m²) gives 10–500 m/Myr — right magnitude; the "m^0.2" guess is wrong for the m=0.5 case |
| 6 | $\theta \approx 0.4$–0.6 | ✓ verified; global median 0.51±0.14 |
| 7 | $\tau_c \sim 10$–100 Pa coarse gravel | ✓ for 10–100 mm gravel; full range wider |
| $\lambda \approx 10$–14 W | meander wavelength | ✓ (Section 7) |
| Niagara 0.3 m/yr to 1–3 m/yr | | ✓ (Section 8) |
| Grand Canyon 400–1700 m/Myr over 5–6 Myr | | ✓ as the young-canyon aggregate (Section 6) |

---

## 3. Drainage-network anatomy

### 3.1 Strahler ordering and Horton's laws

Strahler (1957) ordering: sources (unbranched headwater tips) are order 1; when two streams of equal order $k$ join, the downstream segment is order $k+1$; unequal joins inherit the higher order. Horton's laws (Horton 1945):

- **Law of stream numbers:** $N_\omega = R_B^{\Omega - \omega}$ (geometric decay with order $\omega$)
- **Law of stream lengths:** $\bar{L}_\omega = \bar{L}_1 R_L^{\omega-1}$
- **Law of basin areas:** $\bar{A}_\omega = \bar{A}_1 R_A^{\omega-1}$

Verified values: Horton's own basins gave $R_B$ from ~2 (flat/rolling) to 3–4 (mountainous/dissected), and $R_L$ ~2–3 (average 2.32) ([Horton 1945](https://pdfs.semanticscholar.org/39c3/9bbea565f8f963309e65506d7756f6571c18.pdf)). A modern 800-catchment Carpathian study: mean **$R_B = 3.8$** (σ=0.93; 90% of catchments < 4.8; outliers to 9), mean **$R_L = 2.3$**, mean **$R_A = 4.8$** — matching global norms ($R_B$ 3–5, $R_L$ 1.5–3.5, mean ~2) ([Bryndal 2015](https://doi.org/10.1515/quageo-2015-0008)). **Parent's "bifurcation ratio ~3.5–4, stream-length ratio ~2" is verified.** Two honest caveats: (i) the length law is only fulfilled in ~half of small natural catchments ([Bryndal 2015](https://doi.org/10.1515/quageo-2015-0008)); (ii) Kirchner (1993) showed random-topology networks obey the same laws almost automatically — Horton's ratios are a weak discriminator ([Bryndal 2015](https://doi.org/10.1515/quageo-2015-0008), citing Kirchner). The deep structure is captured by **Tokunaga self-similar trees** (side-branching statistics $T_k = a c^{k-1}$), which unify Horton laws, Hack's law, and fractal dimensions in one parameterized family ([Kovchegov & Zaliapin 2020](https://zaliapin.github.io/pubs/KZ_PS2020.pdf); [Kovchegov, Zaliapin & Foufoula-Georgiou 2022](https://doi.org/10.1103/physreve.105.014301)).

### 3.2 Hack's law

$$L = c A^{h}$$

$L$ = longest stream length, $A$ = basin area. Hack's original (Shenandoah): $L = 1.4 A^{0.6}$ (miles) ([Rigon et al. 1996](https://doi.org/10.1029/96wr02397)). Gray (1961): $h = 0.568$. Regionally $h$ varies; generally "slightly below 0.6"; Muller (1973) found $h$ decreasing for giant basins (0.6 below 20,720 km²; 0.5 to 0.47 above 259,000 km²) ([Rigon et al. 1996](https://doi.org/10.1029/96wr02397)). Arid-vs-humid: US-wide analysis gives **$h \simeq 0.5$ in arid basins, $h \simeq 0.6$ in humid basins** — humid basins get relatively thinner as they grow (groundwater-limited), arid basins scale self-similarly ([Seybold et al. 2018](https://royalsocietypublishing.org/doi/10.1098/rspa.2018.0081)). **Parent's h≈0.6 verified, with the arid/humid 0.5/0.6 split the refinement that matters for procedural generation.**

### 3.3 Drainage density

$D_d = \sum L_{channels}/A$ (km/km²). The climate story is **non-monotonic**: low in arid areas (little runoff), maximum in semi-arid (runoff up, vegetation still sparse), decreasing to a subhumid/humid minimum (vegetation suppresses runoff), possibly rising again in superhumid/tropical (precipitation variability) — the Abrahams (1984) consensus summarized in [Collins & Bras 2010](https://doi.org/10.1029/2009wr008615). Typical magnitudes: **2–12 km/km² across semi-arid to humid landscapes** (NetMap tools summary, citing Abrahams 1972 and Grant 1997: [netmaptools.org](https://www.netmaptools.org/Pages/NetMapHelp/drainage_density.htm)); ~2–5 as a broad "normal" band. Badlands are the extreme: Schumm's Perth Amboy badlands ran to ~60–110+ km/km² (the original table lists drainage density 110.8 for the fifth-order Perth Amboy system — small-scale rilled badlands are one to two orders of magnitude denser than vegetated terrain; [Schumm 1956](https://pdodds.w3.uvm.edu/research/papers/others/1956/schumm1956a.pdf); also [Howard 1997](http://geomorphology.sese.asu.edu/Papers/Howard_ESPL_97.pdf) for Mancos Shale badlands). **Parent's "roughly 1–100+ km/km²" is verified as the full span** (normal landscapes 1–10ish; badlands to ~100). Controls: the channelization threshold $A_c$ — the drainage area needed to sustain a channel — is inversely related to $D_d$; $D_d \sim 1/\sqrt{A_c}$ ([Collins & Bras 2010](https://doi.org/10.1029/2009wr008615)).

### 3.4 Fractal dimension of networks

From Horton ratios, the classic La Barbera–Rosso relation: $D = \log R_B / \log R_L$. With $R_B \approx 4$, $R_L \approx 2$: $D = 2$ exactly; with real-world $R_B=3.8, R_L=2.3$: $D \approx 1.65$. La Barbera & Rosso's field-data synthesis: typical network fractal dimension **1.5–2.0, average ~1.6–1.7** ([La Barbera & Rosso 1989](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/WR025i004p00735)). Peckham's topological fractal dimension for natural networks: **typically 1.7 < $D_T$ < 1.8** ([Gupta & Mesa 2014](https://doi.org/10.5194/npgd-1-705-2014)). **Parent's "~1.6–1.9 for the network as a set" is verified** (1.6–1.8 as the central band; 2.0 is the space-filling limit reached only asymptotically). Note the multiple inequivalent definitions (mainstream sinuosity dimension ~1.1–1.2, network similarity dimension, total-length scaling) — be explicit about which one you compute ([Beer & Borgas 1993](https://doi.org/10.1029/92wr02731)).

### 3.5 Nested self-similarity

Evidence: (i) Horton laws hold within subbasins at all scales; (ii) Tokunaga statistics pass formal self-similarity tests in the majority of 408 US networks ([Zaliapin et al. 2013 per efi.eng.uci.edu](https://efi.eng.uci.edu/papers/efg_128.pdf)); (iii) Hack's law applies to *any interior point* of a basin, not just outlets (the statistical framework of [Rigon et al. 1996](https://doi.org/10.1029/96wr02397)); (iv) deltas' distributary networks obey Hack's law too (Dong et al., per [Wikipedia: Hack's law](https://en.wikipedia.org/wiki/Hack%27s_law) — flag: secondary source). For a game this means: generate one scale-correct branching statistic and it tiles across all zoom levels.

---

## 4. Hillslopes

### 4.1 Linear diffusion — derivation from mass balance

Soil-mantled hillslopes move by creep (rainsplash, bioturbation, freeze–thaw). Continuum mass balance for the soil layer: $\partial h_s/\partial t = -\nabla \cdot \mathbf{q}_s + P_{soil}$, with soil flux $\mathbf{q}_s = -D \nabla z$ (Fickian; Culling 1963, verified in [Roering et al. 1999](https://doi.org/10.1029/1998wr900090)). If soil thickness is steady and production balances erosion, the *land surface* obeys:

$$\boxed{\frac{\partial z}{\partial t} = U + D \nabla^2 z}$$

— uplift plus linear diffusion. At steady state on a 1D hillslope ($\partial z/\partial t = 0$): $D\, z'' = -U$, so $z(x) = \frac{U}{2D}(L^2 - x^2)$: **parabolic, convex-up, constant curvature $-U/D$ everywhere.** The hilltop curvature test: measure $\nabla^2 z$ at the divide, know $U$, get $D$ directly (the standard field inversion; [Perron et al. 2009](https://doi.org/10.1038/nature08174)). Typical $D$: ~10⁻³–10⁻¹ m²/yr in soil-mantled temperate terrain (order 0.01 m²/yr; see [Fernandes & Dietrich 1997 per Roering](https://doi.org/10.1029/1998wr900090) — flagged as order-of-magnitude, not individually verified here).

### 4.2 Critical slope / angle of repose

Linear diffusion has no slope ceiling. Real granular material does. Talus/scree slopes: measured repose angles of coarse angular fragments ~35° (limestone quarry cones: 35°, [Carson 1977](https://doi.org/10.1002/esp.3290020408)); Alpine talus profile break points at **33–34°** ([Francou & Manté 1990](https://doi.org/10.1002/ppp.3430010107)); laboratory sand repose 32–38° with Shields' classic 33° for uniform sand ([Frontiers loess study](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2021.777467/full)). Chandler (1973) argues the *shearing resistance* of talus materials is 39–40° and that the typical 35° inclination is a degradation-limited, not strength-limited, angle — slopes stand at 39°+ only when rapidly eroded or rapidly deposited ([Chandler 1973](https://doi.org/10.1086/627804)). **Parent's "~33–37°, tan(37°)=0.754" verified** (tan 37° = 0.7536 ✓; the 33–37° band spans measured talus and repose values; note the >39° shearing-resistance nuance). For game thermal-erosion passes: talus angle 33–37°, with lower values (28–33°) for rounded/fine material and cohesion (clay/silt) pushing stable angles well above repose (Schumm's Perth Amboy badlands held mean maximum slopes of **48.8°** in cohesive silt-rich fill; [Schumm 1956](https://pdodds.w3.uvm.edu/research/papers/others/1956/schumm1956a.pdf)).

### 4.3 Nonlinear extensions

1. **Nonlinear (Roering) creep:** from a balance of disturbance-driven kinetic energy against friction+gravity,

$$q_s = \frac{D \nabla z}{1 - (|\nabla z|/S_c)^2}$$

with $S_c = \tan\phi$ the critical gradient. Flux ~linear at low slope, diverges as $|\nabla z| \to S_c$; equilibrium hillslopes become convex near the divide and **planar near $S_c$ downslope** — matching lidar morphology in the Oregon Coast Range, calibrated $S_c \approx 0.65$–0.8 there ([Roering, Kirchner & Dietrich 1999](https://doi.org/10.1029/1998wr900090); response times ≤50 kyr vs 4× longer for linear, [Roering et al. 2001](https://doi.org/10.1029/2001jb000323); experimental confirmation of the creep→landslide transition with 1/f flux spectra, [Roering et al. 2001, Geology](https://doi.org/10.1130/0091-7613(2001)029)). Consequence for terrain generation: **average hillslope gradient is a poor erosion-rate proxy in steep terrain; curvature near the divide is the good one.**
2. **Depth-dependent transport:** flux ∝ (soil depth × slope), not just slope — the "illusion of diffusion" paper shows linear diffusion only fits shallow, convex portions ([Heimsath, Furbish & Dietrich 2005](https://doi.org/10.1130/g21868.1)).

### 4.4 The channelization transition — the critical length scale

Perron, Kirchner & Dietrich's result: the governing equation (creep + SPIM) is a nonlinear advection–diffusion equation; its dimensionless group is a Péclet number $Pe = K L^{2m+2}/D$ (advection/diffusion, at horizontal length scale $L$). Setting $Pe = 1$ defines the **characteristic length**:

$$\boxed{L_c = \left(\frac{D}{K}\right)^{1/(2m+2)}}$$

(up to an $m$-dependent constant). Measured valley spacing across five US field sites is proportional to $L_c$ ([Perron et al. 2009](https://doi.org/10.1038/nature08174); fuller derivation in [Perron et al. 2008](https://doi.org/10.1029/2007jf000977)). Notable predictions: valley spacing is *independent of erosion rate*; the hillslope→valley transition occurs at drainage area $A_c \sim L_c^2$; low $Pe$ → smooth undissected slopes, high $Pe$ → branching networks. **This is the parent's "critical hillslope length scale" — verified, with the exact $L_c$ formula above.** For a game: pick $D/K$ to set your valley wavelength directly.

---

## 5. Landscape evolution models (LEMs)

### 5.1 The field

- **CHILD** (Tucker, Lancaster, Gasparini) — triangulated irregular network, stochastic rainfall, detachment/transport-limited options (cited throughout, e.g., [Tucker & Hancock 2010](https://doi.org/10.1002/esp.1952)).
- **LandLab** — Python component library; its FastScape-style stream power component implements exactly $E = K A^m S^n -$ threshold with $K_{sp}=0.001$, $m=0.5$, $n=1$ defaults ([LandLab docs](https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html)).
- **FastScape** (Braun & Willett) — the algorithm that made geological-scale runs cheap: an **O(n), implicit-in-time solver of the stream power equation**. The trick: process nodes in order of decreasing elevation (a stack built once per step), which makes the otherwise-nonlinear implicit update a single ordered sweep; stable at large timesteps and parallelizable ([Braun & Willett 2013](https://doi.org/10.1016/j.geomorph.2012.10.008)). The library couples SPL + sediment transport/deposition (Yuan et al. 2019) + hillslope diffusion + marine transport, plus a **spectral flexure solver** (thin-elastic-plate biharmonic over an inviscid asthenosphere, E=10¹¹ Pa, ν=0.2) for isostatic rebound ([FastScapeLib docs](https://fastscape.org/fastscapelib-fortran/)). **Parent's "stream-power + flexure solver used in real research" verified.**

### 5.2 Steady state and response times

**Topographic (geomorphic) steady state:** erosion rate = uplift rate everywhere; mean elevation, relief, and hypsometry stop changing. Willett et al.'s coupled models put time-to-steady-state at **~1–50 Myr for mountain belts 50–200 km wide, uplift 0.1–1 mm/yr** — longer times for lower uplift ([Willett et al. 2014 per ajsonline.org PDF](https://ajsonline.org/article/88260-uplift-shortening-and-steady-state-topography-in-active-mountain-belts.pdf)). Whipple & Tucker's SPIM analysis: response time to base-level fall scales as $T \propto L^{?}$... their headline result is that response time is *relatively insensitive to basin size* but depends on $n$: for $n<1$ longer for small perturbations, $n=1$ independent of uplift, $n>1$ shorter for large perturbations ([Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120)). **Parent's "order 1e5–1e7 yr" verified**: hillslopes adjust in ≤50 kyr ([Roering et al. 2001](https://doi.org/10.1029/2001jb000323)); Taiwan-scale basins reach steady state in 0.5–2 Myr ([Chen et al. 2012 per sciencedirect](https://www.sciencedirect.com/science/article/abs/pii/S0169555X1200205X)); whole mountain belts 1–50 Myr; post-orogenic decay ~50 Myr e-folding with isostasy, longer with resistant rock ([Pelletier 2004](https://doi.org/10.1029/2004gl020052)); the southeastern US is *still* far from equilibrium millions of years after the tectonics quit ([Willett et al. 2014](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf)).

### 5.3 Transients and knickpoints — the messenger

A sudden base-level fall injects a **knickpoint** (steepened reach) that propagates upstream as a kinematic wave with celerity

$$\boxed{C = \frac{dn}{dt} = K A^m S^{n-1}}$$

(derivation: perturb the SPIM about steady state; the characteristic speed of the advective equation $\partial z/\partial t = U - K A^m |\partial z/\partial x|^n$ is $K A^m S^{n-1}$.) For $n=1$ the knickpoint preserves its shape as it migrates. Field rates: **0.001–0.1 m/yr typical, >1 m/yr exceptional** (Niagara, active orogens) ([Loget & Van Den Driessche / wave-train model](https://archimer.ifremer.fr/doc/00000/11075/8056.pdf), compiling Van Heijst & Postma 2001, Philbrick 1970, Tinkler 1994). Retreat rate scales with drainage area, approximately $\propto \sqrt{A}$ ($V = C\sqrt{A}$ with $C \sim 10^{-5}$ yr⁻¹ across the Messinian Salinity Crisis data) ([Loget et al.](https://archimer.ifremer.fr/doc/00000/11075/8056.pdf)). Counterintuitive field result: catchments crossing higher-throw-rate faults have *faster* knickpoints — amplitude speeds the response, partly via channel narrowing ([Whittaker et al. / JGR 2011](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2011JF002157): 0.2–2 mm/yr for 6–65 km² catchments in Turkey/Italy). Knickpoints can also stall (resistant lithology, landslide armoring, sediment cover), protecting upstream relict surfaces ([Clark et al. 2006](https://doi.org/10.1029/2005jf000294)).

---

## 6. Grand Canyon — the worked case study

**The standard story:** Colorado River integrated through the canyon 5–6 Ma, carving most of ~1.5 km depth since → **average incision ~250–300 m/Myr integrated over 5–6 Myr**, with shorter-term terrace-derived rates of **150–175 m/Myr over the last 2–3 Myr** below Lees Ferry ([Darling 2011 thesis, compiling Pederson 2002, Polyak 2008, Karlstrom 2008](https://digitalrepository.unm.edu/eps_etds/15)); and faster late-Pleistocene pulses: Glen Canyon dates ≤500 ka yield rates **up to 500 m/Myr** (though the Darling work argues some of these are age-underestimates), and speleothem water-table decline rates of **166–411 m/Myr in the eastern canyon, 55–123 m/Myr in the west** ([Polyak et al. 2008, Science](https://www.science.org/doi/10.1126/science.1151248)). **Parent's "integrated ~400–1700 m/Myr over 5–6 Myr" is partially verified:** the 5–6 Myr window with 150–500 m/Myr is well-supported; 1700 m/Myr exceeds any main-stem integrated rate I found — the closest are the ≤500 ka local pulses (~500) and eastern-canyon speleothem rates (~411). **Disagreement reported: 1700 m/Myr is not supported by the sources found; treat 150–500 m/Myr as the verified integrated range with local short-term excursions to ~500.**

**The "old Grand Canyon" controversy:** Flowers & Farley's apatite ⁴He/³He thermochronometry suggests the *western* canyon was excavated to within a few hundred meters of modern depth by **~70 Ma** ([Flowers & Farley 2012 / Science summary](https://www.science.org/doi/10.1126/science.1229390)). Karlstrom et al.'s reconciliation — now the mainstream view — is segment-by-segment: Hurricane segment ~half depth by 70–55 Ma, Eastern Grand Canyon 25–15 Ma, but **Marble Canyon and the Westernmost Grand Canyon are young (carved in the past 5–6 Ma)**; the modern canyon is the Colorado River reusing and stitching older palaeocanyons ([Karlstrom et al. 2014, Nature Geoscience](http://geomorphology.sese.asu.edu/Papers/Karlstrom-2014-NatGeoscience.pdf)). Later re-analysis of westernmost-canyon ⁴He/³He data (with measured U-Th zonation) supports the young interpretation ([Fox et al. 2017](https://doi.org/10.1016/j.epsl.2017.06.049)). Genuine literature disagreement — report both.

**Role of uplift and terraces:** incision through Grand Canyon exceeds isostatic-rebound predictions by ~100 m/Myr, implying a real rock-uplift component (mantle-flow tilting of the plateau) ([Darling 2011](https://digitalrepository.unm.edu/eps_etds/15), discussing Karlstrom 2008, Moucha 2008). The **Lees Ferry knickpoint** is interpreted as a transient set up by 6-Ma drainage integration, with incision waves diffusively bypassing it; above it, incision is slower (126 m/Myr at Bullfrog over 1.5 Ma) than below. Terraces record the climate-cycle modulation of discharge and sediment load on top of the tectonic signal.

---

## 7. Meandering

### 7.1 Why rivers bend

The accepted mechanism is a positive feedback between **helical secondary flow** and **bank erosion/deposition**: at a bend, centrifugal force piles superelevated water at the outer bank; the cross-channel pressure gradient drives near-bed flow *toward the inner bank, while surface flow goes outward* — a helical (corkscrew) cell. The near-bed flow toward the point bar sweeps bedload onto the inner bank (deposition, point bar) while high velocity at the outer bank erodes it (cutbank). Bend → bar/cutbank asymmetry → more curvature → stronger helical cell. Experiments show two ingredients are *necessary and sufficient* for self-sustaining meandering: bank strength exceeding bed strength (vegetated/cohesive banks) and fine-sediment deposition plugging chute channels behind point bars — constant discharge is NOT required ([Braudrick et al. 2009 PNAS](https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/)).

### 7.2 Wavelength scaling

Empirical bankfull relation from a 438-site composite: **$L_m = 8.36\, W^{1.05} \approx 10.2\, W$** (the fixed-exponent model; ERDC channel-design manual fig. 7.2, [erdc-library download](https://erdc-library.erdc.dren.mil/bitstreams/81b728f8-6e6d-4ef8-e053-411ac80adeb3/download)); classic texts give the 10–14×W band; the Braudrick experiment stabilized at ≈14 W ([Braudrick et al. 2009](https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/)). **Parent's λ ≈ 10–14 W verified; the parent's specific "11.2 W" is within band but I could not find that exact coefficient — unverified, use 10.2 (Soar/Thorne-type composite) or the band.** Radius of curvature at bends: ~2–3 W (progressively migrating Sacramento bends: R = 2.8 W; chute-cutoff-prone bends: 2.1 W) ([Micheli & Larsen 2010](https://onlinelibrary.wiley.com/doi/10.1002/rra.1360)).

### 7.3 Migration rates, cutoffs, oxbows

Natural single-bend migration: mostly **0.01–0.02 channel-widths/yr**, max ~0.18 W/yr ([Braudrick et al. 2009](https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/), compiling the literature). Sacramento River 1904–1997 (W≈250 m): average lateral change **5.5 ± 0.6 m/yr** (~0.02 W/yr); progressive migration 4.7 m/yr, chute cutoffs locally 22.1 m/yr; one cutoff every ~2.5 years per 160 km ([Micheli & Larsen 2010](https://onlinelibrary.wiley.com/doi/10.1002/rra.1360)). **Parent's "cm–m/yr scale by river size" verified**: small streams cm–dm/yr; large rivers like the Sacramento ~5 m/yr; catastrophic chute events faster. So in absolute terms migration spans ~0.01–20 m/yr with size.

**Neck vs chute cutoff:** neck cutoff = the migrating bend's two limbs meet (sinuosity gets extreme, R/W small); chute cutoff = an overbank flood excavates a straighter channel across the floodplain (often along an existing swale), which then captures the flow via upstream knickpoint migration through the chute. Sacramento geometry thresholds: chute cutoff at sinuosity ≈ 1.97, R = 2.1 W, entrance angle ≈ 111°, vs progressive bends at sinuosity 1.31, R = 2.8 W, 66° ([Micheli & Larsen 2010](https://onlinelibrary.wiley.com/doi/10.1002/rra.1360)). Post-cutoff, the abandoned loop plugs at both ends with fine sediment → **oxbow lake**; chute cutoffs reworked 20% of floodplain area despite only 5% of channel length. Timescale: an meander goes from birth to cutoff in ~10²–10⁴ yr depending on river size (Sacramento: ~5 bends in 93 yr on 160 km).

### 7.4 Planform classes: braided vs meandering vs straight vs anastomosing

The discriminator is **sediment supply vs transport capacity, plus bank strength**: braided when load is high relative to capacity and banks are weak (multiple thalwegs around bars, wide/shallow, steep-ish); meandering when banks resist (single thread, point bars); straight where neither perturbation dominates (rare in nature at sinuosity ~1); **anastomosing** = multiple *stable, vegetated, interconnected* channels separated by cohesive floodplain wetlands — low energy, fine sediment, aggradational (distinct from braiding's dynamic high-energy bar-hopping; the standard distinction per the alluvial-fans/rivers literature, e.g., [Blair & McPherson 1994](https://doi.org/10.1306/d4267dde-2b26-11d7-8648000102c1865d) context and river-classification literature — flagged: no single quantitative threshold source verified here).

---

## 8. Longitudinal profile and base level

### 8.1 Graded rivers and concavity

The **graded river** (Gilbert 1877; Mackin 1948): a profile where, at the prevailing discharge and load, slope is just sufficient to transport the supplied sediment — no net deposition or erosion; the profile is a *condition*, not a deposit. Because $Q$ grows downstream while sediment per unit flow generally doesn't keep pace, the required slope falls downstream → **concave-up** profile, consistent with the Section 1.3 derivation ($S \propto A^{-\theta}$, θ>0). Base level (sea level, lake level, resistant rock) is the floor of the whole system: rivers can never cut below it (locally) for long.

### 8.2 Knickpoints and Niagara Falls — the worked retreat example

Niagara Falls: ~52 m high, on the Lockport dolostone over softer shale — the caprock collapses block-by-block once the plunge pool undercuts the shale. Retreat history (Horseshoe Falls, from crest-line surveys): **1842–1905: 5.3 ft/yr ≈ 1.6 m/yr; 1842–1906 ≈ 1.28 m/yr; long-record 1670–1969 average ≈ 1.1 m/yr; declining 1.28 → 0.98 → 0.67 m/yr through the early 20th century** as hydroelectric diversion ramped up ([Gilbert 1907 USGS Bulletin 306](https://pubs.usgs.gov/bul/0306/report.pdf); [Tinkler et al. 1994](https://doi.org/10.1006/qres.1994.1050); NYSGA 1982 guide at [ottohmuller.com](https://ottohmuller.com/nysga2ge/Files/1982/NYSGA%201982%20B2%20-%20Glacial%20And%20Engineering%20Geology%20Aspects%20Of%20The%20Niagara%20Falls%20And%20Gorge.pdf)). Post-1950-Niagara-Treaty flow regulation (2832 m³/s tourist hours, 1416 off-peak, of a mean natural 5720 m³/s) cut retreat to **<0.3 m/yr, currently ~0.1 m/yr** ([SERC Carleton vignette](https://serc.carleton.edu/vignettes/collection/25474.html); [International Joint Commission](https://www.ijc.org/en/niagara-falls-moving)). Philbrick (1970) adds that crest *shape* modulates rate: notched crests erode up to 3× faster than arched crests, and the pool-bottom profile shows episodic "basin-and-high" retreat, with arch-stage rates up to 5.8 m/yr locally ([NYSGA 1982](https://ottohmuller.com/nysga2ge/Files/1982/NYSGA%201982%20B2%20-%20Glacial%20And%20Engineering%20Geology%20Aspects%20Of%20The%20Niagara%20Falls%20And%20Gorge.pdf)). American Falls: ~0.06–0.15 m/yr (Gilbert estimated <0.5 ft/yr, probably 0.2 ft/yr ≈ 0.06 m/yr) because talus armor protects its base. **Parent's "30 cm/yr to 1–3 m/yr historical, slowing" verified** (0.1–0.3 today; 1–2 m/yr natural historical; short pulses to ~6 m/yr). Total retreat ~11 km since ~12.4 ka deglaciation → long-run average ~0.9 m/yr.

### 8.3 Base-level change consequences

- **Base-level fall** → incision, knickpoint migration upstream, terrace staircases. Climatic (glacial-interglacial) cycles stack **stream terraces**: each incision pulse leaves the old floodplain perched; Grand Canyon and Colorado Plateau terraces record 100-kyr-scale modulation.
- **Base-level rise** → drowning: **rias** (drowned river valleys, e.g. SW England/Galicia), Chesapeake Bay — the river's graded profile is progressively buried from the mouth.
- **Superimposed/entrenched meanders**: meanders formed on a low-gradient plain, then uplift or base-level fall causes the river to incise while *retaining* its planform → incised meander gorge (Goosenecks of the San Juan: sinuosity extreme because incision outpaced lateral migration). (Flag: Goosenecks-specific rate figures not individually verified in this pass; mechanism standard.)
- **Lakes as local base level**: the Great Lakes trap Niagara's sediment, keeping the river sediment-starved and the falls unburied ([SERC vignette](https://serc.carleton.edu/vignettes/collection/25474.html)).

---

## 9. Deltas and depositional terrain

### 9.1 Why deltas differ: the process regime (Galloway triangle)

Deltas sit at the junction of three forcings — river (sediment supply prograding the shoreline), waves (alongshore diffusion smoothing it), tides (widening mouths, funnels) ([Galloway 1975]; [Wright & Coleman 1973](https://doi.org/10.1306/819a4274-16c5-11d7-8645000102c1865d) for the original 7-delta spectrum; modern multiscale confirmation in [Vulis et al. 2023](https://research-portal.uu.nl/ws/files/235410242/Geophysical_Research_Letters_-_2023_-_Vulis_-_River_Delta_Morphotypes_Emerge_From_Multiscale_Characterization_of_Shorelines.pdf)):

- **River-dominated (birdfoot, Mississippi):** low nearshore wave energy + flat offshore profile → long distributaries reach far out, mud-dominated, irregular shoreline. The modern Balize "birdfoot" branches at polyfurcation points marking old shorelines ([Chamberlain et al. 2018, Science Advances](https://www.science.org/doi/10.1126/sciadv.aar4740)).
- **Wave-dominated (arcuate/cuspate, Nile/São Francisco/Senegal):** waves rework the river's supply into smooth beach-ridge shorelines; symmetric if waves perpendicular, asymmetric/flying-spit if oblique.
- **Tide-dominated (funnel/estuarine, Fly/Ganges-Brahmaputra):** strong tidal range widens mouths, builds tidal flats/mangrove plains; concave shoreline intruding landward.
- Key physical control on whether the river can dominate at all: the *subaqueous* slope. Rivers build river-dominated shapes only on flat offshore profiles; steep shoreface → wave forms win regardless of river power ([Wright & Coleman 1973](https://doi.org/10.1306/819a4274-16c5-11d7-8645000102c1865d)).

### 9.2 Progradation rates (verified)

Mississippi Lafourche subdelta (late Holocene, river-dominated): **mouth-bar progradation 100–150 m/yr sustained for ~1 kyr**, building 6–8 km²/yr of new land — several times *below* modern human-enhanced loss (~45 km²/yr) ([Chamberlain et al. 2018](https://www.science.org/doi/10.1126/sciadv.aar4740)). Modern birdfoot: southern/western margins still prograding ~7–14 m/yr while eastern margins retreat up to ~58 m/yr under wave attack; net wetland change ~zero over 1990–2022 ([Yang et al. 2025](https://doi.org/10.1029/2024ef005003)). So game-realistic delta growth: **tens of m/yr typical, 100+ m/yr for big muddy rivers on shallow shelves, negative once sediment supply is cut.**

### 9.3 Alluvial fans

Form where an upland feeder channel loses confinement at a mountain front: flows decelerate, deposit, and repeatedly avulse → semiconical, plano-convex piedmont landform, planar-convex cross-section (inverse of a river's trough) ([Blair & McPherson 1994](https://doi.org/10.1306/d4267dde-2b26-11d7-8648000102c1865d)). Two endmember constructions:

- **Debris-flow dominated:** viscous slurry lobes, matrix-supported, boulder-rich, steep (typically >4–10°); common in tectonically active/semiarid fronts.
- **Waterlaid (sheetflood) dominated:** flash-flood sheetfloods deposit planar-couplet gravels; antidune standing-wave deposits (backsets); example: Anvil Spring fan, Death Valley, slopes 2.5–5° over 9.7 km radial length ([Blair 1999](https://doi.org/10.1046/j.1365-3091.1999.00259.x)).
- **Sieve deposits:** the third, long-disputed mode — coarse open-framework gravels deposited when bedload-laden water *infiltrates* into the permeable fan surface, dropping its entire gravel load; verified active in gravel-rich, matrix-poor alpine fans (sub-annual events triggered by >50 mm/24 h rain, >1000 m³ per event; fans built almost entirely of stacked sieve lobes) ([Novak et al. 2022](https://doi.org/10.1002/esp.5508); [Milana 2010](https://ri.conicet.gov.ar/handle/11336/101758)). For voxel terrain: sieve texture = clast-supported open-framework gravel, downward coarsening, no matrix.

### 9.4 Sediment yields per setting

The Milliman & Farnsworth style numbers, verified from primary sources:

| Setting | Sediment yield (t/km²/yr) | Source |
|---|---|---|
| Global average (all rivers) | ~150 | [Kao & Milliman 2008](https://doi.org/10.1086/590921) |
| Taiwan, 16 rivers mean | **9,500** (60× global) | [Kao & Milliman 2008](https://doi.org/10.1086/590921) |
| Taiwan, individual rivers | 500–71,000 (2+ orders of magnitude spread) | [Kao & Milliman 2008](https://doi.org/10.1086/590921) |
| Taiwan orogen denudation | 3–6 mm/yr across all timescales | [Dadson et al. 2003, Nature](https://www.nature.com/articles/nature02150) |
| Taiwan ECR suspended-sediment basins | 2.2–8.3 mm/yr | [Fuller et al. 2003](https://doi.org/10.1086/344665) |
| Liwu River, ¹⁰Be(met)/⁹Be | 8 to >30 mm/yr (highest cosmogenic rates ever) | [GFZ study](https://gfzpublic.gfz.de/rest/items/item_5008148_2/component/file_5008477/content) |
| Old cratons / lowlands | order 1–30 m/Myr (e.g. Brazil QF quartzites 0.8–5 m/Myr) | [Bezerra et al. 2018 EGU abstract](https://meetingorganizer.copernicus.org/EGU2018/EGU2018-19677.pdf) |

**Taiwan's is the verified extreme**: >75% of the long-term flux in <1% of the time (typhoons), ~⅓ reaching hyperpycnal concentrations ([Kao & Milliman 2008](https://doi.org/10.1086/590921)). The sediment cascade: hillslope production → landslides (stochastic, earthquake+typhoon triggered) → rivers → coastal ocean, with landslide dam-and-flush dynamics modulating delivery (the Nature "lifespan" paper: landslide-river feedbacks explain why *inactive* ranges erode slowly — dams armor beds; [Egholm et al. 2013, Nature](https://preview-www.nature.com/articles/nature12218)).

---

## 10. Landscape memory and relict terrain

### 10.1 Erasing a mountain belt

Denudation after uplift cessation decays mean elevation roughly exponentially, e-folding **~50 Myr** (isostasy-adjusted ~45–70 Myr) from the global sediment-yield–elevation correlation ([Pelletier 2004](https://doi.org/10.1029/2004gl020052), citing Ahnert 1970). Yet Paleozoic orogens (Appalachians ~300 Ma, Urals) still stand >1 km. Resolutions of the paradox in the literature (all three supported): (i) **resistant bedrock + broad piedmont** lowers effective K and sets a high local base level (Pelletier 2004); (ii) **landslide-river feedbacks**: in inactive ranges, large landslides dam rivers, cover beds, and cut tool supply — erosion self-throttles ([Egholm et al. 2013](https://preview-www.nature.com/articles/nature12218)); (iii) coupled tectonic models: orogen decay has two phases, with long-wavelength Phase-2 decay lasting "tens to several hundreds of Myr" controlled by erosional efficiency, isostasy, and width — models retain ~1.5 km maximum topography after 150 Myr of decay for wide, efficient-erosion-limited cases ([GFZ/Bedroi study](https://gfzpublic.gfz.de/rest/items/item_5008148_2/component/file_5008477/content) — flag: exact paper identity partially obscured in source snippet; the two-phase decay framework is corroborated by the same PDF). **Parent's "~1e7–1e8 yr, and what survives" verified**: 10–50 Myr as the rule, 10⁸ yr persistence possible for quartzite-cored or resistant belts; measured low-relief ancient landscapes erode at ~1–30 m/Myr (Brazil QF, tectonically stable 500 Myr: quartzite catchments 0.8–5 m/Myr, gneiss up to ~25 m/Myr; [Bezerra et al. 2018](https://meetingorganizer.copernicus.org/EGU2018/EGU2018-19677.pdf)).

**What survives:** peneplains (Davis's graded-to-sea-level endmember), etchplains (deep-weathering-stripped surfaces), inselbergs (resistant bedrock monoliths on plains), and relict low-relief surfaces elevated wholesale — the eastern Tibetan Plateau's relict landscape preserves a low-relief paleosurface now at 3–4 km, dissected only where 9–13 Ma river incision began; knickpoints at the relict/active boundary still propagate, response time >10 Ma ([Clark et al. 2006](https://doi.org/10.1029/2005jf000294)). For a game: ancient stable continents should carry *flat high surfaces with isolated resistant knobs*, not young rugged mountains.

### 10.2 Drainage-divide migration and stream capture

**Gilbert (1877)'s "law of unequal declivities"** — the ancestor of every modern metric: an asymmetric divide implies unequal erosion rates, and the divide migrates toward the gentler/erosion-poorer side. The modern competition metric is **χ** (chi), the steady-state-elevation integral: with reference concavity $\theta_{ref}$,

$$\chi = \int_0^x \left(\frac{A_0}{A(x')}\right)^{\theta_{ref}} dx'$$

At geometric equilibrium, χ must be equal across divides; **χ-anomalies** (high-χ side vs low-χ side) predict divide migration from low-χ toward high-χ channels ([Willett et al. 2014, Science](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf)). Numerical models confirm: divides move until χ equalizes, and the time to network (not profile) equilibrium is many times the longest river's response time. Caveats: χ assumes uniform uplift/erodibility/climate; where those vary, χ-anomalies can be false positives, and **Gilbert metrics** (cross-divide differences in channel-head elevation at reference area, mean headwater gradient, local relief) can be more reliable indicators of *current* motion — use both; if either says unstable, it is ([Forte & Whipple 2018](https://www.sciencedirect.com/science/article/abs/pii/S0012821X18302292); [Ye et al. 2024](https://doi.org/10.1002/esp.5892) shows hillslopes and channels each absorb part of the cross-divide signal). **Stream capture** is the discrete topological version: headward erosion or divide migration reroutes a drainage; diagnostics are windgaps, beheaded valleys, underfit streams, barbed tributaries, and captured biota (Apalachicola→Savannah capture visible in χ maps; [Willett et al. 2014](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf); operational guidance in the RBG technical note, [rbg.emnuvens.com.br](https://rbg.emnuvens.com.br/rbg/article/download/2797/386387055/386400679)). **Enough to implement divide migration: compute χ per node (single ordered traversal, choosing θ_ref ~0.45–0.5), find adjacent channel heads across each divide, move the divide node-by-node toward higher χ, and let captured area feed back through the SPIM.**

### 10.3 Hypsometric integral

The hypsometric curve plots relative area above a given relative elevation; the **hypsometric integral (HI)** is the area under it (practical estimator: $HI \approx (\bar{H} - H_{min})/(H_{max} - H_{min})$, Pike & Wilson). Interpretation (Strahler 1952): convex/high HI ≈ youthful disequilibrium; S-shaped/HI≈0.5 ≈ mature/equilibrium; concave/low ≈ old dissected. Typical values: **0.15–0.85 range, clustering 0.4–0.6** ([opengeology.in summary](https://opengeology.in/hypsometric-curve-and-integral/)); Taiwan steady-state basins: **HI → 0.5 exactly at steady state, S-curves, normally distributed elevations**, 0.5 "critical", with response times 0.5–2 Myr by Strahler order; non-steady basins show HI scale-dependence (small basins higher HI) ([Chen et al. 2012](https://www.sciencedirect.com/science/article/abs/pii/S0169555X1200205X)). Caveat from the modern literature: HI measures the tectonics-vs-erosion balance as much as "age" (Weissel et al. 1994, per [Bhattacharjee 2022](https://doi.org/10.56975/ijcsp.v12i2.303932)) — an uplifting young range can have moderate HI. **Parent's "young vs old values" verified with the 0.6/0.3 thresholds (youth ≥0.6, mature 0.35–0.6, old ≤0.35 per the Loess Plateau application, [Duan et al. 2022](https://doi.org/10.3389/feart.2022.827836)).**

---

## 11. Practical simulation synthesis — the opinionated pipeline

**The blunt call:** do your hydrology at *terrain-generation time*, not run time. A hydrologically-correct heightmap is a one-off O(n log n)–O(n) computation per world tile; trying to fake rivers on top of noise afterward is where every procedural world fails the "where does the water go" sniff test.

**The minimal pipeline (each stage annotated with what breaks if skipped):**

1. **Base field:** fBm/ridged noise + regional warp, OR a tectonic-style uplift field. *Skip hydrology entirely* → no coherent valleys, wrong slope-area statistics (the failure modes cataloged in the parent question bank Q69–70).
2. **Priority-Flood depression filling:** flood the DEM inward from edges via a priority queue, raising pit cells to their outlet level — every cell then drains ([Barnes, Lehman & Mulla 2014](https://doi.org/10.1016/j.cageo.2013.04.024)). O(n log n) floating-point, O(n) integer; the +epsilon variant resolves flats by adding infinitesimal gradients so flow directions are defined. *Skip:* flow routing dead-ends in pits, rivers terminate in the middle of continents, drainage areas are garbage.
   - Alternative for LEM use: the Cordonnier–Bovy–Braun basin-graph method computes flow paths *through* depressions with explicit fill-vs-carve choice, O(n), best-in-class for repeated erosion stepping ([Cordonnier et al. 2019](https://doi.org/10.5194/esurf-7-549-2019)).
3. **Flow routing D8 vs D-infinity:** D8 sends all flow to the steepest of 8 neighbors — grid-aligned, cheap, river-like in aggregate but with visible diagonal artifacts; D∞ (Tarboton) partitions flow between the two steepest downslope neighbors — smoother accumulation fields, better on smooth hillslopes. *Skip or use naive sequential accumulation:* accumulation is the expensive part; do it as a single pass over cells sorted by decreasing filled elevation (the same stack FastScape uses).
4. **SPIM-like carving at generation time:** iterate (fill → route → accumulate → erode) with the Braun–Willett implicit solver: order nodes by decreasing elevation, solve each node's implicit update in one sweep — stable at large timesteps, O(n) per step ([Braun & Willett 2013](https://doi.org/10.1016/j.geomorph.2012.10.008)). This is exactly what LandLab ships ([LandLab docs](https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html)). Add uplift U and run to equilibrium or to a chosen "age."
5. **Hillslope diffusion pass:** linear (or Roering-nonlinear for steep terrain) diffusion smooths ridges and sets valley spacing via $L_c = (D/K)^{1/(2m+2)}$ ([Perron et al. 2009](https://doi.org/10.1038/nature08174)). *Skip:* ridges are knife-sharp noise, slope distributions wrong, no characteristic valley wavelength — the single most recognizable "procedural terrain" tell.
6. **Channel-head threshold:** only erode where $A > A_c$ (typically 0.1–5 km² fluvial/debris-flow transition; [Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035) citing Wobus 2006, Stock & Dietrich 2003). $A_c$ sets drainage density (Section 3.3).
7. **Post-passes:** meander the large-river centerlines (λ = 10–14 W, migration as a time-jittered displacement), terrace carving for climate history, delta shape selection by the process regime (river/wave/tide) at each coastline sink, fan deposition at mountain fronts with talus-angle repose.

**Parameter table (literature-grounded):**

| Parameter | Value | Source |
|---|---|---|
| $m$, $n$ | start $m=0.5$, $n=1$; accept $n$ up to ~2.5 for threshold realism | [Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035) |
| $\theta = m/n$ | 0.45–0.5 | Harel 2016; Willett 2014 |
| $K$ | 10⁻⁶–10⁻⁵ m^0.5/yr equivalents; 9 orders of magnitude across lithologies | Harel 2016; Goren 2014 |
| $D$ (hillslope diffusivity) | ~10⁻³–10⁻¹ m²/yr (order 0.01) | Roering 1999 (order) |
| $A_c$ (channel head) | 0.1–5 km² | Harel 2016 (citing Wobus) |
| $W$ bankfull | 3.0–4.0 × Q^0.5 (m, m³/s), ±50% | NRCS 654; Bray; Nixon; Hey & Thorne |
| Meander λ | 10–14 W | ERDC; Braudrick 2009 |
| Talus repose angle | 33–37° dry (cohesive slopes to ~49°) | Carson 1977; Francou 1990; Schumm 1956 |
| $\tau_c$ gravel | 10–100 Pa (10–100 mm grains) | Lamb et al. 2015 |
| $L_c$ valley spacing | $(D/K)^{1/(2m+2)}$ × O(1) constant | Perron 2009 |
| Knickpoint celerity | $K A^m S^{n-1}$; 0.001–0.1 m/yr typical | Loget; Whittaker 2011 |
| HI sanity | 0.4–0.6 for mature; <0.3 old; >0.6 young | Strahler; Chen 2012 |

**What breaks if you skip each stage (summary table):** no fill → dead-end rivers; no accumulation → no area-dependent erosion, uniform gullies; no SPIM → no concave profiles, no drainage-area-organized networks; no diffusion → wrong valley spacing and ridge sharpness; no threshold → rills on every pixel, over-dissection; no meandering post-pass → rivers are noise-creek polylines; no process-regime deltas → every coast gets the same fan.

---

## Provenance

**Verified (multi-source or exact computation):** all three unit-stream-power anchors (exact arithmetic); $\theta = 0.51 \pm 0.14$ global median; $n = 2.43$ global median; $W \propto Q^{0.512}$; Bray 3.83 (with the 2.3–4.3 regional band); Horton $R_B$ 3.8, $R_L$ 2.3, $R_A$ 4.8 (Carpathians, matching global norms); Hack $h \approx 0.5$–0.6 (arid/humid split); drainage density 2–12 normal, ~100+ badlands (Schumm Perth Amboy 110.8); network fractal dimension 1.6–1.8; talus 33–37° with the Chandler 39–40° shearing-resistance nuance; Roering nonlinear law and $L_c$ valley-spacing; Braun–Willett FastScape O(n) solver; response times 50 kyr (hillslopes) – 1–50 Myr (belts); knickpoint celerity law and 0.001–0.1 m/yr rates; Grand Canyon 150–500 m/Myr integrated, 5–6 Ma young segments, old-segment controversy documented with both sides; meander λ 10–14 W, migration 0.01–0.02 W/yr typical; Sacramento cutoff statistics; Niagara 0.1–1.6 m/yr declining; Mississippi progradation 100–150 m/yr; Galloway delta regime; Taiwan 9,500 t/km²/yr, 3–6 mm/yr; peneplain persistence 10–50 Myr e-folding with 10⁸-yr exceptions; χ divide analysis and Gilbert metrics; HI 0.4–0.6 mature.

**Unverified / flagged:** parent's exact "11.2 W" meander coefficient (within band, no source found); Goosenecks-specific incision figures; anastomosing quantitative thresholds (described qualitatively only); specific $D$ values cited only at order-of-magnitude via secondary references; the GFZ two-phase orogen-decay paper's exact identity (snippet-sourced); the "1700 m/Myr" Grand Canyon upper anchor (see disagreement #1); delta Hack's-law claim (secondary Wikipedia source).

**Anchor disagreements (parent vs sources):**
1. **Grand Canyon 1700 m/Myr:** not supported by any integrated main-stem rate found; verified integrated range is 150–500 m/Myr with short-term local excursions to ~500 and speleothem rates to ~411. Parent upper bound appears ~3× too high.
2. **K units "m^0.2":** wrong for the stated $m$-family; with $m=0.5$ (the standard), units are m$^{0.5}$/yr. The 10⁻⁶–10⁻⁵ magnitude is fine once fixed to m=0.5, n=1, area-in-m².
3. **W = 3.83√Q:** correct as a citation (Bray, Alberta gravel rivers) but presented as if universal — the coefficient spans 2.3–4.3 across regions (Nixon UK 2.99; Hey & Thorne types 2.3–4.33; Emmett 2.8), and site scatter is ±50%.
4. **Meander 11.2 W:** within the verified 10–14 band but no source found for that exact value; the 438-site composite fixed-exponent fit is 10.2.
5. All other anchors (unit stream powers ×3, θ, τc, Horton ratios, fractal dimension, drainage-density span, repose angle, response times, knickpoint celerity, Niagara, HI) verified or exact.

---

## Sources

1. Attal 2013, "The stream power river incision model: evidence, theory and beyond" — https://onlinelibrary.wiley.com/doi/10.1002/esp.3462
2. Harel, Mudd & Attal 2016, global stream power law analysis — https://doi.org/10.1016/j.geomorph.2016.05.035
3. Whipple & Tucker 1999, dynamics of the stream power model — https://doi.org/10.1029/1999JB900120
4. Smith & Fox 2024, concavity index constraints — https://eprints.bbk.ac.uk/id/eprint/54285/1/Smith_and_Fox_2024_Concavity.pdf
5. Mudd et al. 2018, "How concave are river channels?" — https://esurf.copernicus.org/articles/6/505/2018/esurf-6-505-2018.html
6. Goren et al. 2014, constraining the SPL with FastScape + inversion — https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf
7. Pelletier 2021, controls on hydraulic geometry (Dunne–Jerolmack dataset) — https://doi.org/10.5194/esurf-9-379-2021
8. Parker et al. 2007, quasi-universal bankfull geometry — https://doi.org/10.1029/2006jf000549
9. NRCS NEH Part 654 Ch. 9, alluvial channel design (Bray/Nixon/Hey tables) — https://irrigationtoolbox.com/NEH/Part%20654/CHAPTERS/Chapter-09.pdf
10. Julien & Wargadalam 1995, alluvial channel geometry — https://doi.org/10.1061/(asce)0733-9429(1995)121:4(312)
11. Lamb et al. 2015, fluvial bedrock erosion mechanics — https://lamb.caltech.edu/documents/19653/Lamb_etal_Geomorphology_2015.pdf
12. Sklar & Dietrich 2004, saltation-abrasion model — https://doi.org/10.1029/2003wr002496
13. Turowski, Lague & Hovius 2007, exponential cover effect — https://doi.org/10.1029/2006jf000697
14. Beer et al. 2015, bedload transport controls bedrock erosion (Erlenbach) — https://esurf.copernicus.org/articles/3/291/2015/esurf-3-291-2015.pdf
15. Aubert et al. 2016, bedrock incision by bedload (DNS) — https://doi.org/10.5194/esurf-4-327-2016
16. Johnson & Whipple 2010, experimental bedrock incision controls — https://doi.org/10.1029/2009jf001335
17. Snyder et al. 2003, stochastic floods and erosion thresholds — https://doi.org/10.1029/2001jb001655
18. Horton 1945, erosional development of streams — https://pdfs.semanticscholar.org/39c3/9bbea565f8f963309e65506d7756f6571c18.pdf
19. Bryndal 2015, Horton's and Schumm's laws in Carpathians — https://doi.org/10.1515/quageo-2015-0008
20. Rigon et al. 1996, On Hack's law — https://doi.org/10.1029/96wr02397
21. Seybold et al. 2018, shapes of river networks (aridity) — https://royalsocietypublishing.org/doi/10.1098/rspa.2018.0081
22. Wikipedia, Hack's law (delta Hack's-law claim, flagged) — https://en.wikipedia.org/wiki/Hack%27s_law
23. Collins & Bras 2010, drainage density and climate — https://doi.org/10.1029/2009wr008615
24. NetMap, drainage density topic page — https://www.netmaptools.org/Pages/NetMapHelp/drainage_density.htm
25. La Barbera & Rosso 1989, fractal dimension of stream networks — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/WR025i004p00735
26. Beer & Borgas 1993, Horton's laws and fractal nature of streams — https://doi.org/10.1029/92wr02731
27. Kovchegov & Zaliapin 2020, random self-similar trees / Horton laws — https://zaliapin.github.io/pubs/KZ_PS2020.pdf
28. Kovchegov, Zaliapin & Foufoula-Georgiou 2022, critical Tokunaga model — https://doi.org/10.1103/physreve.105.014301
29. Gupta & Mesa 2014, Horton laws for hydraulic-geometric variables — https://doi.org/10.5194/npgd-1-705-2014
30. Zaliapin et al. 2013, are American rivers Tokunaga self-similar — https://efi.eng.uci.edu/papers/efg_128.pdf
31. Roering, Kirchner & Dietrich 1999, nonlinear diffusive sediment transport — https://doi.org/10.1029/1998wr900090
32. Heimsath, Furbish & Dietrich 2005, depth-dependent transport — https://doi.org/10.1130/g21868.1
33. Roering et al. 2001a, hillslope evolution experiment (Geology) — https://doi.org/10.1130/0091-7613(2001)029
34. Roering et al. 2001b, nonlinear transport steady state & timescales (JGR) — https://doi.org/10.1029/2001jb000323
35. Carson 1977, angles of repose and talus slopes — https://doi.org/10.1002/esp.3290020408
36. Chandler 1973, inclination of talus — https://doi.org/10.1086/627804
37. Francou & Manté 1990, alpine talus profile segmentation — https://doi.org/10.1002/ppp.3430010107
38. Frontiers 2022, angle of repose in loess (Shields 33° context) — https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2021.777467/full
39. Perron, Kirchner & Dietrich 2009, evenly spaced ridges and valleys — https://doi.org/10.1038/nature08174
40. Perron et al. 2008, controls on spacing of first-order valleys — https://doi.org/10.1029/2007jf000977
41. Schumm 1956, Perth Amboy badlands — https://pdodds.w3.uvm.edu/research/papers/others/1956/schumm1956a.pdf
42. Howard 1997, badland morphology and evolution — http://geomorphology.sese.asu.edu/Papers/Howard_ESPL_97.pdf
43. Braun & Willett 2013, O(n) implicit stream power solver — https://doi.org/10.1016/j.geomorph.2012.10.008
44. FastScapeLib documentation (flexure, SPL+transport) — https://fastscape.org/fastscapelib-fortran/
45. LandLab FastScape stream power component — https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html
46. Willett et al. 1999/2014, uplift, shortening, steady-state topography — https://ajsonline.org/article/88260-uplift-shortening-and-steady-state-topography-in-active-mountain-belts.pdf
47. Whipple 2004, bedrock rivers and geomorphology of active orogens — https://www.eoas.ubc.ca/~mjelline/453website/eosc453/E_prints/newfer06/2004whippleAREPS.pdf
48. Loget et al., wave train model for knickpoint migration — https://archimer.ifremer.fr/doc/00000/11075/8056.pdf
49. Whittaker et al. 2011 (JGR), knickpoint retreat rates and response times — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2011JF002157
50. Flowers & Farley 2012 / Science summary, ancient Grand Canyon — https://www.science.org/doi/10.1126/science.1229390
51. Karlstrom et al. 2014, Grand Canyon 5–6 Ma through palaeocanyons — http://geomorphology.sese.asu.edu/Papers/Karlstrom-2014-NatGeoscience.pdf
52. Fox et al. 2017, westernmost Grand Canyon incision — https://doi.org/10.1016/j.epsl.2017.06.049
53. Darling 2011, Colorado River incision rates (thesis) — https://digitalrepository.unm.edu/eps_etds/15
54. Polyak et al. 2008, Grand Canyon speleothem U-Pb ages — https://www.science.org/doi/10.1126/science.1151248
55. Braudrick et al. 2009, experimental meandering (PNAS) — https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/
56. Micheli & Larsen 2010, Sacramento cutoff dynamics — https://onlinelibrary.wiley.com/doi/10.1002/rra.1360
57. ERDC channel design manual (meander wavelength, W–Q) — https://erdc-library.erdc.dren.mil/bitstreams/81b728f8-6e6d-4ef8-e053-411ac80adeb3/download
58. Gilbert 1907, rate of recession of Niagara Falls (USGS Bull. 306) — https://pubs.usgs.gov/bul/0306/report.pdf
59. SERC vignette, deglaciation and Niagara knickpoint — https://serc.carleton.edu/vignettes/collection/25474.html
60. IJC, Niagara Falls is moving — https://www.ijc.org/en/niagara-falls-moving
61. NYSGA 1982 guide, glacial/engineering geology of Niagara — https://ottohmuller.com/nysga2ge/Files/1982/NYSGA%201982%20B2%20-%20Glacial%20And%20Engineering%20Geology%20Aspects%20Of%20The%20Niagara%20Falls%20And%20Gorge.pdf
62. Wright & Coleman 1973, delta morphology vs wave/river regimes — https://doi.org/10.1306/819a4274-16c5-11d7-8645000102c1865d
63. Chamberlain et al. 2018, Mississippi delta anatomy (Science Advances) — https://www.science.org/doi/10.1126/sciadv.aar4740
64. Vulis et al. 2023, delta morphotypes from shoreline characterization — https://research-portal.uu.nl/ws/files/235410242/Geophysical_Research_Letters_-_2023_-_Vulis_-_River_Delta_Morphotypes_Emerge_From_Multiscale_Characterization_of_Shorelines.pdf
65. Yang et al. 2025, Mississippi birdfoot wetland gain/loss — https://doi.org/10.1029/2024ef005003
66. LibreTexts Coastal Dynamics, delta classification — https://geo.libretexts.org/Bookshelves/Oceanography/Coastal_Dynamics_(Bosboom_and_Stive)/02%3A_Large-scale_geographical_variation_of_coasts/2.07%3A_Process-based_classification/2.7.3%3A_Classification_of_deltas
67. Blair & McPherson 1994, alluvial fans vs rivers — https://doi.org/10.1306/d4267dde-2b26-11d7-8648000102c1865d
68. Blair 1999, waterlaid Anvil Spring fan — https://doi.org/10.1046/j.1365-3091.1999.00259.x
69. Novak et al. 2022, sieve deposits on an active fan — https://doi.org/10.1002/esp.5508
70. Milana 2010, the sieve lobe paradigm — https://ri.conicet.gov.ar/handle/11336/101758
71. Kao & Milliman 2008, water and sediment discharge from Taiwanese rivers — https://doi.org/10.1086/590921
72. Dadson et al. 2003, erosion–runoff–seismicity in Taiwan (Nature) — https://www.nature.com/articles/nature02150
73. Fuller et al. 2003, erosion rates for Taiwan mountain basins — https://doi.org/10.1086/344665
74. GFZ study, ¹⁰Be(met)/⁹Be upper limit of denudation, Liwu — https://gfzpublic.gfz.de/rest/items/item_5008148_2/component/file_5008477/content
75. Egholm et al. 2013, lifespan of mountain ranges (Nature) — https://preview-www.nature.com/articles/nature12218
76. Pelletier 2004, piedmont deposition and mountain-belt denudation timescale — https://doi.org/10.1029/2004gl020052
77. Bezerra et al. 2018, persistence of topography in ancient belts (EGU abstract) — https://meetingorganizer.copernicus.org/EGU2018/EGU2018-19677.pdf
78. Clark et al. 2006, relict landscape of eastern Tibet — https://doi.org/10.1029/2005jf000294
79. Willett et al. 2014, dynamic reorganization of river basins (Science) — http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf
80. Forte & Whipple 2018, divide stability criteria and tools — https://www.sciencedirect.com/science/article/abs/pii/S0012821X18302292
81. Ye et al. 2024, cross-divide channel-head elevation controls divide migration — https://doi.org/10.1002/esp.5892
82. Oliveira et al. (RBG technical note), inferring divide migration and capture — https://rbg.emnuvens.com.br/rbg/article/download/2797/386387055/386400679
83. Chen et al. 2012, scale independence of basin hypsometry, steady state — https://www.sciencedirect.com/science/article/abs/pii/S0169555X1200205X
84. opengeology.in, hypsometric curve and integral — https://opengeology.in/hypsometric-curve-and-integral/
85. Duan et al. 2022, hypsometric integral of Loess Plateau basins — https://doi.org/10.3389/feart.2022.827836
86. Bhattacharjee 2022, hypsometry operator evaluation — https://doi.org/10.56975/ijcsp.v12i2.303932
87. Barnes, Lehman & Mulla 2014, Priority-Flood — https://doi.org/10.1016/j.cageo.2013.04.024
88. Barnes 2013 depression reference implementation — https://github.com/r-barnes/Barnes2013-Depressions
89. Cordonnier, Bovy & Braun 2019, linear-complexity flow routing in depressions — https://doi.org/10.5194/esurf-7-549-2019

---

## Appendix A — Forty further questions

1. **How should SPIM erodibility K vary spatially for a game biome map?** Map lithology bands to 2–4 orders of magnitude; climate scales it further (Harel 2016 shows insensitivity of θ but strong K variation). Disposition: implementable from Section 1.2's ranges.
2. **What timestep is safe for the Braun–Willett implicit SPIM?** Formally stable at any dt, but transient accuracy degrades (LandLab docs, citing Braun & Willett App. B) — use dt ≤ ~0.1×(dx/celerity) for faithful transients; larger for equilibrium-only runs.
3. **D8 vs D∞ for game rivers — which looks better?** D8 for the carving step (crisper channels, matches SPIM derivation), D∞ if you need smooth accumulation for moisture/biome maps. Disposition: engineering judgment from Section 11.
4. **How do you prevent priority-filling from drowning your map in lakes?** Use Priority-Flood+epsilon (fills to just-draining) or the carving variant; reserve real lakes for explicitly-marked closed basins (Barnes Alg. 3/4).
5. **What's the cheapest way to get Hack's-law-correct basins?** Don't enforce it directly — it emerges from SPIM evolution + the A_c threshold; check it as a validation metric (h≈0.5–0.6 emerges; Seybold 2018).
6. **Can you run SPIM on a heightmap at voxel resolution (7.8 mm voxels)?** No — run it on a coarse macro-grid (10–100 m cells), then displace/detail downward with noise constrained not to invert drainage. Disposition: engineering call.
7. **How do terraces form in an LEM?** Climate-cycle discharge/sediment-load oscillation on top of steady incision leaves perched floodplains; needs a transport-limited or cover-effect model, not pure detachment SPIM. Disposition: literature-consistent (Section 8.3), not implemented in minimal pipeline.
8. **What sets drainage density in your generator?** The channel-head threshold A_c; D_d ~ A_c^{-1/2} (Collins & Bras 2010). Pick A_c per biome from Section 3.3's climate story.
9. **How do you render an oxbow?** Detect cutoff events in the meander simulation, keep the abandoned loop as a water body, shrink it ~exponentially (fine-sediment plug + evaporation). Disposition: standard, rates from Section 7.3.
10. **What is the sinuosity distribution of real rivers?** Sacramento bends average ~1.4, cutoff-prone ~2.0 (Micheli & Larsen); full distribution unverified — use 1.2–2.2 as the working band.
11. **Why do deltas avulse?** Superelevation of the channel belt above the floodplain until a flood finds a steeper path; avulsion sets subdelta lifetimes (~1 kyr for Lafourche; Chamberlain 2018).
12. **How would you simulate delta shape selection?** Classify each coastal sink by (river sediment flux, nearshore slope, wave climate, tidal range) and pick a morphotype template (river/wave/tide; Wright & Coleman; Vulis 2023).
13. **What grain-size story does a sieve deposit carry into voxels?** Open-framework clast-supported gravel, downward coarsening, <2% mud (Novak 2022) — a distinctive material pass for fan fronts.
14. **How fast do alluvial fans aggrade?** Sieve events: >1000 m³ per >50 mm/24h storm, sub-annual frequency where active (Novak 2022); fan-scale long-term rates unverified — use event-based, not average-rate, generation.
15. **Does landscape memory matter at game timescales?** Only if your world has history: relict surfaces, windgaps, and captures are the visible memory (Sections 10.1–10.2); a "young" world can skip them.
16. **How do you implement divide migration?** Compute χ per node with θ_ref≈0.45–0.5, find cross-divide χ contrasts, nudge divide cells toward higher χ, re-route (Section 10.2 recipe; Willett 2014).
17. **When do χ-maps lie?** Non-uniform uplift, erodibility, or climate — cross-check with Gilbert metrics (Forte & Whipple 2018); if either flags instability, believe it.
18. **What's the game-facing test for "hydrologically correct"?** Every cell drains to a coast or marked sink; slope–area plots in log-log show θ≈0.45–0.5 above A_c; valley spacing matches L_c; Hack exponent 0.5–0.6.
19. **How many SPIM iterations to reach steady state?** Scales as (relief)/(U·steps); with the implicit solver, tens to hundreds of macro-steps — bounded by your patience, not stability (Braun & Willett 2013).
20. **Should uplift be uniform?** No — a tilted or gradient uplift field produces asymmetric ranges and migrating divides (Willett 2014 model setup), which is exactly the interesting terrain.
21. **How do you get glacial U-valleys in the same pipeline?** Out of scope here (Agent 3's domain); simplest hack is a curvature-dependent extra erosion term above the snowline. Disposition: deferred.
22. **What does n>1 do to your terrain?** Slower response to small perturbations, faster to large; produces taller, steeper transient relief; Harel 2016 says n≈2.4 is closer to reality than 1 — but n=1 is the cheap, well-behaved default.
23. **How does vegetation enter the erosion story?** Through K (erodibility), bank strength (meandering necessity, Braudrick 2009), and drainage density (the arid/humid non-monotonicity; Collins & Bras 2010).
24. **How do you validate against Earth statistics?** Compare slope distributions, hypsometric integrals (target 0.4–0.6), drainage density, Hack exponent, and slope–area concavity (Section 11 checks; parent Q79).
25. **Can knickpoints be player-triggered events?** Yes — a base-level drop (dam removal, earthquake, sea-level fall) injects a wave at celerity KA^mS^{n-1}; computing that per river is cheap (Section 5.3).
26. **How do you keep rivers from being grid-aligned?** D8 gives 45°-multiples; either route on the macro-grid then smooth/meander the centerline polyline, or use D∞/TIN (CHILD-style) for the routing layer.
27. **What's the failure mode of thermal-erosion-only passes?** They enforce a repose angle but produce no concave profiles and no area organization — ridges look right, valleys look wrong (Section 11 "skip SPIM" row).
28. **How do you pick the flood magnitude for an effective-erosion model?** Use the stochastic-threshold insight: erosion is dominated by rare large floods (Snyder 2003; Harel 2016 on n>1); a single "bankfull" rate underestimates and linearizes.
29. **How would you age a terrain after generation?** Run the same SPIM forward without uplift — relief decays ~exponentially with ~50 Myr e-folding (Pelletier 2004); or stamp relict surfaces and skips per Section 10.1.
30. **How do entrenched vs free meanders differ in the sim?** Free meanders migrate laterally; entrenched ones (Goosenecks) incise vertically with frozen planform — gate the migration term on incision rate vs lateral-erosion capacity. Disposition: mechanism standard, specific thresholds unverified.
31. **What controls where a river braids?** High sediment supply relative to capacity + weak banks (Section 7.4); in-game, check bed material and bank cohesion from biome/soil maps.
32. **How do you make a delta prograde visibly in gameplay time?** Real rates are 10–150 m/yr (Section 9.2); gameplay needs 10–100× that or a time-lapse mechanic — a deliberate, documented fudge.
33. **What is the cheapest flexure?** A spectral 2D FFT solution of the biharmonic plate equation (FastScape's own; docs give E=10¹¹ Pa, ν=0.2) — worth it only for continent-scale tiles.
34. **How do you keep SPIM from sawtooth-instability on steep slopes?** The implicit ordered-sweep solver exists precisely for this (Braun & Willett 2013); explicit schemes need dt ∝ dx²/K.
35. **Do you need the cover effect in a game?** Only if sediment routing matters (deltas, fans, valley fills); otherwise K alone suffices — but a tools/cover toggle is one multiply per node (Section 1.6 forms).
36. **How does base level at a lake differ from the sea?** Lakes can fill or drain (near-instant base-level change); Messinian-style 1500 m falls produce canyon-and-terrace cascades (Loget; Section 5.3).
37. **What density of streams is "too many"?** Above ~10–15 km/km² in a vegetated biome you've set A_c too low or D too high; badlands can justify 50–100+ (Section 3.3).
38. **How do you place waterfalls?** At lithologic breaks and along migrating knickpoints; height set by caprock/plunge-pool mechanics (Niagara is the archetype, Section 8.2) — deterministic placement on K-field discontinuities.
39. **What's the honest ceiling for real-time fluvial realism?** Full macro-hydrology precomputed; run-time effects limited to water rendering, localized erosion decals, and event-driven knickpoints (parent Q80's answer, in brief).
40. **What single number best diagnoses a fake procedural landscape?** The slope–area concavity: fBm noise gives θ ≈ 0 (no area dependence); any real-or-simulated fluvial terrain shows θ ≈ 0.4–0.6 above the channel head (Sections 1.3–1.4). If θ≈0, no river ever flowed there.


# Part 3 - Glacial, Coastal and Periglacial Terrain: Ice and Waves as Sculptors

**Scope:** Part 3 of the merged Earth-terrain reference. How glaciers carve (the "peaks of peaks" question), what they deposit, how the Quaternary ice ages overprinted nearly every landscape on Earth, how coasts erode and build, and how periglacial processes texture everything cold. Wave *hydrodynamics* (dispersion, orbital motion, breaking, shoaling) is deliberately not re-derived here — see `water-physics-and-wave-simulation.md` §2; this document treats waves as an **erosion and transport agent** only. Every measured value is a range with a source; anchor disagreements with the parent question bank are flagged inline as **[anchor correction]**.

**How to read this:** §1–2 answer the user's "why are glaciated mountains peaky" question mechanistically and quantitatively — read those first. §3 is the controversial part. §4–5 are the lowland and overprint story. §6–7 coasts. §8–9 periglacial. §10 is the opinionated simulation synthesis.

---

## 1. Glacial erosion mechanics

### 1.1 How ice moves: the two transport modes

A glacier is not a bulldozer; it is a viscous fluid that erodes only where it touches bedrock and only where that contact is *wet*. Ice velocity is the sum of internal deformation (Glen's flow law) and basal sliding:

$$u_{surf} = \underbrace{\frac{2A}{n+2}\left(\rho g \sin\alpha\right)^n h^{n+1}}_{\text{internal deformation}} + u_s, \qquad n \approx 3$$

with $A$ the temperature-dependent ice softness ($2.4\times10^{-24}\,\mathrm{s^{-1}Pa^{-3}}$ temperate, $1.7\times10^{-24}$ cold), $h$ ice thickness, $\alpha$ surface slope ([Cook et al. 2020](https://www.nature.com/articles/s41467-020-14583-8)). The two terms matter because erosion couples almost entirely to the *sliding* component $u_s$:

- **Internal deformation** is what a frozen-to-bed (cold-based) glacier does. The bed does not move relative to ice. Cold-based ice is essentially non-erosive — it can preserve a landscape beneath it for a full glacial cycle (this is why relict surfaces survive under the former Fennoscandian and Laurentide sheets; [Kleman et al. via Hättestrand](https://www.cambridge.org/core/journals/annals-of-glaciology/article/size-distribution-of-two-crosscutting-drumlin-systems-in-northern-sweden-a-measure-of-selective-erosion-and-formation-time-length/FDE73F476077D5946D1C0C43357D4961)).
- **Basal sliding** requires the bed at pressure-melting point plus liquid water. Sliding speed is strongly enhanced by high basal water pressure (which reduces effective stress $N = P_i - P_w$) and by deformable sediment. This is where all the geomorphic work happens.

The practical trigger for a simulation: **erosion requires warm-based ice**, and the erosion rate is a steeply nonlinear function of sliding velocity.

### 1.2 Quarrying vs abrasion

Two distinct mechanisms, and they scale differently:

**Abrasion** — debris entrained in basal ice is dragged across the bed like sandpaper, producing rock flour (silt) and striations. Hallet's theory has the abrasion rate proportional to the product of the contact force of a clast against the bed and the clast's drag velocity — both scale with sliding, giving roughly $E \propto u_s^2$. Abrion needs *tools* (debris) and is suppressed when a till layer cushions the bed.

**Quarrying (plucking)** — ice freezes onto fracture-bounded blocks in the lee of bed obstacles (where water pressure fluctuates in cavities), and sliding pulls them out. Quarrying is episodic, produces the coarse debris that then feeds abrasion, and is favored by bedrock that is jointed, thinly bedded, or stressed by water-pressure cycles at the bergschrund and down-valley of bed steps ([Hooke's model via Belknap 2019](https://doi.org/10.17615/adg6-ps54); Iverson 2012 predicts an erosion exponent $l < 1$ for weak rock). Field sediment budgets suggest quarrying is the dominant volumetric process where the bed is intact rock — abrasion cannot generate the blocks it abrades with.

The signature of this split shows up in the landform: abrasion-dominated beds are smooth and striated; quarrying-dominated beds are knocked-over, crag-and-tail, stepped.

### 1.3 The erosion law used in models

Nearly every glacial landscape evolution model (LEM) since Braun et al. (1999) uses one rule:

$$\boxed{E = K_G\, u_s^{\,l}}$$

- **Single-glacier fits:** $l \approx 2$. Herman et al. (2015) constrained this on Franz Josef Glacier, New Zealand by simultaneous sliding-velocity and erosion mapping: $l = 2.02$, $K_G \approx 10^{-4}$ (dimensionless), with erosion $\propto u_s^2$ ([Herman et al. 2015, Science](https://doi.org/10.1126/science.aab2386)). Koppes et al. (2015) independently found $l = 2.34$–$2.62$ at Patagonian tidewater glaciers ([EGU abstract](https://meetingorganizer.copernicus.org/EGU2016/EGU2016-14074.pdf)). Both agree with the theoretical prediction for abrasion.
- **Landscape-scale fits:** $l \approx 0.65$. Cook/Swift et al.'s 38-glacier global compilation found $l \le 1$ when glacier-averaged sliding is regressed against glacier-averaged erosion, and they argue the discrepancy is real: erosion is optimized in the upper ablation zone just below the ELA (thick ice + meltwater flushing + high water pressures), so along-profile averaging flattens the single-point slope ([Cook et al. 2020, Nat. Commun.](https://www.nature.com/articles/s41467-020-14583-8)).
- **State of the art (2025):** a 181-rate global synthesis plus elastic-net regression finds velocity is *not* the dominant predictor in any environment — mean annual precipitation, latitude (temperature), and glaciological/geological variables compete with or beat it; 99% of the world's glaciers erode between 0.02 and 2.68 mm/yr ([Norris et al. 2025, Nat. Geosci.](https://www.nature.com/articles/s41561-025-01747-8)).

**Engineering call:** for a *single valley* simulation use $l=2$; for *regional* sediment budgets use $l \lesssim 1$ and add a precipitation multiplier. Both are defensible; mixing them is not.

### 1.4 Measured erosion rates — and the ice-vs-water verdict

Basin-averaged glacial erosion rates span $10^{-2}$ to $>10$ mm/yr across four orders of magnitude. The anchor range of 0.1–10+ mm/yr is **confirmed as the typical band for temperate alpine glaciers** ([Koppes & Montgomery 2009](https://glaciers.pdx.edu/fountain/readings/TopicsInGeomorphology/Koppes&Montgomery2009_GlacialVsFluvialErosion.pdf)):

| Setting | Rate |
|---|---|
| Polar/cold-based | 0.01–0.1 mm/yr |
| Continental/ice-sheet interior | ~0.26 mm/yr (log-mean) |
| Alpine, land-terminating (temperate) | ~0.58 mm/yr (log-mean), commonly 0.1–10 |
| Alpine tidewater (Alaska, Patagonia) | ~2.2 mm/yr log-mean; >10 mm/yr when rapidly retreating |
| Global glacial log-mean | 0.51 mm/yr |
| Global fluvial log-mean (for contrast) | 0.067 mm/yr |

The 400+-measurement compilation of Wilner et al. (2024) found glacial exceeds fluvial by roughly an order of magnitude *globally*, after showing the apparent timescale dependence (the "Sadler effect") is a measurement artifact — and this ratio survives correction for precipitation, hillslope, and latitude ([Wilner et al. 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11661439/)). Two caveats from Koppes & Montgomery that the game designer should internalize: (1) glacial rates *measured since the Little Ice Age* are transient spikes — averaging over full glacial cycles reduces them 10×, over the whole Quaternary 100×, because re-advancing ice must first erode through its own old sediment cover; (2) fluvial rates in rapidly uplifting ranges (Himalaya, Taiwan: >10 mm/yr) match the fastest glaciers. **Ice wins on average, not by default.**

### 1.5 Why ice erodes differently from water

Three structural differences, all of which shape terrain:

1. **No sediment-capacity limit in the same sense.** A river's carrying capacity saturates: overload it and it deposits, aggrades, and stops incising. A glacier's "capacity" is set by its internal stress field and the freezing-on of debris at depth — it entrains whatever it quarries and keeps it until it melts out. There is no glacial analogue of an alluvial river's grade. Consequence: a glacier can keep cutting *below* the elevation its own outlet allows.
2. **Overdeepening.** Because erosion scales with ice discharge (thickness × velocity) rather than local slope, a glacier erodes deepest where flow is constricted and thick — which is often *upstream* of a widening, producing closed basins whose floors lie below their outlet thresholds (riegers and rock bars). Overdeepened basins of several hundred m depth are routine under modern glaciers (Gamburtsev Mountains: 432 m of overdeepening beneath 3–16 km-long basins; [Overdeepening overview](https://en.m.wikipedia.org/wiki/Overdeepening)). Water could never do this: a river that cut a closed basin would fill it with a lake and decouple.
3. **Lateral erosion ≈ vertical erosion.** Ice is 100–1000× more viscous than water; a valley glacier fills its valley and abrades the *walls* as well as the floor, widening as it deepens (see §2.3). Water concentrates stress in a thread and undercuts walls only indirectly, by mass-wasting the slopes it steepens.

---

## 2. Alpine glacial landforms, quantitatively — the "peaks of peaks" answer

### 2.1 The cirque → arête → horn sequence

A cirque is an armchair hollow: steep headwall, overdeepened floor, and a rock lip (riegel) at the outlet. The growth sequence that produces peaky mountains is:

1. **Nivation initiation.** Snow patches in a pre-glacial hollow foster freeze-thaw weathering and the hollow deepens.
2. **Cirque glacier occupation.** Once ice is thick enough to rotate (rotational sliding in the bergschrund zone), subglacial erosion focuses on the floor and the base of the headwall; frost weathering and rockfall attack the headwall itself. Headward retreat of the headwall outpaces lateral widening: Oskin & Burbank's Kyrgyz Range reconstruction found headwall retreat ≈ 3× vertical incision; Naylor & Gabet measured 4:1 lateral:vertical in the Bitterroots ([Belknap 2019 thesis summary](https://doi.org/10.17615/adg6-ps54)).
3. **Intersection.** As cirques on opposite sides of a ridge each eat inward, the ridge thins to a knife-edge **arête**; where three or more cirque headwalls intersect from three directions, the surviving pyramid is a **horn** (Matterhorn, the type example).
4. **Trough extension.** When the cirque glacier overtops its lip and becomes a valley glacier, the erosion center moves down-valley, and the cirque is left "hanging" above the trough head.

**Measured rates.** A 6-year terrestrial-lidar survey of deglaciating cirques in the Austrian Alps measured mean cirque-wall retreat of 1.9 mm/yr (max 10.3 mm/yr at the most deglaciated wall; glacier-proximal 10 m of wall retreats 7.6 mm/yr, an order of magnitude faster than distal sections) ([Hartmeyer et al. 2020](https://esurf.copernicus.org/articles/8/753/2020/esurf-8-753-2020.pdf)). Long-term, cirque growth is episodic and front-loaded: analysis of 2208 cirques in Britain and Ireland (mean depth 283 ± 108 m) vs modeled glacier-occupation times implies cirques attained most of their size in roughly the *first* glacial occupation of a fresh landscape — 1.0–3.4 mm/yr if continuous during one cycle — with growth slowing afterwards as they reach a "least-resistance" shape and trap protective sediment ([Barr et al. 2020](https://e-space.mmu.ac.uk/623163/1/Barr%20et%20al%20%28accepted%29.pdf)). In the steadily-uplifting Ben Ohau Range (NZ), ~750 ka of occupancy yields downcutting of 0.29 mm/yr and headwall retreat of 0.44 mm/yr, with no equilibrium yet ([Brook et al. 2006](https://www.kiphub.com/paper/61e5082dc5292ecd69f63b9f)).

**Cirque floor pinning to the snowline — the evidence.** Cirque floor altitudes (CFAs) consistently track paleo-ELAs: across Scandinavia's 3984 glacier-free cirques, CFAs and modern cirque-glacier ELAs both increase with distance from the coast and vary with aspect together ([Barr/Spagnolo lineage, via Scandinavian cirque study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X14005236)); in the southern Coast Mountains of BC, cirque floors follow the ELA's 1000 m coast-to-interior rise ([Evans 2017](https://doi.org/10.15551/prgs.2017.47)). The mechanism is Egholm's: a small cirque glacier's catchment cannot supply ice flux far below the ELA, so the ELA acts as a **local erosional base level** — the cirque floor is dragged to just below the snowline and stalls ([Egholm et al. 2009](https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf)). Caveat from the Gangdise Mountains: where relief or a strong monsoon interferes, CFA tracks crest altitude more than climate, and is not a reliable ELA proxy at regional scale ([Cui et al. 2022](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2022.900141/full)).

### 2.2 The quantitative answer: why glaciated ranges are "peaky"

Stack the four mechanisms and the morphology falls out:

**(a) Differential erosion by ice-surface position.** Sliding velocity (hence erosion, $u_s^2$) peaks near and just below the ELA and in trunk confluences; it is near zero under the thin ice of high summit shoulders and under cold-based divides. So a glaciated range does not lower uniformly — it *selectively* gutters the terrain between high points, steepening what remains. A fluvial landscape, in contrast, has smoothed convexo-concave slopes because hillslope diffusion and landslides relax relief faster than the river network can focus it.

**(b) Headwall retreat creates over-steepened above-ELA rock.** Cirque headwalls retreat at mm/yr — slow, but over 10⁵–10⁶ yr that is 10²–10³ m of horizontal retreat per side, intersecting to arêtes at 40–60° and horns. Frost cracking is most intense in the cold, rock-window just above the glacier, so the peaks are attacked from below by ice and by frost from their own slopes.

**(c) U-valleys leave benchy shoulders.** The trough walls rise at 25–40° from a flat floor — a *step change* in slope profile that a fluvial valley (smooth V, concave walls) never shows (§2.3). Truncated spurs are the amputated interfluves of the pre-glacial V-valley, sliced by the trough wall; their triangular facets are the fastest visual cue of "glaciated" in any terrain renderer.

**(d) Hanging valleys and valley steps add vertical rhythm.** Tributary troughs hang 50–700 m above trunk floors (Sognefjord's tributary fjords hang ~400 m at their mouths vs 1100–1300 m in the trunk; [Nesje 2024](https://njg.geologi.no/wp-content/uploads/2024/10/241003_Nesje.pdf)), and long-profiles are stepped with rock basins, so every glaciated valley reads as a staircase of flats and headwalls — waterfalls at every step — instead of a smooth fluvial concavity.

The one-line summary for the engine: **fluvial terrain is a diffusion-smoothed surface drained by a concave network; glacial terrain is a stepped, over-deepened gutter network between frost-sharpened residuals above the old snowline.**

### 2.3 U-valley / trough cross-section geometry

The quantitative literature does **not** primarily use a width:depth ratio — it fits power laws and polynomials. **[anchor correction]** The parent bank's "U-valley width:depth ~2–4?" could not be confirmed as a canonical literature statistic; the field instead uses these:

- **Power law** (Svensson 1959, the founding fit): $y = a x^b$ per half-profile, $x$ horizontal distance from the valley lowest point. Svensson's Lapporten (northern Sweden) fit: $b = 2.045$ and $b = 2.177$ — i.e., essentially a true parabola ($b=2$) ([Svensson 1959, J. Glaciol.](https://www.cambridge.org/core/journals/journal-of-glaciology/article/is-the-crosssection-of-a-glacial-valley-a-parabola/EE16DB83DA62DE75D671DE955242CD43)).
- **Global range of $b$:** mostly 1.3–2.5 (71.4% of Tian Shan profiles), full range 1.0–5; $b\to2$ with maturity but is strongly controlled by rock-mass strength and lithology ([Li et al. 2001, Tian Shan](https://www.sciencedirect.com/science/article/abs/pii/S0169555X00000787)). Graf's Sierra Nevada work found valleys between normal parabola ($b=2$) and semicubic parabola ($b=1.5$) ([Graf 1970](https://scholarcommons.sc.edu/cgi/viewcontent.cgi?article=1041&context=geog_facpub)).
- **Form ratio** $FR = D/W_f$ (depth ÷ top width, Graf's dimensionless descriptor) completes the description alongside $b$; higher-order trunk valleys are narrower and deeper (higher $FR$, higher $b$).
- **Harbor's modeling** (the standard numerical experiment): coupling a 2-D finite-element ice-flow cross-section to a sliding-erosion rule converts a V-shape into a recognizably glacial form in order $10^4$ yr, tending to a steady quasi-parabolic cross-section, with the high-discharge (glacial-maximum) phase dominating form development ([Harbor 1992, GSA Bulletin](https://doi.org/10.1130/0016-7606(1992)104)). Later experiments show spatially variable erodibility breaks the ideal U into compound forms ([Harbor 1995](https://www.sciencedirect.com/science/article/abs/pii/0169555X95000511)).

For a generator: draw valley walls as $y \propto |x|^{1.7 \pm 0.4}$ around the thalweg, add a flat till floor 5–15% of width, and modulate $b$ with mapped "rock strength."

### 2.4 Hanging valleys, truncated spurs, valley steps, rock basins

Covered mechanistically in §2.2. Numbers worth keeping: hanging-valley lips typically 50–150 m (alpine) and up to 400–700 m (fjord systems); valley steps plunge 10–100 m into rock basins (the cirque-scale riegel-and-basin pair); basins overdeepened by tens to hundreds of meters fill with tarns or till. Long-profile "knickpoints" under ice form and migrate like fluvial ones but with the overdeepening instability (steps steepen the bed → supercooled water freezes on → sediment armors the crest → erosion focuses up-glacier of the step).

### 2.5 Fjords — the extreme trough

Fjords are glacial troughs drowned by the sea. **[anchor check]** Parent anchor "fjord max depths ~500–1300 m" — **confirmed at the top end but understated globally**: Sognefjord reaches 1308 m below sea level (1303 m per Nesje's high-resolution survey) ([Wikipedia: Sognefjorden](https://en.wikipedia.org/wiki/Sognefjorden), [Nesje 2024](https://njg.geologi.no/wp-content/uploads/2024/10/241003_Nesje.pdf)); Skelton Inlet (Antarctica) reaches 1933 m and Messier Channel (Chile) ~1290–1358 m ([Wikipedia: Fjord](https://en.wikipedia.org/wiki/Fjord)).

**Sognefjord's numbers (the canonical case):**
- 205 km long, 1–5 km wide, max depth 1308 m; ~200 m of sediment puts bedrock ~1500 m below sea level at the deepest point.
- Mouth sill only ~100–200 m deep: the glacier, unconfined near the coast, spread out and lost erosive power, leaving a threshold (a textbook overdeepening signature).
- Tributary fjords hang 400–700 m above the trunk floor.
- Total Quaternary erosion of the drainage basin: 7610 km³, an average vertical erosion of 610 m over 2.57 Myr — 24 cm/kyr long-term average, but 1–3.3 mm/yr during glacial maxima (2 ± 0.5 mm/yr likely mean) ([Nesje et al. 1992 via academia.edu summary](https://www.academia.edu/30873948/Quaternary_erosion_in_the_Sognefjord_drainage_basin_western_Norway)).
- Ice thickness at LGM: nearly 3000 m over the fjord line; confluences of tributary fjords excavated the deepest basin ([Sognefjorden](https://en.wikipedia.org/wiki/Sognefjorden)).

**Why fjords get so deep:** the full stack of §1's "different from water" — ice thickness (up to km-scale, so basal shear stress stays high even on low-gradient beds), no capacity limit, and the confluence effect (tributary ice fluxes sum, so discharge-∝-discharge erosion concentrates in trunk reaches). Add structural preconditioning: fjords follow fault and lineament zones (Patagonian fjord orientations match strike-slip structure almost 1:1; [Glasser & Ghiglione 2009 lineage](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004583)), and pre-Quaternary fluvial incision of the Paleic surface gave the ice pre-cut valleys. Mean Quaternary glacial erosion rates in western Norway's troughs: 1–2 mm/yr.

---

## 3. The glacial buzzsaw

### 3.1 The hypothesis

The **glacial buzzsaw** (Brozović, Burbank & Meigs 1997) posits that glacial and periglacial erosion, concentrated at and above the equilibrium-line altitude (ELA), caps mountain-belt elevation *irrespective of uplift rate*: terrain lifted above the snowline is rapidly denuded, so the range's hypsometry (area-vs-elevation distribution) piles up just below the ELA and cannot build a tall plateau above it ([Brozović et al. 1997, Science](https://doi.org/10.1126/science.276.5312.571)).

### 3.2 The evidence for

- **NW Himalaya (the birthplace):** mean and modal elevations, hypsometry, and slope distributions correlate with glaciation extent while being independent of exhumation rate varying across the region (Brozović et al. 1997, above).
- **Washington Cascades:** summit elevations and range morphology track the modern and Pleistocene ELAs; Mitchell & Montgomery (2006) is the second pillar ([via Evans 2017](https://doi.org/10.15551/prgs.2017.47)).
- **Global analysis:** Egholm et al. (2009) showed with global 1°×1° SRTM tiles that hypsometric maxima almost never lie above the upper limit of Pleistocene ELA fluctuation — across orogenic ages and tectonic styles — and most summit elevations sit within ~1500 m of the local snowline. Their coupled erosion–isostasy model self-consistently produces the signature: erosional destruction above the snowline plus isostatic uplift of what remains drives elevations into a window just below it ([Egholm et al. 2009, Nature](https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf)).
- **Cirque-ELA coupling:** Mitchell & Humphreys (2015) formalized the cirque-floor/ELA relationship — cirque floors sit at a fixed offset below cirque-crest summits, and the offset tracks ELA across ranges ([Geology 43, 35–38](https://doi.org/10.113Q/G36180.1)).
- **Erosion-rate support:** the global compilation showing glacial ≫ fluvial rates, with the gap widening toward the latitudes where ranges intersect snowlines (Wilner et al. 2024, §1.4 above).

### 3.3 The criticisms — where it breaks

- **Southern Patagonia (the counter-example):** north of ~45°S the Andes match the snowline and thermochronology shows an erosion minimum at the ELA on the windward flank (buzzsaw-consistent). South of 45°S the divide rises *well above* the LGM and modern ELA, and apatite (U-Th)/He ages >10 Ma show the ice was too cold-based to erode: glaciation there **protected** the orogen and let it grow taller — the first recognized case of regional glacial protection ([Thomson 2010](http://geomorphology.sese.asu.edu/Papers/Thomson_Glaciation_andes.pdf)).
- **The Himalaya itself:** van der Beek et al. (2009) argue the NW-Himalaya hypsometric-maximum-near-ELA correlation is a coincidence of pre-existing low-gradient surfaces near that elevation — glaciation "ornamented" inherited relief rather than cutting it ([via Glacial limitation of tropical mountain height, ESurf 2019](https://esurf.copernicus.org/articles/7/147/2019/)).
- **Inheritance at global scale:** Hall & Kleman (2013) argue plateau fragments cited as buzzsaw products are mostly dissected pre-glacial relief; glaciers *destroy* low-relief surfaces rather than create them, and ELA-vs-summit correlations need not be causal ([Hall & Kleman 2013](https://doi.org/10.1016/j.yqres.2013.10.007)).
- **Mean elevation vs peaks:** the buzzsaw claim properly applies to *hypsometric maxima* (modal elevation), not peak elevations — peaks routinely rise >1 km above the ELA (Brozović et al. themselves note >1 km excesses). Evans (2017) finds it operates locally and patchily (Britain, BC, Romania all mix sharp intersecting-cirque ridges with gentle summit plateaus above cirques), and "Teflon peaks" — summits uplifted rapidly through the erosive zone into cold, frozen-on ice — complicate the ceiling story ([Evans 2017](https://doi.org/10.15551/prgs.2017.47)).
- **Pleistocene-acceleration doubt:** Schildgen et al. (2018) re-analyzed 30 sites where enhanced Pleistocene exhumation had been inferred and found 27 explainable by sampling bias or tectonic changes ([cited in ESurf 2019](https://esurf.copernicus.org/articles/7/147/2019/)).

### 3.4 The modern consensus

Widespread but incomplete acceptance: **a hypsometric maximum within the Pleistocene ELA band is a real and common signature of ranges that were extensively glaciated, and cirque-glacier base-level control is the accepted mechanism — but the buzzsaw is neither universal nor uniquely diagnostic.** It fails where ice was cold-based (southern Patagonia), where uplift outpaces the saw (large icefields: Patagonia, Karakoram, St. Elias — Evans's "Teflon peaks"), and where inheritance supplies the correlation (parts of the Himalaya). For terrain generation this is actually convenient: the buzzsaw gives you a *rule* (clamp modal elevation of glaciated ranges toward the ELA, let peaks overshoot ≤1–1.5 km) that is defensible for most mid-latitude ranges and observably wrong in specific places you can opt out of.

---

## 4. Depositional glacial terrain

Roughly 70% of Canada, 50% of Ireland, 40% of Scandinavia and 15% of Britain is covered by subglacial bedforms or their deposits ([Clark et al. 2009](http://nora.nerc.ac.uk/id/eprint/6960/1/Drumlins.pdf)) — this is what makes lowland terrain "glacially textured."

### 4.1 Moraines

- **Lateral/medial:** along valley sides; medial moraines form where two glaciers' lateral moraines merge at a confluence — the dark center-line stripe on any reference glacier photograph.
- **Terminal/recessional:** arcuate ridges of till at the farthest advance and at still-stands during retreat. Heights: meters to a few hundred m for major ice-lobe terminal moraines; typical alpine terminal moraines 10–100 m.
- **Ground moraine:** low-relief (<10 m) lodgement till plains behind the terminus.
- **Rogen (ribbed) moraine:** wavy ridges *transverse* to ice flow, typically 10–30 m high, 150–300 m wide, 300–1200 m long, in regularly spaced groups; formed by brittle fracture of subglacial sediment at frozen/thawed bed boundaries — which makes relict Rogen fields a *paleo-thermal map* of ice sheets ([Rogen moraine overview](https://en.wikipedia.org/wiki/Ribbed_moraine), [Hättestrand & Kleman 1999 via Nature](https://www.nature.com/articles/47005)).

### 4.2 Drumlins

The best-quantified glacial landform thanks to a 58,983-drumlin GIS database for Britain and Ireland ([Clark et al. 2009](http://nora.nerc.ac.uk/id/eprint/6960/1/Drumlins.pdf)):

- **Length:** 250–1000 m typical (mean 629 m); **width:** 120–300 m (mean 209 m); **relief:** 0.5–40 m.
- **Elongation ratio** $E = L/W$: most drumlins 1.7–4.1; mean 2.9. **[anchor correction]** The parent bank's "elongation ratios ~2–10?" overstates the bulk population — 2–10 spans drumlins *plus* the drumlin-to-mega-scale-glacial-lineation continuum, where MSGLs on ice-stream beds reach $E > 10$. Use 1.7–4.1 for classic drumlin fields; reserve >10 for paleo-ice-stream trunks.
- **Scaling laws:** $W \approx 7 L^{1/2}$ (m); maximum elongation $E_{max} \approx L^{1/3}$ — a sharp, unexplained ceiling on the data cloud (Clark et al. 2009).
- **Spacing:** nearest-neighbor peak at 200–450 m (mean 386 m), exceeding drumlin width (mean 226 m) — spacing is not just packing; drumlin fields are *patterned* (self-organized) phenomena ([Clark et al. 2017](https://doi.org/10.1002/esp.4192)).
- **Formation debate:** the ice-molding school (Boulton 1987: subglacial sediment deformation) vs the megaflood school (Shaw: catastrophic subglacial meltwater) — still genuinely polarized — vs the modern instability synthesis (Hindmarsh–Fowler: a flow–sediment coupling instability whose fastest-growing wavelength matches drumlin dimensions; Fowler & Chapwanya's version includes subglacial water erosion). The active drumlin field at Múlajökull, Iceland demonstrably forms by till deformation in years–decades, which tilts the debate but doesn't settle it for all fields.
- **Field examples:** Wisconsin and Ireland are the classic drumlin lands (Ireland's Clew Bay / Connacht fields among the densest); northern Sweden hosts ~9000 mapped lineations in one 22,500 km² study area alone; elongation correlates with cumulative ice slip displacement ([Zoet et al. 2021](https://doi.org/10.1002/esp.5192)).

### 4.3 Eskers

Long sinuous gravel ridges deposited in subglacial meltwater conduits (Röthlisberger channels). From a >20,000-esker Canadian database ([Storrar et al. 2014](https://shura.shu.ac.uk/12940/1/Storrar%20et%20al%20%282014%29%20QSR.pdf)):

- **Individual ridges:** mostly <10 km long, 1–3 km average, log-normal length distribution; ridge heights typically 5–50 m (morphology types with beads and tributaries classified for soft beds in [Poland](https://journals.ltn.lodz.pl/Acta-Geographica-Lodziensia/article/view/1840)).
- **Systems:** traceable up to 760 km; radial patterns fanning from former ice divides, with esker-free zones ~100–150 km wide under the divides themselves.
- **Sinuosity:** 87% of ridges between 1.00 and 1.10 (mean 1.06) — remarkably straight, because the conduits follow the *ice-surface* hydraulic gradient, not the bed (eskers show no preference to trend up or down bed slopes).
- **Lateral spacing:** preferred ~12 km, consistent with theory's 8–25 km.
- **The pattern they encode:** an esker system *is* the drainage network of the ice sheet — trunk, tributaries, and all — time-transgressively deposited as the margin retreated. For a game, an esker fan is literally a river network cast in gravel, frozen in the moment of deglaciation.

### 4.4 Kames, kettles, outwash

- **Kames:** steep, pitted mounds/terraces of sorted sand-and-gravel where meltwater deposited against or within stagnating ice; heights from meters to ~50 m ([Wohl teaching notes](https://sites.warnercnr.colostate.edu/wp-content/uploads/sites/93/2019/01/454part10b-student-2019.pdf)).
- **Kettles:** closed depressions (lakes when water-filled) from the melt-out of buried ice blocks; abundant in ice-contact and outwash deposits (the "pitted outwash plain").
- **Sandar/outwash plains:** flat, braided-river plains of sand and gravel beyond the terminus, in three zones — proximal (entrenched, kettled), intermediate (wide shifting braids), distal (sheet-flow). Jökulhlaup-driven surges of sediment build them.

### 4.5 Ice-contact vs outwash — the terrain-readable dichotomy

- **Ice-contact deposits** (kames, kame terraces, eskers in their ice-walled phase, hummocky moraine): **steep, pitted, chaotically sorted** — collapse structures where supporting ice melted out; 10–40 m of local relief per landform, no preferred downstream grading, kettle holes everywhere.
- **Outwash deposits** (sandur, valley trains): **flat, well-sorted, graded** — topped by braided-river bars; slopes of meters per km downstream; fines with distance.

Till itself: unsorted, unstratified, angular-to-subangular clasts in a fine matrix; lodgement till is dense and fissile, ablation till loose. Terrain code that maps "ice-contact = noise + pits, outwash = flats + braided channels, till = smooth low-relief cap" captures 90% of the visual signal.

---

## 5. The Quaternary overprint

### 5.1 The 40 ka → 100 ka switch

The Mid-Pleistocene Transition (MPT), between ~1.25 and 0.7 Ma, switched glacial cycles from mild ~41-kyr (obliquity-paced) oscillations to the harsh, asymmetric ~100-kyr cycles of the late Quaternary — with no change in orbital forcing ([Clark et al. 2006 review](https://www.sciencedirect.com/science/article/abs/pii/S0277379106002332)). Accepted cause candidates, and the current synthesis:

1. **Regolith removal** (Clark & Pollard 1998): pre-Quaternary North America and Fennoscandia were blanketed in 10–50 m of weathered regolith; glacial erosion stripped it over successive cycles, exposing high-friction crystalline bedrock. Result: post-stripping ice sheets slid less, grew thicker and more stable, could survive insolation maxima — hence longer cycles ([reviewed in Abe-Ouchi et al. 2020](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2020RG000727)).
2. **Declining atmospheric CO₂** (glacial minima dropping over the Pleistocene; glacial–interglacial CO₂ amplitude grew from ~43 to ~75 μatm across the MPT via Southern Ocean dust-fertilization feedbacks; [Chalk et al. 2017 PNAS](https://www.pnas.org/doi/abs/10.1073/pnas.1702143114)).
3. **The synthesis:** transient 3-Myr Earth-system modeling shows **both are needed** — CO₂ decline initiates NH glaciation and sets the amplitude; regolith removal controls the *timing* of the 41→100 kyr switch ([Willeit et al. 2019, Science Advances](https://pmc.ncbi.nlm.nih.gov/articles/PMC6447376/)). Later conceptual-modeling work favors a gradual driver over an abrupt threshold ([Bouchard et al. 2023](https://www.nature.com/articles/s43247-023-00754-0)). Note also: "100-kyr cycles" may really be 82/123-kyr integer multiples of the obliquity cycle (Huybers), not an eccentricity signal.

**Terrain relevance:** the MPT is why the *last* ~1 Myr of ice sheets were monstrous (3-km-thick Laurentide) while earlier ones were thinner and wider — the deep erosion of northern shields is mostly a post-MPT phenomenon.

### 5.2 Ice-sheet extents and the sea-level drop

- **Laurentide:** at LGM (26.5–19 ka), covered all of Canada east of the Rockies, reaching roughly to the Missouri and Ohio rivers, eastward to Manhattan; mid-continent the ice reached ~**38°N**; the margin included the sites of Boston, New York, Chicago, and St. Louis. Keewatin dome ~3200 m thick, Foxe-Baffin ~2200–2400 m; total volume 26.5–37 × 10⁶ km³ ([Laurentide ice sheet](https://en.wikipedia.org/wiki/Laurentide_ice_sheet), [LGM overview](https://en.wikipedia.org/wiki/Last_glacial_maximum)).
- **Fennoscandian/Eurasian complex:** third-largest ice mass; ~5.5 × 10⁶ km² and 7.2 × 10⁶ km³ (~17–20 m sea-level equivalent) at ~22.7 ka, spanning >4500 km from south of Ireland to Franz Josef Land; NW Russia reached its maximum as late as 18–16 ka ([Patton et al. 2016](https://www.sciencedirect.com/science/article/pii/S0277379116304498), [Patton et al. 2017](https://www.sciencedirect.com/science/article/pii/S0277379117302068)).
- **Sea level:** LGM lowstand −120 to −135 m (model reconstructions −116 m in Gowan et al. 2021 vs −130/−135 m in Lambeck et al.; [Gowan et al. 2021](https://www.nature.com/articles/s41467-021-21469-w)). **What it exposed:** Beringia (the Siberia–Alaska land bridge, dry and dusty with yedoma silts), Sundaland (Malaysia + western Indonesia joined to Asia), the Persian Gulf dry basin, and vast shelves worldwide. **What drowned again:** all of the above, plus the river valleys incised during the lowstand — now rias, estuaries, and the continental-shelf edge itself.

### 5.3 Post-glacial rebound

Measured present-day uplift rates, Gulf of Bothnia (the Fennoscandian maximum): **9.5–10.3 mm/yr** — the NKG2016LU model gives 10.3 mm/a absolute near Umeå (9.6 mm/a relative to the geoid) ([Vestøl et al. 2019](https://link.springer.com/article/10.1007/s00190-019-01280-8)), and the BIFROST GNSS field gives ~10 mm/yr with ~9.6 mm/yr of it GIA after elastic corrections ([Kierulf et al.](https://research.chalmers.se/publication/523916/file/523916_Fulltext.pdf)). **[anchor confirmed]** The parent bank's "~1 cm/yr Gulf of Bothnia" is right on. Consequences worth simulating: ~700 ha of new land emerges from the sea *per year* in Sweden/Finland; the former forebulge (Netherlands, northern Germany) is now *subsiding* >1 mm/yr; rebound is still ~90% complete only in the center. Fennoscandian total remaining uplift is of order 10⁵ mm (Ekman & Mäkinen 1996 deduced ~90 m of remaining apparent uplift in the center and the necessity of mantle mass inflow; [Ekman & Mäkinen 1996](https://doi.org/10.1111/j.1365-246x.1996.tb05281.x)). Northern Gotland's rauk coasts rose ~2 mm/yr through the late Holocene, preserving sea stacks by lifting them out of the wave zone ([Frontiers 2022](https://www.frontiersin.org/articles/10.3389/feart.2022.895419/pdf)).

### 5.4 Glacial lake outburst floods — Missoula and the Channeled Scablands

J Harlen Bretz argued from 1923 that the Channeled Scabland of eastern Washington — a 30,000 km² anastomosing complex of coulees, dry cataracts, rock basins, giant bars — was carved by catastrophic megaflood. The establishment resisted "catastrophism" for decades (Flint's "leisurely streams" counter-hypothesis); Pardee's 1942 proof that glacial Lake Missoula (2500 km³, dammed 650 m deep by the Purcell Trench lobe) had drained rapidly, plus Bretz's 1952 field season, swayed consensus; Bretz received the Penrose Medal in 1979 ([Baker 2009 retrospective](https://www.annualreviews.org/content/journals/10.1146/annurev.earth.061008.134726), [Bretz 1969](https://wpg.forestry.oregonstate.edu/sites/default/files/seminars/1969_Bretz.pdf)). This is the textbook vindication of catastrophism *within* uniformitarianism: the floods violate no physics, they were just rare.

**Numbers:**
- **Flood count:** dozens — likely more than a hundred — separate floods during 20–14 ka: at least ~75 before the ~16-ka Mount St. Helens Set-S tephra and 30+ after it; fill-drain cycles spaced up to ~100 yr early on, down to 1–2 yr for the last ([O'Connor et al. 2020/2021 review](https://www.sciencedirect.com/science/article/abs/pii/S0012825220302270)).
- **Discharge:** **[anchor confirmed]** peak ~$17$–$20 \times 10^6\,\mathrm{m^3\,s^{-1}}$ near the ice dam (model estimates 3–35 ×10⁶ across studies); 5–15 × 10⁶ m³/s through Wallula Gap and the Columbia Gorge; the downstream record resolves ≥25 floods >1×10⁶, ≥15 >3×10⁶, 6–7 >6.5×10⁶, and at least one at 10×10⁶ m³/s ([O'Connor et al.](https://people.wou.edu/~taylors/g322/Oconnor_etal_2021_ice_age_floods_review.pdf), [Benito & O'Connor 2003](https://people.wou.edu/~taylors/gs407rivers/benito03_fairbanks_divide_missoula_flood.pdf)). So "discharge ~10⁷ m³/s" is right for the largest floods *at the dam*, ~10⁶.⁵ averaged over the route.
- **Flow depths/scales:** 800 ft (~240 m) deep through Wallula Gap; mid-channel boulder bars >100 ft (~30 m) high; flood was ~2 weeks in duration; ~2000 mi² of basalt stripped of its loess cover ([Bretz 1969](https://wpg.forestry.oregonstate.edu/sites/default/files/seminars/1969_Bretz.pdf)).
- **Grand Coulee:** 40 km long, 270 m of rim-to-floor relief, 2–2.8 km wide in its lower reach, carved by headward cataract retreat through jointed basalt; modeled incision-phase discharge 2.6 × 10⁶ m³/s ([Lapotre et al. 2021 lineage, JGR-ES](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2021JF006135)).
- **Dry Falls:** the recessional cataract complex between upper and lower Grand Coulee — Bretz described the greatest scabland cascade as **9 miles (~14 km) wide**; the preserved Dry Falls amphitheater is the standard interpretive example (commonly quoted as ~5.6 km of rim and ~120 m of drop — treat the 5.6/120 pair as unverified-by-me-here, the 14-km figure is Bretz's).

---

## 6. Coastal erosion

### 6.1 Cliff retreat rates by lithology

The GlobR2C2 database (58 publications, 1530 cliffs, >1680 rate estimates) gives the cleanest global numbers — medians: **hard rocks 2.9 cm/yr, medium rocks 10 cm/yr, weak rocks 23 cm/yr** (weak-rock maximum >10 m/yr in extreme datasets), with rock resistance (Hoek–Brown strength) the strongest explanatory variable, and frost-day count the only climatic variable that matters (positively, for weak rocks) ([Prémaillon et al. 2018](https://esurf.copernicus.org/articles/6/651/2018/)). Classic ranges by lithology: chalk 0.1–1 m/yr; limestone 1–10 mm/yr; granite ~1 mm/yr ([Coastal Dynamics textbook](https://geo.libretexts.org/Bookshelves/Oceanography/Coastal_Dynamics_(Bosboom_and_Stive)/02%3A_Large-scale_geographical_variation_of_coasts/2.04%3A_Pleistocene_inheritance_of_cliffed_coasts)). Episodicity is extreme: a 1944 hurricane cut 12 m of Long Island bluff in one day against an 80-yr mean of 0.5 m/yr; 1983 storms took 14 m of Santa Cruz mudstone cliff in days against a 0.2 m/yr mean ([Sunamura 2015](https://pmc.ncbi.nlm.nih.gov/articles/PMC4754505/)). Holderness (glacial till) retreats ~1–3.5 m/yr, among Europe's fastest. **[anchor confirmed]** "cm/yr to m/yr by lithology" is exactly right.

### 6.2 Rock vs soft coasts — the global proportion

**[anchor correction]** The "~75% rock / 25% soft" split is the older textbook figure (Bosboom & Stive: "about 75% of the world's continental and island margins is lined with cliffs"; Emery & Kuhn 1982 claimed ~80%). The first validated global mapping (SRTM backshore elevations at 1-km intervals, ~1.34 M transects) puts cliffs at **~52% of the global shoreline** (with ~31% of ice-free shores sandy) ([Young & Carilli 2019](https://onlinelibrary.wiley.com/doi/10.1002/esp.4574), [Coastal Wiki summary](https://www.coastalwiki.org/wiki/Rocky_shore_morphology)). Use ~50–75% rocky depending on how strict the "cliff" definition is; the 75% figure is defensible for "cliffed or cliff-backed," the 52% figure for "likely cliffed."

### 6.3 Wave-cut platforms, arches, stacks

- **Platforms:** horizontal-to-sloping rock benches fronting retreating cliffs. Kaikoura (NZ) micro-erosion-meter data: downwearing 0.15–9.2 mm/yr (mean 1.13; mudstone 1.98, limestone 0.88 mm/yr), cliff retreat 0.05–0.91 m/yr — and, heretically, wave forces there never exceed the rock's compressive strength: the cutting is done by wetting–drying weathering (100–380 cycles/yr), with waves merely removing the debris ([Stephenson 1997–2001 Kaikoura studies](https://doi.org/10.26021/6490)). The classic subaerial-vs-wave debate is genuinely unresolved; treat platform elevation as set by the wave/cliff-toe attack zone and lowered by weathering ([Kennedy et al. 2010](https://doi.org/10.1002/esp.2092)).
- **Arches and stacks:** cave → arch → collapse → stack → stump. Lifespans are *centennial*: the Twelve Apostles (45-m Miocene limestone stacks, Victoria) lose a member to collapse (2009; the London Bridge arch fell 1990), and long-term retreat there is 0.22 m/yr ([Bezore et al. 2016](https://doi.org/10.2112/si75-119.1)). Limber & Murray's model: stack formation needs preferential erosion (abrasion tool-rich pocket beaches or structural weakness) cutting a channel through a headland; high sediment supply *discourages* stacks by armoring ([Limber & Murray 2014](https://doi.org/10.1002/esp.3667)); a 2025 continuum theory derives stack formation as an instability of the retreating cliffline itself, driven by rubble-enhanced erosion feedback ([Fowler et al. 2025](https://doi.org/10.1098/rspa.2025.0332)). Relict "drowned Apostles" at 40–50 m depth show stacks can survive eustatic drowning if submerged fast enough. Limestone's compressive strength (~60–170 MPa) makes it the ideal stack-forming lithology ([Frontiers rauk study](https://www.frontiersin.org/articles/10.3389/feart.2022.895419/pdf)).

### 6.4 The carbonate coast (short, as directed)

Reef growth vs sea level follows Neumann & Macintyre's keep-up / catch-up / give-up triad. Holocene vertical reef accretion averages ~5 m/kyr (5.05 mm/yr; range <1 to >20 m/kyr, Indo-Pacific mode 6–7) across 79 dated core sections in Belize/Maldives/French Polynesia, positively correlated with sea-level-rise rate ([Gischler & Hudson 2019](https://doi.org/10.1002/dep2.62)). Modern net carbonate production on Palau/Yap averages 9.7 kg CaCO₃ m⁻² yr⁻¹ ≈ **7.9 mm/yr potential vertical accretion** (best sites ~12 mm/yr) — enough to keep up with RCP 2.6–6.0 sea-level rise, not RCP 8.5 ([van Woesik et al. 2018](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0197077)). A 288-core Indo-Pacific synthesis puts the >90%-failure threshold at relative SLR >5.3 mm/yr ([Nat. Commun. 2026](https://www.nature.com/articles/s41467-026-74612-w)). During meltwater pulse 1A (~50 mm/yr SLR), Tahiti reef communities still tracked upslope at >700 mm/yr lateral displacement while accreting ~10 mm/yr vertically ([Camoin et al., IODP 310](https://doi.org/10.1016/j.margeo.2026.107823)). Reef *hydrodynamics* (wave dissipation over crests) is in the water document §5 — don't duplicate.

---

## 7. Coastal deposition

### 7.1 Longshore drift and the sediment budget

Waves arriving at an angle drive a longshore current; the swash/backwash asymmetry moves sand along the beach face. Transport is maximized at ~30° wave-approach angle (the "magic angle" of coastline dynamics) and the *budget* — sources (cliff erosion, rivers, shoreface) minus sinks (inlets, offshore, dunes) — determines whether a shoreline advances or retreats ([NPS coastal processes summary](https://www.nps.gov/articles/coastal-processes-sediment-transport-and-deposition.htm)). Gradients in the longshore flux are what make landforms: erosion updrift of a groin, a spit at the downdrift end of a sediment cell, a tombolo in the wave shadow of an island or breakwater ([intro-Earth-science summary](https://pressbooks.lib.vt.edu/introearthscience/chapter/12-shorelines/)).

### 7.2 Beach profile: the seasonal berm-and-bar cycle

The core cross-shore oscillation, well-quantified in large-scale flume work ([Eichentopf/gross-flume study 2024](https://www.sciencedirect.com/science/article/pii/S0378383924001091)):

- **Winter/storm (dissipative):** energetic asymmetric waves suspend sand; undertow carries it offshore; the berm erodes and one or more **bars** grow and migrate offshore; breaking is spread across the bar field.
- **Summer/calm (reflective):** wave-asymmetry-driven bedload moves sand onshore; bars dissipate or migrate shoreward and weld on; the **berm** rebuilds and the beach face steepens.
- Mechanics: onshore transport ∝ wave-asymmetry bedload; offshore ∼ suspended load in undertow; the *changing breakpoint position* couples the two — as the bar dissipates, breaking moves inshore and feeds the berm directly (surf–swash exchange).

Macrotidal multi-bar systems (Dundrum Bay, N. Ireland) obey the same seasonality — summer stability, winter crest-lowering and channel migration — with storm sequencing (not just magnitude) setting the response ([Biausque et al. 2021/2023](https://doi.org/10.1016/j.geomorph.2023.108728)).

### 7.3 Spits, recurved spits, tombolos, baymouth bars

- **Spits:** longshore-drift fed ridges extending from headlands into bay water; wave refraction around the tip curves it into a **recurved hook** (often with successive hooks recording episodic growth).
- **Baymouth bars:** spits that close the bay.
- **Tombolos:** a sand ridge linking an island/stack/breakwater to the mainland, formed where the island's wave shadow lets sand settle — the classic emergent feature of *deposition*, not erosion (Venice, CA's breakwater tombolo being the textbook artificial case).

### 7.4 Barrier islands and rollover

Barriers form 10–13% of the world's coastline, 76% of them along rifted/passive margins with wide shelves and big sediment supplies ([Carruthers thesis intro](https://doi.org/10.1575/1912/4834)). **The load-bearing concept: rollover.** Under sea-level rise, a barrier survives by migrating landward: storms overtop the low dunes, washover fans carry sand from the shoreface over the crest into the back-barrier lagoon — a *unidirectional* landward flux that balances the rise. The barrier keeps its shape while its material recycles landward, like a caterpillar track. Leatherman's "critical barrier width": overwash is rare until the barrier thins below a threshold, then it enters a rollover phase.

**Measured numbers:** south shore of Martha's Vineyard, MA: ~1.4 m/yr shoreline retreat over ~200 yr; measured overwash flux 2.4–3.4 m³/m/yr (from ~2.2×10⁴ m³ washover fans at ~28-yr recurrence) against a geometric requirement of ~10 m³/m/yr to hold shape — so that barrier is currently *thinning*, not in equilibrium ([Carruthers et al. 2013](https://doi.org/10.1575/1912/4834)). Plum Island, MA — an inlet-stabilized counter-example — has been essentially stable (0.09 ± 0.6 m/yr over 150 yr) with 25–40-yr erosion/accretion cycles driven by ebb-delta bypassing ([Hein et al. 2019](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2019.00103/full)). Theory (Lorenzo-Trueba & Ashton's morphodynamic model) separates four fates — height drowning, width drowning, steady rollover, and *periodic* retreat driven by internal shoreface-lag dynamics even at constant SLR ([Lorenzo-Trueba & Ashton 2014](https://doi.org/10.1002/2013JF002941)).

### 7.5 Ebb and flood deltas; estuaries

- **Tidal inlets** store and bypass sediment via their **ebb deltas** (seaward) and **flood deltas** (landward). Bypassing cycles of 4–8 yr (natural) to 25–40 yr (jetty-modified, as at Merrimack Inlet) pulse sand to downdrift beaches as migrating bar complexes that weld on ([Frontiers 2019](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2019.00103/full)).
- **Estuary types:** drowned river valleys (**rias** — Chesapeake, Sydney Harbour; funnel-shaped, dendritic, from post-glacial inundation of a fluvial dendritic network), **fjords** and **fjards** (glacial troughs, rock-hard walls vs low rocky coasts), **bar-built** (lagoons behind barriers), **tectonically constrained** (San Francisco Bay — fault-bound), and **delta-fronted** where sediment supply wins over drowning (see Agent 2's §4 for deltas). Tide vs wave dominance sets the planform: tide-dominated estuaries are long and funnel-shaped with linear banks; wave-dominated ones have a beach/barrier front and central basin.

---

## 8. Periglacial terrain

### 8.1 Permafrost and the active layer

Permafrost (ground <0 °C for ≥2 consecutive years) underlies ~11% of Earth's land surface (~14 M km²; 15% of the NH; some definitions give 17–25% including subsea), with non-polar mountain ranges holding ~30% of it — ~20× the glacierized area of the same mountains ([Nature Comms 2024 rock glacier study](https://preview-www.nature.com/articles/s41467-024-52093-z)). The **active layer** (seasonal thaw) is typically 0.3–1 m in continuous permafrost, up to ~2–3 m in discontinuous zone and coarse blocky debris; relict sorted polygons in the Sudetes imply LGM active layers of 0.9–1.6 m ([Engel et al. 2021](https://web.natur.cuni.cz/~kfggsekr/rggg/pdf/engel_ea_21_EW.pdf)).

### 8.2 Solifluction and gelifluction

Slow downslope creep of freeze-thaw-affected soil, in five overlapping components (needle-ice creep, diurnal frost creep, annual frost creep, gelifluction, plug-like flow) ([Matsuoka 2001 global review](https://www.sciencedirect.com/science/article/abs/pii/S0012825201000575)):

- **Rates: cm/yr — confirmed.** Generally <1 m/yr ceiling; typical 1–10 cm/yr. Austrian alpine lobes: mean surface velocity 3.5–8.9 cm/yr, movement depth 32–40 cm ([Kellerer-Pirklbauer 2017](https://doi.org/10.1080/00291951.2017.1399164)); Colorado Front Range experimental sites: 0.4–4.3 cm/yr maxima, movement confined to the upper 50 cm ([Benedict 1970](https://www.tandfonline.com/doi/abs/10.1080/00040851.1970.12003576)).
- **Lobe geometry:** riser 0.2–2 m high; tread 2–50 m wide and long (Matsuoka 2001). Risers scale with the depth of movement — the lobe is the frozen-ground equivalent of a earthflow front.
- Climate control: warm margins → thin (5–10 cm) fast diurnal-creep layers and small stone-banked lobes (tropical high mountains); cold permafrost → two-sided freezing and plug-like flow of >60-cm-thick masses (High Arctic).

### 8.3 Patterned ground

- **Sorted circles/stone circles:** fine domain 1–3 m in diameter ringed by gravel borders 0.5–1 m wide rising 1–5 dm; formed by **convection-like circulation** of soil in the active layer — the fine center rises, spreads radially outward at the surface (~1–2 cm/yr measured by repeat SfM photogrammetry at Kvadehuksletta, Spitsbergen), and the gravel border material creeps inward along the inner slopes, closing the cell; soil cycling time ~300–400 yr ([Kääb et al. 2014](https://doi.org/10.5194/tc-8-1041-2014); Hallet & Prestrud 1986). The Kessler & Werner (2003) model reproduces circles/polygons/stripes from differential frost heave plus slope.
- **Stripes:** the same sorting stretched downslope on 3–15° slopes; gradational into stone-banked solifluction lobes.
- **Ice-wedge polygons:** thermal-contraction cracking in continuous permafrost. Spacings: large polygons 5–30 m (ice wedges to 5 m deep — Adventdalen, Spitsbergen), medium 1–5 m (soil wedges), small 0.1–1 m (desiccation cracks). Mature regular orthogonal networks with Y-junctions need ~4 kyr of cracking activity ([Bertran 2022](https://doi.org/10.1002/ppp.2137), [Matsuoka et al. 2004](https://www.jstage.jst.go.jp/article/grj2002/77/5/77_5_276/_pdf)).

### 8.4 Pingos

Ice-cored hills, up to ~50–70 m high and a few hundred m across, in two genetic types:
- **Hydrostatic (closed-system, Mackenzie delta type):** a lake drains, talik refreezes from all sides, expelled water pools and freezes as an ice core — growth 10s of cm/yr for decades.
- **Hydraulic (open-system, Spitsbergen/Alaska type):** artesian sub-permafrost groundwater flows up a through-talik and freezes at the surface; one observed Spitsbergen mound grew **3 m in 3 years** (~1 m/yr, an upper bound) ([Matsuoka et al. 2004](https://www.jstage.jst.go.jp/article/grj2002/77/5/77_5_276/_pdf)); modeled present aggradation rates feeding them: up to 5 cm/yr ([Gilbert et al. 2020](https://tc.copernicus.org/articles/14/4627/2020/)).

### 8.5 Rock glaciers

Lobate or tongue-shaped bodies of ice-cemented debris creeping downslope — permafrost's landform. **[anchor confirmed]** "how fast, m/yr scale?" — yes: 50 rock-glacier velocity series across the European Alps span **0.04 to 6.23 m/yr** (1995–2022), with climate-driven acceleration phases and 8 of 43 studied bodies destabilized ([ERL 2024](https://iopscience.iop.org/article/10.1088/1748-9326/ad25a4)). The contiguous US hosts ~10,000 active rock glaciers (~1000 km²) vs ~5000 glaciers (670 km²); typical speeds 10–60 cm/yr, all accelerating with air temperature ([Nature Comms 2024](https://preview-www.nature.com/articles/s41467-024-52093-z)). IPA kinematic classes: active >0.1 m/yr, transitional 0.01–0.1, relict <0.01 ([Lilleøren et al. 2022](https://doi.org/10.5194/esurf-10-975-2022)). Velocity follows permafrost temperature (viscosity) and meltwater infiltration (pore pressure).

### 8.6 Thermokarst and palsas

- **Thermokarst:** thaw-settlement terrain — pits, thaw lakes, beaded drainage, retrogressive thaw slumps — where ice-rich permafrost degrades. The negative space of the ground-ice budget; in yedoma regions it reorganizes whole plains.
- **Palsas:** peat-cored frost mounds (0.3–10+ m high) in mires at the southern fringe of discontinuous permafrost; field-size clusters ("palsa fields" / peat plateaus) form the visible southern boundary of permafrost in Fennoscandia.

---

## 9. Cold-region terrain that ISN'T glacial

**Why the highest point of many landscapes is a glacial horn but the smoothest is a plateau never touched by ice (parent Q39):**

- **Unglaciated uplands stay smooth because cold-based and never-occupied terrain keeps its regolith and its diffusion.** The classic cases: the paleic surface remnants above Sognefjord (high-lying low-relief mountain plateaus little affected by glacial erosion — Nesje 2024, §2.5); Beringia — the unglaciated subcontinent from the Verkhoyansk Range across the land bridge to the Yukon — persisted under *continuous permafrost* through the entire Quaternary, accumulating yedoma silts rather than being scoured ([Murton 2021](https://doi.org/10.1002/ppp.2102)).
- **Southern England is the relict-periglacial archetype:** frost-susceptible chalk and slate lowlands, repeatedly perglaciated but never overridden, evolved "whale-backed," subdued convexo-concave hillslopes — ice segregation brecciated the upper meters, gelifluction and slopewash then *smoothed* the terrain (periglaciation as a planation agent, the opposite of the glacial buzzsaw) ([Murton 2021](https://doi.org/10.1002/ppp.2102)).
- **Periglaciation reached far beyond the ice sheets — verified.** Relict thermal-contraction polygons in Europe extend to **43.5°N** in France (Aquitaine, lower Rhône) and **47°N** in central Europe (Hungary), concentrated north of 51°N — i.e., permafrost-related ground cracking reached ~1500+ km beyond the Fennoscandian margin ([Bertran 2022](https://doi.org/10.1002/ppp.2137)). The Northern European Loess Belt (48–51°N, Brittany to Poland) is the Fennoscandian outwash's wind-blown twin, its thickness and extent paced by ice-sheet history ([ESSD 2023 loess database](https://essd.copernicus.org/articles/15/4689/2023/)). In North America the permafrost limit at LGM hugged the ice margin (permafrost "did not reach far south of the ice sheets except at high elevations" — sharp gradient), while loess mantled the whole Midwest downwind.
- **The asymmetry to encode:** glacial ice *roughens at all scales* (horns, troughs, overdeepenings) while *planing off its own high plateaus* when warm-based; periglacial processes *roughen at small scales* (patterned ground, lobes, ice-wedge meshes — meters to tens of m) while *smoothing at large scales* (gelifluction + slopewash soften hillslopes). A never-glaciated but perglaciated plateau is therefore smooth-with-texture; a glaciated range is rough-at-every-scale with smoothed trough floors.

---

## 10. Practical simulation synthesis (opinionated)

### 10.1 The glacial pass

Run *after* a fluvial landscape exists (glaciers inherit valleys; they don't invent lineaments — every fjord and trough study above says structure and pre-glacial drainage precondition the flow). Per climate zone, compute a Pleistocene ELA field (modern ELA minus ~900–1000 m for a full-glacial world). Then:

1. **Mask:** ice where (accumulation-area-ratio test passes) — snow input above ELA, slope < ~45°, plus flow downhill. You do not need an ice-sheet model: a flow-accumulation over the *snow* field with a thickness ∝ flux^(1/2?) heuristic gets 90% of the geometry.
2. **Carve U-valleys along fluvial valleys where the ice-line intersects:** for each column under ice, deform the valley cross-section toward $y \propto |x|^{1.7\pm0.4}$ (§2.3), amplitude scaling with ice-flux; deepen trunk reaches at confluences (fjord logic, §2.5), overdeepen by lowering closed segments 10–200 m below their downstream riegel, and drop in rock-basin tarns at 5–15% of steps.
3. **Stamp cirques at valley heads:** place cirques where slope-aspect (N/E-facing in mid-latitudes — the aspect asymmetry is real and measured) and elevation ≥ ELA; each cirque = a parabolic-bowl carve with a 0.2–2 m... no — a 10–100 m lip-and-basin pair, headwall at 30–40°, sized ~200–800 m wide, growing with "age" (multiple cycles). Let cirques from adjacent catchments intersect; leave the un-eaten ridge core as arête/horn where ≥3 overlap. Raise nothing.
4. **Hanging valleys:** tributary troughs whose ice flux was < ~10% of the trunk stop at their own floor; clamp tributary floors to trunk floor minus (30–700 m, log-uniform).
5. **Deposit below the ice limit:** terminal/recessional moraine arcs (10–100 m relief) at readvance still-stands; drumlin swarms on low-slope (< 0.5°) soft beds inside the former ice margin — lengths 250–1000 m, elongation 1.7–4.1, spacing ~300–400 m, aligned to the (interpolated) flow field, `W = 7√L`; esker networks on the same flow field — ridge heights 5–30 m, sinuosity ≈ 1.05, ~12-km lateral spacing, radial from ice divides; kettled ice-contact chaos and flat sandur grading beyond the moraine.
6. **Isostasy:** add a broad rebound warp (up to ~10 mm/yr × 10 kyr ≈ 100 m) centered on former ice divides, subsiding forebulge at 300–500 km offset (§5.3).

**Visually load-bearing (do these):** U-valley cross-sections + truncated spurs (the single strongest "glaciated" cue), cirques-and-horns above the snowline, tarn-in-rock-basin, hanging-valley lips with waterfalls, terminal moraine arcs, drumlin swarms, eskers. **Skippable:** Rogen moraine (regional subtlety), pitted outwash micro-kettles, periglacial stripes at less than ~1 m/voxel-scale resolution, medial moraines (paintable as texture), tunnel valleys.

### 10.2 The coastal pass

On any heightmap with a sea level: (1) classify lithology-typed cliff cells (retreat 0.03–0.3 m/yr for hard, 0.1–1 m/yr median weak-rock; scale by wave-energy proxy from fetch) vs beach cells; (2) run a longshore-drift budget along the shoreline polyline — spit growth at drift terminals, tombolo deposition in island/stack shadows, barrier chains on low-slope (< 0.05°) shelves with sand supply; (3) migrate barriers landward at 0.5–2 m/yr × sea-level-rise rate with episodic (storm-return-interval) washover fans; (4) attach platforms (width ∝ age × retreat rate, ~10–100× the retreat distance) and stack/arch stamps on structurally-weak headland cells; (5) drown valleys at any rapid SLR step — rias for dendritic drainages, fjords where the glacial pass ran.

**Load-bearing:** cliff+platform pair, spits, barrier rollover, drowned estuaries. **Skippable:** ebb/flood delta cycles (unless gameplay cares), berm/bar seasonality (render-time detail), reef triad (hand-place or skip; see water doc).

### 10.3 The honest ceiling

You cannot run Herman-law erosion over Myr at voxel resolution, and you should not try. The measured literature itself says the memory is *in the landforms, not the rates*: erosion-rate inferences vary by 100× with measurement timescale (§1.4), so the only stable quantities are shapes and sizes — b-exponents, drumlin scaling laws, esker spacings, ELA offsets. Simulate the **landform statistics**, not the process PDEs.

---

## Provenance

- **Method:** 6 Exa web-search batches (~24 queries) over primary literature and reviews, 2026-02 session. Every numeric anchor flagged "?" by the parent bank was checked against at least one primary or review source; disagreements are flagged inline as **[anchor correction]** / **[anchor confirmed]** and summarized: (1) drumlin elongation "2–10" → measured bulk 1.7–4.1 (2–10 only with the MSGL continuum); (2) "75%/25% rock/soft coasts" → 52% (modern global mapping) vs 75% (older textbook, loose "cliff-backed" definition) — both reported; (3) "fjord max ~500–1300 m" → Sognefjord 1308 m confirmed but global max 1933 m (Skelton Inlet) exceeds it; (4) "U-valley width:depth 2–4" → not a canonical literature statistic; power-law exponent $b$ ≈ 1.5–2.5 and form ratio are the standard descriptors; (5) all rate anchors (glacial erosion 0.1–10 mm/yr, cirque mm/yr, rebound ~1 cm/yr, solifluction cm/yr, rock glacier m/yr, Missoula ~10⁷ m³/s) **confirmed**.
- **Unverified-but-included:** the "Dry Falls 5.6 km × 120 m" interpretive dimensions (flagged in §5.4; Bretz's 14-km "greatest cascade" width is the sourced figure).
- **Cross-references:** wave theory, shoaling, breaking, reef hydrodynamics → `water-physics-and-wave-simulation.md` §2/§5; rivers, stream power, drainage stats, deltas → Agent 2's document; loess/desert processes → Agent 5; procedural noise/LEM machinery → Agent 7.

## Sources

1. Herman, F. et al. (2015) *Erosion by an Alpine glacier*. Science 350:193–195. https://doi.org/10.1126/science.aab2386 (mirror: https://tectonics.caltech.edu/publications/pdf/Herman_Science-2015.pdf)
2. Cook, S.J., Swift, D.A. et al. (2020) *The empirical basis for modelling glacial erosion rates*. Nat. Commun. 11:759. https://www.nature.com/articles/s41467-020-14583-8 (open: https://pmc.ncbi.nlm.nih.gov/articles/PMC7005307/)
3. Norris, S.L. et al. (2025) *Drivers of global glacial erosion rates*. Nat. Geosci. https://www.nature.com/articles/s41561-025-01747-8
4. Wilner, J. et al. (2024) *Limits to timescale dependence in erosion rates* (glacial 0.51 vs fluvial 0.067 mm/yr). https://pmc.ncbi.nlm.nih.gov/articles/PMC11661439/
5. Koppes, M. & Montgomery, D. (2009) *The relative efficacy of fluvial and glacial erosion over modern to orogenic timescales*. https://glaciers.pdx.edu/fountain/readings/TopicsInGeomorphology/Koppes&Montgomery2009_GlacialVsFluvialErosion.pdf
6. Herman, F. (2016) EGU abstract, *Constraints on the glacial erosion rule*. https://meetingorganizer.copernicus.org/EGU2016/EGU2016-14074.pdf
7. Headley, V. et al. (2015) *Ice flow models and glacial erosion over multiple glacial–interglacial cycles*. ESurf 3:153–174. https://esurf.copernicus.org/articles/3/153/2015/esurf-3-153-2015.pdf
8. Brozović, N., Burbank, D.W., Meigs, A. (1997) *Climatic limits on landscape development in the northwestern Himalaya*. Science 276:571–574. https://doi.org/10.1126/science.276.5312.571
9. Egholm, D.L. et al. (2009) *Glacial effects limiting mountain height*. Nature. https://www.d.umn.edu/~kgran/Geol4550/Egholm%202009.pdf
10. Thomson, S.N. (2010) *Glaciation as a destructive and constructive control on mountain building* (Patagonian Andes). http://geomorphology.sese.asu.edu/Papers/Thomson_Glaciation_andes.pdf
11. Hall, A.M. & Kleman, J. (2014) *Glacial and periglacial buzzsaws: fitting mechanisms to metaphors*. Quat. Res. https://doi.org/10.1016/j.yqres.2013.10.007
12. Evans, I.S. (2017) *The glacial buzzsaw and its limitations: British Columbia and Britain*. PG 47. https://doi.org/10.15551/prgs.2017.47
13. *Glacial limitation of tropical mountain height* (2019) ESurf 7:147–. https://esurf.copernicus.org/articles/7/147/2019/
14. Mitchell, S.G. & Humphries, E.E. (2015) *Glacial cirques and the relationship between ELAs and mountain range height*. Geology 43:35–38. https://doi.org/10.113Q/G36180.1
15. Harbor, J. (1992) *Numerical modeling of the development of U-shaped valleys by glacial erosion*. GSA Bull. 104:1364–1375. https://doi.org/10.1130/0016-7606(1992)104
16. Svensson, H. (1959) *Is the cross-section of a glacial valley a parabola?* J. Glaciol. 3:362–363. https://www.cambridge.org/core/journals/journal-of-glaciology/article/is-the-crosssection-of-a-glacial-valley-a-parabola/EE16DB83DA62DE75D671DE955242CD43
17. Graf, W.L. (1970) *The geomorphology of the glacial valley cross section*. https://scholarcommons.sc.edu/cgi/viewcontent.cgi?article=1041&context=geog_facpub
18. Li et al. (2001) *Glacial valley cross-profile morphology, Tian Shan Mountains, China*. https://www.sciencedirect.com/science/article/abs/pii/S0169555X00000787
19. Hartmeyer, I. et al. (2020) *A 6-year lidar survey reveals enhanced rockwall retreat in deglaciating cirques*. ESurf 8:753–. https://esurf.copernicus.org/articles/8/753/2020/esurf-8-753-2020.pdf
20. Barr, I.D. et al. (2020) *The dynamics of mountain erosion: cirque growth slows as landscapes age*. https://e-space.mmu.ac.uk/623163/1/Barr%20et%20al%20%28accepted%29.pdf
21. Belknap, S.M. (2019) PhD thesis, cirque formation modeling, Sierra Nevada. https://doi.org/10.17615/adg6-ps54
22. Nesje, A. (2024) *Sognefjorden in western Norway — physiography, geomorphology, marine levels and deglaciation history*. NJG 104. https://njg.geologi.no/wp-content/uploads/2024/10/241003_Nesje.pdf
23. Nesje, A. et al. (1992) *Quaternary erosion in the Sognefjord drainage basin*. https://www.academia.edu/30873948/Quaternary_erosion_in_the_Sognefjord_drainage_basin_western_Norway
24. *Sognefjorden* (dimensions, ice thickness, sill, eroded volume). https://en.wikipedia.org/wiki/Sognefjorden
25. *Fjord* (global depth/length rankings, hanging valleys, sills). https://en.wikipedia.org/wiki/Fjord
26. *Overdeepening* (mechanics, tunnel valleys, Gamburtsev). https://en.m.wikipedia.org/wiki/Overdeepening
27. Clark, C.D. et al. (2009) *Size and shape characteristics of drumlins, derived from a large sample*. QSR 28:677–692. http://nora.nerc.ac.uk/id/eprint/6960/1/Drumlins.pdf
28. Clark, C.D. et al. (2017) *Spatial organization of drumlins*. ESPL. https://doi.org/10.1002/esp.4192
29. Zoet, L. et al. (2021) *Factors that contribute to the elongation of drumlins beneath the Green Bay Lobe*. ESPL. https://doi.org/10.1002/esp.5192
30. Storrar, R.D., Stokes, C.R., Evans, D.J.A. (2014) *Morphometry and pattern of a large sample (>20,000) of Canadian eskers*. QSR 105:1–25. https://shura.shu.ac.uk/12940/1/Storrar%20et%20al%20%282014%29%20QSR.pdf
31. *Rogen (ribbed) moraine* (dimensions, distribution). https://en.wikipedia.org/wiki/Ribbed_moraine
32. Wohl, E. (2019) teaching notes, glacial depositional landform dimensions. https://sites.warnercnr.colostate.edu/wp-content/uploads/sites/93/2019/01/454part10b-student-2019.pdf
33. Willeit, M. et al. (2019) *Mid-Pleistocene transition in glacial cycles explained by declining CO₂ and regolith removal*. Sci. Adv. https://pmc.ncbi.nlm.nih.gov/articles/PMC6447376/
34. Abe-Ouchi, A. et al. (2020) *On the Cause of the Mid-Pleistocene Transition*. Rev. Geophys. https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2020RG000727
35. Clark, P.U. et al. (2006) *The middle Pleistocene transition*. QSR. https://www.sciencedirect.com/science/article/abs/pii/S0277379106002332
36. Chalk, T. et al. (2017) *Causes of ice age intensification across the MPT*. PNAS. https://www.pnas.org/doi/abs/10.1073/pnas.1702143114
37. Bouchard et al. (2023) *A gradual change is more likely to have caused the MPT*. Comms. Earth Env. https://www.nature.com/articles/s43247-023-00754-0
38. *Laurentide ice sheet* (extent to 38°N, dome thicknesses). https://en.wikipedia.org/wiki/Laurentide_ice_sheet
39. *Last Glacial Maximum* (sea level −125 m, Beringia/Sundaland, permafrost limits). https://en.wikipedia.org/wiki/Last_glacial_maximum
40. Patton, H. et al. (2016/2017) Eurasian ice sheet build-up and deglaciation. https://www.sciencedirect.com/science/article/pii/S0277379116304498 and https://www.sciencedirect.com/science/article/pii/S0277379117302068
41. Gowan, E. et al. (2021) *A new global ice sheet reconstruction for the past 80,000 years*. Nat. Commun. https://www.nature.com/articles/s41467-021-21469-w
42. Vestøl, O. et al. (2019) *NKG2016LU: a new land uplift model for Fennoscandia*. J. Geodesy. https://link.springer.com/article/10.1007/s00190-019-01280-8
43. Kierulf, H. et al. *A GNSS velocity field for geophysical applications in Fennoscandia* (BIFROST). https://research.chalmers.se/publication/523916/file/523916_Fulltext.pdf
44. Ekman, M. & Mäkinen, J. (1996) *Recent postglacial rebound, gravity change and mantle flow in Fennoscandia*. GJI. https://doi.org/10.1111/j.1365-246x.1996.tb05281.x
45. Baker, V.R. (2009) *The Channeled Scabland: A Retrospective*. Annu. Rev. Earth Planet. Sci. https://www.annualreviews.org/content/journals/10.1146/annurev.earth.061008.134726
46. Bretz, J H. (1969) *The Lake Missoula Floods and the Channeled Scabland*. J. Geol. 77:505–543. https://wpg.forestry.oregonstate.edu/sites/default/files/seminars/1969_Bretz.pdf
47. O'Connor, J. et al. (2020/2021) *The Missoula and Bonneville floods — a review of ice-age megafloods*. EPSL. https://www.sciencedirect.com/science/article/abs/pii/S0012825220302270 (open: https://people.wou.edu/~taylors/g322/Oconnor_etal_2021_ice_age_floods_review.pdf)
48. Benito, G. & O'Connor, J. (2003) *Number and size of last-glacial Missoula floods*. GSA Bull. https://people.wou.edu/~taylors/gs407rivers/benito03_fairbanks_divide_missoula_flood.pdf
49. Lapotre/Baynes-area authors (2021) *Pleistocene Megaflood Discharge in Grand Coulee*. JGR-ES. https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2021JF006135
50. Prémaillon, M. et al. (2018) *GlobR2C2 (Global Recession Rates of Coastal Cliffs)*. ESurf 6:651–668. https://esurf.copernicus.org/articles/6/651/2018/
51. Young, A.P. & Carilli, J.E. (2019) *Global distribution of coastal cliffs*. ESPL. https://onlinelibrary.wiley.com/doi/10.1002/esp.4574
52. Sunamura, T. (2015) *Rocky coast processes: soft rock cliff recession*. J. Coast. Zone Manag. https://pmc.ncbi.nlm.nih.gov/articles/PMC4754505/
53. Stephenson, W.J. (2001) *Development of shore platforms on Kaikoura Peninsula*. https://doi.org/10.26021/6490
54. Bezore, R., Kennedy, D.M., Ierodiaconou, D. (2016) *The Drowned Apostles: the longevity of sea stacks over eustatic cycles*. J. Coast. Res. SI75. https://doi.org/10.2112/si75-119.1
55. Limber, P.W. & Murray, A.B. (2014) *Sea stack formation and the role of abrasion*. ESPL. https://doi.org/10.1002/esp.3667
56. Fowler, A.C. et al. (2025) *Towards a theory for the formation of sea stacks*. Proc. R. Soc. A. https://doi.org/10.1098/rspa.2025.0332
57. van Woesik, R. et al. (2018) *Keeping up with sea-level rise: carbonate production rates in Palau and Yap*. PLOS ONE. https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0197077
58. Gischler, E. & Hudson, J.H. (2019) *Holocene tropical reef accretion and lagoon sedimentation*. Depositional Rec. https://doi.org/10.1002/dep2.62
59. Camoin, G. et al. *Sea-level rise and reef development during the last deglaciation at Tahiti (IODP 310)*. Mar. Geol. https://doi.org/10.1016/j.margeo.2026.107823
60. Carruthers, E.A. et al. (2013) *Quantifying overwash flux in barrier systems: Martha's Vineyard*. https://doi.org/10.1575/1912/4834
61. Lorenzo-Trueba, J. & Ashton, A.D. (2014) *Rollover, drowning, and discontinuous retreat*. JGR-ES. https://doi.org/10.1002/2013JF002941
62. Hein, C. et al. (2019) *Shoreline dynamics along a developed river mouth barrier island (Plum Island)*. Front. Earth Sci. https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2019.00103/full
63. Bar/berm seasonal cycle flume study (2024) *Bar and berm dynamics during transition from dissipative to reflective beach profile*. https://www.sciencedirect.com/science/article/pii/S0378383924001091
64. Biausque, M. et al. (2023) *Inter-annual morphological evolution of a meso-macrotidal multiple-bar beach*. Geomorphology. https://doi.org/10.1016/j.geomorph.2023.108728
65. NPS *Coastal Processes — Sediment Transport and Deposition*. https://www.nps.gov/articles/coastal-processes-sediment-transport-and-deposition.htm
66. Matsuoka, N. (2001) *Solifluction rates, processes and landforms: a global review*. Earth-Sci. Rev. 55:107–134. https://www.sciencedirect.com/science/article/abs/pii/S0012825201000575
67. Kääb, A. et al. (2014) *Surface kinematics of periglacial sorted circles using SfM*. The Cryosphere 8:1041–. https://doi.org/10.5194/tc-8-1041-2014
68. Matsuoka, N. et al. (2004) *Present-day periglacial environments in central Spitsbergen*. Geogr. Rev. Japan 77(5):276–300. https://www.jstage.jst.go.jp/article/grj2002/77/5/77_5_276/_pdf
69. Benedict, J.B. (1970) *Downslope soil movement in a Colorado alpine region*. Arct. Alp. Res. https://www.tandfonline.com/doi/abs/10.1080/00040851.1970.12003576
70. Kellerer-Pirklbauer, A. (2017) *Solifluction rates and environmental controls in central Austria*. Norsk Geogr. Tidsskr. https://doi.org/10.1080/00291951.2017.1399164
71. *Acceleration and interannual variability of rock glacier velocities in the European Alps 1995–2022*. Environ. Res. Lett. (2024). https://iopscience.iop.org/article/10.1088/1748-9326/ad25a4
72. *Rock glaciers across the United States predominantly accelerate* (2024). Nat. Commun. https://preview-www.nature.com/articles/s41467-024-52093-z
73. Lilleøren, K.S. et al. (2022) *Transitional rock glaciers at sea level in northern Norway*. ESurf 10:975–. https://doi.org/10.5194/esurf-10-975-2022
74. Gilbert, G. et al. (2020) *Numerical modelling of permafrost spring discharge and open-system pingo formation*. The Cryosphere 14:4627–. https://tc.copernicus.org/articles/14/4627/2020/
75. Murton, J.B. (2021) *What and where are periglacial landscapes?* Permafr. Periglac. Process. https://doi.org/10.1002/ppp.2102
76. Bertran, P. (2022) *Distribution of Pleistocene ground thermal contraction polygons in Europe*. PPP. https://doi.org/10.1002/ppp.2137
77. *Last Glacial loess in Europe: luminescence database and chronology* (2023). ESSD 15:4689–. https://essd.copernicus.org/articles/15/4689/2023/
78. Engel, Z. et al. (2021) *10Be exposure age for sorted polygons in the Sudetes Mountains*. https://web.natur.cuni.cz/~kfggsekr/rggg/pdf/engel_ea_21_EW.pdf
79. *Frozen-bed Fennoscandian and Laurentide ice sheets during the LGM* (ribbed-moraine paleo-thermal mapping). Nature. https://www.nature.com/articles/47005
80. Hättestrand & co. (2004) *Size distribution of two cross-cutting drumlin systems in northern Sweden*. Ann. Glaciol. https://www.cambridge.org/core/journals/annals-of-glaciology/article/size-distribution-of-two-crosscutting-drumlin-systems-in-northern-sweden-a-measure-of-selective-erosion-and-formation-time-length/FDE73F476077D5946D1C0C43357D4961

---

## Appendix A — Forty further questions

*Generated beyond the parent bank (Q31–39); each answered or explicitly dispositioned.*

1. **How fast does a cirque glacier actually erode its floor?** — Pyrenees cirque-moraine volumes: 0.03–0.35 mm/yr during cirque-glacier phases vs 0–0.03 under icefield cover; first-occupation estimates 1–3.4 mm/yr (Barr et al. 2020, §2.1). Episodic, not steady.
2. **Do cirques reach an equilibrium size?** — Contested: Ben Ohau shows no convergence in 750 ka; Barr et al. argue a "least-resistance" armchair shape is approached and growth then slows. Dispositioned as genuinely open.
3. **What sets cirque aspect asymmetry?** — Solar radiation (N-facing favored in mid-latitudes) plus wind-drifted snow; aspect-CFA contrast is largest under clear skies ([Evans 2006 via Cui et al. 2022](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2022.900141/full)).
4. **Why do troughs develop *steps* along-valley?** — Overdeepening instability: quarrying thresholds at bed slopes; supercooled water freezes sediment onto riegel crests, armoring them while basins deepen (Hooke 1991 via [Belknap 2019](https://doi.org/10.17615/adg6-ps54)).
5. **What is a bergschrund and does it drive headwall retreat?** — The crevasse at the ice-headwall contact; delivers meltwater to the bed, driving water-pressure fluctuations and quarrying just down-valley; the frost-weathering-only alternative is disfavored by cirque lengthening data (§2.1).
6. **How thick was LGM ice over the Alps?** — Not covered in my searches this session (I fetched Fennoscandian/Laurentide figures). Disposition: flag for Agent 1's orogen synthesis.
7. **Do glaciers follow pre-existing faults or make their own lineaments?** — Follow: Patagonian fjord orientations match strike-slip structure; Sognefjord's trend is decoupled from Caledonian fold axes but follows deep fracture zones (§2.5 sources).
8. **What is the "paleic surface"?** — Norway's pre-Quaternary gently-uplifted etch surface, surviving as high plateaus above the fjords — the smooth reference the troughs are cut into (Nesje 2024).
9. **How do rock basins (tarns) differ from glacial lakes behind moraines?** — Rock basins are closed erosion overdeepenings in bedrock (with or without a rock lip); moraine-dammed lakes sit *on* the deposit. Both render as tarns; only the former implies overdeepening.
10. **Why do some drumlin fields show cross-cutting alignments?** — Successive ice-flow phases; later fast-flow corridors erode small lineations and spare large ones (northern Sweden's MIS 5d vs deglacial systems, source 80).
11. **How quickly can a drumlin form?** — Years to decades: actively forming drumlins at Múlajökull, Iceland are stratigraphically dated to within the modern regime (§4.2 sources).
12. **What are mega-scale glacial lineations (MSGLs)?** — The elongation-continuum end-member (E>10, km-scale) formed under ice streams; drumlin–MSGL is one population spanning E≈1–13 (Clark 2009/2017).
13. **What did the ice-sheet beds' thermal mosaic look like?** — Frozen-bed patches preserved relict landscapes and uplands; thawed-bed zones eroded and drumlinized; ribbed-moraine distributions map the boundaries (source 79).
14. **How much sediment did Quaternary ice sheets move overall?** — Sognefjord basin alone: 7610 km³; western Norway total ~35,000 km³ (Nesje 1992). Global figure not fetched — dispositioned to Agent 2's sediment-cascade section.
15. **What is a tunnel valley?** — Subglacial meltwater-carved valley up to 100 km long, 4 km wide, 50–400 m deep overdeepened, usually sediment-filled (Overdeepening source) — the flood-carved sibling of fjords under ice sheets.
16. **Did the 100-kyr cycles have true 100-kyr periodicity?** — Arguably not: they may be 82/123-kyr skipped-obliquity multiples, with chaotic termination timing (Abe-Ouchi 2020, §5.1).
17. **How fast was post-LGM sea-level rise?** — ~120 m over 21–7 ka with meltwater pulses; MWP-1A ≈ 17 m in ~300 yr ≈ 50 mm/yr (Tahiti IODP, source 59).
18. **What is a ria and how does it differ from a fjord?** — Ria = drowned *fluvial* dendritic valley (V-inherited, gentle); fjord = drowned glacial trough (U, overdeepened, sill). Fjards = low-rocky-glacial equivalents.
19. **What raised the "Ancylus" and "Littorina" shorelines of the Baltic?** — Isostatic rebound outpacing eustatic rise in a semi-enclosed basin — strandline sequences at up to +45 m (rauk study, source set §6.3).
20. **Are there modern analogues of the Missoula floods?** — Jökulhlaups (Iceland, Grímsvötn) at 10³–10⁴ m³/s — three orders smaller; the only 10⁷-scale known floods are Quaternary outbursts (Missoula, Bonneville, Altai).
21. **What did Bonneville flood do?** — Pleistocene Lake Bonneville overtopped at Red Rock Pass ~18 ka, ~1×10⁶ m³/s peak, carving the Snake River gorge (O'Connor review, source 47). Not covered in depth here.
22. **How do wave-cut platforms keep flat?** — Wave attack fixes the elevation of the cliff-toe notch; weathering lowers the surface between; unresolved which dominates globally (Kaikoura says weathering, §6.3).
23. **Can stacks survive sea-level cycles?** — Rarely, but yes: "drowned Apostles" at −40–50 m off Victoria formed at MIS-3 sea level and survived rapid postglacial submergence (Bezore 2016).
24. **What is a strandflat?** — The low rock platform fringe along glaciated coasts (Norway) — likely a combined frost-weathering + glacial + wave product over multiple cycles; segments flank Sognefjord's outer reaches (Nesje 2024).
25. **What is the "Sadler effect" and why does it matter here?** — Measured erosion rates decline with measurement timescale; Wilner et al. showed it's three measurement biases, and the glacial≫fluvial contrast survives correction (§1.4).
26. **How do notch profiles on stacks record sea level?** — Tidal notches form at fixed relation to mean sea level; uplifted rauk notches preserve Holocene RSL steps (Frontiers 2022, §6.3).
27. **Why do reefs "back-step" during rapid SLR?** — When accretion can't keep up at the current position, the wave-resistant rim re-establishes landward/up-slope where accommodation allows (catch-up/give-up, §6.4).
28. **What is the fetch/bathymetry control on cliff retreat?** — Wave energy is refraction-controlled by nearshore bathymetry; GlobR2C2 found marine forcings weakly explanatory vs rock strength — headland focusing matters locally, not globally (§6.1).
29. **How do tidal inlets migrate?** — Ebb-delta breaching and channel avulsion on 4–8-yr cycles; jetties slow it and shift the bar-bypassing cycle to 25–40 yr (Plum Island, §7.5).
30. **What are washover fans made of, at what rate?** — 2–3 × 10⁴ m³ per event, 2.4–3.4 m³/m/yr sustained flux at ~28-yr recurrence (Martha's Vineyard, §7.4).
31. **What is "critical barrier width"?** — The barrier thickness below which overwash becomes frequent and rollover engages (Leatherman 1979, via Lorenzo-Trueba 2014).
32. **How deep is the permafrost itself?** — Continental-scale hundreds of m (Sudetes LGM model: 220–250 m; deepest measured ~1500 m in Siberia — the latter is textbook-common but I did not verify it this session; dispositioned unverified).
33. **What is yedoma?** — Ice-rich syngenetic permafrost silt of Beringia; ground-ice volume huge; thermokarst-prone (Murton 2021, §9).
34. **How do palsas grow?** — Snow-thickness differential → insulation differential → peat freezes into a core; growth is cm/yr, lifespans centuries; degradation is the modern norm.
35. **What is a "periglacial trimline"?** — The elevation boundary between frost-shattered bedrock below and blockfield/tor preservation above — used to reconstruct former cold-based ice cover (Engel 2021 references it).
36. **Do rock glaciers store meaningful water?** — Yes: ~70% volumetric ice content typical; Andes/Himalaya rock glaciers are significant reservoirs where glaciers decay (ice-content modeling, §8.5 sources).
37. **What are cryoturbation and involutions?** — Active-layer churning that deforms sediment stratigraphy (frost heave + thaw settlement); the soft-sediment signature used to identify relict periglacial terrain in section.
38. **How far did LGM permafrost reach in North America?** — Close to the ice margin (sharp gradient, high-elevation exceptions) — unlike Europe's wide periglacial belt (LGM source, §5.2/§9). Asymmetry worth encoding.
39. **What is paraglacial relaxation?** — Post-glacial re-adjustment: debuttressed slopes fail, sediment is released over 10³–10⁴ yr; explains rockfall spikes in deglaciating cirques (Hartmeyer 2020's proximal-wall rates) and valley infill.
40. **What should a "frost pass" add to a non-glacial cold terrain?** — Meter-scale: sorted nets/stripes (1–30 m wavelength), ice-wedge polygons (5–30 m, troughs to 1–3 m depth), solifluction lobes (0.2–2 m risers, tens of m treads) on 3–15° slopes, palsa hummocks in peat. All parameters sourced in §8.


# Part 4 - Caves, Karst and the Underground

**Scope:** the full underground — limestone dissolution chemistry, speleogenesis, cave geometry and scale statistics, rates, speleothems, breakdown mechanics, karst surface terrain, lava tubes, and cave climate — written for a **voxel engine whose active goal is cave generation**. Geometry, sizes, passage densities, and depth distributions get priority treatment because those are the numbers a generator needs. Cave climate is included at simulation depth because the project's audio/reverb system switches on an "outside/cave" boundary. Every formula is either a standard derivation (shown) or cited to a specific source; measured quantities are ranges with inline links. This is Part 4 of the merged Earth-terrain document; section numbering is fixed and merge-stable.

**How to read this:** §1–2 are the chemistry and mass balance everything else rests on. §3–5 are the "what caves actually look like and how fast they form" core. §11 is the opinionated voxel-generation recipe — read it last, after the statistics in §4 make the parameter choices non-arbitrary.

---

## 1. The chemistry of limestone dissolution

### 1.1 CO₂ hydration and the dissolution reaction

Rainwater is weakly acidic before it hits the ground. Atmospheric CO₂ (~420 ppm, $p_{CO_2} \approx 4.2\times10^{-4}$ atm) hydrates to carbonic acid:

$$CO_2(g) \rightleftharpoons CO_2(aq) \quad (K_H \approx 3.4\times10^{-2}\ \text{mol/(L·atm at 25 °C)})
$$
$$CO_2(aq) + H_2O \rightleftharpoons H_2CO_3^* \rightleftharpoons H^+ + HCO_3^- \quad (K_1 \approx 4.5\times10^{-7}\ \text{at 25 °C})
$$

($H_2CO_3^*$ = CO₂(aq) plus true carbonic acid, mostly the former.) This alone gives pH ≈ 5.6 — aggressive enough to dissolve limestone slowly. The dissolution reaction:

$$\boxed{CaCO_3 + CO_2 + H_2O \rightarrow Ca^{2+} + 2\,HCO_3^-}$$

One mole of dissolved calcite consumes one mole of CO₂. The open-system (constant $p_{CO_2}$) saturation calcium concentration is, to first order:

$$c_{eq} \approx \left(\frac{K_1 K_H K_c}{4 K_2 \gamma^3}\, p_{CO_2}\right)^{1/3}$$

The load-bearing consequence is the **one-third power**: solubility grows only as $p_{CO_2}^{1/3}$, so multiplying soil CO₂ by 100 multiplies dissolvable calcium by only ~4.6, and solubility *decreases* with temperature (Henry's constant drops fastest of all the constants over 0–30 °C) — cold water dissolves more limestone, which is why alpine and glacial-period dissolution is efficient despite low soil CO₂ ([White 2016](https://doi.org/10.3986/ac.v44i3.1896); [denudation concepts review](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).

### 1.2 The CO₂ source: soil and ground air

The dissolution engine is not atmospheric CO₂ but **soil air**, enriched by root respiration and microbial oxidation of organic matter. Measured ranges:

- Temperate karst soils (Moravian Karst, Czech Republic): $p_{CO_2(soil)} \sim 10^{-2.7}$ atm ≈ 2% ([Faimon et al. 2012](https://doi.org/10.3986/ac.v41i1.47)).
- Lascaux site soils, France: 0.6–6% CO₂ seasonally, efflux 6–25 g CO₂/m²/day ([Lascaux epikarst study](https://www.springerprofessional.de/the-co2-dynamics-in-the-continuum-atmosphere-soil-epikarst-and-i/16644174)).
- Tropical forest (Mulu, Borneo): soil $p_{CO_2}$ 3–5% ([Mulu karst study](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).
- Atmosphere: ~0.042%.

Parent anchor "soil-air pCO₂ ~10–100× atmosphere" **verifies as 25–150×** (0.1–6% vs 0.042%) — correct in spirit, slightly wider at both ends. Generator defaults: temperate 1–3%, tropical 3–6%, arid/alpine 0.1–0.5%.

An important refinement: dripwater chemistry at several well-studied sites requires $p_{CO_2}$ *higher* than the overlying soil (Moravian: $10^{-1.53}$ ≈ 0.03 atm reconstructed from dripwater vs 0.02 atm in soil), implying an additional **"ground air" CO₂ source deeper in the epikarst** — organic matter washed down into karren fissures and oxidized at depth ([Faimon et al. 2012](https://doi.org/10.3986/ac.v41i1.47); [Gibraltar karst study](https://www.sciencedirect.com/science/article/abs/pii/S001670371630028X)). For simulation: the aggressive-water factory is the top ~10 m of rock, not just the soil.

### 1.3 Open vs closed system evolution

Two end-members control how much limestone a given water parcel can dissolve ([Savoy watershed study](https://www.sciencedirect.com/science/article/abs/pii/S0009254118301219)):

- **Open system:** water stays in contact with a large CO₂ reservoir (soil/epikarst air) while dissolving — $p_{CO_2}$ constant, Ca climbs to full open-system saturation (~1–2 mmol/L at soil CO₂). The epikarst and vadose-zone regime.
- **Closed system:** water seals off from the gas phase (phreatic flow). The initial dissolved CO₂ is the only budget; as dissolution proceeds, CO₂ is consumed and total capacity drops to roughly 40–60% of the open-system value. Closed-system conditions lengthen initial passage development by about a factor of five (Palmer, cited in the Savoy study above).

### 1.4 The rate law: Plummer–Wigley–Parkhurst (PWP)

The empirical workhorse for calcite dissolution kinetics is the three-term rate law of Plummer, Wigley & Parkhurst (1978), from pH-stat and free-drift experiments at 5–60 °C and $p_{CO_2}$ from 0.0003 to 0.97 atm ([original paper](https://doi.org/10.2475/ajs.278.2.179); [critical review](https://doi.org/10.1021/bk-1979-0093.ch025)). Three parallel surface reactions:

$$
\begin{aligned}
CaCO_3 + H^+ &\rightleftharpoons Ca^{2+} + HCO_3^- &&(k_1\ \text{term: dominates at pH} < 4)\\
CaCO_3 + H_2CO_3^* &\rightleftharpoons Ca^{2+} + 2HCO_3^- &&(k_2\ \text{term: the CO₂-dependent path, karst waters})\\
CaCO_3 + H_2O &\rightleftharpoons Ca^{2+} + HCO_3^- + OH^- &&(k_3\ \text{term: neutral-water hydrolysis})
\end{aligned}
$$

$$\boxed{R_{net} = k_1 a_{H^+} + k_2 a_{H_2CO_3^*} + k_3 a_{H_2O} - k_4\, a_{Ca^{2+}}\, a_{HCO_3^-}}$$

with $k_4$ the backward (precipitation) term, temperature- and $p_{CO_2}$-dependent. Rate constants follow Arrhenius dependence with activation energies of ~2, 10 and 8 kcal/mol for $k_1$, $k_2$, $k_3$ ([BRGM review](https://infoterre.brgm.fr/rapports/RR-39062-FR.pdf)). Predictions are good to within a factor of 2–3 over many orders of magnitude in rate, worst near equilibrium — exactly where caves care.

### 1.5 The kinetic trigger — why caves are shallow

The single most consequential fact in speleogenesis: **the reaction order changes near saturation.** Field and laboratory studies converge on (White 2016, [summarized here](https://doi.org/10.3986/ac.v44i3.1896); [Dreybrodt 1990](https://doi.org/10.1086/629431)):

- At high undersaturation ($SI_C < -1$ or so): rate is first-order in (undersaturation), fast, partly transport/mass-transfer controlled.
- Approaching equilibrium ($SI_C > -0.3$, roughly >70–90% saturation): the rate law becomes **fourth-order** ($R \propto (c_{eq}-c)^4$), surface-reaction controlled, and rates drop by **orders of magnitude** — experimentally to ~10⁻⁶ of the far-from-equilibrium value at 95% saturation (Dreybrodt's $k$ parameter for natural limestone is 0.5–0.9, i.e. the switch happens at 50–90% of saturation).

Numerically: water reaches ~70% saturation within **less than a meter** of flowing over fresh limestone ([Palmer, allogenic-water dynamics](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1032&context=kip_articles)). The first meter of flow does nearly all the dissolving; beyond that point the water is nearly inert under first-order kinetics, and passage growth is governed by the slow fourth-order tail: a fracture widens everywhere at a glacial rate set by how fast water is pushed through while staying undersaturated.

This is the "kinetic trigger" that decides **where caves form**:

1. **Epikarst (top ~3–15 m):** water constantly re-equilibrates with soil/ground-air CO₂ (open system), so it stays aggressive. Dissolution rates are highest here ([Klimchouk 2004](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=6334&context=kip_articles)).
2. **Water table / epiphreatic zone:** mixing of waters with different $p_{CO_2}$/Ca histories (Bögli mixing corrosion) plus short flow paths to springs keeps water undersaturated along the whole route — this is where trunk passages race ahead ([Dreybrodt & Gabrovšek 2003](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=5791&context=kip_articles): modeled maximal dissolution rates active close to the water table, dropping rapidly with depth).
3. **Deep phreatic zone:** water is saturated and closed-system; dissolution proceeds at the fourth-order crawl. Deep loops form only where fissures are sparse but large (Ford's State 1, §3.3) or where deep aggressiveness is injected (hypogenic systems).

**Generator consequence:** aggressive dissolution is a *shallow* phenomenon. A believable cave generator should concentrate horizontal passage growth at paleo-water-table levels and vertical shaft growth in the top 10–30 m, with deep phreatic loops as the exception (sparse fissure frequency), not the rule.

### 1.6 Gypsum: same game, different rules

Gypsum (CaSO₄·2H₂O) dissolves by simple hydration with no acid required, and its kinetics are near-linear to saturation — it never hits calcite's fourth-order wall. Measured dissolution rates:

- Gypsum in pure water: 0.03–0.05 g/m²/s; rock salt ~3 g/m²/s (~100× gypsum); calcite in CO₂-water: 0.3–3×10⁻⁴ g/m²/s. **Gypsum dissolves ~100–1000× faster than limestone** by mass flux ([INERIS gypsum guide](https://www.ineris.fr/sites/default/files/contribution/Documents/Ineris-Guide_Gypse_VA-20_11Nov-WEB.pdf)).
- Gypsum is also ~10–30× more soluble than limestone by equilibrium concentration ([Li & Einstein 2017](https://doi.org/10.1002/2017wr021776)), with dissolution largely transport-controlled (limited by how fast fresh water sweeps the surface).

Two generator-relevant consequences. First, gypsum karst features form on human timescales — cavities can grow perceptibly over decades ([evaporite karstification modeling](https://mdpi-res.com/d_attachment/energies/energies-15-00761/article_deploy/energies-15-00761-v2.pdf?version=1642738727)) — and denudation rates of ~0.4 mm/yr (400 mm/kyr, ~10× typical limestone) are modeled and measured. Second, because dissolution stays fast near saturation, epigene gypsum caves are **laterally limited**: water saturates within tens of meters of the insurgence, so you get large sinkholes feeding small, short conduits rather than long branchwork systems ([Stafford et al. 2008](https://doi.org/10.5038/1827-806x.37.2.1)). Hypogenic gypsum mazes (the giant Ukrainian mazes) form instead by rising flow and free convection through fractured gypsum — geometry from the fracture field, not the surface.

---

## 2. Mass balance and denudation rates — parent-anchor verification

### 2.1 The mass balance

A drainage basin exports dissolved CaCO₃ at rate = runoff × concentration. The equivalent surface lowering (denudation):

$$\boxed{D = \frac{I \cdot c}{\rho_{rock}} \qquad \left[\frac{\text{mm}}{\text{kyr}}\right] = \frac{I\,[\text{mm/yr}] \times c\,[\text{mg/L}]}{\rho\,[\text{g/cm}^3]}}$$

with $I$ = specific runoff/infiltration, $c$ = dissolved CaCO₃ equivalent, $\rho \approx 2.6–2.7$ g/cm³. This is White's (1984) maximal-denudation form; Corbel's (1959) $D = 4IH/100$ (I in dm/yr, H in mg/L, ρ = 2.5) is the same equation in disguise ([concepts and methods review](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).

**Parent's anchor, re-derived and checked:** with open-system soil pCO₂ of ~0.02–0.03 atm, calcite saturation gives Ca ≈ 1.2–1.8 mmol/L ≈ 120–180 mg/L CaCO₃ — the parent's 1.5 mmol/L is dead center. With I = 0.5 m/yr (500 mm/yr, a wet temperate-to-subtropical catchment; Slovenia's Kras gets ~1400 mm precipitation, Borneo's Mulu >4000 mm):

$$D = \frac{500 \times 150}{2.7} \approx 28\ \text{mm/kyr}$$

The arithmetic verifies. **But the parent's "spread over 1–5 m of epikarst → 5.6–28 mm/kyr" framing is wrong, and this is the documented disagreement:** denudation rate is a *catchment-averaged surface-lowering equivalent* — the depth over which dissolution physically occurs does not divide the catchment-average number. Spreading the same mass flux over a deeper zone changes the *local* porosity gain, not the equivalent lowering. The correct comparison is against measured catchment denudation:

### 2.2 Measured denudation rates

- **Humid-climate global compilation (cosmogenic ³⁶Cl, China climate gradient):** arid-zone rates climb with precipitation up to ~700 mm/yr MAP, then humid rates **converge to 34.1 ± 11.7 mm/kyr**, kinetically limited ([cosmogenic study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).
- **Mass-balance (solute load) rates, temperate Europe:** Slovenia 77–80, Derbyshire 55–100, Yorkshire 42–51 mm/kyr (historical compilations in [Banks, UK dissolution rates](https://nora.nerc.ac.uk/id/eprint/503972/1/Dissolution%20rates%20in%20limestone%20v4.pdf)).
- **Direct surface measurements are systematically LOWER:** micro-erosion meter and tablet studies give 10–48 μm/yr (10–48 mm/kyr) — e.g. 28 mm/kyr on the Trieste Karst, 35 mm/kyr in a 129-year railway cutting in the Pennines, 11–48 μm/yr in the Austrian Alps with a catchment mass balance of 95 μm/yr at Kläffer Spring ([Austrian tablet study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X04003101)). The gap is physical: only ~30% of the dissolution potential is spent on the exposed surface; the rest works on fractures and in the epikarst ([concepts review](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).
- **Subarctic (Svartisen, Norway, 2600 mm runoff):** autogenic denudation 32.5 ± 10.2 mm/kyr ([Lauritzen 1990](https://doi.org/10.1002/esp.3290150206)).
- **Tropical (Florida estimate, White 1988):** ~1–2 inches per thousand years ≈ 25–50 mm/kyr ([FDOT sinkhole evaluation](https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0)).
- **Gypsum:** ~400 mm/kyr (§1.6) — an order of magnitude above limestone.

**Verdict on the parent's anchor:** the mass-balance arithmetic and the Ca concentration verify; the "5.6–28 mm/kyr" low end is an artifact of the epikarst-spreading misframing and is **below** the measured humid-climate band (roughly 20–100 mm/kyr; best-constrained central value ~34 mm/kyr). Where the parent's balance *is* conservative relative to reality: (a) allogenic runoff sinking from adjacent non-carbonate catchments can raise local denudation well above the autogenic balance; (b) conduit (point-recharge) flow delivers undersaturated water deeper than diffuse flow, so real cave-forming dissolution is not limited to the surface term; (c) tropical runoff often exceeds 1 m/yr, pushing D toward 50–80 mm/kyr. Where it is generous: arid and boreal karsts run 5–20 mm/kyr.

**Generator default:** 10–50 mm/kyr surface lowering in any humid limestone terrain, with karst landscape evolution timescales of 10⁵–10⁷ yr (see §5.3, tower karst).

---

## 3. Speleogenesis: how cave systems form

### 3.1 The two genetic end-members

**Epigenic caves** (the great majority, ~80–85% of explored systems) form by descending meteoric water that acquires aggressiveness from the surface — soil CO₂, and where allogenic streams sink directly into karst, from the mixing and dilution effects at confluences. The diagnostic pattern is the **branchwork**: tributary passages join downstream like a surface drainage net, discharging at one or few springs. Passage cross-sections are graded (fairly uniform along a flow path), with scallops and clastic sediment recording turbulent flow ([Palmer 1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2)).

**Hypogenic caves** (~15–20%) form by waters rising from below, with aggressiveness generated at depth. Branchwork patterns **never** form in hypogenic settings ([Klimchouk 2007 morphogenesis review](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004108)). Instead you get:

- **Network mazes** — tall narrow fissure passages intersecting in roughly orthogonal patterns, following the joint set;
- **Spongework** — coalesced intergranular pores, blobby 3D voids;
- **Ramiform / sequential-outward** rooms — chambers with irregular galleries branching at several levels;
- The diagnostic **morphological suite of rising flow**: floor slots (inlet fissures), rising wall channels, ceiling cupolas (buoyant upwelling), and interchange structures.

**Sulfuric acid speleogenesis (SAS) — the Lechuguilla/Carlsbad mechanism, verified:** these Guadalupe Mountains caves were dissolved not by carbonic acid but by **sulfuric acid generated at (and above) the water table**. H₂S, produced at depth by sulfate-reducing microbes reacting hydrocarbons with sulfate from dissolved Castile anhydrite, rose along joints into the incipient caves. Where H₂S-bearing water met oxygenated groundwater/atmosphere — at the water table and in the subaerial cave above — it oxidized ($H_2S + 2O_2 \rightarrow H_2SO_4$, microbially mediated, with native sulfur as an intermediate). The sulfuric acid attacked the limestone ($H_2SO_4 + CaCO_3 \rightarrow CaSO_4 + H_2O + CO_2$), dissolving the wall **and precipitating gypsum** (floor deposits up to 10 m thick in Carlsbad), with the released CO₂ forming carbonic acid for a second dissolution round. Evidence: patterns indicating in-situ acid generation, low-pH alteration minerals (alunite, endellite), deep solution rills, and δ³⁴S down to −25.8‰ in cave gypsum pointing to microbial fractionation ([Hill 1990](https://doi.org/10.1306/0c9b2565-1710-11d7-8645000102c1865d); [Palmer, SAS support](https://nmgs.nmt.edu/publications/guidebooks/downloads/57/57_p0195_p0202.pdf); [NCKRI special paper](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1017&context=kip_monographs)). SAS at each cave level lasted tens to hundreds of thousands of years, ending by mid-Pliocene; the active analogs are Movile Cave (Romania) and the Wyoming H₂S caves; 84 SAS areas are known globally.

### 3.2 Maze formation — the Q/L rule

Palmer's key quantitative result ([1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2); [epigenic vs hypogenic mazes](https://www.sciencedirect.com/science/article/abs/pii/S0169555X11001498)): enlargement rate scales with the ratio of discharge to flow distance, **Q/L**. Maze caves form wherever *many alternate routes* all have high Q/L simultaneously — which happens in three ways:

1. **Diffuse recharge through an insoluble but permeable caprock** — every fracture under the cap gets equally aggressive water (epigenic network caves under sandstone caps, usually < 0.5 km² areal extent);
2. **Floodwater pulses** — sinking streams backed up during floods push aggressive water into every available fissure (angular networks in fractured rock, anastomotic mazes along low-angle partings, spongework in porous rock);
3. **Hypogenic uniform injection from below** — rising water saturates the whole fracture network with aggressiveness at once.

Epigenic mazes typically enlarge to traversable size within ~500 ka; hypogenic transverse mazes in limestone within 1 Ma require permeable adjacent beds and ≤90% calcite saturation, which is rare — deep aggressiveness sources are usually needed ([Palmer 2011](https://www.sciencedirect.com/science/article/abs/pii/S0169555X11001498)).

### 3.3 The water-table vs deep-phreatic debate, resolved: the Ford–Ewers model

The century-long argument — vadose, deep-phreatic, or water-table origin for trunk passages — was resolved by Ford and Ewers (1978) with a model that contains all three as states of one variable: **the frequency of fissures penetrable by groundwater** ([Ford & Ewers 1978](https://doi.org/10.5038/1827-806x.10.3.1); [Ford's retrospective](https://digitalcommons.usf.edu/kip_articles/6893)):

- **State 1 (sparse fissures):** water is forced into deep loops below spring elevation — *deep phreatic* caves, high-amplitude looping passages. Requires few but large fractures.
- **State 2:** intermediate fissure frequency — loops with decreasing amplitude.
- **State 3:** high fissure frequency — passages track the water table closely (*water-table caves*), multiple contemporaneous levels.
- **State 4:** fissure frequency/matrix porosity so high the cave never concentrates into enterable passages.
- Later amplified with **State 0** (fissures too sparse for any genesis) and **State 5** (porosity too high for enterable scale) — so the parent's "five-state model" is right: Ford's amplification is 0–5, the 1978 paper has 4 ([Ford 2003 perspective](https://www.academia.edu/35559784/Perspectives%5Fin%5Fkarst%5Fhydrogeology%5Fand%5Fcavern%5Fgenesis)).

The water table does not precede the cave — the evolving plumbing *creates* it, with the master conduit propagating headward from the spring (Rhoades & Sinacori's refinement). Numerical modeling since confirms the sequence: karst aquifers evolve from distributed flow to a few breakthrough conduits that then drain their neighbors ([Dreybrodt & Gabrovšek 2003](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=5791&context=kip_articles); [Perne et al. 2014](https://doi.org/10.5194/hess-18-4617-2014)).

### 3.4 Breakthrough: the feedback that makes caves

The early evolution of a single fracture under constant head is dominated by the fourth-order kinetics: slow widening everywhere (water saturates in <1 m). But widening → more flow → fresh aggressive water penetrates deeper → the first-order zone extends further → a positive feedback. At **breakthrough**, flow and widening rate jump by several orders of magnitude within a geologically instant interval, after which widening is roughly uniform along the whole conduit at ~10⁻² cm/yr (Dreybrodt 1990, [The role of dissolution kinetics](https://doi.org/10.1086/629431); [Bakalowicz analysis](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/96WR01332)). Breakthrough times range from 10⁴ yr (short, steep, well-fed fractures) to >10⁶ yr (long, low-gradient). The conduit that breaks through first captures the flow of its competitors — this *competition* is what turns a uniform fracture mesh into a branchwork with a few big trunks and many starving tributaries.

### 3.5 Joint and fracture control on passage density and orientation

- **Orientation:** epigenic passages follow the two or three dominant joint/bedding orientations of the massif; the map of a branchwork cave is effectively a map of the fracture tensor. Carlsbad/Lechuguilla networks are "roughly orthogonal" because the joint sets are ([Palmer, SAS support paper](https://nmgs.nmt.edu/publications/guidebooks/downloads/57/57_p0195_p0202.pdf)).
- **Density:** passage density scales with penetrable-fissure frequency (Ford States 1→3→4, above). Sparse, wide fissures → few large deep passages; dense fissures → many small passages at the water table; extremely dense → no enterable caves at all.
- **Stratigraphy:** bedding-plane partings host anastomotic tubes; interbeds of shale/sandstone act as aquitards that perch water and localize dissolution at their contacts ("inception horizons"). Mammoth Cave's passages are stratigraphically pinned to specific Mississippian members ([NPS karst summary](https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf)).

**Generator consequence:** generate the joint field first (2–3 orientation sets with regionally consistent strikes), then bias passage alignment to it. Passage count per unit volume is a direct function of the fissure-frequency parameter, and the Ford state (deep loops vs water-table levels) should be a single slider.

---

## 4. Cave geometry and scale statistics

The numbers a voxel generator needs, with sources.

### 4.1 The longest systems

| System | Length | Notes |
|---|---|---|
| Mammoth Cave (Kentucky, USA) | **685.6 km** (2022–2026) | Longest known; parent's ">670 km" verifies and should be updated to ~686 km ([Wikipedia list, 2026](https://en.wikipedia.org/wiki/List_of_longest_caves); [NPS](https://www.nps.gov/articles/000/exploring-the-worlds-longest-known-cave.htm)) |
| Sistema Ox Bel Ha (Quintana Roo, Mexico) | **541.7 km** (Feb 2026), depth 57.3 m | Longest *underwater* system, 160+ cenote entrances ([Wikipedia](https://en.wikipedia.org/wiki/Sistema_Ox_Bel_Ha)) |
| Sistema Sac Actun (Mexico) | ~377–452 km | Second underwater ([showcaves stats](https://showcaves.com/english/explain/Statistics/Longest.html)) |
| Mulu systems (Borneo) | >355 km mapped | Includes Clearwater, Deer, etc. ([caves of Malaysia stats](https://cavesofmalaysia.wordpress.com/cave-statistics/)) |

**Parent anchor "Mammoth >670 km": verified, current figure 685.6 km.** The Yucatán flooded systems are the deep insight for a generator: an *epikarst-at-sea-level* platform with no surface drainage can host 500+ km of cave in a horizontal sheet only ~10–60 m thick — horizontal passage density can be enormous when the water table is flat and the recharge diffuse.

### 4.2 The deepest

- **Veryovkina Cave (Arabika Massif, Western Caucasus, Abkhazia/Georgia): 2,209 m** (2024 GNSS-traced revision; earlier figures 2,212–2,223 m from siphon surveys). Entrance at 2,285 m asl; length 17.5 km; includes the 155-m Babatunda shaft. Second-deepest: **Krubera Cave** in the same massif, the only other known cave past 2,000 m ([Wikipedia, Veryovkina](https://en.wikipedia.org/wiki/Veryovkina_Cave)).
- **Parent anchor "Veryovkina ~2.2 km": verified** (2,209 m current best).
- The >2 km club exists only in high alpine mountains with glacially-recharged, steeply-fissured massifs. Below ~−2,212 m the lower siphons defeat even cave divers; the 6,000+ m of subhorizontal passages found below −2,100 m in Veryovkina (an ancient aquifer collector) show that deep systems do exist where recharge is huge ([exploration history, same source](https://en.wikipedia.org/wiki/Veryovkina_Cave)).

### 4.3 Chambers

- **Sarawak Chamber (Gua Nasib Bagus, Mulu, Borneo):** laser-scanned 2011 at **600 m long × 435 m wide × up to 115 m high; area 164,459 m²; volume 9.58×10⁶ m³** — largest by area; second by volume ([Wikipedia, Sarawak Chamber](https://en.wikipedia.org/wiki/Sarawak_Chamber); [Mulu Caves Project](https://mulucaves.org.uk/articles/sarawak-chamber); [caves of Malaysia](https://cavesofmalaysia.wordpress.com/cave-statistics/)). **Parent anchor "600×415×80 m": partially wrong — width is 435 m and max height 115 m (the 80 m figure likely reflects an early estimate; the expedition's own conservative numbers were 700×400×100 from tape-and-compass, refined by laser to 600×435×115).**
- **Miao Room (Gebihe system, China):** volume 10.78×10⁶ m³ — largest by volume, smaller (140,900 m²) by area ([caves of Malaysia](https://cavesofmalaysia.wordpress.com/cave-statistics/)).
- **Son Doong (Vietnam):** ~4.5–5 km long, passage up to **150–250 m high and ~90–200 m wide**; a 2010 survey found a 2-km chamber 250 m high and 200 m wide; internal jungle with 40-m trees beneath collapsed skylights. **Parent anchor "5 km long, 200 m high": verified** (5 km, 150–250 m) ([caves of Malaysia](https://cavesofmalaysia.wordpress.com/cave-statistics/)).
- **Deer Cave (Mulu):** 4.1 km long, max width 168.7 m, average ceiling >120 m, highest roof (Antler Passage) 226 m, entrance 146 m wide ([same source](https://cavesofmalaysia.wordpress.com/cave-statistics/)).
- **Big Room, Carlsbad:** ~355×255 m footprint, ~78 m high (for scale on the ~3× smaller tier).

### 4.4 Passage cross-section shapes — the water-history decoder

The single most useful morphological fact for a generator: **cross-section shape encodes the flow regime that carved it** ([Palmer, Mulu and classical treatments](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf); [NPS Mammoth geology](https://digitalcommons.usf.edu/kip_articles/8226/)):

- **Phreatic tube:** circular/elliptical full-pipe cross-section, scalloped walls, gradient following the hydraulic grade (can go uphill in the flow direction). Formed below the water table. Diameter 1–10 m typical for trunk passages.
- **Vadose canyon:** tall, narrow (width:height commonly 1:3 to 1:10), meandering, cut by a free-surface stream on the floor; gradient follows gravity. Formed above the water table. Mulu's vadose canyons average 3.4 m wide × 14.8 m high ([morphology table](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).
- **Keyhole (composite):** phreatic tube above + vadose trench below — the classic "the water table dropped mid-history" signature.
- **Breakdown chamber:** angular, block-floored, ceiling height >> passage width; Mulu mean 42.6 × 31.2 m ([same source](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)) — this class contains all the giant chambers.
- **Basal slots / fissures:** 0.3–3 m wide, tens of meters tall.

Mulu's surveyed length split is a good generator prior for a mixed tropical system: phreatic tubes 31%, vadose canyons 23%, breakdown chambers 17%, mazes 13%, composite 9%, active streamways 6.5% ([same source](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).

**Scallops** (asymmetrical dissolution flutes) record paleo-flow velocity; mean scallop length inversely tracks velocity — 10 cm scallops ≈ dm/s flow, 1 cm scallops ≈ m/s. Useful as a texture-direction hint keyed to the generated paleo-flow field.

### 4.5 Vertical organization

The canonical karst vadose-to-phreatic stack ([Klimchouk 2004](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=6334&context=kip_articles); [Williams 2008](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1173&context=ijs)):

1. **Soil (0–0.5 m)** and **epikarst (typically 3–15 m; up to 30+ m in alpine fissured rock):** high-porosity weathered zone, 2–3 orders of magnitude more permeable than the bulk rock, functioning as a perched aquifer that stores storm water and delivers it *concentrated* to the few penetrating fissures (>50% of recharge arrives in the vadose zone already concentrated, per Kiraly).
2. **Vadose transmission zone:** largely unweathered bedrock (porosity <2%), vertical percolation. Hosts **shafts** (vertical to near-vertical wells, commonly several tens of m deep, fed by epikarst drainage — "the most common feature among explored vertical caves" per Klimchouk) and **meandering canyon passages** on bedding planes. Shaft density follows the epikarst drainage pattern — they cluster under doline axes and karren fields.
3. **Epiphreatic (flood) zone:** the band around the water table that floods seasonally or per-storm — passages here oscillate between air-filled and pipe-full, and dissolution is maximal (§1.5).
4. **Phreatic zone:** below the water table — saturated, low-gradient flow in tubes and loops, siphons at the downstream end.

Passage levels stack downward through time as base level drops: the oldest levels are highest, the active drain is deepest ([Mammoth NPS](https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf)). Multi-level systems with 3–5 levels spaced tens of meters vertically over a few hundred meters of total relief are the classic configuration (Mammoth, Swildon's Hole with four major levels, [Ford's Mendip work](https://legacy.caves.org/pub/journal/JCKS/PDF/V27/v27n4-Ford.htm)).

---

## 5. Rates and ages

### 5.1 Passage enlargement rates

Palmer's synthesis, confirmed by field measurements and modeling ([Palmer 1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2); [Dynamics of allogenic water](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1032&context=kip_articles)):

- Wall retreat in active conduits: **maximum ~0.01–0.1 cm/yr (0.1–1 mm/yr)**, set by kinetics and nearly independent of discharge beyond a threshold. Mean-annual rates ~0.01 cm/yr (0.1 mm/yr) are typical.
- The rate depends on **Q/L** before saturating: early-stage conduits grow at wildly varying rates; only those that gain discharge reach the kinetic maximum ("zone 2").
- Floodwater caves: adjacent fissures with initial widths ≥0.01 cm can reach traversable size within **10,000 years** (Palmer, allogenic-water paper above).
- Local measured wall retreat (MEM in active streamways): 0.087 mm/yr (Mulu active) declining to 0.021 mm/yr in relict passages ([Mulu study](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).

**Parent anchor "wall retreat ~0.01–1 mm/yr": verified** — the kinetic ceiling is 0.1–1 mm/yr and typical active conduits run near 0.1 mm/yr.

**Time for a 10-m trunk:** a 10-m-diameter tube grown from a ~0.1–1 cm initial fracture needs ~5 m of radial growth. At the typical 0.1 mm/yr that's ~50 kyr; at the kinetic maximum 1 mm/yr, ~5 kyr — but the early slow (pre-breakthrough) phase adds 10–100 kyr. Palmer's summary figure: **most caves require 10⁴–10⁵ yr to reach traversable size** ([Palmer 1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2)). Full integration of a large system (sink-to-spring): 10⁵–10⁶ yr. Denudation-based landscape maturity: 10⁶–10⁷ yr.

### 5.2 Denudation by region (summary table)

| Setting | Denudation (mm/kyr) | Source |
|---|---|---|
| Arid karst (MAP < 700 mm) | < 5–15 | [cosmogenic gradient study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133) |
| Humid temperate (Europe) | 20–100 (mass balance); 10–50 (surface) | [Banks UK compilation](https://nora.nerc.ac.uk/id/eprint/503972/1/Dissolution%20rates%20in%20limestone%20v4.pdf) |
| Humid global best-fit | 34 ± 12 | [cosmogenic study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133) |
| Subarctic, high runoff | ~32 | [Lauritzen 1990](https://doi.org/10.1002/esp.3290150206) |
| Tropical (Florida) | ~25–50 | [FDOT](https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0) |
| Gypsum karst | ~300–400 | [evaporite modeling](https://mdpi-res.com/d_attachment/energies/energies-15-00761/article_deploy/energies-15-00761-v2.pdf?version=1642738727) |

### 5.3 How old are the big systems?

- **Mammoth Cave:** the rocks are Mississippian (~330 Ma) but the cave is young — groundwater began interacting with the Girkin Limestone ~10 Ma ago; **upper levels fully developed by 3.2 Ma** (cosmogenic ²⁶Al/¹⁰Be dating of cave quartz pebbles); lower levels cut during the Pleistocene as the Green River incised, with the active level still forming. The passage-level stack is a Plio-Pleistocene record of base-level history ([NPS cave & karst summary](https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf); [Granger et al. 2001, cited therein](https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf)). **Parent anchor "Mammoth Plio-Pleistocene vs older precursors": verified** — ~10 Ma inception, main levels 3.2 Ma to present; karstification of the region began late Tertiary/early Quaternary per several independent lines ([AIPG field guide](https://www.uky.edu/KGS/geoky/fieldtrip/2005%20AIPG%20Guidebooks/MammothCave.pdf)).
- **Carlsbad/Lechuguilla:** SAS speleogenesis over ~8 Ma of episodic uplift, ending mid-Pliocene; vadose overprinting in the Pleistocene ([NCKRI special paper](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1017&context=kip_monographs)).
- **Mulu:** U-series stalagmite tiers at 412 ka / 187 ka / 34 ka record stepwise base-level lowering ([Mulu study](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).
- **Tower karst:** >10 Myr of evolution for fengcong→fenglin transformation ([Guangxi karst chapter](https://link.springer.com/chapter/10.1007/978-90-481-3055-9_30)).

---

## 6. Speleothems

### 6.1 The precipitation mechanism (degassing)

Drip water arrives at the cave carrying Ca²⁺ + 2HCO₃⁻ in equilibrium with the *epikarst's* high pCO₂ (0.02–0.05 atm). The cave air holds far less CO₂ (600–6,000 ppm typical; see §10). The disequilibrium drives CO₂ **outgassing** from the thin water film, which raises pH, shifts HCO₃⁻ → CO₃²⁻, and forces calcite to precipitate:

$$Ca^{2+} + 2HCO_3^- \rightarrow CaCO_3\downarrow + CO_2\uparrow + H_2O$$

Precipitation rate in thin films follows $R = \alpha\,(c - c_{eq})$ with $\alpha$ a temperature-dependent kinetic constant (α ≈ 1.3×10⁻⁵ cm/s at 10 °C, fitted 0–30 °C in [Dreybrodt & Romanov](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1049&context=kip_articles)). Rate depends on drip-water Ca, supersaturation, film thickness, temperature, and **cave-air CO₂** — doubling ambient CO₂ from 390 to 2,500 ppm roughly halves growth rate, and deposition ceases above ~5,000 ppm in several Texas caves ([James et al. 2015 global ventilation model](https://doi.org/10.1002/2014gc005658)).

### 6.2 Measured growth rates — parent anchor checked

- Modeled/theoretical rates validated against 31 European drip sites: good agreement (R² = 0.69), growth correlating with Ca²⁺ (R² = 0.61) and temperature, *not* drip rate (R² = 0.09) for the range investigated (drip intervals 0.01–2 s⁻¹... i.e. drips faster than every ~100 s) ([Baker et al. 2000, intra/inter-annual growth](https://www.sciencedirect.com/science/article/abs/pii/S0009254100003995)).
- Site means from the same body of work and the StalGrowth program: ~0.1–0.5 mm/yr for actively growing temperate stalagmites; one Gibraltar-site pair modeled at 0.18–0.32 mm/yr; seasonal bias 55–99% of annual growth in the fast season ([StalGrowth](https://www.mdpi.com/2076-3263/11/5/187)).
- Slow end: arid/alpine caves and dead caves — <0.01 mm/yr, effectively zero; hiatuses are common in the record.
- Fast end: tropical caves with high-Ca drip water — up to ~1 mm/yr sustained; >1 mm/yr is rare and usually short-lived.

**Verdict on parent anchor "stalagmites typically ~0.01–3 mm/yr": the top end is too generous.** Verified range: **~0.01–1 mm/yr, with the bulk of measured temperate/tropical sites between 0.05 and 0.5 mm/yr.** The controlling variables are drip-water calcium concentration (hence soil CO₂ and temperature) and cave-air CO₂; drip *rate* controls the shape (next) more than the vertical growth speed except at very slow drip (drip interval > ~100 s, where slow supply genuinely limits growth and produces candle-shaped forms) ([Sofular modeling study](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2022.969211/full)).

### 6.3 Shape: the equilibrium-radius law

Under constant conditions a stalagmite converges on an invariant shape that translates upward without changing form (Franke 1965; proven and modeled in [Dreybrodt & Romanov, Regular stalagmites](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1049&context=kip_articles)). The key result — a simple mass balance on the apex:

$$\boxed{R_{eq} = \sqrt{\frac{V_{drop}}{\pi\,\tau\,\alpha}} \quad \text{(equilibrium radius; } V_{drop} \approx 0.1\ \text{cm}^3, \tau = \text{drip interval, } \alpha \approx 1.3\times10^{-5}\ \text{cm/s)}}$$

Notably $R_{eq}$ is independent of the supersaturation. Worked example from the source: τ = 30 s, α at 10 °C → R = 9 cm — very realistic. The scaling is verified over three orders of magnitude (5 cm to 20 m diameter; the Cuban giant at Cueva San Martín Infierno is ~70 m tall, 20 m diameter, and *shape-similar* to a 4.5-cm specimen). Recent analytic work extends this to a Damköhler-number classification with three shapes — flat-top (Da>1), columnar (Da=1), conical (Da<1) — all observed in nature ([PNAS ideal stalagmites](https://pmc.ncbi.nlm.nih.gov/articles/PMC12557760/)).

Generator recipe: sample a drip interval per site (log-uniform 5–1000 s maps to radii ~3–40 cm); heights then follow age × growth rate (0.05–0.5 mm/yr), capped by passage ceiling. Columns form where stalactite + stalagmite meet — mostly under long-lived fracture drips.

### 6.4 Forms and why some caves are dead

- **Stalactites:** straw forms (thin tubes, active feed) to massive tapering cones; growth is self-limiting as the feed tube clogs.
- **Flowstone:** sheet flow over walls/slopes — visit any decorated cave; thickness cm–m over 10⁴–10⁵ yr.
- **Rimstone dams (gours):** precipitation lips at pool edges, terracing downhill; can grow to meter-scale walls.
- **Helictites:** ignore gravity, driven by capillary feed through a central canal; direction wanders (thin films evaporate/degas at the tip).
- **Dead caves:** drip water that has already equilibrated (long residence in big conduits, or prior calcite precipitation upstream in the epikarst during drought — "prior calcite precipitation" strips the supersaturation) deposits nothing. Also: high cave-air CO₂ (>2,000–5,000 ppm) suppresses or stops deposition ([James et al. 2015](https://doi.org/10.1002/2014gc005658)). Arid-region and hypogenic caves are typically bare. Generator: speleothem density should key to (a) distance below an active epikarst drip field, (b) cave-air ventilation (near entrances → more degassing → more deposition, until you get close enough to the entrance that dust/drying dominates), (c) CO₂ state.

---

## 7. Breakdown: the mechanics of underground voids

### 7.1 What holds a ceiling up

The roof of a bedded limestone passage behaves as **voussoir beams**: a cracked "beam" of rock between vertical joints, arching under compression between abutments. Classic beam theory *underestimates* their stability because the cracked beam re-arches; failure modes are midspan snap-through buckling (thin beams, span/thickness >10), abutment crushing, and shear slip along cross-joints ([Diederichs & Kaiser, voussoir analogue](https://www.sciencedirect.com/science/article/abs/pii/S0148906298001806)). Field extensometer data put the linear-behavior limit at ~10% of bedding thickness of midspan deflection.

Empirical stability envelopes from mining and cave engineering:

- **Barton's Q-system unsupported-span envelope** (for man-made openings): Span = 2·Q^0.66 — the design curve for permanent unsupported excavations ([Barton 1976, in Jordá-Bordehore](https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf)).
- **Jordá-Bordehore's natural-cave fit (137 large-span caves):** the stable/unstable boundary is **Span = 5.4·Q^0.73** — natural caves sit systematically above the mining envelope because they are unlined, unblasted, dome-arched, and have had 10⁴–10⁶ yr of natural proof-testing ([same source](https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf)).
- Practical spans: temperate-karst passage sections are usually <10 m wide; optimal sites reach 50 m; tropical chambers exceed 100 m; the record is Sarawak's ~300–435 m. US room-and-pillar limestone mines average 13.5 m rooms and stay naturally stable in good roof ([NIOSH roof stability survey](https://stacks.cdc.gov/view/cdc/226788)).
- Long-term: cave roof beams creep. Compressive failure by subcritical crack propagation over 10⁴+ yr produces the huge observed deflections, with failure by splitting/delamination ([Tharp & Holdrege, very long-term loading](https://doi.org/10.1201/9781003761365-140)). The 1,400-year-old Heidong quarry caverns hold an **81-m unsupported span** (dome roof, massive tuff, Q≈51) — beyond all engineering expectations, proving that arch geometry and intact rock matter more than conservative design lines ([Heidong study](https://doi.org/10.18814/epiiugs/2013/v36i1/006)).

**How 100-m chambers survive:** (1) thick, massive, strong beds (Sarawak's strata are separated by 15–20 m; [Jordá-Bordehore](https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf)); (2) dome/arch geometry rather than flat spans; (3) deep burial giving confining stress; (4) natural selection — the spans that didn't work already fell, and their breakdown now supports the remaining roof.

### 7.2 Breakdown initiation and the pile

Breakdown is not only "the ceiling got too wide." Documented initiators: bedding-plane separation above passages, lateral stream undercutting of walls, loss of buoyant support when the water table drops (buoyancy contributes ~40% of ceiling support in flooded limestone — [Andrejchuk & Klimchouk, gypsum breakdown](https://doi.org/10.5038/1827-806x.31.1.4)), and **vadose weathering**: oxidation of sulfides and clay hydration along veins and impure beds wedges blocks loose over time; fallen blocks are typically bounded by *pre-existing* discontinuities, with coated (not fresh) faces ([Osborne 2002, breakdown by vadose weathering](https://doi.org/10.5038/1827-806x.31.1.3)). In Mammoth's dry passages, **gypsum-crystal wedging** — gypsum crystallizing in wall fractures plus replacement of limestone by gypsum — produces characteristic shard-and-rock-flour breakdown with curved ceiling plates hanging at steep angles ([gypsum wedging in Mammoth](https://caves.org/wp-content/uploads/Publications/JCKS/v65/v65n1-White.pdf)).

**Pile geometry and the self-arrest law.** When a ceiling fails, a breakout dome stopses upward while breakdown accumulates below. The bulk volume of broken rock exceeds the solid volume by the **coefficient of loosening** $K_{loos}$ ≈ 1.1–1.3 (clays) to 1.5–2+ (hard rock). The dome stops migrating upward when the pile chokes it:

$$h_c = \frac{h_0}{K_{loos}-1}$$

the "height of closure" above a receptacle cavity of initial height $h_0$. For $h_0$ = 5 m and $K_{loos}$ = 1.4, closure occurs 17.5 m up; with $K_{loos}$ = 1.5–2, 5–10 m. This is why big cave chambers under 45–60+ m of overburden almost never break through to the surface — the pile catches up first ([Andrejchuk & Klimchouk 2002, Kungurskaya Cave](https://doi.org/10.5038/1827-806x.31.1.5)). Kungurskaya recorded 64 failure events in 23 years, predominantly slab breakdown, with breakout domes 20–24 m high whose floors stay under 10 m of clearance because the talus fills them.

**What a breakdown pile looks like inside:** a cone or ridge of angular blocks (slab to multi-meter block size) grading from coarse irregular blocks at the base through cobbles to rock flour at the top of gypsum-wedged piles; slope at roughly the angle of repose (~35–40°) but *mobile* — piles creep and re-sort without new collapses ([Osborne 2002](https://doi.org/10.5038/1827-806x.31.1.3)). Ceiling above the pile shows a fresh breakout dome (cupola) with exposed bedding edges. In maze caves, breakdown talus is the main exploration obstacle.

---

## 8. Karst surface terrain

### 8.1 Sinkholes (dolines): types, mechanics, sizes

Four mechanisms, after the Florida classification ([USGS WRI 85-4126](https://pubs.usgs.gov/wri/1985/4126/report.pdf); [FL FGS favorability report](https://inspectapedia.com/vision/Florida-Sinkhole-Report.pdf)):

1. **Solution sinkhole:** bare/thinly-covered limestone dissolves and subsides at the same rate; funnel-shaped, gentle; diameter 5–100+ m, depth up to ~10 m. Dominant on exposed karst.
2. **Cover-subsidence sinkhole:** sand cover ravels slowly downward into widening fissures in the buried limestone; shallow broad depression, meters to tens of m across, forming over months to millennia.
3. **Cover-collapse sinkhole:** cohesive clay layer bridges a growing void; when the bridge fails, sudden collapse — steep-walled, 1–100 m across (median new Florida collapse ~3–25 m; the biggest exceed 100 m), hours from trigger to hole ([Marion County FL stats](https://pmc.ncbi.nlm.nih.gov/articles/PMC6509126/); [FDOT](https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0)). Triggers: water-table decline (pumping, drought), heavy rain, vibration.
4. **Rock collapse (rare):** the cave roof itself fails to the surface — the terminal-breakdown mechanism of §7.2, mostly self-arrested unless overburden is thin.

**Densities (parent anchor "hundreds per km²" — verified):** Slovenia's national lidar census found **471,192 dolines**; average solution doline: 9 m deep, 42 m diameter, 14,000 m³; density on level surfaces **up to 500/km²** (covering up to 60–80% of the ground in extreme patches); the Classical Kras plateau averages ~60/km²; Postojna area ~300/km²; collapse dolines are rare (314 in the country, mean depth 49 m, mean volume 1.2×10⁶ m³, largest 11.6×10⁶ m³) ([Mihevc & Mihevc 2021](https://doi.org/10.3986/ac.v50i1.9462); [UNESCO Classical Karst nomination](https://whc.unesco.org/fr/listesindicatives/6072/)). Pinellas County, FL: ~2.2/km² identified in 1926 ([USF thesis](https://digitalcommons.usf.edu/etd/1306)). Yucatán state: 6,717 depressions over 454 km² — 4,620 dolines, 2,021 uvalas, 76 poljes ([Aguilar et al.](https://doi.org/10.4311/2015es0124)).

### 8.2 Cenotes and the Chicxulub ring

Cenotes are collapse dolines that intersect the water table in the flat Yucatán platform — the ceiling openings of the flooded Ox Bel Ha/Sac Actun systems. The **Ring of Cenotes** is a 165–180-km-diameter semicircular band of high cenote density tracing the buried rim of the 66-Ma Chicxulub impact crater beneath ~1 km of post-impact carbonate: fracturing and slumping along the crater's outer slump zone localized groundwater flow, dissolution, and collapse, marking the boundary between unfractured limestone inside the ring and fractured limestone outside ([Perry et al. 1995](https://doi.org/10.1130/0091-7613(1995)023%3c0173:ROCSNW%3e2.3.CO;2); [Connors et al. 1996](https://doi.org/10.1111/j.1365-246x.1996.tb04066.x); [surficial geology of Chicxulub](https://link.springer.com/article/10.1007/BF00575099)). The cenotes themselves are young — likely <130 ka, formed during the last interglacial highstand — but their alignment is inherited from impact-age structures ([Northwestern explainer](https://sites.northwestern.edu/monroyrios/ring-of-cenotes/); [EGU 2024 analogue modeling](https://doi.org/10.5194/egusphere-egu24-3188): ~6,500 cenote outlines mapped, elongation E-W regionally, deviations above the crater margin attributed to impact-induced stress/isostatic relaxation). Crater diameter estimates from the ring and gravity: ~180 km (cenote-ring school) to ~240 km (gravity school).

**Generator gold:** an ancient buried structure (impact, graben, reef front) can express on the surface millions of years later purely through the karst drainage-density field. Ring-shaped sinkhole swarms are a legitimate, real-Earth pattern.

### 8.3 Poljes, karren, and the rest of the surface vocabulary

- **Poljes:** giant flat-floored closed basins with perennial or seasonal flooding through ponors (swallow holes) and estavelles (openings that alternate between sinking and rising with flood stage). Slovenia's Cerknica polje: 38 km² floor, intermittent lake up to 26 km²; Planina polje: 6×2 km, floods lasting months ([UNESCO nomination](https://whc.unesco.org/fr/listesindicatives/6072/)). Yucatán's 76 poljes occupy ~half the depression area ([Aguilar et al.](https://doi.org/10.4311/2015es0124)).
- **Karren / karrenfeld:** cm-to-meter solution sculpting of bare limestone — rillenkarren (solution flutes, cm-scale, on steep faces), clints and grikes (pavement blocks and their 0.1–3 m slots), kamenitzas (solution pans). The surface texture of every bare karst.
- **Uvalas:** compound depressions from coalesced dolines — the intermediate step between doline field and polje.

### 8.4 Tower karst vs cockpit karst — the Guilin question

Two interlocked tropical end-members ([Day & Tang, Guilin slope survey](https://doi.org/10.1002/1096-9837(200010)25:11%3C1221::AID-ESP133%3E3.0.CO;2-D); [Waltham, karst lands of southern China](https://www.researchgate.net/publication/263374259_The_karst_lands_of_southern_China)):

- **Fengcong (peak cluster / cockpit):** clustered conical hills sharing a common base, enclosed doline/cockpit depressions between them; hillslope angles 60–75°; formed under *rapid uplift* with thick vadose zones and vertical flow. Dominates the uplifted Guizhou plateau.
- **Fenglin (peak forest / tower karst):** isolated steep towers rising from an alluvial karst plain; mean slope 62.4° at Guilin, commonly near-vertical walls >100 m tall (Yangshuo; Lijiang plain towers average 130 m). Formed where base level is *stable or slowly falling*: aggressive allogenic water at the plain's water table undercuts tower feet with dissolution notches and foot caves, undercut-collapse converts cones to towers, and lateral planation proceeds around the residuals.

Requirements for mature fenglin: pure strong horizontally-bedded limestone in great thickness; an alluviated plain maintained by allogenic sediment; water table pinned at the plain; hot wet climate; slow uplift matched to denudation. Timescale >10 Myr ([Guangxi chapter](https://link.springer.com/chapter/10.1007/978-90-481-3055-9_30)). Tower edges follow sub-vertical fault planes; tower summits preserve a log-normal height distribution echoing an old base-level surface ([Lijiang structural study](https://www.schweizerbart.de/papers/zfg/detail/36/98327/Structural_and_hydrogeological_origin_of_tower_karst_in_southern_China_Lijiang_plain_in_the_Guilin_region)). Field evidence at Guilin supports *parallel* development of fengcong and fenglin by different mechanisms rather than a strict sequence ([Day & Tang](https://doi.org/10.1002/1096-9837(200010)25:11%3C1221::AID-ESP133%3E3.0.CO;2-D)).

### 8.5 Hydrologic surface expressions

- **Losing streams / sinking rivers (ponors):** allogenic surface streams that sink at the karst boundary, leaving **blind valleys** terminated by swallow holes.
- **Resurgences (springs):** the outlets. Karst springs carry the conduit system's discharge; the Slovenian Ljubljanica drains a ~1,100+ km² catchment almost entirely underground through the polje chain ([UNESCO nomination](https://whc.unesco.org/fr/listesindicatives/6072/)).
- **Estavelles:** openings that function as ponor in low stage and spring in high stage — the polje's overflow plumbing.
- **Mature karst has *no* surface drainage** — the Kras plateau is "a bleak, waterless place" (the original meaning of *kras*), with everything underground ([USGS](https://pubs.usgs.gov/wri/1985/4126/report.pdf); [UNESCO](https://whc.unesco.org/fr/listesindicatives/6072/)).

**Generator consequence:** karst is detectable from the surface hydrology alone — sinking streams, blind valleys, dry crossings of expected valleys, resurgent springs at stratigraphic/lithologic boundaries, and flood pulses.

---

## 9. Lava tubes and other cave types

### 9.1 Lava tube formation mechanics

A lava tube is the drained conduit of a channelized basalt flow. Four roofing mechanisms are observed ([Greeley, USGS PP1350 ch. 59](https://pubs.usgs.gov/pp/1987/1350/pdf/chapters/pp1350_ch59.pdf); [Valerio et al. 2008](https://doi.org/10.1029/2007jb005435); [Dragoni et al. 1995](https://doi.org/10.1029/94jb03263)):1. **Channel crust-over:** on sluggish-moderate channel flow (<1–3 m/s), crust grows inward from the levees and closes "like a zipper" along the medial bright zone;
2. **Rafted-crust jamming:** crustal plates torn from levees raft downflow and weld together at constrictions (the majority of Mauna Ulu channel surfaces were >50% crusted);
3. **Levee overgrowth:** overflow splashes build levees upward and inward until they arch over (2–5 m/s flows);
4. **Lava-toe budding / sheet-flow lobation:** pahoehoe toes extend beneath a solidified crust with no open-channel phase at all — many tubes never see daylight.

Physics of the transition: a stationary roof becomes possible where the shear stress at the crust base falls below the crust's yield strength — favored by *wide, thin* channels (reduced basal shear), *gentle slopes* (the Etna model finds tube formation for slopes <~6°), and *low-to-moderate effusion rates* (<~10 m³/s) ([Valerio et al. 2008](https://doi.org/10.1029/2007jb005435)). Once roofed, the tube insulates the flow spectacularly: the Ai-laau flow feeding Kazumura lost only ~4 °C over its 39-km run ([Allred & Allred, Kazumura development](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)). Inside, active tubes deepen by **thermal erosion** (melting + sweeping of the floor), producing stacked multi-level systems as the master tube incises below its own earlier floors; measured downcutting reached 10 cm/day at Kilauea and ~15 m of incision in 18 months at Mauna Ulu ([same source](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).

### 9.2 Sizes — parent anchor checked

- **Typical diameters: 1–15 m**, with exceptional tubes to ~21 m wide × 18 m high (Kazumura's largest cross-sections). **Parent anchor "up to ~30 m": slightly generous — ~21×18 m is the documented Kazumura maximum**; some Hawaiian skylight-adjacent chambers approach 25–30 m but the robust figure is ~21 m ([Kazumura study](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- **Kazumura Cave, Hawai'i: 59.3 km surveyed (1995 expedition), vertical extent 1,098 m, average slope 1.9° over 32 km linear, mean cross-section 20.3 m², ~1.2×10⁶ m³ volume, 82 entrances** — the world's longest lava tube. **Parent anchor "65 km": disagreement — the published survey figure is 59.3 km** (some later popular lists cite ~65 km; I found no survey source beyond the 59.3 km paper, so use 59.3–65 km with the survey number primary) ([Allred & Allred](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- Kazumura's age is only 350–500 years BP — lava tubes are geologically *instant* caves compared to limestone's 10⁴–10⁶ yr.

### 9.3 Interior features, drainage, and survival

- **Cooling lips / floor levees:** crust welded along the flow margins at the level of the last active lava surface, forming raised benches along walls — near entrances (where cooling air circulated) they grow into "tube-in-tube" profiles ([Kazumura study](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- **Rafted floor:** the frozen final flow surface — pahoehoe ropes, shove-pressure ridges; walls show horizontal flow ridges and glaze; ceilings near entrances are frothy/popcorn-textured from degassing.
- **Skylights and pit craters:** roof collapse windows; Kazumura retains 50+ pre-drainage skylights ([same source](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- **Relict survival:** after drain-out, tubes persist as caves; subsequent roof collapse segments them (each entrance = a collapse), and ceiling accretion linings spall. Because the host is young basalt with no subsequent speleogenesis, lava tubes are pristine snapshots — no speleothems beyond tiny secondary deposits, no sediment fill beyond entrance wash.

**Generator contrast (limestone vs lava):** lava tubes are *single-trunk, downdip-following, low-sinuosity-meandering, mostly single- or few-level* conduits whose cross-section is set by the flow's thermal budget, not by fracture statistics; they begin at a vent and end at a flow-front delta, with braided-distributary complexity near the front.

### 9.4 Other cave types (brief)

- **Talus caves:** voids in rockfall boulders — no genesis of their own; geometry = boulder packing.
- **Tafoni:** cavernous weathering hollows in cliff faces (salt/hydration weathering + wind), dm-to-room scale, shallow.
- **Glacier caves:** meltwater channels in/under ice — meters to tens of m, fast-forming (seasons), fast-dying.
- **Sea caves:** wave-cut notches along weak zones (joints, dykes) in cliffs — typically 5–50 m deep, tall narrow mouths, tidally flooded floors.
- **Piping / pseudokarst:** suffosion pipes in badlands loess and colluvium — unconsolidated material washed out along subvertical pipes, producing sinkhole-like depressions without bedrock dissolution (cover-subsidence minus the limestone).
- **Quartzite/silicate karst:** exists (Venezuelan tepuis) via extreme residence times — slow, but the same geometry vocabulary.

---

## 10. Cave climate and environment

### 10.1 Temperature

**Verified anchor: cave temperature ≈ local mean annual surface temperature.** The rock mass acts as a low-pass thermal filter; the cave asymptotically approaches the flow-weighted mean temperature of infiltrating water, set by the local annual mean ([Badino 2004](https://doi.org/10.5038/1827-806x.33.1.10)). Refinements that matter:

- Annual amplitude inside deep cave zones: **0.1–8.8 °C** measured across a global cave sample — Planinska (Slovenia) 0.1 °C vs Balcões (Azores) 8.8 °C ([comparative study](https://pmc.ncbi.nlm.nih.gov/articles/PMC10676404/)); equilibrium is typically reached within meters to a few hundred meters of an entrance depending on airflow.
- Lapse rate inside deep caves is not the atmospheric 6.5 °C/km but between the water-adiabatic (~2.3 °C/km) and moist-air (~5 °C/km) values ([Badino 2006](https://doi.org/10.3986/ac.v34i2.261)).
- Caves lag climate change by centuries (mountain-scale equilibration times).
- Lava tubes track the host rock and local microclimate — Kazumura runs 15 °C near the crater to 22 °C at the coast ([Kazumura study](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).

### 10.2 Humidity and CO₂

- Deep-cave RH is **95–100%** (near-condensation); it varies little with surface RH ([James et al. 2015](https://doi.org/10.1002/2014gc005658)).
- **Cave-air CO₂**: 420 ppm (atmosphere) at entrances → **~600–6,000 ppm in typical ventilated cave interiors** → 1.1–3.7% at Lascaux's confined galleries ([Lascaux aerology study](https://www.springerprofessional.de/the-co2-dynamics-in-the-continuum-atmosphere-soil-epikarst-and-i/16644174)) → >5% in poorly-ventilated deep systems, where it is the growth-limiting variable for speleothems (§6). CO₂ is the main caving hazard gas; it stratifies and accumulates in poorly-ventilated lower galleries during summer stagnation, flushed in winter.

### 10.3 Ventilation regimes: chimney vs barometric

Two airflow engines, different signatures ([Pflitsch et al. 2010](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=2599&context=kip_articles); [Gomell & Pflitsch 2022](https://doi.org/10.5038/1827-806x.51.3.2437)):

- **Chimney-effect (convective) caves** — the common case, any cave with two entrances at different elevations: winter = cold air sinks in, warm cave air out the top; summer reverses. Flow <0.5 m/s typically, strongly seasonal. Driver: density contrast between cave air (≈annual mean) and outside air; in the tropics, diurnal density cycling ventilates caves daily ([James et al. 2015](https://doi.org/10.1002/2014gc005658)).
- **Barometric caves** — one effective entrance and a huge volume-to-opening ratio: the cave behaves as a **low-pass pressure filter**. Falling outside pressure → the cave exhales; rising → it inhales — through the same openings, in the *same* direction at all openings, reversing on timescales of seconds to days as cyclones pass. **Wind Cave (South Dakota) is the classic example** (found by its audible cave wind; Conn 1966 proved the barometric mechanism), Jewel Cave second. Airflow of **several m/s at the openings**. Measured dynamics: internal pressure signals are displaced (altitude), delayed (travel time), smoothed (semi-diurnal atmospheric tides damped by 0.06–0.19 hPa), and damped (multi-day cyclonic variation reduced up to ~1.6 hPa at Jewel's Deep Camp); pressure response explains >99% of internal pressure at most sites ([Gomell et al. 2021](https://doi.org/10.5038/1827-806x.50.3.2393); [Gomell & Pflitsch 2022](https://doi.org/10.5038/1827-806x.51.3.2437)). Pressure-to-temperature coupling: ~2–14×10⁻³ °C/hPa via adiabatic compression, plus up to 1 °C/hPa barometric-wind advection near entrances ([French cave APV study](https://www.sciencedirect.com/science/article/pii/S2772883822001200)).

### 10.4 Entrance zones

From outside in: **entrance zone** (variable T/RH, daily and seasonal swing, green plants, twilight) → **twilight zone** (dim light, mosses, temperature still tracks surface with lag) → **transition/mist zone** (constant T near the cave mean, RH near 100%, condensation mist when warm humid air enters a cool cave — the drip-forming belt) → **deep cave** (total dark, T constant to <1 °C, RH 95–100%, air still unless convective/barometric flow). Zone lengths scale with entrance size and airflow: meters for a tight crawl, hundreds of meters for a large walk-in entrance ([Pflitsch et al. 2010](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=2599&context=kip_articles)).

### 10.5 Acoustics — the one-paragraph cross-reference

For the project's audio/reverb system, the physically-derived switch variables are: **(1) reverberation time rises steeply past the twilight zone** — a cave passage is a hard-walled, low-absorption waveguide, so RT60 grows from open-air ~0 s to seconds in chambers (Sarawak-class volumes toward 10+ s); model it as a function of local passage volume/wall area (Sabine: $T_{60} = 0.161\,V/(A\,\bar\alpha)$) rather than a binary flip. **(2) The "outside/cave" boundary is also a humidity/temperature/density boundary** — the mist zone's condensation and the temperature step are what a player *feels* and hears (air density changes sound speed ~0.6 m/s per °C). **(3) Barometric and chimney airflow is audible wind** — a few m/s at openings of large systems, reversing on weather timescales; same noise-budget category as surface wind but keyed to a different clock. **(4) Water** — drip rate (which the speleothem density model of §6 predicts) and cave-stream noise replace surface ambience; see the project's existing treatment in `research/natural-sound-generation-physics.md` §3.4 for the flow-noise synthesis side, which applies unchanged to underground streams.

---

## 11. Practical simulation synthesis: a cave-generation recipe for a voxel engine

Opinionated, numbers-first, targeting our engine's actual architecture (SVO octree at 2⁻⁷ m finest voxels near the camera, 512-m root regions, FastNoise2 for fields) — but architecture-agnostic where possible. The one-line philosophy: **generate the water first, then carve what the water carves.** Every real pattern in §§1–8 follows from flow + chemistry + time; faking the patterns directly (random worm tunnels) gets the statistics wrong in ways players feel.

### 11.1 The generation stack

**Step 0 — the joint/fracture field.** Generate 2–3 regionally-consistent orientation sets (strike sampled per-biome, dip 80–90°) plus near-horizontal bedding (1–3° regional dip) — domain-warped ridged 3D noise thresholded into planes, or fracture intensity sampled per column from 2D noise. This field is *the* skeleton: passage orientation (§3.5), maze density (§3.2), and doline alignment (§8.2) all key to it. Fracture spacing: 1–10 m (dense, Ford State 3–4) to 20–100 m (sparse, State 1–2).

**Step 1 — water table and base level.** One 2D field $z_{WT}(x,y)$: the heightfield low-pass filtered toward river/sea level, with aquifer drawdown near resurgences. Passage levels = paleo-water-table surfaces: 3–5 discrete levels spaced 20–60 m vertically, each older-and-higher (§4.5, Mammoth geometry). A per-region base-level history (100–300 m incision over ~3 Myr) generates the level stack for free.

**Step 2 — the flow graph.** A directed graph on the region's coarse 2D grid: inputs (doline swarms, sinking streams, diffuse epikarst recharge) → conduits → springs at valley floors/lithologic contacts. Grow headward along steepest water-table descent (the Rhoades–Sinacori direction, §3.3), branching with Horton-like statistics (the surface-drainage numbers from Part B apply underground too). Each edge carries discharge Q and flow length L — keep the **Q/L** ratio (§3.2) as the growth-rate proxy: top-decile edges become trunk passages, the rest crawlways or sub-voxel.

**Step 3 — carve the density field.** Per graph edge, carve a cross-section into the voxel density field:

- **Horizontal trunk at water-table levels:** phreatic tube, diameter 2–15 m (Mammoth trunks to 20 m; Mulu tubes mean 8.7×6.3 m; Deer Cave is the 100-m-class outlier, §4.3). Elliptical, slightly wider than tall.
- **Vadose input passages:** vertical shafts (cylindrical wells 2–10 m diameter, 10–100+ m deep; Veryovkina's Babatunda is 155 m) at doline axes and sinking-stream points; meandering canyons (3–15 m deep, 1–3 m wide, §4.4) along bedding routes from shafts to the water table.
- **Keyholes:** wherever a level was phreatic then drained — tube above, 1–2 m trench below. Two extrusions, instantly reads as "real cave."
- **Chambers:** from the size distribution, not uniform placement — lognormal-ish, median ~10–20 m span, 1% tail to 50 m, ~0.01% to 100+ m. Place at passage confluences (Q jumps), the water-table/spring margin, and under thin roof beds; bias survival to "massive bed" stratigraphic units (a per-region bed-competence field), capping spans at ~10–30 m elsewhere (§7.1).
- **Mazes:** where fracture intensity is high *and* uniform aggressiveness holds (under sandstone caprock; floodwater backflooding above trunk confluences; hypogenic settings), fill 0.1–0.5 km² footprints with network/spongework — orthogonal fissure passages 1–4 m wide, 5–15 m tall.

**Step 4 — breakdown.** Post-process every carved span with a stability check (span vs bed competence, §7.1): spans over threshold get a breakout dome above plus a talus cone below at 35–38° repose, block sizes 0.3–3 m, dome height capped by the self-arrest law ($h_c = h_0/(K_{loos}-1)$, $K_{loos}$ 1.4–1.8). Terminal breakdowns where passages meet valley walls = entrances.

**Step 5 — decoration.** Speleothem density per §6.4: high where the ceiling is under an active drip field, moderate near entrances (ventilated → degassing), zero in flood zones, dead caves where a prior-precipitation or high-CO₂ flag is set. Stalagmite radius from the $R_{eq}$ law with drip interval log-uniform 5–1000 s → 3–40 cm radius; straws/flowstone/rimstone as tiling variants; helictites as rare wander-noise forms. Scale check: 0.05–0.5 mm/yr vertical growth against the cave's simulated age gives believable heights without simulating growth.

**Step 6 — fill with water/climate.** Below $z_{WT}$: fill to the water table (siphons at level downstream ends). Tag each carved cell with climate state: distance-to-entrance → temperature blend from surface MAT to cave-constant, RH ramp to ~100%, CO₂ field (entrances 420 ppm → interior 1,000–6,000, higher in dead-end lowers), and ventilation regime (chimney if two entrances straddle elevation; barometric if volume/opening ratio is huge — then modulate an audible wind with a slow weather-timescale oscillator, §10.3). These tags feed the audio system's outside/cave switch as continuous parameters rather than a boolean.

### 11.2 Parameter table (all values sourced above)

| Parameter | Range | Default | Source § |
|---|---|---|---|
| Soil/epikarst pCO₂ (temperate/tropical) | 0.001–0.06 atm | 0.02 | §1.2 |
| Open-system saturation Ca | 1–2 mmol/L | 1.5 | §2.1 |
| Denudation rate (humid) | 10–100 mm/kyr | 30 | §2.2 |
| Conduit wall retreat (active) | 0.01–1 mm/yr | 0.1 | §5.1 |
| Time to traversable passage | 10⁴–10⁵ yr | — | §5.1 |
| Epikarst thickness | 3–15 m (30+ alpine) | 8 | §4.5 |
| Vadose shaft depth | 10–155 m | 30–80 | §4.5 |
| Water-table level spacing (multi-level) | 20–60 m | 40 | §4.5 |
| Trunk passage diameter | 2–20 m | 6 | §4.4 |
| Canyon width:height | 1:3–1:10 | 1:5 | §4.4 |
| Chamber span distribution | 10–435 m, lognormal tail | median 15 m | §4.3 |
| Chamber height:span | 0.1–0.35 | 0.2 | §4.3 |
| Cave-system areal footprint | 0.5 km² (epigenic maze) – 200+ km² (Mammoth-class) | 5–50 km² | §4.1 |
| Passage density (map length per area) | 1–10 km/km² typical; up to ~30 in maze patches | 4 | §4.1/§4.4 |
| Doline density (mature karst) | 60–500 /km² | 100–200 | §8.1 |
| Doline diameter (solution) | 14–80 m (5th–95th pct, Slovenia) | 40 | §8.1 |
| Collapse doline depth | 20–100+ m | 50 | §8.1 |
| Polje size | 2–40 km² | — | §8.3 |
| Tower karst: tower height/width | 50–300 m / 50–150 m | — | §8.4 |
| Fengcong hillslope | 60–75° | — | §8.4 |
| Lava tube diameter | 1–21 m | 3–10 | §9.2 |
| Lava tube length (master) | 1–59 km | — | §9.2 |
| Lava tube gradient | 1–6° (mean Kazumura 1.9°) | 2–4° | §9.2 |
| Cave interior temperature | local MAT ± 0.1–8.8 °C annual swing | MAT | §10.1 |
| Cave RH (deep) | 95–100% | 99% | §10.2 |
| Cave CO₂ | 600–60,000 ppm | 1,000–6,000 | §10.2 |
| Barometric airflow (openings) | 0.5–10 m/s, weather-paced | — | §10.3 |
| Stalagmite radius | 3–40 cm (giant 10 m) | 9 cm @ τ=30s | §6.3 |
| Stalagmite vertical growth | 0.01–1 mm/yr | 0.1 | §6.2 |
| Gypsum denudation | ~400 mm/kyr | — | §1.6 |
| Breakdown loosening K | 1.1–2.0 | 1.5 | §7.2 |
| Breakdown repose angle | 35–40° | 37° | §7.2 |

### 11.3 Coupling to the surface heightfield

Karst must deform the surface, or it's invisible. Cheapest-first:

1. **Doline swarms:** where (limestone AND low slope AND humid) flags coincide, subtract dolines from the heightfield: point process at 60–500/km², diameter lognormal centered ~40 m, depth ≈ diameter/4 for solution forms, steeper (≈ diameter/2, walls 60–90°) for the rare collapse forms. Align elongation with the joint field. Dolines are the surface expression of shafts — place the Step-3 shafts at doline centers, and drain them.
2. **Sinking streams and dry valleys:** route the surface hydrology; where a stream crosses onto karst, terminate it at a ponor (blind valley) and hand its discharge to the cave graph as an input edge. Leave "expected" valleys dry across karst plateaus. Resurgences: re-add the discharge as springs at valley contacts, with flood-pulse behavior (the epiphreatic concept).
3. **Sinkhole plains vs caprock plateaus:** the Mammoth configuration — a resistant cap over the limestone preserves plateau ridges with caves beneath, while adjacent uncapped terrain becomes a sinkhole plain lowering 10–100 mm/kyr. Implement as a caprock mask gating doline density and surface lowering.
4. **Uvalas/poljes:** coalescence in the doline point process — after N Myr of landscape age, merge overlapping dolines into compound depressions and, at the largest scales, flood-stage poljes with estavelle behavior.
5. **Tower karst / cockpit (tropical flag):** where "fenglin" conditions hold (stable base level, allogenic input), flatten plains aggressively while leaving fracture-bounded residual towers (edges from the joint field, 60–75° slopes, 50–300 m tall); under "fengcong" conditions (rapid uplift), steepen and deepen instead.
6. **The Chicxulub trick:** if the world has any buried ancient structure (impact crater, rift, reef front), modulate the fracture-intensity field with it — surface karst density will inherit the pattern with geological delay, a wonderful piece of world-lore made physical.

### 11.4 What to fake (blunt calls)

- **Do not** simulate the PWP kinetics per water parcel — the kinetics matter only through their *consequences* (where caves form, §1.5), which the graph+Q/L approach already encodes.
- **Do not** grow caves incrementally in simulated time — place them at their end-state from the statistics; reserve a "passage age" attribute for decoration choices (older = more breakdown, more sediment fill, more speleothems if drip-fed).
- **Do** keep a continuous water-table field rather than per-chunk decisions — the #1 artifact risk in voxel caves is discontinuity at chunk boundaries; the graph is the seam contract.
- **Do** make the audio boundary (outside/cave) a function of measured local state (enclosure from the density field, distance-to-entrance, climate tags) so reverb, wind, and drip ambience all agree — this is the payoff the climate section exists for.
- **Voxel-scale honesty check:** at 2⁻⁷ m (7.8 mm) finest voxels with ~4 m full-res radius, a 6-m trunk passage near the player renders beautifully, and the 15-m chambers hold up at one LOD step back; the 100–400 m chamber class needs the SVO's sparse-brick LOD to be affordable at all — which is exactly what it's for. Cave *volumes* are tiny relative to rock: even Mammoth's ~1–2% passage-per-rock volume means the octree stays overwhelmingly solid — carve, don't rebuild.

---

## Provenance

Written 2026-09 by the terrain-research subagent (Agent 4, caves/karst domain) as Part 4 of the merged Earth-terrain document, following the house style of `research/water-physics-and-wave-simulation.md`. Questions 40–50 (Section D) of `research/_terrain_question_bank.md` are answered in §§1–10; section numbering here is fixed for the merge. All parent numeric anchors were independently verified against sources; disagreements are reported inline and consolidated below. Web research: 9 search batches / ~28 queries via Exa against primary literature (Palmer, Ford & Ewers, Dreybrodt, Klimchouk, Plummer–Wigley–Parkhurst, Badino, Faimon), agency reports (USGS, NPS, NIOSH, FL FGS/FDOT, INERIS), and the national Slovene doline lidar census. URLs were captured from live search results; a few claim-adjacent numbers (e.g. post-1995 Kazumura resurvey) could not be pinned to a primary source and are flagged rather than asserted.

**Consolidated anchor-disagreement ledger (parent value vs source value):**

| # | Anchor | Parent | Source | Verdict |
|---|---|---|---|---|
| 1 | Epikarst-spreading denudation | 5.6–28 mm/kyr (spread over 1–5 m epikarst) | 20–100 mm/kyr measured humid; ~34 mm/kyr best-fit | **Framing error** — denudation is catchment-average lowering; depth-spreading doesn't divide it. Arithmetic and Ca verify. |
| 2 | Soil pCO₂ 10–100× atmosphere | 10–100× | 25–150× (0.1–6%) | Verified in spirit; range slightly wider. |
| 3 | Mammoth Cave length | >670 km | 685.6 km | Verified, update. |
| 4 | Veryovkina depth | ~2.2 km | 2,209 m | Verified. |
| 5 | Sarawak Chamber | 600×415×80 m | 600×435×115 m (laser 2011) | **Width and height wrong** (415→435, 80→115). |
| 6 | Son Doong | 5 km, 200 m high | ~4.5–5 km, 150–250 m high | Verified. |
| 7 | Wall retreat | 0.01–1 mm/yr | 0.01–1 mm/yr (max 0.1–1; typical 0.1) | Verified. |
| 8 | Stalagmite growth | 0.01–3 mm/yr | 0.01–1 mm/yr (bulk 0.05–0.5) | **Top end too generous** (3 → 1). |
| 9 | Kazumura length | 65 km | 59.3 km (survey paper) | **Disagreement**; popular ~65 km unsourced in survey literature. |
| 10 | Lava tube max diameter | up to ~30 m | up to ~21 m (Kazumura) | Slightly generous. |
| 11 | Sinkhole density hundreds/km² | "up to hundreds" | up to 500/km² (Slovenia) | Verified. |
| 12 | Cave temp ≈ local MAT | — | confirmed (Badino 2004; global comparative study) | Verified. |

## Sources

1. Plummer, Wigley & Parkhurst (1978), kinetics of calcite dissolution — https://doi.org/10.2475/ajs.278.2.179
2. Plummer, Parkhurst & Wigley (1979), critical review of calcite kinetics — https://doi.org/10.1021/bk-1979-0093.ch025
3. White, W. (2016), Chemistry and karst — https://doi.org/10.3986/ac.v44i3.1896
4. BRGM review of carbonate kinetic data — https://infoterre.brgm.fr/rapports/RR-39062-FR.pdf
5. Faimon et al. (2012), epikarstic pCO₂ from drip hydrochemistry — https://doi.org/10.3986/ac.v41i1.47
6. Gibraltar karst CO₂ / ground air — https://www.sciencedirect.com/science/article/abs/pii/S001670371630028X
7. Savoy watershed CO₂ and dissolution dynamics — https://www.sciencedirect.com/science/article/abs/pii/S0009254118301219
8. Lascaux CO₂ dynamics (soil–epikarst continuum) — https://www.springerprofessional.de/the-co2-dynamics-in-the-continuum-atmosphere-soil-epikarst-and-i/16644174
9. Denudation concepts and methods — https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133
10. Lauritzen (1990), Svartisen autogenic/allogenic denudation — https://doi.org/10.1002/esp.3290150206
11. Banks, UK limestone dissolution rates — https://nora.nerc.ac.uk/id/eprint/503972/1/Dissolution%20rates%20in%20limestone%20v4.pdf
12. Austrian Alps carbonate tablet field test — https://www.sciencedirect.com/science/article/abs/pii/S0169555X04003101
13. Palmer (1991), Origin and morphology of limestone caves — https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2
14. Palmer (2011), Distinction between epigenic and hypogenic maze caves — https://www.sciencedirect.com/science/article/abs/pii/S0169555X11001498
15. Hill (1990), sulfuric acid speleogenesis, Carlsbad — https://doi.org/10.1306/0c9b2565-1710-11d7-8645000102c1865d
16. Palmer, support for sulfuric acid origin, Guadalupes — https://nmgs.nmt.edu/publications/guidebooks/downloads/57/57_p0195_p0202.pdf
17. NCKRI Special Paper 2, H₂S in Guadalupe caves — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1017&context=kip_monographs
18. Klimchouk (2007), Morphogenesis of hypogenic caves — https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004108
19. Ford & Ewers (1978), four-state model — https://doi.org/10.5038/1827-806x.10.3.1
20. Ford (2014), four-state model retrospective — https://digitalcommons.usf.edu/kip_articles/6893
21. Ford (2003), perspectives in karst hydrogeology — https://www.academia.edu/35559784/Perspectives%5Fin%5Fkarst%5Fhydrogeology%5Fand%5Fcavern%5Fgenesis
22. Dreybrodt (1990), dissolution kinetics in karst aquifer development — https://doi.org/10.1086/629431
23. Dreybrodt & Gabrovšek (2003), basic processes of karst evolution — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=5791&context=kip_articles
24. Bakalowicz (1997), early karst conduit development — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/96WR01332
25. Perne, Covington & Gabrovšek (2014), pressurized-to-free-surface conduit evolution — https://doi.org/10.5194/hess-18-4617-2014
26. Palmer, dynamics of cave development by allogenic water — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1032&context=kip_articles
27. List of longest caves (Wikipedia, 2026) — https://en.wikipedia.org/wiki/List_of_longest_caves
28. Sistema Ox Bel Ha — https://en.wikipedia.org/wiki/Sistema_Ox_Bel_Ha
29. Showcaves longest-cave statistics — https://showcaves.com/english/explain/Statistics/Longest.html
30. Veryovkina Cave — https://en.wikipedia.org/wiki/Veryovkina_Cave
31. Sarawak Chamber — https://en.wikipedia.org/wiki/Sarawak_Chamber
32. Mulu Caves Project, Sarawak Chamber — https://mulucaves.org.uk/articles/sarawak-chamber
33. Caves of Malaysia, statistics (Son Doong, Deer Cave, Miao Room) — https://cavesofmalaysia.wordpress.com/cave-statistics/
34. Mulu speleogenesis & morphology study — https://www.geojournal.net/uploads/archives/8-4-227-593.pdf
35. Jordá-Bordehore (2017), stability of natural caves, 137 spans — https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf
36. Diederichs & Kaiser (1999), voussoir beam analogue — https://www.sciencedirect.com/science/article/abs/pii/S0148906298001806
37. Tharp & Holdrege, very long-term loading of cave roof beams — https://doi.org/10.1201/9781003761365-140
38. Heidong quarry long-term span study — https://doi.org/10.18814/epiiugs/2013/v36i1/006
39. NIOSH roof stability in US limestone mines — https://stacks.cdc.gov/view/cdc/226788
40. Osborne (2002), cave breakdown by vadose weathering — https://doi.org/10.5038/1827-806x.31.1.3
41. Andrejchuk & Klimchouk (2002), Kungurskaya breakdown mechanisms — https://doi.org/10.5038/1827-806x.31.1.5
42. Klimchouk & Andrejchuk (2002), Western Ukraine gypsum breakdown — https://doi.org/10.5038/1827-806x.31.1.4
43. Pisarowicz/White, gypsum wedging in Mammoth Cave — https://caves.org/wp-content/uploads/Publications/JCKS/v65/v65n1-White.pdf
44. USGS WRI 85-4126, west-central Florida sinkholes — https://pubs.usgs.gov/wri/1985/4126/report.pdf
45. Florida FGS, sinkhole favorability — https://inspectapedia.com/vision/Florida-Sinkhole-Report.pdf
46. FDOT, central Florida sinkhole evaluation — https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0
47. Pinellas County sinkhole modification thesis — https://digitalcommons.usf.edu/etd/1306
48. Marion County FL sinkhole susceptibility — https://pmc.ncbi.nlm.nih.gov/articles/PMC6509126/
49. Perry et al. (1995), Ring of Cenotes / Chicxulub — https://doi.org/10.1130/0091-7613(1995)023%3c0173:ROCSNW%3e2.3.CO;2
50. Connors et al. (1996), Yucatán karst and Chicxulub size — https://doi.org/10.1111/j.1365-246x.1996.tb04066.x
51. Surficial geology of the Chicxulub crater — https://link.springer.com/article/10.1007/BF00575099
52. Ring of Cenotes explainer (Northwestern) — https://sites.northwestern.edu/monroyrios/ring-of-cenotes/
53. EGU 2024, cenote formation above Chicxulub — https://doi.org/10.5194/egusphere-egu24-3188
54. Aguilar et al., Yucatán karst depression density — https://doi.org/10.4311/2015es0124
55. Mihevc & Mihevc (2021), Slovenian lidar doline census — https://doi.org/10.3986/ac.v50i1.9462
56. UNESCO tentative list, Classical Karst — https://whc.unesco.org/fr/listesindicatives/6072/
57. Day & Tang (2000), Guilin tower-karst slopes — https://doi.org/10.1002/1096-9837(200010)25:11%3C1221::AID-ESP133%3E3.0.CO;2-D
58. Waltham, karst lands of southern China (fengcong/fenglin) — https://www.researchgate.net/publication/263374259_The_karst_lands_of_southern_China
59. Guangxi fenglin/fengcong chapter — https://link.springer.com/chapter/10.1007/978-90-481-3055-9_30
60. Lijiang plain tower karst structure — https://www.schweizerbart.de/papers/zfg/detail/36/98327/Structural_and_hydrogeological_origin_of_tower_karst_in_southern_China_Lijiang_plain_in_the_Guilin_region
61. Klimchouk (2004), epikarst origin and classification — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=6334&context=kip_articles
62. Williams (2008), epikarst review — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1173&context=ijs
63. Jones (2013), physical structure of epikarst — https://doi.org/10.3986/ac.v42i2-3.672
64. Greeley, lava tube formation (USGS PP1350 ch. 59) — https://pubs.usgs.gov/pp/1987/1350/pdf/chapters/pp1350_ch59.pdf
65. Valerio, Tallarico & Dragoni (2008), lava tube formation mechanisms — https://doi.org/10.1029/2007jb005435
66. Dragoni, Piombo & Tallarico (1995), tube roofing model — https://doi.org/10.1029/94jb03263
67. Allred & Allred, Kazumura Cave development and morphology — https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf
68. Badino (2004), cave temperatures and global climatic change — https://doi.org/10.5038/1827-806x.33.1.10
69. Badino (2006), underground drainage and geothermal flux — https://doi.org/10.3986/ac.v34i2.261
70. James, Banner & Hardt (2015), global cave ventilation model — https://doi.org/10.1002/2014gc005658
71. Pflitsch, Wiles & Horrocks (2010), barometric caves (Jewel/Wind) — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=2599&context=kip_articles
72. Gomell et al. (2021), air pressure propagation in barometric caves — https://doi.org/10.5038/1827-806x.50.3.2393
73. Gomell & Pflitsch (2022), airflow dynamics in Wind and Jewel Cave — https://doi.org/10.5038/1827-806x.51.3.2437
74. French caves, APV-induced temperature variations — https://www.sciencedirect.com/science/article/pii/S2772883822001200
75. Global comparative cave temperature study — https://pmc.ncbi.nlm.nih.gov/articles/PMC10676404/
76. Dreybrodt & Romanov, regular stalagmites — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1049&context=kip_articles
77. PNAS, shapes of ideal stalagmites (Damköhler classification) — https://pmc.ncbi.nlm.nih.gov/articles/PMC12557760/
78. Sofular Cave stalagmite shape modeling — https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2022.969211/full
79. Baker et al., intra/inter-annual stalagmite growth — https://www.sciencedirect.com/science/article/abs/pii/S0009254100003995
80. StalGrowth program — https://www.mdpi.com/2076-3263/11/5/187
81. NPS, Mammoth Cave karst summary — https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf
82. NPS, exploring the world's longest known cave — https://www.nps.gov/articles/000/exploring-the-worlds-longest-known-cave.htm
83. AIPG field guide, Mammoth Cave geology — https://www.uky.edu/KGS/geoky/fieldtrip/2005%20AIPG%20Guidebooks/MammothCave.pdf
84. Palmer, geology of Mammoth Cave — https://digitalcommons.usf.edu/kip_articles/8226/
85. Gypsum reactive-transport karstification (Energies) — https://mdpi-res.com/d_attachment/energies/energies-15-00761/article_deploy/energies-15-00761-v2.pdf?version=1642738727
86. Stafford et al. (2008), Castile Formation gypsum karst — https://doi.org/10.5038/1827-806x.37.2.1
87. INERIS gypsum dissolution guide — https://www.ineris.fr/sites/default/files/contribution/Documents/Ineris-Guide_Gypse_VA-20_11Nov-WEB.pdf
88. Li & Einstein (2017), gypsum cavity evolution — https://doi.org/10.1002/2017wr021776

---

## Appendix A — Forty further questions

My own questions, beyond the parent's 40–50, each answered or explicitly dispositioned.

1. **How fast does an epikarst develop on fresh rock?** Decades–centuries for the fissure network (stress-release driven), 10³–10⁴ yr for full hydrologic function — slower under soil (cushioned), faster with biotic CO₂. Dispositioned: quantitative rates per lithology not pinned to a single source; see [61].
2. **What is the diameter distribution of epikarst storage voids?** Millimeter–decimeter fissures, a few % bulk porosity vs <0.5% in the bulk rock. Dispositioned from [61]/[62] qualitative ranges.
3. **Does dissolution deepen or flatten karst drainage divides?** Both: divides lower by diffuse denudation while conduits incise below — net effect is drainage concentration and divide migration toward the weaker aquifer. Dispositioned: not sourced quantitatively here; flagged for Part B cross-reference.
4. **What controls spring density (springs per km of strike)?** Lithologic boundaries and stratal dips; contact springs line the karst/non-karst boundary. Dispositioned qualitatively via [56].
5. **How much of a karst aquifer's flow is conduit vs diffuse at maturity?** >50% of recharge reaches the vadose zone already concentrated ([61], citing Kiraly); mature aquifers transmit the majority of storm discharge through conduits. Answered.
6. **What is the depth of the "no-cave" floor — below which karstification shuts off?** Set by base level and the 4th-order kinetics: practically, caves stop where flow paths are long and closed-system; no universal depth. Answered via §1.5/§3.3.
7. **How do scallop sizes distribute along a real passage?** Lognormal around a flow-velocity-set mean; coarser near inlets. Dispositioned: scallop–velocity calibration (Curl's equation) not independently sourced here; texture hint only.
8. **What is the temperature of hypogenic vs epigenic cave air?** Epigenic ≈ MAT; hypogenic can be warm (thermal input) — e.g. movile-type and hydrothermal caves exceed MAT. Answered via [69] framework.
9. **How deep can siphons go and still be dived?** The deepest cave siphons push past 100 m in the Caucasus systems; Veryovkina's terminal siphon is 26 m at −2,209 m. Answered from [30].
10. **What is the world's deepest single shaft?** ~450 m-class (e.g. some Mexican pits); Veryovkina's Babatunda (155 m) is cited for that cave. Dispositioned: global superlative not pinned to a primary source in this research.
11. **How does seawater mixing-zone speleogenesis (halocline) differ?** Dissolution at the fresh/salt interface by mixing corrosion; produces flank-margin caves — horizontally extensive, low-density mazes in coastal platforms (Bahamas model per Mylroie & Carew, cited in [16]). Answered.
12. **Can caves form below the sea?** Yes — blue holes and drowned flank-margin caves; the Yucatán systems' deepest point is 57.3 m. Answered from [28].
13. **What is the CO₂ production rate of a tropical soil?** 6–25 g CO₂/m²/day measured at Lascaux's temperate site; tropical higher. Answered from [8].
14. **How do drip intervals distribute through a cave?** Broadly lognormal, site means from seconds to hours; the shape-defining range is 5–1000 s. Dispositioned from the modeling literature [76]/[78].
15. **What is the survival time of a straw stalactite before clogging?** Variable; straws that keep feeding grow for 10³–10⁴ yr. Dispositioned; no hard number sourced.
16. **Do caves have weather?** Yes — barometric pressure fronts, fog formation at entrance zones, and "cave clouds" in big chambers. Answered via [71]–[74].
17. **What is the fastest recorded passage growth?** Floodwater-cave fissures reaching traversable size in 10 kyr ([26]); gypsum cavities in decades ([85]). Answered.
18. **How often do catastrophic chamber collapses happen?** Kungurskaya: 64 documented failure events in 23 years — small ones are continuous; chamber-scale ones are millennial. Answered from [41].
19. **What is the largest known breakdown pile?** Chamber-filling piles of 10⁵–10⁶ m³ in the Ukrainian gypsum mazes (Zoloushka-scale). Dispositioned from [42] qualitative.
20. **How do denudation rates vary with relief, holding climate fixed?** Higher relief → deeper flow paths → more closed-system → *lower* specific rates; but allogenic inputs raise local rates. Partially answered via [9]'s e-folding model; flagged for Part B synthesis.
21. **What is the aspect ratio of dolines globally?** Depth ≈ diameter/4 to /10 for solution forms; steeper for collapse. Answered from [55] statistics (mean 9 m deep / 42 m diameter ≈ /4.7).
22. **How do uvalas form — coalescence or single mega-dissolution?** Mostly coalescence of dolines with joint control; the Yucatán census separates them statistically ([54]). Answered.
23. **What is the flood periodicity of a polje?** Seasonal (Cerknica floods months each year) to multi-annual. Answered from [56].
24. **How big can an estavelle be?** Meter-scale openings up to canyon-scale overflow conduits. Dispositioned qualitatively.
25. **Does karst drainage obey Horton's laws?** Yes — conduit networks are drainage networks; bifurcation ratios near surface-river values. Dispositioned; cross-ref Part B rather than independently sourced.
26. **What is the matrix porosity of a telogenetic vs eogenetic carbonate?** Telogenetic (uplifted, cemented): <2%, up to 0.005–0.5% in the bulk mass; eogenetic (young): 20–40%, caves rare at enterable scale (Ford State 5). Answered from [61]/[62].
27. **How quickly does a lava tube cool after drain-out?** Years–decades for a 10-m-class tube to reach ambient; Kazumura now tracks local MAT gradient 15–22 °C. Dispositioned from [67] temperature data; explicit cooling model not sourced.
28. **Can lava tubes re-activate?** Yes — reoccupation by later flows is common and produces multi-level stacking with cross-cutting. Answered from [67].
29. **What is the maximum slope at which lava tubes form?** Model says <~6°; Kazumura mean 1.9°. Answered from [65]/[67].
30. **How many entrances does a long lava tube have?** Kazumura: 82, mostly roof collapses. Answered from [67].
31. **What lives in caves (for ecology-driven generation)?** Troglobites, troglophiles, bats; guano deposits drive entire food webs. Dispositioned: out of scope for terrain; flagged for the ecology part.
32. **How do cave winds sound at different passage scales?** Barometric entrance winds reach several m/s (audible, weather-paced); interior flows are cm/s (silent). Answered from [71]–[73].
33. **What is the pH range of cave drip water?** ~6.4–8.2 measured at Mulu. Answered from [34].
34. **How does agriculture/land-use change karstification rates?** Enhanced soil CO₂ and focused recharge can accelerate dissolution and conduit development irrespective of seasonality ([epikarst land-use study](https://doi.org/10.1002/esp.4768)). Answered.
35. **What is the characteristic spacing of inception horizons?** Bedding-controlled, meters–tens of meters in a thick limestone sequence. Dispositioned from [83]'s stratigraphic treatment.
36. **How much sediment do caves store?** Entire valley-fill sequences — Mammoth's paleo-entrances and passages store quartz-pebble caches datable by cosmogenics. Answered from [81]/[83].
37. **What is the relict survival rate of cave levels vs surface destruction?** Caprock preservation is the dominant control — uncapped karst destroys upper levels as fast as lower ones form. Answered from [82].
38. **How do you detect karst from orbit (validation target for generated worlds)?** Dolines on DEMs, absence of surface drainage, thermal springs, collapse density anomalies — the Yucatán and Slovenia censuses both used remote sensing at scale. Answered from [54]/[55].
39. **What is the geometry of the water table inside a karst massif?** A gently sloping surface with steep draws toward conduits; gradients orders of magnitude below the surface topography. Answered from [19]/[21].
40. **What is the single best one-number summary of cave "believability" for validation?** Passage-length per unit area (km/km² of karst), checked against 1–10 for typical systems and ~30 in maze patches — everything else (chambers, levels, shapes) hangs off the same generation graph. My call; grounded in §4.1/§4.4 statistics.


# Part 5 - Deserts, Wind and Climate Geography

**Scope:** the physics of wind-blown sand, dune-type selection as an implementable rule, dune and sand-sea dynamics, aeolian erosion, the climate machinery that places deserts on the globe, the dust–loess chain beyond desert margins, desert surface chemistry and soils, water in deserts, and a practical synthesis for a voxel engine that wants real deserts rather than grassland worlds with a sand texture. Every formula is either derived or attributed. Measured quantities are ranges with sources. Numeric anchors distributed by the parent were independently recomputed or re-sourced; disagreements are reported in §11, not silently absorbed.

**How to read this:** §1 is the transport physics everything else builds on. §2–§3 are the core dune deliverables (the dune-type selection rule is boxed in §2.4). §6 is the climate-geography rule set for biome placement. §10 is the engineering synthesis — read it last.

---

## 1. The physics of wind-blown sand

### 1.1 The three transport modes

Wind moves sand in three modes, set by grain size relative to the wind's ability to loft and sustain it:

- **Creep/reptation** (grains ≳ 0.5–2 mm): too heavy to launch; rolled or nudged along the bed by grain impacts. A minority of total mass flux (Bagnold estimated ~25% moved by creep for typical sand) but the mode that builds impact ripples.
- **Saltation** (≈ 0.1–0.5 mm; dune sand is almost always 0.15–0.30 mm): grains launch ballistically in hops of decimetres, accelerate in the wind, and strike the bed at ~10–13° with enough energy to splash out further grains. This is *the* dune-building mode — 75%+ of the total mass flux typically travels in the lower ~0.5 m.
- **Suspension* (≲ 0.06–0.07 mm): once airborne, settling velocities are low enough that turbulence keeps fines aloft for days; this is dust, not sand. Dust is largely emitted *indirectly*, by saltation impacts fragmentating soil aggregates or bombarding clay crusts — sand and dust are mechanically coupled systems ([Kok et al. 2014](https://acp.copernicus.org/articles/14/13023/2014/acp-14-13023-2014.pdf)).

The saltation mode is autocatalytic: one launched grain can eject several, so the population grows exponentially until the wind has lost enough momentum to the sand cloud that the bed shear stress falls to a new equilibrium. This feedback is why thresholds, not mean winds, control everything.

### 1.2 Shear velocity u* and roughness

The wind's sediment-moving capacity is not its speed at some height but its momentum flux to the bed, expressed as the shear velocity:

$$u_* = \sqrt{\tau/\rho_{air}}, \qquad \tau = \rho_{air} u_*^2$$

The log wind profile over a rough surface, $u(z) = (u_*/\kappa)\ln(z/z_0)$, ties $u_*$ to measurable winds and exposes the controlling role of roughness length $z_0$: a stony or vegetated surface dumps momentum on obstacles rather than on grains, raising the threshold for the same free-stream wind. During active saltation the moving sand itself is the roughness — the profiles for different $u_*$ converge near a "focal point" a few cm up (Ungar & Haff's result, used by [Andreotti 2010](https://www.phys.ens.psl.eu/~foldingslidingstretchinglab/papers/A47_LSatSaltation.pdf)), which is why the near-bed wind becomes nearly independent of $u_*$ while the flux keeps rising.

### 1.3 The two thresholds, and why saltation is hysteretic

- **Fluid (static) threshold** $u_{*ft}$: the shear velocity at which aerodynamic lift first plucks a grain from rest. For loose 0.25 mm quartz sand this is $u_{*ft} \approx 0.20$–0.25 m/s (the minimal standardized threshold for optimally erodible 100 µm sand is $u_{*st0} \approx 0.16$ m/s, [Kok et al. 2014](https://acp.copernicus.org/articles/14/13023/2014/acp-14-13023-2014.pdf)).
- **Impact (dynamic) threshold** $u_{*it}$: the *lower* shear velocity at which already-saltating sand can sustain itself, because impacting grains entrain bed grains more efficiently than fluid drag does.

For loose sand on Earth the ratio is $u_{*it}/u_{*ft} \approx 0.82$ (Bagnold 1937, wind tunnels; Kok's analytical model). **Anchor verified:** the first field-based separation ([Martin & Kok 2018](https://doi.org/10.1029/2017jf004416)) measured 0.813 ± 0.018, 0.863 ± 0.027 and 0.837 ± 0.007 at three field sites — and, importantly for simulation, found that *time-averaged* (~30 min) sand flux is governed by the impact threshold alone, the fluid threshold only mattering for the instantaneous onset of rare events. On Mars the ratio is ~0.1 ([Kok 2010](https://export.arxiv.org/pdf/1001.4840v1.pdf)), which is why Martian dunes can be active at winds that would never initiate transport.

Consequences worth simulating: hysteresis (transport stops at a lower wind than it starts at), and memory — once a gust starts saltation, ordinary winds keep it going.

### 1.4 The mass-flux law — Bagnold and its modern revisions

Bagnold's 1941 dimensional argument: the saturated (equilibrium) transport rate per unit width should scale as (density of the transporting medium) × (a characteristic speed)³ / g, because the flux is (mass per area) × (speed) and the sand cloud's inertia scales with the wind's:

$$\boxed{\; q \;=\; C\,\frac{\rho_{air}}{g}\,\sqrt{\frac{d}{D}}\; u_*^3 \;}$$

with $D$ = 0.25 mm reference grain size and $C$ = 1.5 for uniform sand, 1.8 for naturally graded sand, 2.8 for poorly sorted sand ([Bagnold formula summary](https://en.wikipedia.org/wiki/Bagnold_formula); [van Rijn & Strypsteen 2019](https://doi.org/10.1016/j.coastaleng.2019.103600)).

**Parent anchor verified by recomputation.** With $C = 1.5$, $\rho_{air} = 1.2$ kg/m³, $d = D = 0.25$ mm:

$$q = 0.1835\, u_*^3 \ \text{kg m}^{-1}\text{s}^{-1}$$

| $u_*$ (m/s) | q (kg/(m·s)) | q (t/(m·h)) | parent value |
|---|---|---|---|
| 0.3 | 0.0050 | **0.018** | 0.02 ✓ |
| 0.5 | 0.0229 | **0.082** | 0.08 ✓ |
| 0.8 | 0.0938 | **0.338** | 0.34 ✓ |

The parent's table is arithmetically correct. Three caveats the raw cubic hides:

1. **The threshold subtraction matters near threshold.** Bagnold's own 1954 revision used $(u_* - u_{*t})^3$, which [van Rijn & Strypsteen (2019)](https://doi.org/10.1016/j.coastaleng.2019.103600) show *under*predicts badly; the best-performing modern form is $u_*^3 - u_{*t}^3$ (their fits give exponent 0.95–1.1 on that form). With $u_{*it} = 0.82\,u_{*ft} \approx 0.21$ m/s, the correction removes ~31% of the flux at $u_* = 0.3$ m/s but only ~2% at 0.8 m/s. Below ~1.5× threshold, use the subtracted form; above, the plain cubic is fine.
2. **The scaling itself is contested for moderate winds.** [Valance (2015)](https://comptes-rendus.academie-sciences.fr/physique/item/10.1016/j.crhy.2015.01.006.pdf) reviews the modern consensus from wind tunnels and DEM: hop length and particle velocity are nearly *independent* of wind strength (the near-bed wind saturates), so $q \propto u_*^2 - u_{*t}^2$ (quadratic, Creyssels et al. 2009 line of work), not cubic. The cubic is recovered only for $u_* \gtrsim 5u_{*t}$ (very strong winds) or transport over rigid (non-eroding) beds. Blunt call: for game wind regimes, which sit at 1.2–2× threshold, the quadratic form is more defensible — but the coefficient spread between published laws (a factor ~2 either way, see [Pähtz et al. 2021](https://doi.org/10.1029/2020jf005859), whose unified model captures threshold and rate within ~×2 across air, water and snow) exceeds the scaling difference anyway. Pick cubic with $C=1.5$, document it, move on.
3. **The $d^{0.5}$ term is real but second-order.** Bagnold's $\sqrt{d/D}$ (and van Rijn's $(d/d_{ref})^{0.5}$) encodes coarser sand hopping farther per unit wind; it predicts ~40% less flux for 0.15 mm Saharan sand and ~40% more for 0.44 mm, consistent with Belly's wind-tunnel data being under/over-predicted by ~×1.5 at the ends ([van Rijn & Strypsteen 2019](https://doi.org/10.1016/j.coastaleng.2019.103600)).

A practical saturation detail: the flux does not appear instantly — it grows over the **saturation length** $\ell_s \sim 1$–2 m (measured directly, [Andreotti 2010](https://www.phys.ens.psl.eu/~foldingslidingstretchinglab/papers/A47_LSatSaltation.pdf)), which turns out to be the single most important length scale in all of dune physics (§3.3).

---

## 2. Dune morphology as a phase diagram

### 2.1 The controlling variables

Three axes span the observed dune-type space:

1. **Wind-direction variability**, classically measured by the *resultant drift potential* ratio: RDP/DP = |Σqᵢ| / Σ|qᵢ| over the annual population of transport vectors. RDP/DP → 1 is unimodal (trade winds); → 0 is fully multidirectional ([Fryberger & Dean 1979](https://doi.org/10.2110/jsr.2004-041), as summarized in [Gao et al. 2015](https://www.nature.com/articles/srep14677)).
2. **Sand availability**: a continuous erodible sand bed vs a hard ground with localized sources. This is the axis the classical scheme under-weighted; it turns out to select between two entirely different growth mechanisms (§2.4).
3. **Vegetation cover**, which pins horns and inverts dune curvature (parabolics, §2.6).

The classical field taxonomy: **barchan** (crescent, unidirectional wind + limited supply, migrating over firm ground); **transverse** (unidirectional + abundant supply, laterally connected ridges); **linear/seif** (sinuous sharp-crested ridges, bimodal oblique winds); **star** (pyramidal, arms radiating from a central peak, multidirectional winds); **parabolic** (U-shape, nose downwind, vegetated arms trailing upwind); **dome** (rounded, slip-face-less); **reversing** (transverse forms whose slip face flips seasonally, transitional to stars).

### 2.2 Measured sizes and speeds

- **Barchans**, Atlantic Sahara (Tarfaya–Laâyoune): widths 20–600 m across the field, heights from 1 m to ~10 m; migration **25–100 m/yr** inversely with size — a 1 m dune does >100 m/yr, a 7 m dune ~25 m/yr, mega-barchans ~2 m/yr ([Elbelrhiti 2011](https://hal.science/hal-01950849/document); [Elbelrhiti et al. 2008](https://doi.org/10.1029/2007jf000767)). Landsat-based measurement across the same coast gives 15–90 m/yr by dune size ([Aydda & Algouti 2014](https://doi.org/10.14195/978-989-96253-3-4_17)); ~32 m/yr average for ~9 m dunes near Laâyoune ([ISPRS 2016 study](https://isprs-archives.copernicus.org/articles/XLII-2-W1/53/2016/isprs-archives-XLII-2-W1-53-2016.pdf)). Inland (Jorf/Tafilalet) rates are lower, 9–24 m/yr ([ecoeet.com study](https://www.ecoeet.com/pdf-213427-131849?filename=Evaluation-of-the-mobilit.pdf)).
- **Star dunes**, global synthesis: heights 4–291 m (mean 75 m), widths 125–3071 m (mean 895 m), densities 7–81 per 100 km² ([Lancaster et al. 2021... Goudie global analysis](https://www.sciencedirect.com/science/article/abs/pii/S1875963721000227)). The Gran Desierto (Sonora) star dunes commonly exceed 100 m; the world's largest star-dune province is the southern Grand Erg Oriental (~66,000 km² of stars, individual dunes to 230 m high and 2.4 km across — [Grand Erg Oriental](https://en.wikipedia.org/wiki/Grand_Erg_Oriental)). Star dunes grow vertically and migrate negligibly.
- **Linear/seif dunes**: Sinai seif elongates >1 m/month with crest peaks advancing 0.7 m/month ([Tsoar 1983](https://doi.org/10.1111/j.1365-3091.1983.tb00694.x)); Namib linear dunes are nearly static bodies tens of km long. Grand Erg Oriental ghourd (star-chain) dunes reach 160–260 m relative height, spaced 1.3–5 km ([Persée French analysis](https://www.persee.fr/doc/tigr_0048-7163_1984_num_59_1_1151)).
- **Parabolic dunes**: Saskatchewan 3.3–3.5 m/yr over 60 yr; Oregon 2.0–2.8 m/yr; North Queensland 5.6–6.4 m/yr; Colorado up to 30 m/yr in drought years (~6× wet-year rates) — all compiled in [Goudie 2016](https://doi.org/10.56093/aaz.v50i3-4.63760).

### 2.3 Werner's cellular model — the proof that dune types are attractors

Werner (1995) showed dune fields emerge from almost comically simple rules ([paper PDF](https://sseh.uchicago.edu/doc/Werner_1995.pdf); [accessible reimplementation notes](https://smallpond.ca/jim/sand/dunefieldMorphology/index.html)):

- Sand moves as slabs: pick a cell at random, move a slab a fixed hop length downwind, deposit with probability $p_{ns}$ if the landing cell is bare, $p_s > p_{ns}$ if sand-covered, 1.0 in lee shadow zones.
- Enforce a 30–34° angle of repose by toppling.
- Vary only wind direction distribution and sand supply: barchans → transverse ridges as supply rises; linear dunes under reversing winds with a mean drift; star forms when the transport direction rotates around the compass.

The key interpretation: dune *classes are attractors* — the field organization is decoupled from transport detail. This is the license every game dune generator since has relied on: you do not need CFD, you need the right rules at the slab level. The known defects (fields eventually coarsen to one transverse ridge; symmetric profiles unless upwind avalanching is blocked) were fixed by Momiji's wind speed-up term and Baas's vegetation layer ([model genealogy](https://smallpond.ca/jim/sand/dunefieldMorphology/index.html)).

### 2.4 The implementable rule: Rubin–Hunter orientation + the two growth modes

**The Gross Bedform-Normal Transport rule (GBNR).** [Rubin & Hunter (1987)](https://doi.org/10.1126/science.237.4812.276) rotated a sand board in steady bidirectional winds and found that bedforms — ripples through dunes — adopt the trend that **maximizes the total transport *across* the crest**, regardless of whether that trend is transverse, oblique or longitudinal relative to the *resultant* transport direction. [Rubin & Ikeda (1990)](https://doi.org/10.1111/j.1365-3091.1990.tb00628.x) confirmed it in a flume turntable: divergence angles 45–90° produced transverse bedforms, 112–135° longitudinal, with the crossover at ~90–100°. In vector form, for annual transport vectors $\{\vec{q}_i\}$, the bedform trend $\hat{t}$ maximizes

$$\boxed{\; \hat{t}_{GBNR} \;=\; \arg\max_{\hat{t}} \sum_i \big|\, \vec{q}_i \times \hat{t} \,\big| \;}$$

**The 2014 correction — two modes, not one.** [Gao et al. (2015)](https://www.nature.com/articles/srep14677) (numerics confirming [Courrech du Pont et al. 2014](https://doi.org/10.1029/2014JF003220)) showed a single wind regime produces **two** possible dune orientations depending on sand availability:

- **Bed-instability mode** (erodible sand everywhere): dunes grow in height and migrate; orientation = GBNR (maximize normal-to-crest transport). This yields transverse, oblique, or longitudinal *linear* dunes from the *same* wind regime.
- **Fingering mode** (hard ground, localized source): a dune extends from the source **along the resultant sand flux at the crest** — the orientation where the normal-to-crest components *cancel*. This yields "finger" linear dunes (seifs) and barchans; fingers break into barchan trains when the regime becomes too asymmetric (divergence → 0° or 180°, or transport ratio > ~5).

The morphological selection between barchan and finger on low-availability ground is captured by the growth-rate ratio $\sigma_F/\sigma_I$ ([Gao et al. 2015](https://www.nature.com/articles/srep14677)). For a generator this is glorious: the same wind rose, evaluated twice (max vs cancel of normal transport), gives you both possible crest trends, and sand-supply maps decide which one you draw.

**Star dunes** are what the GBNR cannot resolve — with three or more comparable transport directions there is no dominant alignment; sand converges on central nodes and the dune grows vertically. [Lancaster (1989)](https://doi.org/10.1111/j.1365-3091.1989.tb00607.x) documented the Gran Desierto sequence: crescentic dunes migrating into opposed winds first grow a reversing crestal ridge, then convergent lee-side secondary flows build linear arms parallel to each major wind, concentrating sand at the centre. Star-dune provinces correlate with topographic barriers (which complicate wind fields and stall sand transport — [global analysis](https://www.sciencedirect.com/science/article/abs/pii/S1875963721000227)).

### 2.5 Linear dunes: the two formation hypotheses (verified)

1. **Extension/elongation** (Bagnold → modern fingering mode): barchans in bidirectional oblique winds shed and extend a horn downwind of the resultant; [Lancaster (1980)](https://doi.org/10.1127/zfg/24/1984/160) found Namib field evidence for barchan→seif conversion; the modern statement is the fingering mode above.
2. **Helicoidal (roll-vortex) flow**: longitudinal roll vortices aligned with the mean wind concentrate deposition on parallel ridges ([Tseo 1993](https://doi.org/10.1002/esp.3290180706) reviews the evidence for evenly spaced fields).

Tsoar's field measurements on a Sinai seif ([1983](https://doi.org/10.1111/j.1365-3091.1983.tb00694.x)) decided it empirically for sinuous seifs: the helicoidal theory's predicted wind structure was *contradicted* — instead, winds striking the crest obliquely are **deflected along the lee flank**, and the incidence angle controls everything: < 40° between wind and crest → deflected flow accelerates and transports sand *along* the dune (elongation); > 40° → deceleration and deposition on the lee flank. The 40° rule is a genuinely implementable micro-rule if you ever simulate individual large dunes.

### 2.6 Parabolic dunes and the vegetation axis

Parabolics are barchans with the curvature inverted: vegetation colonizes the crest (the only no-erosion/no-deposition zone), the windward slope flips from convex to concave, the arms anchor while the nose advances. Documented live on Israel's coast: as vegetation cover rose 4.3% → 17% (1950s–1990s), advance rates fell 3.4 → 1.9 m/yr ([Tsoar & Blumberg 2002](https://doi.org/10.1002/esp.417)). The full barchan→parabolic transition, including the abrupt crest-curvature reversal at a threshold vegetation intensity, is now reproduced in cellular models ([Alkalla et al. 2025](https://doi.org/10.1029/2024jf008220)) — vegetation above a critical intensity fully stabilizes the dune at finite size. Re-activation runs the film backwards: drought, groundwater drawdown, or fire converts parabolics back to barchans ([Goudie 2016](https://doi.org/10.56093/aaz.v50i3-4.63760)).

---

## 3. Dune dynamics

### 3.1 Migration rates — parent anchor verified with corrections

Conservation of mass: a dune of height H advancing at speed c must be fed the sand flux q that crosses its crest and is deposited on the slip face, so

$$\boxed{\; c \;=\; \frac{q}{\rho_{bulk}\,H} \;}, \qquad \rho_{bulk} \approx 1550\text{–}1700\ \text{kg/m}^3$$

**Parent anchor recomputed** (ρ = 1650, q from §1.4 with C=1.5, d=0.25 mm):

| $u_*$ | q (kg/(m·s)) | c at H=5 m | c at H=20 m | parent |
|---|---|---|---|---|
| 0.4 | 0.0117 | **44.9 m/yr** | **11.2 m/yr** | 11–45 m/yr ✓ |
| 0.6 | 0.0396 | **151.5 m/yr** | **37.9 m/yr** | 38–152 m/yr ✓ |

Arithmetically correct. **Now the honest comparison with Morocco** (the parent expected 5–70 m/yr):

- Measured: 1 m barchans ~100 m/yr; 7 m barchans ~25 m/yr ([Elbelrhiti 2011](https://hal.science/hal-01950849/document)). The formula at u* = 0.4 gives 45 m/yr for H = 5 m — but the H = 1 m prediction (~220 m/yr) is ~2× the measurement, and the H = 7 m prediction (~32 m/yr) is close to the 25 m/yr measured.
- **Why it over-predicts small dunes and drifts on large ones:**
  1. **Time integration of u*³.** The formula assumes saturated flux year-round. Real trade winds fluctuate; the annual flux is $\langle u_*^3\rangle$ minus threshold effects, not $(\langle u_*\rangle)^3$ — and Morocco's barchans do most of their migration April–September only ([Elbelrhiti 2011](https://hal.science/hal-01950849/document)). Martin & Kok's finding that the *impact* threshold governs time-averaged flux compounds this: near-threshold intermittency cuts the effective flux substantially.
  2. **Sand-trapping efficiency / brink dynamics.** Not all saltating sand crossing the crest is trapped on the slip face; the brink-to-slip-face geometry and crest speed-up let a fraction blow over, and horns continuously leak sand to the passing flux (barchans lose sand from horns proportional to horn width while gaining from their width-proportional capture area — this imbalance is why solitary barchans have *no* stable size, [Hersen et al. 2004](https://journals.aps.org/pre/abstract/10.1103/PhysRevE.69.011304)).
  3. **The c ∝ 1/H scaling itself is only approximately observed** — measured c(W) laws flatten for large dunes because big dunes gather proportionally more sand (their windward footprint is bigger), and mega-barchans crawl at ~2 m/yr rather than the ~11 m/yr the H=20 m line suggests at u* = 0.4.

**Engineering summary:** use $c = q_{eff}/(\rho H)$ with $q_{eff} = f_{trap} \cdot \langle q \rangle_{annual}$, $f_{trap} \approx 0.5$–1.0, and expect factor-2 agreement with nature at best — which is the actual state of the published art ([Pähtz et al. 2021](https://doi.org/10.1029/2020jf005859) capture the field within ×2; so will you).

### 3.2 Barchan-field self-organization

Barchan fields are not populations of independent dunes. Because c ∝ 1/H, small dunes catch large ones; collisions either merge (small into large) or calve elementary-size barchans off the horns of large ones. Field measurement over 3 years in Morocco showed large dunes are destabilized by collisions and wind-direction changes into propagating "surface waves" that break at the horns and spawn new elementary dunes — the sand-loss mechanism that prevents dune fields from merging into one giant dune, and the actual size-selection process ([Elbelrhiti et al. 2005, Nature](https://pubmed.ncbi.nlm.nih.gov/16193049/); [Hersen & Douady 2005](https://doi.org/10.1029/2005gl024179); [corridor analysis](https://doi.org/10.1029/2007jf000767)). The result is the striking **corridor** pattern: 300 km-long lanes of barchans with sharp transverse transitions in dune size and packing density, ~10,000 yr transit time for a mid-size dune.

### 3.3 Wavelength selection and the minimum dune size

The flat-sand-bed dune instability comes from the coupling of bed topography → shear stress (destabilizing; the stress maximum sits slightly upwind of the crest) versus two stabilizers: the finite distance the sand flux lags the wind (the saturation length) and slope-dependence of the threshold. Linear stability analysis with these terms selects a most-unstable wavelength

$$\lambda_m \approx 12\, \ell_s, \qquad \ell_s \approx 4.4\, \frac{\rho_s}{\rho_{air}} d \ \text{(drag-length scaling)}$$

**Verified numbers** ([Elbelrhiti et al. 2008](https://doi.org/10.1029/2007jf000767); [Andreotti 2010](https://www.phys.ens.psl.eu/~foldingslidingstretchinglab/papers/A47_LSatSaltation.pdf)): Morocco, d = 175 µm → $\ell_s \approx 1.7$ m measured directly → $\lambda_m \approx 20$ m, and the cutoff wavelength below which bedforms decay is $\lambda_c \approx 6.3\,\ell_s \approx 11$ m. Check the scaling: $4.4 × (2650/1.2) × 1.75×10^{-4}$ m = 1.70 m. ✓ This is **why minimum dune size exists**: dunes smaller than ~10–20 m on Earth cannot hold a slip face (proto-dunes with vanishing slip faces are observed at exactly this scale), and the same physics scales to Mars where $\rho_{air}$ is ~1% of Earth's, giving $\lambda_m$ ~600 m — matching the observed minimum Martian dune size. On Earth the minimum-dune scale grows sharply near threshold: proto-dunes up to 220 m in the Rub' al Khali where characteristic winds are only ~1.2 u_th, versus 35 m wavelength patterns in the Atlantic Sahara at ~1.4 u_th ([Andreotti 2010](https://www.phys.ens.psl.eu/~foldingslidingstretchinglab/papers/A47_LSatSaltation.pdf)).

### 3.4 Hierarchy: ripples ⊂ dunes ⊂ draa

Real dune fields are fractal in a specific, ordered way:

- **Ripples** (cm–dm) on every stable sand surface (§10.3).
- **Dunes** (10 m–several 100 m wavelength, set by $\lambda_m$ and subsequent coarsening).
- **Draa / megadunes** (wavelengths of 500 m–several km; heights 50–300 m in the Rub' al Khali — [Al-Masrahy & Mountney](https://eprints.whiterose.ac.uk/id/eprint/80266/1/Remote%20sensing%20of%20spatial%20variability%20in%20aeolian%20dune%20and%20interdune%20morphology.pdf)) — with the smaller dunes riding on them as **superimposed bedforms**, aligned to whichever wind component the draa's flank amplifies. The Rubin & Hunter rule applies *recursively*: each scale's superimposed pattern reflects the wind as modified by the larger form.

Mature dune fields coarsen (dunes merge, wavelengths grow) over time; the pattern age is readable in defect density and crest sinuosity ([Gran Desierto backstripping](https://onlinelibrary.wiley.com/doi/10.1111/j.1365-3091.2006.00814.x)).

---

## 4. Sand seas (ergs)

### 4.1 The big ones — verified

- **Rub' al Khali** (Empty Quarter): **650,000–660,000 km²**, the largest continuous sand desert; active dune/interdune cover 522,340 km²; dunes (draa) 50–300 m high; 1,000 × 500 km, elevations 800 m (SW) to near sea level (NE) ([Wikipedia summary](https://en.wikipedia.org/wiki/Rub_al-Khali); [Al-Masrahy & Mountney 2013](https://eprints.whiterose.ac.uk/id/eprint/80266/1/Remote%20sensing%20of%20spatial%20variability%20in%20aeolian%20dune%20and%20interdune%20morphology.pdf); [NASA Earth Observatory](https://science.nasa.gov/earth/earth-observatory/ar-rub-al-khali-sand-sea-arabian-peninsula-50744/)). **Parent's ~650,000 km² confirmed.**
- **Grand Erg Oriental**: 185,000–192,000 km² (Algeria/Tunisia), ~70% sand-covered, mean dune height 117 m, net sand inflow ~6 million t/yr, active sand-blowing life ~1.35 Myr (Wilson's estimate); the southern half is the world's largest star-dune province ([NASA/JPL geoserver plate](http://geoinfo.amu.edu.pl/wpk/geos/geo_8/GEO_PLATE_E-6.HTML); [Wikipedia](https://en.wikipedia.org/wiki/Grand_Erg_Oriental)).
- Organization within ergs: dune type changes systematically from centre to margin — compound complex megadunes in the interior degrade to simpler, smaller forms with widening interdune corridors at the edges (supply, availability and transport capacity all fall toward margins — [Al-Masrahy & Mountney 2013](https://eprints.whiterose.ac.uk/id/eprint/80266/1/Remote%20sensing%20of%20spatial%20variability%20in%20aeolian%20dune%20and%20interdune%20morphology.pdf)). The Al Liwa basin in the Rub' al Khali shows crescentic megadunes in the north grading to star megadunes SE under a multimodal wind regime ([Atkinson 2012](https://onlinelibrary.wiley.com/doi/10.1002/esp.3318)).
- **Sand-flow corridors**: the Moroccan barchan field is literally a sand river — corridors hundreds of m wide, tens of km long, collimated over 300 km from the coastal sand source (§3.2). In the Rub' al Khali, linear dunes trend at right angles to the Shamal (NW) trades with secondary barchans and stars forming from monsoonal SW winds ([NASA](https://science.nasa.gov/earth/earth-observatory/ar-rub-al-khali-sand-sea-arabian-peninsula-50744/)).
- **Sand sheets vs dunes**: in the Gran Desierto, more than two-thirds of the area is sand sheets and streaks, not dunes ([Gran Desierto de Altar summary](https://iiab.live/kiwix/content/wikipedia_en_all_maxi_2023-05/A/Gran_Desierto_de_Altar)) — dune fields are the dramatic minority of most erg surfaces. Generate accordingly.

### 4.2 Where the sand came from — the paleo-hydrology answer

Erg sand is almost always fluvial or lacustrine sediment reworked by wind:

- Grand Erg Oriental: Pliocene–Pleistocene rivers (Oued Igharghar from the Ahaggar, Atlas/Aurès rivers from the north) delivered sand to the basins; ergs began assembling ~1.6 Ma and reworked during arid phases to the Holocene ([Wikipedia summary of published geology](https://en.wikipedia.org/wiki/Grand_Erg_Oriental); Wilson's 6 Mt/yr inflow from upwind alluvium, [NASA plate](http://geoinfo.amu.edu.pl/wpk/geos/geo_8/GEO_PLATE_E-6.HTML)).
- Rub' al Khali: aeolian-reworked Pliocene alluvial sediments plus wadi inputs; reddish southern sands carried in by wadis from surrounding highlands ([Al-Masrahy & Mountney 2013](https://eprints.whiterose.ac.uk/id/eprint/80266/1/Remote%20sensing%20of%20spatial%20variability%20in%20aeolian%20dune%20and%20interdune%20morphology.pdf); [NASA](https://science.nasa.gov/earth/earth-observatory/empty-quarter-arabian-peninsula-77714/)).
- **The Sahara's wet phases are the supply mechanism.** During African Humid Periods the Sahara hosted rivers, lakes and vegetation (§6.5): the Tamanrasset paleoriver — a Ganges-scale basin river active as recently as ~5 ka, discovered by radar beneath the sand ([Tamanrasset River](https://en.wikipedia.org/wiki/Tamanrasset_River)) — and Lake Megachad, whose dried floor is now the Bodélé dust machine (§7.1). Fluvial deposition during wet phases *stocks* the sand; wind mines it during dry phases. Ergs are therefore paleoclimate archives with lag: their sand has been recycled through multiple climate cycles (see the Mu Us / Chinese desert-margin records, [Xu et al. 2018](https://doi.org/10.1130/g45105.1)).

---

## 5. Aeolian erosional landforms

### 5.1 Yardangs

Streamlined wind-carved ridges, carved from bedrock or consolidated sediment (the name is Turkmen for "steep bank," coined by Hedin at Lop Nur). Requirements: erodible but cohesive substrate (lacustrine sediments, poorly consolidated rock), strong persistent unidirectional winds, minimal rainfall (<50 mm/yr typical), sparse vegetation ([Angles & Muños 2018... Wang et al.](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2018JE005719)).

**Verified dimensions:**

- **Lop Nur (Bailongdui)**: mean length 63.4 m, mean width 14.0 m, mean aspect ratio R = 4.37; 65% of yardangs 25–75 m long, 80% 5–20 m wide; the classic mesas 10–30 m tall ([Lop Nur morphometric study](http://alg.xjegi.com/EN/abstract/abstract9934.shtml); [Dong et al. on Kumtagh/Lop Nur](https://www.sciencedirect.com/science/article/abs/pii/S0169555X11005289)).
- **Qaidam Basin** (largest field on Earth, ~38,800 km²): long-ridge yardangs to 3,900 m long, heights 0.5–30 m (ridge group); mesa-type yardangs up to 60 km × 1 km; gale (≥17 m/s) winds 57–105 days/yr; formation requires high-energy, narrow unimodal wind regimes — unlike dunes, which thrive at low-to-intermediate energy ([Wang et al. 2018](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2018JE005719); [wind-regime study](http://jal.xjegi.com/EN/abstract/abstract634.shtml)). Yardang development there commenced ~0.8 Ma and expanded SE with Northern-Hemisphere ice-sheet growth strengthening the Siberian High ([Sun et al. 2026](https://doi.org/10.1038/s43247-026-03202-x)).
- Aspect ratios globally 2.7–40 (Chad 20–30; cemented Andean mega-yardangs 20–40) — the "ideal 4:1" is a central tendency, not a law ([comparative table](http://alg.xjegi.com/EN/abstract/abstract9934.shtml)).
- **Erosion rates**: Saharan meso-yardangs incised into Holocene lake sediments imply 0.4–4 mm/yr ([Dong et al. 2012](https://www.sciencedirect.com/science/article/abs/pii/S0169555X11005289)) — but wind alone rarely does this: fluvial incision prepares the corridors and salt weathering attacks the substrate; wind is the finisher (Li Daoyuan proposed exactly this 1,500 years ago, and the modern literature agrees).

### 5.2 Ventifacts, deflation, pavement, tafoni (compressed)

- **Ventifacts**: wind-faceted stones. The faceting mechanism is saltation sandblasting concentrated in the lowest ~0.5 m (where the saltation cloud's mass flux peaks), so ventifacts record the upwind direction; multi-facet forms record changes in the dominant wind or stone tipping. Requires coarse, hard clasts + abundant mobile sand + strong winds. (Standard textbook mechanics — [opengeology deserts chapter](https://opengeology.org/textbook/13-deserts/) — no modern quantitative rates exist that I could verify; treat as qualitative.)
- **Deflation basins/pans**: wind removal of fines down to the water table or a coarse lag. Pans (shallow closed depressions in semiarid plains) form by a combination of deflation, livestock trampling and salt weathering; their floors typically sit at the capillary fringe where moisture defeats further deflation (qualitative; [opengeology](https://opengeology.org/textbook/13-deserts/)).
- **Desert pavement** — antiquity *verified with a twist*: cosmogenic ³He exposure ages of pavement clasts on Mojave basalt flows are statistically indistinguishable from the flows themselves (up to ~10⁵ yr), proving clasts stay at the surface while an *accretionary dust mantle inflates beneath them* — pavements are "born at the surface," not deflation lags ([Wells et al. 1995](http://geomorphology.sese.asu.edu/Papers/Wells_etal_1995_Geology_StonePavements3He.pdf)). ¹⁰Be surface ages extend stable pavements to 30–332 ka in the Sonoran Desert and, in hyperarid settings, to ~10⁶ yr ([Dorn life-expectancy review](https://www.sciencedirect.com/science/article/abs/pii/S0012825216302252)). The apparent conflict with OSL ages of Av horizons (mostly ≤5 ka) is resolving toward thermal resetting of the OSL signal in 35–50 °C summer soils, not genuinely young pavements ([McDonald et al. 2020](https://doi.org/10.5194/egusphere-egu2020-11978)); a numerical model shows pavements take ~10⁴ yr to fully form but heal in decades–centuries after disturbance ([Pelletier et al. model](https://onlinelibrary.wiley.com/doi/10.1002/esp.1500)).
- **Rock varnish micro-environments**: darkest and thickest on stable, sun-exposed, rain-flushed but dew-fed surfaces (see §8.1).
- **Tafoni/honeycomb weathering**: cavernous decay by salt crystallization at the evaporation front within pores — needs marine or desert aerosol salt supply, wetting/drying cycles, and case-hardened skins that force subsurface weathering. Also common on coastal granites; keep it in the coastal biome's back pocket. (Qualitative; [opengeology](https://opengeology.org/textbook/13-deserts/).)

---

## 6. The climate geography of deserts

### 6.1 Hadley-cell subsidence: the master placement rule

The annually averaged descending branch of the Hadley circulation lies over ~25° N/S (subtropical highs sit 20–40°), and adiabatic compression of dry upper-tropospheric air warms and stabilizes it — suppressing convection and rain ([Hadley cell review](https://en.wikipedia.org/wiki/Hadley_cell); [MIT Lorenz Center teaching page](http://weathertank.mit.edu/links/projects/general-circulation-an-introduction/general-circulation-atmosphere-hadley); [PSU Meteo 3](https://courses.ems.psu.edu/meteo3/node/2048)). The world's hot deserts concentrate in the **15–35° latitude belts** of both hemispheres: Sahara, Arabian, Thar, Australian interior, Kalahari margin, Sonoran–Chihuahuan, Atacama, Namib ([opengeology](https://opengeology.org/textbook/13-deserts/)). Why the descending branch is at ~25–30° and not 45°: conservation of angular momentum would accelerate poleward upper flow to ~134 m/s by 30° — eddies and turbulence dissipate that, and the flow converges and sinks in the subtropics ([Hadley cell](https://en.wikipedia.org/wiki/Hadley_cell)).

Simulation number: **primary desert band |latitude| ∈ [15°, 35°]**, with aridity peaking 20–30° and modulated by the continentality and coast-temperature terms below.

### 6.2 Rain shadows — verified gradients

Mechanism: orographic lift wrings moisture out on windward slopes; lee descent warms and dries the air, evaporating hydrometeors. Two verified magnitude anchors:

- **Olympic Mountains, Washington**: >6,600 mm/yr on windward (SW-facing) peaks vs ~400–500 mm/yr in the lee lowlands — a >13:1 gradient within ~60 km ([Purnell & Kirshbaum 2018](https://doi.org/10.1175/mwr-d-17-0267.1); [Anders et al. 2023, Esurf](https://esurf.copernicus.org/articles/11/849/2023/), which gives the modern range as ~6,500 vs ~500 mm). Note the parent's "3,600 mm windward" figure: I could not verify a 3,600/200 station pair for the Cascades specifically; the robust published pair is the Olympic one above. Use **lee:windward ratio ~1:10 across a 1.5–2 km barrier** as the default rule.
- Rain-shadow strength is *variable*: it is modulated by storm type (warm-sector postfrontal storms with strong mountain waves give strong shadows; warm-frontal passages with blocked leeside stagnant layers give weak ones) and by ENSO — La Niña years strengthen the Washington shadow, El Niño weakens it ([Siler & Roe 2013](https://atmos.uw.edu/~durrand/pdfs/AMS/2013_SilerRoeDurran_Hydro.pdf); [Mass et al. 2015](https://doi.org/10.1175/jhm-d-14-0149.1)). For a generator: shadow strength ∝ (barrier height) × (wind-perpendicularity) × (upstream moisture), with a lee plateau of semi-arid (not hyperarid) climate unless other factors stack.

### 6.3 Cold-coast deserts: Benguela, Humboldt, and the Atacama

Cold eastern-boundary currents cool the marine boundary layer → inversion → coastal stratus but no rain. The **Namib** (Benguela current) gets 5–50 mm/yr with fog as the main moisture input. The **Atacama** stacks every mechanism in this document onto one coastline ([Atacama Desert](https://en.wikipedia.org/wiki/Atacama_Desert)):

1. subtropical Pacific subsidence + coastal anticyclone;
2. the cold Humboldt/Peru current and its persistent inversion;
3. a **double rain shadow** — the Chilean Coast Range blocks Pacific moisture, the Andes block Atlantic moisture (the two-sided rain shadow);
4. continentality with respect to the Atlantic.

**Verified numbers:** average annual rainfall ~15 mm over the desert as a whole; the hyperarid core receives **<2 mm/yr** with some stations recording only 1–3 mm/yr and some never recording rain; no significant rainfall 1570–1971 in parts; MAP rises from <20 mm at 2,300 m to >300 mm at 5,000 m on the Andean slope ([Atacama Desert](https://en.wikipedia.org/wiki/Atacama_Desert); [hyperarid-core palaeoclimate study](https://preview-www.nature.com/articles/s41598-019-41743-8); [McKay et al. 2003](https://www.astro.uantof.cl/wp-content/uploads/2015/06/McKay_etal_2003_AtacamaMet.pdf) — 4 years of in-situ meteorology with exactly one 2.3 mm rain event). **Parent's ~1 mm/yr core figure: confirmed (1–3 mm/yr at the driest stations).** Hyperaridity dates back at least to the Miocene (19–13 Ma transition, contested between ~25 Ma and 2–1 Ma candidates — [Ehlers & Poulsen 2009](https://www.sciencedirect.com/science/article/abs/pii/S0012821X10000464)).

Generator rule: **west coasts between 15–30° latitude with a cold current get fog desert; add a coast range + inland mountain barrier for hyperaridity.** (Cold-current coasts alone are cool deserts — the hyperaridity requires the stacked factors.)

### 6.4 Continentality and cold deserts

Deep continental interiors (Gobi, Taklamakan) are dry because moist air masses rain out or divert around them; they additionally get *cold* winters, so their aridity is compounded by thermal, not just moisture, stress — which suppresses the vegetation that would otherwise stabilize sand and soil ([opengeology](https://opengeology.org/textbook/13-deserts/)). The Taklamakan is the showcase of continentality + basin topography (surrounded by 4–5 km ranges on three sides) — a cold desert that still supplies major dust to the loess system (§7.2). Polar deserts (Dry Valleys, ~25–50 mm water equivalent) are the extreme continentality/cold case.

### 6.5 Deserts move: the paleoclimate of aridity

Desert belts are not fixed. The Sahara has flipped between hyperarid desert and vegetated, lake-dotted savanna on orbital timescales:

- **Pacing: ~20–21 kyr precession, NOT 41 kyr.** The African Humid Periods (NAHPs) are paced by precession of the equinoxes, which controls boreal-summer insolation and hence the African monsoon's northward reach; eccentricity modulates the amplitude via its control on ice-sheet extent (glacials suppress NAHPs — the "skipped beats"), and obliquity contributes a weaker, intermittent signal. 230 NAHPs identified over 8 Myr; 20 simulated over 800 kyr, all at precession minima ([Liu et al. 2023, Nat. Comms](https://www.nature.com/articles/s41467-023-41219-4); [Scitable explainer](https://go.nature.com/2wVEuTB)). **Anchor disagreement — see §11: the parent's "41-kyr precession pacing" conflates obliquity (41 kyr) with precession (~20 kyr).** The 41-kyr power in Saharan precipitation records is real but subordinate and glacially mediated.
- **The most recent AHP**: ~14.8 (or 11.5)–5.5 ka; summer insolation over North Africa ~7% higher than today at its peak ~10 ka; lakes rose (Lake Bosumtwi +100 m), the Sahara was largely vegetated with lakes, rivers, hippos and giraffes ([de Menocal 2015](https://www.whoi.edu/cms/files/demenocal15nat_220285.pdf); [Scitable](https://go.nature.com/2wVEuTB)).
- **The end (~5.5–5 ka)** was time-transgressive: monsoon rains weakened first in the north (5–6 ka above 15°N) and progressively later at lower latitudes (2.5–4 ka below 15°N), locally abrupt due to vegetation/soil feedbacks ([Shanahan et al. 2015](https://www.whoi.edu/cms/files/shanahan12nat_220305.pdf); [de Menocal 2015](https://www.whoi.edu/cms/files/demenocal15nat_220285.pdf)). Human occupation collapsed across North Africa 6.3–5.2 ka and consolidated along the Nile.
- A separate mechanism also fired the terminal drying: mid-high-latitude cooling 6.0–5.0 ka slowed the Tropical Easterly Jet, tipping the region over a threshold amplified by vegetation-dust feedbacks ([Nature Comms 2018](http://preview-www.nature.com/articles/s41467-017-01454-y.pdf)).

Generator consequence: deserts should be a *state*, not a permanent biome attribute; on geological timescales their boundaries sweep by 10–20° of latitude. Even on human timescales, dune fields at desert margins switch between active and vegetated states with climate (the desert margin as a dust source during glacials, trap during interglacials — [Xu et al. 2018](https://doi.org/10.1130/g45105.1)).

---

## 7. The sediment-transport chain beyond deserts

### 7.1 Dust storms and the global dust belt — Bodélé verified

- **The Bodélé Depression, Chad, is the single largest dust source on Earth**: ~0.2% of the Sahara's area producing roughly **half the Sahara's mineral aerosol output** ([Washington et al., PNAS tipping-element paper](https://www.pnas.org/doi/10.1073/pnas.0711850106)). Measured/estimated fluxes: 58 ± 8 Tg emitted annually (of which ~45 Tg are loaded onto the trade winds), ~0.7 Tg per emission day, active ~40% of winter days; ~40 ± 13 Tg of the 240 ± 80 Tg Africa exports to the Atlantic each year, with ~50 Tg reaching and fertilizing the Amazon ([Koren et al. 2006](https://iopscience.iop.org/article/10.1088/1748-9326/1/1/014005/pdf)). Field campaign (BoDEx 2005): emission triggered at near-surface winds >10 m/s, pulses with the diurnal wind cycle, 1.18 ± 0.45 Tg/day during a major event, 6–18% of global emissions from this one 10,800 km² depression ([Todd et al. 2007](https://doi.org/10.1029/2006jd007170)).
- Why there: the Tibesti and Ennedi massifs form a 44,000 km² caldera-like funnel whose narrow pass accelerates a low-level jet over the depression — a natural wind lens ([Koren et al. 2006](https://iopscience.iop.org/article/10.1088/1748-9326/1/1/014005/pdf)). And the erodible material is **diatomite** — the floor of paleo-Lake Megachad, which covered an area larger than all the Great Lakes ~7,000 years ago ([NASA](https://science.nasa.gov/earth/earth-observatory/bodele-dust-146011/)). Dust output ceased entirely during the mid-Holocene wet phase and is sensitive enough to flip again within a season ([PNAS](https://www.pnas.org/doi/10.1073/pnas.0711850106)).
- The global dust belt: a semi-continuous band from the Sahara through the Middle East (Sistan, Makran), Taklamakan/Gobi, downwind of which sit the great loess accumulations.

### 7.2 Loess — verified thicknesses and rates

- **Chinese Loess Plateau**: ~440,000 km²; **100–300 m thick** (up to >400 m near Lanzhou in the west, thinning to <5 m for the last-glacial Malan Loess east of Xi'an); the underlying red clay extends the aeolian record to ~7 Ma ([Kohfeld & Harrison-derived compilation... Roberts et al. 2003, "Glacial-interglacial changes in dust deposition on the CLP"](https://www.sciencedirect.com/science/article/abs/pii/S0277379103001665)). **Parent's "hundreds of metres" confirmed (100–300 m typical, >400 m extreme).**
- **Mass accumulation rates**: regional median 310 g/m²/yr in MIS 2 (last glacial) vs 65 g/m²/yr in MIS 5 (last interglacial) — a 4.3:1 glacial:interglacial contrast ([Roberts et al. 2003](https://www.sciencedirect.com/science/article/abs/pii/S0277379103001665)); mean dust flux 0.35 mm/yr glacial vs <0.1 mm/yr interglacial in the central plateau ([Sun/An... Chinese loess monsoon review](https://www.sciencedirect.com/science/article/abs/pii/S0012825201000435)).
- **Mechanism**: glacial-period dust storms from the northern deserts deposit silt; interglacial soil-forming weathering converts it to paleosol — the loess–paleosol couplets are a monsoon/ice-volume proxy. A live wrinkle: the upwind desert margin *switches* between dust source (glacial, dunes active) and dust trap (interglacial, vegetated) — "aeolian cannibalism" of older loess contributes much of the LGM peak ([Qiang et al. 2021](https://doi.org/10.3389/feart.2021.661874); [Xu et al. 2018](https://doi.org/10.1130/g45105.1)).

### 7.3 The periglacial coupling: the European sand belt

The Northern European Sand Belt (coversands from Brittany to Poland, ~48–51°N) and the loess belt south of it are the direct sedimentological signature of glacial outwash: Fennoscandian ice-sheet outwash plains supplied sand and silt, which bare, windy periglacial tundra-steppe deflated — sand deposited as coversands upwind, grading to sandy loess and loess downwind ([ChronoLoess database study](https://essd.copernicus.org/articles/15/4689/2023/)). Deposition ran 32–21.8 ka b2k in the north, peaking at the LGM when ice-sheet coalescence diverted meltwater through the Channel River, exposing fresh outwash; mean loess MAR across Europe 792 g/m²/yr over 60 kyr, with maxima to ~4,993 g/m²/yr ([Bosq et al. 2023](https://essd.copernicus.org/articles/15/4689/2023/)). So "periglacial desert" is literal: during glacials, cold, windy, unvegetated sand seas with dune fields existed across northwest Europe, and their deposits are the loess parent material today. For an engine with glacial-cycle world states, this is the template: glacial world → periglacial sand belt + dust plume; interglacial world → stabilized, vegetated, soil-mantled version.

---

## 8. Desert surface chemistry and soils

### 8.1 Rock (desert) varnish — growth rates and the mechanism debate

- **Composition**: ~70% clay minerals (illite/montmorillonite) cemented by Mn/Fe oxyhydroxides; MnO 10–30 wt% — a 50–200× enrichment over the crust, the geochemical puzzle at the heart of the debate ([Xu et al. 2019 Chemical Geology](https://www.sciencedirect.com/science/article/abs/pii/S0009254119302426); [Liu/Dorn microlamination paper](https://www.sciencedirect.com/science/article/abs/pii/S0169555X06002212)).
- **Growth rates: 1–40 µm/kyr** (Liu & Broecker 2000, the standard citation), thicknesses <5 to 600 µm, typically ~100 µm ([Liu/Dorn](https://www.sciencedirect.com/science/article/abs/pii/S0169555X06002212); [Xu et al. 2019](https://www.sciencedirect.com/science/article/abs/pii/S0009254119302426)). **Anchor note: the parent's "~1–10 µm/kyr" is too narrow — the published range is <1 to 40 µm/kyr, and topmost layers can grow much faster** ([Spilde via Xu 2019](https://www.sciencedirect.com/science/article/abs/pii/S0009254119302426)).
- **The mechanism debate, current state**: (a) *biotic* — Mn-oxidizing bacteria and, newly, *Chroococcidiopsis* cyanobacteria that hyperaccumulate Mn²⁺ as an antioxidant system, their necromass becoming the Mn-oxide cement ([Lingappa et al. 2021, PNAS](https://pmc.ncbi.nlm.nih.gov/articles/PMC8237629/)); (b) *abiotic* — pH-dependent Mn²⁺ mobility through dust, or photochemical oxidation on semiconducting oxide surfaces (ROS detected in varnish suspitions accelerate Mn(II) oxidation 2–8×; [Xu et al. 2019](https://www.sciencedirect.com/science/article/abs/pii/S0009254119302426)); (c) *polygenetic* models mixing both ([Chaddha et al. 2024](https://www.sciencedirect.com/science/article/pii/S000925412400041X)). Dorn's 2024 hypothesis-testing review finds **seven of eight proposed mechanisms fail five or more of nine falsification tests** (most cannot explain Fe enrichment, clay dominance, or cold-climate varnishes) — the field genuinely has not converged ([Dorn 2024](https://doi.org/10.1177/03091333241248038)).
- Microlaminations (Mn-rich dark layers in wet/glacial times, Mn-poor orange in dry/interglacials) make varnish a paleoclimate archive and a relative-dating tool for geomorphic surfaces and petroglyphs ([Liu/Dorn](https://www.sciencedirect.com/science/article/abs/pii/S0169555X06002212)).

### 8.2 Duricrusts: caliche/calcrete, gypcrete, salcrete

- **Caliche/calcrete**: pedogenic CaCO₃ accumulation (nodules → plugged horizons → indurated calcrete) where carbonate-bearing dust and rainwater meet evaporation; the hardpan caps and armours desert hillslopes (qualitative textbook mechanics, [opengeology](https://opengeology.org/textbook/13-deserts/)). Calcrete-forming soils are the arid-zone analog of humid-zone leaching: everything mobile leaves downward except what precipitates.
- **Gypcrete**: near-surface gypsum crusts in the driest deserts (gypsum is more soluble than calcite, so it survives only under <~100–150 mm/yr rainfall); common in the central Sahara and Arabian Peninsula.
- **Salcretes/playas/evaporites — the Bonneville and Uyuni sequences verified:**
  - **Lake Bonneville** (pluvial Great Basin lake, 30–13 ka): peak area ~51,000 km² ("~20,000 sq mi"), depth >300 m; the catastrophic Bonneville flood (~1.0 × 10⁶ m³/s over <1 yr) when it overtopped Red Rock Pass, dropping the lake 125 m; the Stansbury/Bonneville/Provo shorelines are isostatically warped by up to 74 m (central-basin rebound after desiccation — Gilbert's insight, confirmed) ([Lake Bonneville summary](https://en.wikipedia.org/wiki/Lake_Bonneville); [USGS Bonneville salt-flats hydrology](https://doi.org/10.3133/wsp2057)). Its desiccation sequence left the Bonneville Salt Flats (~100 km² of salt crust) and Great Salt Lake as remnants.
  - **Salar de Uyuni**: 10,000 km² at 3,653 m on the Bolivian Altiplano; **halite crust up to 11 m thick**, porosity 30–40%, filled with Na-Cl brine carrying up to 1.5 (avg 0.3–0.4) g/L Li and high K/Mg/B; ~7–10 Mt of lithium — the world's largest Li deposit ([Risacher & Fritz](https://horizon.documentation.ird.fr/exl-doc/pleins_textes/pleins_textes_5/b_fdi_31-32/34857.pdf); [Sieland thesis](https://tu-freiberg.de/sites/default/files/2023-08/fog_volume_37.pdf)). A 121-m core shows **12 salt crusts separated by 11 mud layers** — the lake-cycle stratigraphy of alternating pluvial lakes (Tauca 18.1–14.1 ka, deepest ~140 m; Coipasa ~13–11 ka; Sajsi 24–20.5 ka; Ouki 120–98 ka) and desiccations through the Quaternary ([Baker et al. geochronology](https://app.ingemmet.gob.pe/biblioteca/pdf/Reg-96.pdf); [Freiberg volume](https://tu-freiberg.de/sites/default/files/2023-08/fog_volume_57.pdf)).

### 8.3 Surface-type taxonomy

- **Reg/serir**: gravel-covered plains (pavement over fines); "serir" traditionally for finer, desert-pavement-like surfaces, "reg" coarser.
- **Hammada**: bare rock/hamada plateaus — wind-swept bedrock or duricrust-capped surfaces separating the big ergs (e.g., the 100-km Tademaït plateau between the two Grand Ergs — [NASA plate](http://geoinfo.amu.edu.pl/wpk/geos/geo_8/GEO_PLATE_E-6.HTML)).
- **Sabkha**: salt-cemented flats (coastal or continental) — damp because the brine table sits centimetres below the surface; the Rub' al Khali's interdunes grade to sabkha in the NE.
- Areal proportions in real deserts: ergs are a minority of even "sand deserts" (the Sahara is only ~20–25% sand-covered; the Gran Desierto is >2/3 sand sheet; §4.1) — the modal Sahara surface is reg/hammada, and generators that fill every desert with dunes are wrong.

---

## 9. Water in deserts

### 9.1 Flash floods and the runoff regime

Desert runoff is **Hortonian** (infiltration-excess): high-intensity convective rainstorms exceed the infiltration capacity of thin, crusted, often impermeable soils, so rainfall converts to overland flow within minutes. Add sparse vegetation (no interception, no root macropores) and you get the classic flash flood: a dry wadi filling wall-to-wall within an hour of cloudbursts in headwaters tens of km away, then draining into the substrate and vanishing. Where slopes fail and colluvium is fine-rich, the flood becomes a debris flow (see below). (Standard desert-geomorphology mechanics; [opengeology](https://opengeology.org/textbook/13-deserts/).)

### 9.2 Alluvial fans — process dominance set by lithology, verified

Blair's paired-fan study in Death Valley is the clean result: two adjacent fans with identical climate, relief, area and vegetation differ completely in process — the **Warm Spring fan is 75–98% debris-flow deposits**, the **Anvil Spring fan is purely water-laid (sheetflood + incised-channel)** — because the catchments' bedrock differs: shale/quartzite/dolomite weathers to mud-rich colluvium whose low permeability raises pore pressures and transforms slope failures into debris flows; granite/andesite yields permeable, mud-poor sediment that stays fluvial ([Blair 1999a](https://doi.org/10.1046/j.1365-3091.1999.00261.x); [Blair 1999b](https://onlinelibrary.wiley.com/doi/10.1046/j.1365-3091.1999.00260.x)). Across 60 Death Valley-region fans, fan deposition area ≈ 1/3 to 1/2 of source-basin area ([USGS PP-466](https://pubs.usgs.gov/pp/0466/report.pdf)). Debris-flow fans are steeper and less dissected; fluvial fans are more channelized — fan slope correlates with transport efficiency ([Milana & Ruzycki 1999](https://doi.org/10.2110/jsr.69.553)). Arid vs humid fans in one line: arid fans are debris-flow-dominated, steep, and constructional; humid fans are fluvially reworked, flatter, and entrenched.

### 9.3 Ephemeral rivers, inland deltas, terminal sinks

Desert rivers mostly terminate in closed basins (endorheic): the Okavango into its fan-delta and evaporative sink; the Tarim into Lop Nur's playa complex; Saharan wadis into chotts (the Grand Erg Oriental's northern boundary is the Chott Melrhir/Jerid at −12 m). The terminus of every desert drainage is a playa or sabkha (§8.2) — the evaporite factory.

### 9.4 Oases — the water-table mechanism

An oasis is not a spring from nowhere: it is a **topographic low where the water table intersects the land surface**. Three variants: (1) valley-floor oases where alluvium is shallow over bedrock in a wadi thalweg; (2) artesian-basin oases at the downdip edge of a confined aquifer (the entire northern Sahara sits on the Continental Intercalaire aquifer — ~60,000 km³ of mostly fossil Pleistocene water, discharging at places like the Algerian-Tunisian oases — [Grand Erg Oriental hydrogeology](https://en.wikipedia.org/wiki/Grand_Erg_Oriental)); (3) fault-line oases where fracture zones drain bedrock aquifers. Oases are the surface expression of a regional groundwater story; the Rub' al Khali's sabkha interdunes are the same story half-told (brine, not fresh water, 10–20 cm down — [Al-Masrahy & Mountney 2013](https://eprints.whiterose.ac.uk/id/eprint/80266/1/Remote%20sensing%20of%20spatial%20variability%20in%20aeolian%20dune%20and%20interdune%20morphology.pdf)).

### 9.5 Desert lakes and the playa cycle

Playas fill every few to every few decades, evaporate in weeks-months, and desiccate into cracked clay pavers. The spectacular special case, now solved: **Racetrack Playa's sailing stones** move when a winter pond (fed by a once-in-years storm) freezes into a 3–6 mm "windowpane" sheet that breaks up under light 4–5 m/s midday winds and bulldozes embedded rocks at 2–5 m/min, for minutes at a time — GPS-instrumented rocks moved up to 224 m in the 2013–14 winter ([Norris et al. 2014, PLOS ONE](https://pmc.ncbi.nlm.nih.gov/articles/PMC4146553/); [Lorenz et al.](http://www.racetrackplaya.org/wp-content/uploads/2014/07/TrailFormationObserved_RD0812.pdf)). For a game with a desert weather system, a rock-trail playa is a cheap, physically honest easter egg.

---

## 10. Practical simulation synthesis (opinionated)

### 10.1 The dune-field generator

**Inputs per region:** wind rose (direction/magnitude distribution, e.g., 8–16 sectors with annual transport weights), grain size d (0.15–0.30 mm), sand-supply map (0 = bare rock → 1 = full bed), vegetation density (0–1).

**Step 1 — transport vectors.** Convert the wind rose into transport vectors $\vec{q}_i$ using $|\vec{q}_i| \propto u_{*,i}^3 - u_{*,it}^3$ (impact threshold; §1.3–1.4). Compute RDP = |Σqᵢ|, DP = Σ|qᵢ|, and RDP/DP.

**Step 2 — dune type via the Rubin–Hunter + two-mode rule.** Compute the two candidate orientations:
- $\hat{t}_{GBNR}$ (maximize Σ|qᵢ × t̂|) — the bed-instability orientation, for sand-covered ground;
- $\hat{t}_{F}$ (resultant flux direction, Σqᵢ normalized) — the fingering orientation, for point-source sand on hard ground.

Then select morphology:

| RDP/DP | supply | vegetation | dune type |
|---|---|---|---|
| > 0.7 | low (localized) | ~0 | **barchans**, migrating; size from supply |
| > 0.7 | high (blanket) | ~0 | **transverse** ridges along t̂_GBNR |
| 0.3–0.7 (bimodal, divergence > 90°) | any | ~0 | **linear** (bed-instability mode if blanket; finger/seif mode if source) |
| < 0.3 (multidirectional) | high | ~0 | **star** dunes (vertical growth, ~static) |
| any | any | moderate | **parabolic** (nose advance, arms pinned) |
| low energy near threshold | moderate | ~0 | **dome/slip-faceless proto-dunes** |

Sanity anchors: Morocco barchan country is RDP/DP ≈ 0.9 with localized coastal supply; the Grand Erg Oriental's star province is RDP/DP → 0 with full coverage; Israel's coastal parabolics are unimodal winds + recovering vegetation.

**Step 3 — sizes.** Saturation length $\ell_s \approx 4.4\,(\rho_s/\rho_{air})\,d$ → 1.5–2 m for Earth sand. Minimum dune wavelength ~12 ℓ_s (20–25 m); proto-dune cutoff ~6.3 ℓ_s. Field-scale spacing: dune wavelength grows by coarsening with age — young fields 20–100 m, mature draa 500 m–3 km. Heights: aspect ratio H/λ ≈ 1:15–1:30 for simple dunes; draa to 100–300 m in the big ergs. Star dunes: width ~900 m mean, height ~75 m mean, up to ~290 m.

**Step 4 — migration (optional dynamic).** $c = f_{trap}\,q_{eff}/(\rho H)$ with $f_{trap} \approx 0.5$–0.8 and $q_{eff}$ the *annually integrated* flux (apply the threshold to the actual wind distribution, not the mean wind). Expected fidelity: factor ~2. A 7 m barchan at 25 m/yr crosses a 100 m chunk in 4 years — fast enough to be worth simulating only if the player returns to places.

**Step 5 — hierarchy.** Always place superimposed bedforms one scale down on the big features (ripples on dunes, dunes on draa), each set to the Rubin–Hunter orientation of the wind as locally deflected. And leave most of the desert as sand sheet, reg, or hammada, not dunes (§8.3).

### 10.2 The desert-biome placement rule set

Combine additive aridity factors, all with §6 numbers:

1. **Latitude band**: base aridity ∝ exp(−((|φ| − 25°)/10°)²) peaking 20–30°.
2. **Rain shadow**: for each moisture-bearing wind direction, integrate terrain lift along back-trajectories; lee precipitation ≈ windward/10 across 2 km barriers (Olympic anchor: 6,600 → 400–500 mm).
3. **Coast temperature**: cold current + west coast at 15–30° → fog desert; add a coast range for Atacama-class hyperaridity (<2–15 mm/yr).
4. **Continentality**: aridity grows with distance-to-coast along prevailing winds; at distance > ~1,000–2,000 km interiors are desert unless monsoon mechanics intervene.
5. **Paleoclimate state**: if the world has an orbital/climate phase parameter, sweep desert margins ±10–20° latitude (AHP analog: wet-phase Sahara has lakes, rivers, vegetation, no dust; dry-phase has ergs + Bodélé-class dust engines).

Thresholds to hand the biome mapper: <250 mm/yr = arid (desert proper), 250–500 mm/yr = semiarid (steppe/desert margin with parabolic-dune potential), and the vegetation-stabilization crossover sits in that semiarid band (parabolics stabilize; reactivation in drought).

### 10.3 Aeolian micro-texture: ripples

Impact ripples: wavelength set by the **mean reptation length** — about **6×** the mean reptation hop, not the saltation path length as Bagnold guessed ([Anderson 1987](https://sseh.uchicago.edu/doc/Anderson_MFRP_2012_Sedimentology_1987.pdf)). Measured initial wavelengths: 15, 45, 85 mm at u* = 0.35, 0.51, 0.65 m/s; fully-developed field ripples saturate after coarsening, and both initial and final wavelengths are linear in wind speed ([Andreotti et al. 2006, PRL](https://journals.aps.org/prl/abstract/10.1103/PhysRevLett.96.028001)). **Parent's "5–10 cm" ripple wavelength: confirmed as the correct order for ordinary sand and moderate winds** (with coarse-grained "megaripples" reaching decimetres-to-metre scale by the same mechanism plus coarseness armoring). Ripples migrate at cm–dm/min when active — pure texture, no physics needed: tile the sand surface with a sinusoidal displacement whose wavelength scales with the local design wind, coarser-grained patches getting longer ripples.

### 10.4 What sand movement implies for the engine's wind system

The project's current wind practice — constant direction, Perlin-magnitude modulation — is *exactly* RDP/DP ≈ 1, i.e., the trade-wind limit. That single choice locks every desert in the world into barchan/transverse morphology (and honest trade-wind vegetation sway). To get the seifs, stars, and parabolics that make real deserts visually diverse, the wind needs **directional** variability, and the cheapest honest version is:

- wind direction θ(t) = θ₀ + A·noise_slow(t), with A in degrees controlling the local RDP/DP (A ≈ 0 → 1.0; A ≈ 40–60° → ~0.7; A ≈ 90°+ bimodal → <0.5);
- or two regime channels (a dominant θ₀ and a seasonal θ₁ at ±50–150° divergence) mixed by a slow seasonal weight — this directly gives you the GBNR inputs of §10.1 with zero extra runtime cost, and the vegetation-sway system gets believable seasonal wind shifts for free;
- dust events as the visible tail of the same field: when |wind| exceeds the local threshold (higher where crusted/vegetated, lower on fresh sand), spawn a dust volume whose source clusters in topographic wind-lenses (the Bodélé lesson: a mountain gap + a dry lakebed is a dust machine).

What *not* to simulate: real-time saltation cascades, per-grain avalanche physics on dunes, and threshold hysteresis at pixel scale. The Werner slab model is the honest ceiling if you want dynamic dunes at all — and even that is a background job, not a frame-time one.

---

## 11. Anchor-verification summary (parent values vs sources)

| # | Anchor | Parent | Verified | Disposition |
|---|---|---|---|---|
| 1 | Bagnold flux q at u* = 0.3/0.5/0.8 | 0.02/0.08/0.34 t/(m·h) | 0.018/0.082/0.338 | **Confirmed** (arithmetic + coefficient C=1.5 verified); note threshold-subtraction and quadratic-scaling caveats (§1.4) |
| 2 | Impact/fluid threshold ratio | ~0.8 | 0.82 lab/theory; 0.813–0.863 field | **Confirmed** (§1.3) |
| 3 | Migration c = q/(ρH), ρ=1650 | 11–45 m/yr (u*=0.4); 38–152 (u*=0.6) | 11.2–44.9; 37.9–151.5 | **Arithmetic confirmed**; over-predicts small dunes ~2× vs Morocco (100 m/yr measured for H=1 m); causes quantified (§3.1) |
| 4 | Morocco barchans 5–70 m/yr | 5–70 | 15–100 (coastal), 9–24 (inland Jorf) | **Broadly confirmed**; low end is inland, high end is small coastal dunes (§2.2) |
| 5 | Sahara green-phase pacing | "41-kyr precession" | precession is ~20–21 kyr; 41 kyr is obliquity | **DISAGREEMENT — parent conflates obliquity with precession** (§6.5) |
| 6 | AHP end ~5–6 ka | 5–6 ka | 5.8–4.8 ka regionally; time-transgressive to 2.5–4 ka at <15°N | Confirmed with structure (§6.5) |
| 7 | Rub' al Khali area | ~650,000 km² | 650,000–660,000 km² | **Confirmed** (§4.1) |
| 8 | Atacama core rainfall | ~1 mm/yr | 1–3 mm/yr at driest stations; <2 mm/yr hyperarid core | **Confirmed** (§6.3) |
| 9 | Washington rain shadow | 3,600 mm windward / 200 lee | Olympics: >6,600 windward / 400–500 lee | **Partially confirmed** — gradient real, exact parent figures unverified; the verified pair is starker (§6.2) |
| 10 | Varnish growth | ~1–10 µm/kyr | 1–40 µm/kyr (Liu & Broecker) | **Disagreement — parent range too narrow** (§8.1) |
| 11 | Bodélé biggest source | yes | ~half of Saharan output from 0.2% of its area | **Confirmed** (§7.1) |
| 12 | Chinese loess thickness | hundreds of m | 100–300 m typical, >400 m at Lanzhou | **Confirmed** (§7.2) |
| 13 | Ripple wavelength ~5–10 cm | 5–10 cm | 15–85 mm initial; cm-to-dm mature | **Confirmed as order of magnitude**; mechanism is 6× reptation length, not saltation path (§10.3) |

---

## Provenance

This document was researched with live web search (Exa) on 2026-03-05. All URLs were returned by search and spot-verified by reading their content; none are fabricated. Sections 5.2 (ventifacts, deflation pans) and parts of §9.1 rely on standard textbook mechanics summarized at [opengeology.org](https://opengeology.org/textbook/13-deserts/) because no modern quantitative source surfaced within the search budget; these are flagged inline as qualitative. Where the parent's numeric anchors disagreed with sources, both values are reported in §11. The Bagnold flux and migration tables were recomputed independently rather than copied from any source.

## Consolidated sources

1. Bagnold formula — https://en.wikipedia.org/wiki/Bagnold_formula
2. Bagnold, *The Physics of Blown Sand and Desert Dunes* (1941) — https://link.springer.com/book/10.1007/978-94-009-5682-7
3. Valance, "The physics of aeolian sand transport" — https://comptes-rendus.academie-sciences.fr/physique/item/10.1016/j.crhy.2015.01.006.pdf
4. van Rijn & Strypsteen, "A fully predictive model for aeolian sand transport" — https://doi.org/10.1016/j.coastaleng.2019.103600
5. Kok, "Analytical calculation of the impact threshold" — https://export.arxiv.org/pdf/1001.4840v1.pdf
6. Martin & Kok, "Distinct Thresholds..." (JGR 2018) — https://doi.org/10.1029/2017jf004416 (PDF: https://jasperfkok.com/wp-content/uploads/2018/09/martinkok_2018_jgr_aeolian_thresholds.pdf)
7. Kok et al., "Improved dust emission model" — https://acp.copernicus.org/articles/14/13023/2014/acp-14-13023-2014.pdf
8. Pähtz et al., "Unified model of sediment transport" — https://doi.org/10.1029/2020jf005859
9. Rubin & Hunter, "Bedform Alignment in Directionally Varying Flows" — https://doi.org/10.1126/science.237.4812.276
10. Rubin & Ikeda, flume alignment experiments — https://doi.org/10.1111/j.1365-3091.1990.tb00628.x
11. Gao et al., "Phase diagrams of dune shape and orientation" — https://www.nature.com/articles/srep14677
12. Werner, "Eolian dunes: computer simulations..." — https://sseh.uchicago.edu/doc/Werner_1995.pdf
13. Werner-model genealogy & reimplementation — https://smallpond.ca/jim/sand/dunefieldMorphology/index.html
14. Bishop/Momiji discrete-dune-field models — https://carretero.sdsu.edu/publications/postscript/dunes2.pdf
15. Elbelrhiti et al., "Barchan dune corridors" — https://doi.org/10.1029/2007jf000767
16. Elbelrhiti, morphodynamics by GPS/aerial photos (2011) — https://hal.science/hal-01950849/document
17. Elbelrhiti et al., "Field evidence for surface-wave-induced instability" — https://pubmed.ncbi.nlm.nih.gov/16193049/
18. Hersen et al., "Corridors of barchan dunes" — https://journals.aps.org/pre/abstract/10.1103/PhysRevE.69.011304
19. Hersen & Douady, "Collision of barchan dunes" — https://doi.org/10.1029/2005gl024179
20. Andreotti, "Measurements of the sand transport saturation length" — https://www.phys.ens.psl.eu/~foldingslidingstretchinglab/papers/A47_LSatSaltation.pdf
21. Aydda & Algouti, Landsat barchan rates — https://doi.org/10.14195/978-989-96253-3-4_17
22. ISPRS Laâyoune dune extraction study — https://isprs-archives.copernicus.org/articles/XLII-2-W1/53/2016/isprs-archives-XLII-2-W1-53-2016.pdf
23. Jorf barchan mobility study — https://www.ecoeet.com/pdf-213427-131849?filename=Evaluation-of-the-mobilit.pdf
24. Tsoar, "Dynamic processes acting on a longitudinal (seif) dune" — https://doi.org/10.1111/j.1365-3091.1983.tb00694.x
25. Tsoar, "Linear dunes — forms and formation" — https://doi.org/10.1177/030913338901300402
26. Tseo, "Two types of longitudinal dune fields" — https://doi.org/10.1002/esp.3290180706
27. Rozier et al., "Elongation and Stability of a Linear Dune" — https://doi.org/10.1029/2019gl085147
28. Lancaster, "The dynamics of star dunes: Gran Desierto" — https://doi.org/10.1111/j.1365-3091.1989.tb00607.x
29. Star-dune global analysis — https://www.sciencedirect.com/science/article/abs/pii/S1875963721000227
30. Gran Desierto composite-pattern OSL study — https://onlinelibrary.wiley.com/doi/10.1111/j.1365-3091.2006.00814.x
31. Gran Desierto de Altar summary — https://iiab.live/kiwix/content/wikipedia_en_all_maxi_2023-05/A/Gran_Desierto_de_Altar
32. Tsoar & Blumberg, parabolic-dune formation, Israel coast — https://doi.org/10.1002/esp.417
33. Goudie, "Parabolic Dunes: Distribution, Form, Morphology and Change" — https://doi.org/10.56093/aaz.v50i3-4.63760
34. Alkalla et al., barchan–parabolic transition model — https://doi.org/10.1029/2024jf008220
35. Al-Masrahy & Mountney, Rub' Al-Khali dune/interdune morphology — https://eprints.whiterose.ac.uk/id/eprint/80266/1/Remote%20sensing%20of%20spatial%20variability%20in%20aeolian%20dune%20and%20interdune%20morphology.pdf
36. Rub' al Khali (Wikipedia, area/dimensions) — https://en.wikipedia.org/wiki/Rub_al-Khali
37. NASA: Ar Rub' al Khali — https://science.nasa.gov/earth/earth-observatory/ar-rub-al-khali-sand-sea-arabian-peninsula-50744/
38. Grand Erg Oriental (geology, star dunes, aquifer) — https://en.wikipedia.org/wiki/Grand_Erg_Oriental
39. NASA/JPL geoserver plate E-6 (Grand Erg Oriental) — http://geoinfo.amu.edu.pl/wpk/geos/geo_8/GEO_PLATE_E-6.HTML
40. Grand Ergs classification (Persée, French) — https://www.persee.fr/doc/tigr_0048-7163_1984_num_59_1_1151
41. Atkinson, Al Liwa megadunes — https://onlinelibrary.wiley.com/doi/10.1002/esp.3318
42. Tamanrasset paleoriver — https://en.wikipedia.org/wiki/Tamanrasset_River
43. Wang et al., Qaidam yardangs (JGR Planets) — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2018JE005719
44. Lop Nur Bailongdui yardang morphometry — http://alg.xjegi.com/EN/abstract/abstract9934.shtml
45. Dong et al., Kumtagh/Lop Nur yardangs — https://www.sciencedirect.com/science/article/abs/pii/S0169555X11005289
46. Sun et al., Qaidam yardang ages (2026) — https://doi.org/10.1038/s43247-026-03202-x
47. Qaidam long-ridge yardang wind regime — http://jal.xjegi.com/EN/abstract/abstract634.shtml
48. Wells et al., cosmogenic ³He dating of pavements — http://geomorphology.sese.asu.edu/Papers/Wells_etal_1995_Geology_StonePavements3He.pdf
49. Dorn, "Evaluating the life expectancy of a desert pavement" — https://www.sciencedirect.com/science/article/abs/pii/S0012825216302252
50. McDonald et al., Av-horizon OSL ages (EGU 2020) — https://doi.org/10.5194/egusphere-egu2020-11978
51. Fuchs et al., desert pavement review — https://doi.org/10.1002/esp.70213
52. Pelletier et al., pavement dynamics model — https://onlinelibrary.wiley.com/doi/10.1002/esp.1500
53. Lingappa et al., varnish Mn enrichment (PNAS 2021) — https://pmc.ncbi.nlm.nih.gov/articles/PMC8237629/
54. Dorn, "Rock varnish revisited" (2024) — https://doi.org/10.1177/03091333241248038
55. Xu et al., varnish photochemical mechanism — https://www.sciencedirect.com/science/article/abs/pii/S0009254119302426
56. Chaddha et al., biotic–abiotic varnish — https://www.sciencedirect.com/science/article/pii/S000925412400041X
57. Liu/Dorn varnish microlamination & growth rates — https://www.sciencedirect.com/science/article/abs/pii/S0169555X06002212
58. Hadley cell (mechanism overview) — https://en.wikipedia.org/wiki/Hadley_cell
59. MIT Lorenz Center, General Circulation: Hadley — http://weathertank.mit.edu/links/projects/general-circulation-an-introduction/general-circulation-atmosphere-hadley
60. PSU Meteo 3, Subtropical Highs — https://courses.ems.psu.edu/meteo3/node/2048
61. Purnell & Kirshbaum, OLYMPEX orographic precipitation — https://doi.org/10.1175/mwr-d-17-0267.1
62. Anders et al., Olympic precipitation gradient (Esurf 2023) — https://esurf.copernicus.org/articles/11/849/2023/
63. Siler & Roe, Cascade rain-shadow variability — https://atmos.uw.edu/~durrand/pdfs/AMS/2013_SilerRoeDurran_Hydro.pdf
64. Mass et al., cross-barrier precipitation ratios — https://doi.org/10.1175/jhm-d-14-0149.1
65. Atacama Desert (rainfall, double rain shadow) — https://en.wikipedia.org/wiki/Atacama_Desert
66. Atacama hyperarid-core palaeoclimate — https://preview-www.nature.com/articles/s41598-019-41743-8
67. McKay et al., Atacama four-year meteorology — https://www.astro.uantof.cl/wp-content/uploads/2015/06/McKay_etal_2003_AtacamaMet.pdf
68. Ehlers & Poulsen, Andean uplift vs Atacama aridity — https://www.sciencedirect.com/science/article/abs/pii/S0012821X10000464
69. Liu et al., NAHPs over 800 kyr — https://www.nature.com/articles/s41467-023-41219-4
70. AHP drivers (Comms Earth & Env 2021) — https://preview-www.nature.com/articles/s43247-021-00309-1
71. de Menocal, "End of the African Humid Period" — https://www.whoi.edu/cms/files/demenocal15nat_220285.pdf
72. Shanahan et al., time-transgressive AHP termination — https://www.whoi.edu/cms/files/shanahan12nat_220305.pdf
73. Green Sahara / AHP explainer (Scitable) — https://go.nature.com/2wVEuTB
74. AHP-termination high-latitude trigger (Nat Comms 2018) — http://preview-www.nature.com/articles/s41467-017-01454-y.pdf
75. Koren et al., Bodélé–Amazon dust (ERL 2006) — https://iopscience.iop.org/article/10.1088/1748-9326/1/1/014005/pdf
76. Washington et al., Bodélé tipping element (PNAS) — https://www.pnas.org/doi/10.1073/pnas.0711850106
77. Todd et al., BoDEx 2005 emission measurements — https://doi.org/10.1029/2006jd007170
78. NASA, Bodélé dust / Lake Megachad — https://science.nasa.gov/earth/earth-observatory/bodele-dust-146011/
79. Roberts et al., CLP dust MAR compilation — https://www.sciencedirect.com/science/article/abs/pii/S0277379103001665
80. Chinese loess monsoon record (Sun/An line) — https://www.sciencedirect.com/science/article/abs/pii/S0012825201000435
81. Xu et al., CLP dust seesaw — https://doi.org/10.1130/g45105.1
82. Qiang et al., upwind desert dynamics — https://doi.org/10.3389/feart.2021.661874
83. Bosq et al., European loess ChronoLoess database — https://essd.copernicus.org/articles/15/4689/2023/
84. Anderson, "A theoretical model for aeolian impact ripples" — https://sseh.uchicago.edu/doc/Anderson_MFRP_2012_Sedimentology_1987.pdf
85. Andreotti et al., ripple fully-developed states (PRL 2006) — https://journals.aps.org/prl/abstract/10.1103/PhysRevLett.96.028001
86. Blair, sheetflood vs debris-flow fan dominance — https://doi.org/10.1046/j.1365-3091.1999.00261.x
87. Blair, Warm Spring Canyon debris-flow fan — https://onlinelibrary.wiley.com/doi/10.1046/j.1365-3091.1999.00260.x
88. USGS PP-466, Death Valley fans — https://pubs.usgs.gov/pp/0466/report.pdf
89. Norris et al., Racetrack Playa sailing stones (PLOS ONE) — https://pmc.ncbi.nlm.nih.gov/articles/PMC4146553/
90. Lorenz et al., trail formation observed — http://www.racetrackplaya.org/wp-content/uploads/2014/07/TrailFormationObserved_RD0812.pdf
91. Lake Bonneville history — https://en.wikipedia.org/wiki/Lake_Bonneville
92. Lines, Bonneville Salt Flats hydrology (USGS) — https://doi.org/10.3133/wsp2057
93. Risacher & Fritz, Uyuni geochemical evolution — https://horizon.documentation.ird.fr/exl-doc/pleins_textes/pleins_textes_5/b_fdi_31-32/34857.pdf
94. Sieland, Uyuni hydrogeology (Freiberg) — https://tu-freiberg.de/sites/default/files/2023-08/fog_volume_37.pdf
95. Uyuni paleolake geochronology (Baker et al.) — https://app.ingemmet.gob.pe/biblioteca/pdf/Reg-96.pdf
96. Uyuni salt-crust tomography — https://journals.agh.edu.pl/geol/article/view/223
97. OpenGeology, Deserts chapter — https://opengeology.org/textbook/13-deserts/

---

## Appendix A — Forty further questions

1. **How does grain size sort spatially over a single dune?** Coarser on crests and ripple-covered stoss slopes, finest on the plinths and interdunes — sorting by wind competence over the dune's own topography. *Disprositioned qualitatively; not simulated at voxel scale.*
2. **Why is dune sand so uniformly 0.15–0.30 mm everywhere on Earth?** Saltation is the only mode that builds dunes, and its efficiency peaks in that window; below it, suspension carries sand away; above it, thresholds exceed ordinary winds.
3. **How fast does dune sand redden?** Iron-oxide coatings accumulate over ~10⁵–10⁶ yr of exposure; Rub' al Khali reds are Pliocene-reworked; active coastal dunes are pale. A cheap age cue for generators. *Not quantified in this pass.*
4. **Can zircon U–Pb provenance fingerprint where an erg's sand came from?** Yes — this is how the Bodélé's Amazon connection and the CLP source debates were settled (§7.2). Not needed for a generator.
5. **What sets dune sand roundness?** Aeolian transport rounds grains efficiently (grain-on-grain impacts); fluvial sand is less round. Visible in extreme close-up only.
6. **How deep is the sand body under an erg?** Tens to hundreds of metres — the Grand Erg Oriental's mean spread-out thickness is ~26 m (Wilson's estimate, §4.1); the Rub' al Khali basin fill is kilometres of mostly aeolian-reworked sediment. Surface dunes are the skin of a much bigger reservoir.
7. **Do dune fields have groundwater signatures?** Yes — sabkha interdunes mark the brine table; the Rub' al Khali's NE interdunes are damp at decimetre depth (§9.4).
8. **What is a nebkha?** A coppice dune: sand piled around an individual shrub — the smallest vegetation–aeolian landform, ideal voxel-scale decoration in semiarid transition zones.
9. **How do biocrusts (microbiotic crusts) change sand transport?** Cyanobacterial–lichen–moss crusts raise thresholds by an order of magnitude on mm-thick skins; their loss (trampling, drought) reactivates surfaces. The desert-margin stabilization story in miniature.
10. **What precipitation threshold kills vegetation-stabilized dunes?** Semiarid ~250–500 mm/yr is the parabolic-dune band (§2.6, §10.2); below ~150–250 mm/yr vegetation cannot keep pace with burial and dunes go free.
11. **How do slip-face avalanches actually trigger?** Grainfall deposits oversteepen past ~33–34°, then granular avalanches (with characteristic picket-fence stratification) restore the angle. A frame-scale effect on active dunes only.
12. **What are grainfall vs grainflow strata?** The two lee-face deposit types (airfall sorting vs avalanche mixing); they are how dune internal architecture records wind regime. GPR-relevant, not gameplay-relevant.
13. **What does ground-penetrating radar see inside dunes?** Slip-face migration strata and bounding surfaces; used to reconstruct dune histories and palaeowinds. Skipped — no gameplay path.
14. **How are dune fields dated?** OSL on buried sand grains (last sunlight exposure); the Gran Desierto's five constructional events since ~25 ka were separated this way (§2.2 source 30).
15. **How loud is a dune field?** Booming/singing dunes (a barchan in the Gobi is the classic) produce 70–105 dB sustained tones when sand avalanches — a grain-surface-scale resonance effect. A sound-design freebie.
16. **What is a sand sea's "sediment state"?** The supply (stock) / availability (competence to erode it) / transport-capacity triad of Kocurek & Lancaster — the framework explaining why dune fields turn on and off with climate. Directly implemented in simplified form in §10.
17. **Why do some deserts have no dunes at all?** No sand supply (hammada/reg deserts like the Tademaït plateau) or winds below threshold — aridity alone is insufficient; you need the sediment chain of §4.2.
18. **What is a sand-sheet and how does it differ from a dune field?** Low-amplitude (cm–m) sand cover migrating without slip faces where supply is marginal or the surface is damp/vegetated — >2/3 of the Gran Desierto (§4.1).
19. **What are zibar?** Coarse-grained, low-amplitude, rippled sand sheets that migrate without dunes because their coarse armour defeats dune-building. A distinct texture for marginal zones.
20. **How do dunes interact with roads and canals?** Barchan corridors bury infrastructure at their migration rates (the Moroccan corridors threaten the Laâyoune road) — sand fences and vegetation belts are the engineering answers. Gameplay: obstacle-interaction rules for migrating dunes.
21. **How do dunes respond to sea-level change?** Coastal dune sources turn on/off with shelf exposure; the Empty Quarter's sand partly came from exposed Gulf floors (§4.1, NASA source).
22. **Can deserts self-limit through dust?** Yes — dust radiative effects feed back on climate; the Bodélé is called a tipping element precisely because small circulation changes flip its output (§7.1).
23. **What wind-data products exist for driving generators?** Reanalyses (ERA5) and scatterometer wind roses; Fryberger–Dean drift-potential computed from station data is the classical dune-wind summary. Use any gridded wind rose, not raw mean wind.
24. **How well do mesoscale models (WRF) predict aeolian transport?** At 1–10 km they capture the winds; the threshold/flux parameterizations remain the weak link (factor ~2, §1.4). A generator does not need WRF — a wind rose plus the §10 rules suffices.
25. **What are the desert margins' dust emission hotspots?** Dry lakebeds with fine sediment + topographic wind focusing (Bodélé pattern); anthropogenically, drying seas (Aral) replicate the pattern.
26. **How do playas cycle chemically?** Flooding → evaporative concentration → carbonate → gypsum → halite → bitterns (Mg/K/Li brines) in that order, seasonally at small scale, over lake cycles at basin scale (§8.2, Uyuni).
27. **Where does desert lithium come from?** Volcanic-hosted brines concentrated by evaporite cycling — Uyuni's 7–10 Mt Li (§8.2); the gameplay-facing fact is that "salt flats are young lakes with a mineral history."
28. **Why do desert soils have vesicular Av horizons?** Trapped-air vesicles form in the fine dust mantle on every wetting/drying cycle; they reduce infiltration and generate the pavement's runoff behaviour (§5.2's pavement sources).
29. **What is desertification's actual geomorphic signature?** Not desert advance but localized activation of stabilized dunes plus sheet erosion of thin soils — state changes in the sediment-state framework, not biome translation. §6.5's paleoclimate is the natural experiment.
30. **How does fog sustain fog deserts?** Coastal advective fog (Namib, Atacama) delivers 10s–100s of mm/yr water equivalent to intercepting surfaces; enough for lichens, darkling beetles, and hypolithic ecosystems but not for soil leaching.
31. **What lives under desert pavement?** Hypolithic cyanobacteria under translucent clasts — the Atacama's dry core is too dry even for them (McKay §6.3 source), marking the aridity floor for life. Relevant only to lore, not terrain.
32. **How do phreatophytes structure oasis vegetation?** Deep-rooted species (date palm, tamarisk, mesquite) tap the water table; their transpiration is the visible pump of §9.4's aquifer story.
33. **What is an inland delta?** An alluvial fan or wetland fan building into a terminal sink (Okavango); the permanent-water oasis biome at desert scale.
34. **How strong are desert winds, typically?** Trade-wind dune country: monthly maxima ~15–19 m/s, extremes 130 km/h (Laâyoune records, §2.2 source); Bodélé triggers at >10 m/s (§7.1); Qaidam gales ≥17 m/s on 57–105 days/yr (§5.1).
35. **Are there named regional winds a generator should encode?** Yes — Shamal (Mesopotamia), Harmattan (West Africa, the Bodélé's carrier), Chergui (Morocco, the dune-destabilizing storm wind, §3.3's source), Sirocco, Kharif (Arabian Sea monsoon that builds Rub' al Khali stars, §4.1).
36. **Do dunes exist on other planets, and does the physics transfer?** Yes — Mars (minimum dune size ~600 m from the same ℓ_s scaling, §3.3), Venus, Titan (coarse sand in thick atmosphere), Pluto. The equations of §1 carry over with ρ_air and g swapped; the engine's alien-biome shortcut.
37. **What is the elevation control on deserts?** High basins (Qaidam at 2,800 m — one of the highest deserts, §5.1; Altiplano salars at 3,650 m, §8.2) are cold deserts with intense UV weathering; orographic plateaus add a rain-shadow double downwind.
38. **Are there deserts in the polar regions?** Yes — the Antarctic Dry Valleys (~<50 mm water equivalent, katabatic winds, and the famous polygenetic varnishes and sandstones); the parent's cold-desert axis should include polar deserts as a distinct biome.
39. **What is the loudest honest simplification for a game dune system?** The Werner slab CA at regional scale, run offline (world-gen time), then frozen; §10's analytic rules for placement and appearance; ripples as static texture. Everything else is fidelity you cannot see.
40. **What did the parent's question bank not ask that matters most?** The sediment-state framing (supply/availability/capacity, Q16 above) — it is the unifying control on when any of this machinery turns on, and it maps one-to-one onto a generator's input parameters (§10.1). A desert with no supply is a hammada; a desert with supply and no wind is a sand sheet; only all three make dunes.

---

*End of Part 5. Word count ~7,900 (excluding this note). Anchor disagreements: 2 substantive (AHP pacing 41-kyr→~20-kyr precession; varnish growth range 1–10→1–40 µm/kyr), 1 partial (Washington rain-shadow figures), 2 caveated confirmations (Bagnold table threshold/scaling; migration-rate over-prediction causes), 10 clean confirmations.*


# Part 6 - Vegetation Spawn Rates and Spatial Statistics

**Scope:** the stand level, not the tree level. The companion document `tree-motion-growth-and-appearance.md` answers "what is one tree and how does it move" (LAI 1.25–3.6 for oak, Pipe Model, leaf-count derivation); this document answers "how many are there, how old are they, how clustered, and why." Every number below is a range with a source, or is flagged as an anchor disagreement. Formulas are derived or cited. Section numbering is fixed for merge into the Earth-terrain document as Part 6.

**The motivating question — "what is the spawn rate of trees, and other plants, on the land itself?" — has a real, quantitative answer.** Trees spawn (are established) at densities spanning six orders of magnitude across Earth's biomes, from ~0.1 stems/ha for a commercial mahogany species in Amazonia to >500,000 seedlings/ha in a first-post-fire lodgepole pine flush. The number a generator needs depends entirely on (a) biome, (b) successional age, and (c) the size class counted. Sections 1 and 7 give the tables; Sections 3–4 explain why the placement is never uniform; Section 10 converts all of it into a pipeline.

---

## 1. Tree density by biome and successional stage

### 1.1 The anchor verification

The parent's anchor values were: boreal ~500–2,000 stems/ha, temperate ~200–1,000, tropical ~400–800. Verification results:

- **Boreal 500–2,000: CONFIRMED at the mature-stand level, but the full range is far wider.** Crowther et al.'s (2015) global map ([Crowther 2015 PDF](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)) found the *highest* forest tree densities on Earth in boreal/tundra zones — their boreal biome mean density is ~1,000–2,000+ trees/ha (implied by 0.74 trillion boreal trees over ~700 Mha of boreal forest per [Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)), with dense young boreal regeneration reaching 5,000–10,000+ stems/ha. Madrigal-González et al. (2023), analyzing 3,000+ plots in 23 protected regions, found the highest densities "irrespective of latitude" in plots dominated by *small* trees, with high-productivity plots converging on **≈500–800 trees/ha** for large-tree-dominated stands ([Madrigal-González 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC9839683/)) — a striking cross-biome convergence worth knowing: at high productivity, mature stands of ~500–800 stems/ha are typical *everywhere*.
- **Temperate 200–1,000: CONFIRMED with caveats.** Spies & Franklin's classic Pacific-Northwest data give young Douglas-fir stands **758–1,154 stems/ha**, mature **373–548**, old-growth **394–511** (trees >5 cm dbh) ([Spies & Franklin 1991](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)). Tappeiner et al. found natural old-growth Douglas-fir regenerated at **~100–120 trees/ha** and grew at that density with little self-thinning, while post-harvest young stands exceed 500 stems/ha ([Tappeiner et al. 1997](https://doi.org/10.1139/x97-015)). The Crowther temperate broadleaf biome mean is ~1,000/ha (362.6 billion trees over ~370 Mha implied).
- **Tropical 400–800: PARTIALLY CONFIRMED — the anchor is right for ≥10 cm dbh but misleadingly low as a total.** Lewis et al.'s 260 African plots give a mean of **426 ± 11 stems/ha ≥10 cm dbh** (notably *lower* than Amazonian ~600, a signature structural feature of African forests), with AGB 395.7 Mg/ha ([Lewis et al. 2013](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)). But Schietti et al.'s 55 central-Amazonian 1-ha plots found **2,051–11,475 stems/ha at ≥1 cm dbh** versus 450–1,088 at ≥10 cm — an order of magnitude more small stems hiding under the same canopy ([Schietti et al. 2016](https://doi.org/10.1111/1365-2745.12596)). The ≥10 cm threshold is a *measurement convention*, not an ecological reality; a generator must pick a size cutoff and stay consistent.
- **Anchor not in parent list, but load-bearing: the wet-tropical liana/understory structure.** Lianas reach high loads in gaps and disturbed tropical forest and measurably increase tree mortality ([McDowell et al. 2018](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)), which feeds back to keep tropical stands *denser and smaller-stemmed* than self-thinning alone predicts (Section 2).

### 1.2 Savanna: the rainfall-driven gradient

This is the cleanest density-vs-climate curve in all of ecology, and a generator should hard-code it:

- **Sankaran et al. (2005), 854 sites across Africa:** maximum (99th-quantile) woody cover increases linearly with MAP from ~100 to 650 mm, then saturates. **Trees are typically absent below 101 mm MAP.** The upper-bound line is $\text{Cover}(\%) = 0.14 \cdot \text{MAP} - 14.2$ between 101 and 650 mm MAP; above 650 ± 134 mm MAP, rainfall suffices for canopy closure and *disturbance* (fire, herbivory) becomes necessary to keep a savanna open ([Sankaran et al. 2005](https://edepot.wur.nl/27945)). This single piecewise relationship is the savanna biome-mask rule.
- **Stem density specifically:** the increases in woody cover along the rainfall gradient come **more from bigger crowns than more stems** — across an African 876-site remote-sensing survey, mean crown size rose 233%, crown density only 73%, and cover 491% from the driest (<200 mm) to wettest (1,200–1,400 mm) MAP bins ([Cramer et al. 2017](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)). Crown density spans roughly ~10–100 crowns/ha across that gradient. A southern-African threshold analysis found a biodiversity–biomass threshold at **~180 mature stems/ha** below which competitive interactions collapse ([Loubota Panzou et al. 2020](https://nph.onlinelibrary.wiley.com/doi/10.1111/nph.17639)).
- **The Kalahari sand-sheet transect** (200–1,000 mm MAP on homogeneous deep sands): woody basal area rises **~2.5 m²/ha per 100 mm MAP** above the 200 mm floor; maximum tree height reaches 20 m at ~800 mm; the dominant family shifts from Mimosaceae (<400 mm) to Combretaceae/mopane (400–600 mm) to Caesalpinaceae (>600 mm) ([Scholes et al., Kalahari transect](https://the-eis.com/elibrary/sites/default/files/downloads/literature/Trends%20in%20savanna%20structure%20and%20composition%20along%20an%20aridity%20gradient%20in%20the%20Kalahari.pdf)). Family turnover is a cheap, defensible proxy for species turnover in procedural savannas.

### 1.3 Other biomes

- **Mediterranean shrubland/chaparral:** shrub-dominated, effectively 0 large trees/ha in mature chaparral; woody shrub densities are high (hundreds to thousands per ha, clumped). Crowther's Mediterranean-forest category carries ~53 billion trees globally at low density ([Crowther 2015](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)). Chaparral is now heavily grass-invaded: grass/herb cover is ~34% of historically chaparral land in southern California National Forests, with 24% of chaparral showing >50% herbaceous cover ([Park 2020](https://par.nsf.gov/servlets/purl/10303007)).
- **Mangrove:** stem densities of hundreds to a few thousand per ha (often >1,000 for *Rhizophora* stands); total biomass carbon 14–1,000+ Mg C/ha with Indo-Pacific stands >1,000 Mg C/ha ([Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)). Salinity and flooding duration, not competition, control the density (Section 6).
- **Tropical dry forest:** lower density than moist forest — Pan et al. give 159–199 Mg C/ha and Madrigal-González confirm "seasonal dry tropical and subtropical forests" have relatively low tree density under water scarcity.

### 1.4 Successional stages: the two orders-of-magnitude swings

**Dense regeneration thickets — the parent anchor "10,000–100,000+ stems/ha in young stands" is CONFIRMED and if anything conservative:**

- Post-1988-fire Yellowstone lodgepole pine: mean stand density at 24 years = **21,738 stems/ha, range 0–344,067 stems/ha** ([Turner et al. 2016](https://doi.org/10.1890/15-1585.1)); high-serotiny sites commonly exceed **100,000 stems/ha** ([Schoennagel et al. 2003](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Schoennagel_etal2003_Ecology_FireIntervalSerotiny.pdf)). First-year lodgepole seedling density in the northern Rockies has a median of **25,270 ha⁻¹ and maximum >500,000 ha⁻¹** ([Clark-Wolf et al. 2023](https://par.nsf.gov/servlets/purl/10441749)).
- The 1 cm+ Amazonian understory (Section 1.1) shows the same phenomenon without fire: 2,000–11,000 stems/ha of sub-canopy trees coexisting with the ~600 ≥10 cm stems.
- Dense young temperate stands: 758–1,154 stems/ha in young Douglas-fir (>5 cm dbh — a bigger minimum size, hence lower count).

**Thinning toward old-growth — the parent anchor "old-growth temperate ~50–200 stems/ha of large trees" is CONFIRMED as the count of *large* trees, with an important nuance:**

- Old-growth Douglas-fir: **19 trees/ha ≥100 cm dbh** (with total stand density still 394–511 stems/ha >5 cm dbh, because shade-tolerant hemlock fills the understory) ([Spies & Franklin 1991](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf); [Van Pelt et al. 2015](https://www.mdpi.com/1999-4907/6/9/3177)). Old-growth definition commonly requires ≥20 Douglas-fir/ha older than 200 years, ≥10 large snags/ha, >37 t/ha of large logs.
- Hypermaritime old-growth British Columbia: ~**170 stems/ha >7.5 cm dbh** total, with ~30–70 old trees (>250 yr)/ha, some exceeding 1,000 years ([Hoffman et al. 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)).
- **The nuance (Tappeiner et al.):** natural old-growth did NOT necessarily pass through a dense-thicket phase. Oregon old-growth regenerated at 100–120 stems/ha over a prolonged period and self-thinned little, whereas post-harvest even-aged stands start >500/ha and thin hard. Two legitimate old-growth pathways exist — and a procedural generator that always spawns dense thickets will get post-disturbance forest right and fire-suppressed-origin old-growth wrong.

### 1.5 The consolidated table

| Biome / stage | Typical stems/ha (≥10 cm dbh unless noted) | Extremes with source |
|---|---|---|
| Boreal mature | 500–2,000 | highest densities on Earth in boreal/tundra zones ([Crowther 2015](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)) |
| Temperate young (post-disturbance) | 500–1,500 (>5 cm dbh) | 758–1,154 PNW young ([Spies & Franklin](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)); >500 post-harvest ([Tappeiner](https://doi.org/10.1139/x97-015)) |
| Temperate mature | 300–600 | 373–548 PNW mature |
| Temperate old-growth, large trees | 15–100 (≥50–100 cm dbh) | 19/ha ≥100 cm Douglas-fir; ~170/ha >7.5 cm hypermaritime |
| Temperate old-growth, all sizes | 250–550 | 394–511 (Spies & Franklin) |
| Tropical moist, ≥10 cm | 400–700 | 426 African mean ([Lewis](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)); 450–1,088 Amazonia ([Schietti](https://doi.org/10.1111/1365-2745.12596)) |
| Tropical moist, ≥1 cm | 2,000–11,500 | 2,051–11,475 (Schietti) |
| Tropical, large emergents (≥70 cm) | 0–11 | 1.8/ha mean central Amazonia (Schietti) |
| Savanna, arid (<350 mm MAP) | ~0–5 (trees essentially absent <101 mm) | Sankaran: absent below 101 mm MAP |
| Savanna, mesic (650–1,000 mm) | ~50–300 (crowns) | cover bound 0.14·MAP−14.2 % ([Sankaran](https://edepot.wur.nl/27945)); crowns ~10–100/ha ([Cramer](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)) |
| Mediterranean shrubland | ~0 trees; shrubs hundreds–thousands | grass invasion now 34% of chaparral land ([Park](https://par.nsf.gov/servlets/purl/10303007)) |
| Mangrove | 500–2,000+ | biomass up to >1,000 Mg C/ha ([Pan](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)) |
| Post-fire serotinous conifer flush (yr 1–5) | 5,000–100,000+ | median 25,270, max >500,000 ([Clark-Wolf](https://par.nsf.gov/servlets/purl/10441749)); 0–344,067 at yr 24 ([Turner 2016](https://doi.org/10.1890/15-1585.1)) |
| Glacier-foreland pioneer (yr 0–10) | ~0 trees; <2% vascular cover | ground cover <2% to yr ~20, ~10% by yr 50, ~60% by yr 120+ ([Fickert 2017](https://doi.org/10.5772/intechopen.69479)) |

**The blunt engineering call:** a single "trees per hectare" constant per biome is unsalvageable. The minimum viable model is (biome × size-class × successional-stage) — three axes, with the stage axis worth at least a 10× density swing in fire-adapted and tropical biomes.

---

## 2. The self-thinning law

### 2.1 The law and its modern reassessment

Yoda et al. (1963), from overcrowded pure stands: mean plant mass $\bar{m}$ and surviving density $N$ follow a power law:

$$\boxed{\;\bar{m} = K\,N^{-3/2}\;}\qquad\Longleftrightarrow\qquad \log \bar{m} = \log K - \tfrac{3}{2}\log N$$

Equivalently, in stand-biomass form ($B = N\bar m$): $B \propto N^{-1/2}$ — a log–log plot of biomass against density falls on a line of slope −1/2. The classic geometric interpretation: a plant of mass $m$ linear size $\ell$ with $m \propto \ell^3$ controls ground area $\propto \ell^2$, so $N \propto \ell^{-2} \propto m^{-2/3}$, giving $m \propto N^{-3/2}$. Reineke (1933) had earlier proposed the forestry form $\log N = a - 1.605 \log D$ (quadratic mean diameter), essentially the same law with a diameter exponent near −3/2 after mass–diameter allometry ([West 2026, spruce self-thinning](https://link.springer.com/article/10.1007/s00468-026-02787-2)).

**The modern debate, verified:** Weller (1987) and Lonsdale (1990) re-examined the evidence and found the slope much more variable than claimed. Lonsdale's combined-data slope was **−0.379** (biomass–density form), shallower than −1/2, plausibly because only stem mass is usually measured — and he concluded there was "no evidence at present for a −1/2 power rule" though an ideal slope might exist ([Lonsdale 1990](https://doi.org/10.2307/1938275)). The −4/3 alternative comes from the metabolic-theory/WBE tradition (Enquist, Brown, West): if plant resource use scales as $m^{3/4}$ and resources per area are fixed (energetic equivalence), then $N \propto m^{-3/4}$, i.e. $m \propto N^{-4/3}$. Deng et al.'s work on the metabolic-rate/population-density trade-off found the *ratio* of the metabolic and density exponents stayed near −1 under resource limitation, supporting a generalized energetic-equivalence view with variable individual exponents, rather than fixed −3/2 or −4/3 ([Deng et al. 2008](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.804.4516)). Peters et al. (2018) showed shifting slopes emerge naturally from adaptive plasticity — plant architecture changes along the trajectory, so no single slope should hold for the whole trajectory ([Peters et al. 2018](https://ideas.repec.org/a/eee/ecomod/v390y2018icp1-9.html)). Kikuzawa's synthesis: the slope depends on the height–mass allometry coefficient and dry-matter density; $-3/2$ is one special case ([Kikuzawa 1999](https://doi.org/10.1006/anbo.1998.0782)).

**Reconciliation for a generator (opinionated):** the −3/2 geometric packing argument holds when *light* (one-sided, size-asymmetric competition) is the limiting resource — dense even-aged stands. The −4/3 metabolic argument holds when a *below-ground* resource (water, nutrients — size-symmetric competition) limits; Stoll et al. showed experimentally that reducing size-asymmetry of competition gives more biomass at a given density, i.e. shallower effective slopes ([Stoll et al. 2002](https://doi.org/10.1098/rspb.2002.2137)). And the cross-biome data (Section 1.1) says self-thinning is *more intense under severely limiting conditions* and weakest in high-productivity tropical forest, where dense small-stem packing persists ([Madrigal-González 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC9839683/)). So:

- **Use −3/2 (mass basis) for dense young even-aged stands thinnning toward a closed canopy — light competition.**
- **Use something closer to −4/3 for water/nutrient-limited stands, and expect wide local scatter.**
- **Connect to the Pipe Model** (tree doc §1.2): the tree's sapwood area is proportional to its leaf area; stand-level leaf area saturates (LAI ceiling ~3–6 in closed temperate/tropical canopy, cross-referenced from the tree doc's oak 1.25–3.6), and self-thinning is the demographic consequence of that ceiling — as each surviving tree's pipe cross-section grows, fewer trees fit under a fixed total-leaf-area budget. Self-thinning is the stand-level shadow of the Pipe Model.

### 2.2 Worked example: N(t) for a temperate stand

Take a productive Douglas-fir stand (site index typical of the Oregon Cascades). Use Reineke's diameter form with the −3/2 mass law translated into practice via observed stand trajectories. Real growth data give the anchor points:

- **Year 0 (post-harvest/planting):** 500–1,000 stems/ha planted or seeded ([Tappeiner](https://doi.org/10.1139/x97-015)); natural post-fire lodgepole-type flushes can start at 10,000–100,000 (Section 1.4).
- **Year 30:** dense pole stage, PNW young stands measure 758–1,154 stems/ha (>5 cm dbh) ([Spies & Franklin](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)).
- **Year 80–150 (mature):** 373–548 stems/ha.
- **Year 200–400+ (old-growth):** 394–511 stems/ha total, but only ~19 stems/ha ≥100 cm dbh; the biomass that was in 1,000 small stems now sits in ~50–100 large ones plus understory hemlock.

Now the analytic version. With $\bar m = K N^{-3/2}$ and a mean-mass growth curve for temperate conifer (quadratic mean dbh growing roughly $D_q \approx 0.4\,\text{cm/yr}$ to a 60–80 cm ceiling; $\bar m \approx 0.08 D^{2.5}$ in kg for softwoods — standard allometry): eliminating $\bar m$ between the thinning law and the growth curve gives

$$N(t) = \left(\frac{K}{0.08\,(0.4t)^{2.5}}\right)^{2/3} \;\propto\; t^{-5/3}$$

Check against the data: from year 30 (N≈1,000, D≈12 cm) to year 150 (N≈450, D≈60 cm), the predicted drop is $(150/30)^{-5/3} \approx 9.5\times$-too-fast; the real drop is only ~2.2×. **This is the honest empirical result: individual temperate stands thin far more slowly than the bounding law, because they spend most of their life below the self-thinning boundary.** The law is a *ceiling*, the maximum-size–density line; stands approach it from below and then ride along it ([West 2026](https://link.springer.com/article/10.1007/s00468-026-02787-2) makes exactly this point for dynamic vs. boundary thinning lines). For a generator, use the ceiling to *cap* density given tree size, and use a slower empirical trajectory (like the PNW sequence above) as the default age–density curve. The clean implementation: pick target N₀ at establishment, grow each tree, and kill the smallest tree whenever $N \cdot \bar m > K^{2/3}$-violations of the ceiling occur — a queue, not a formula.

---

## 3. Spatial point processes: where exactly the trees are

### 3.1 The three patterns and their statistics

The null model is **complete spatial randomness (CSR)** — a homogeneous Poisson process of intensity $\lambda = N/A$. Against it, real stands are:

- **Aggregated/clumped** (most seedlings and saplings; most tropical species at some scale),
- **Regular/overdispersed** (mature stands in arid competition zones; plantation-like even-aged stands after thinning),
- **Mixed across scales** — the modern, verified consensus: "spatial patterns of species change depending on the scale of analysis... may follow an aggregated distribution at some scales, random or regular at others" ([MDPI Forests 2024](https://www.mdpi.com/1999-4907/15/4/714)). A Pan-Amazonian inventory study of 15 commercial species found most species aggregated at 0–1,110 m, random for 12 of 15 species at 0–370 m, and even one regular-at-large-scale case — with conspecific densities as low as 0.07–6 trees/ha ([Embrapa, Saracá-Taquera](https://www.alice.cnptia.embrapa.br/alice/bitstream/doc/1182840/1/28140.pdf)).

**Detection statistics** (all standard, all implementable):

- **Clark–Evans index:** $R = \bar d_{obs} / \bar d_{exp}$, ratio of observed mean nearest-neighbor distance to the Poisson expectation $1/(2\sqrt\lambda)$. R < 1 aggregated, R ≈ 1 random, R > 1 regular. Cheap, first-order; only sees nearest neighbors ([reviewed in Velázquez/Silva-navarrete-style summaries](https://link.springer.com/article/10.1186/s13717-021-00314-4)).
- **Ripley's K function** — the workhorse. $K(r)$ = expected number of additional points within distance $r$ of a typical point, divided by $\lambda$:

$$\boxed{\;\hat K(r) = \frac{A}{n^2}\sum_{i\ne j}\frac{I(d_{ij}\le r)\,w_{ij}}{}, \qquad L(r) = \sqrt{K(r)/\pi} - r\;}$$

($A$ plot area, $n$ points, $I$ indicator, $w_{ij}$ edge-correction weight; $L(r)$ is Besag's variance-stabilizing transform.) Under CSR, $K(r) = \pi r^2$ and $L(r) = 0$; $L>0$ aggregation, $L<0$ regularity, at each scale $r$ — this is *the* scale-resolving tool, which Clark–Evans is not ([Ripley 1977](https://doi.org/10.1111/j.2517-6161.1977.tb01615.x); methods summary in [Springer 2021 review](https://link.springer.com/article/10.1186/s13717-021-00314-4)).
- **Pair-correlation function $g(r)$:** the non-cumulative derivative — probability-density of a neighbor at exactly distance $r$. $g>1$ aggregated at that radius; increasingly preferred because K accumulates all scales below $r$ and can smear effects ([MDPI 2024](https://www.mdpi.com/1999-4907/15/4/714)).

**Caveat that matters for interpretation (and for validation suites):** environmental heterogeneity produces *virtual aggregation* — clumping that is habitat preference, not plant–plant interaction. The fix is the heterogeneous-Poisson null model rather than CSR ([Wiegand & Moloney tradition](https://link.springer.com/article/10.1186/s13717-021-00314-4)).

### 3.2 What field data actually shows, by life stage

- **Seedlings/saplings: aggregated, strongly.** Boreal Fennoscandian succession study: *Picea abies* saplings significantly clumped at 1–10 m in all three successional stages; mature trees random-to-clumped; interspecific repulsion between mature spruce and birch — stands organize as **species-specific mosaics** ([Aakala/Kreutz, Silva Fennica](https://www.silvafennica.fi/pdf/1279)).
- **Temperate broadleaf (remote sensing, whole-stand):** all-species pattern random at most scales; *Fagus* and *Betula* clustered; **large trees (>17.5 m) random; small trees clustered** — exactly the scale/stage dependence ([Nuske et al. 2005-era study](https://onlinelibrary.wiley.com/doi/10.1111/j.1654-1103.2005.tb02400.x)).
- **Chinese temperate forest (8 species, juveniles and adults):** all species aggregated at small scales; aggregation *decreases* with scale, ending random; juvenile aggregation at 0–40 m; aggregation significantly reduced when habitat heterogeneity is excluded, concentrating residual aggregation at 0–5/10 m — i.e. dispersal limitation acts at meters, habitat filtering at tens of meters ([MDPI 2024](https://www.mdpi.com/1999-4907/15/4/714)). Dispersal-limitation-driven aggregation correlates with seed mass, wood density, and maximum height ([Arnell et al. 2021](https://onlinelibrary.wiley.com/doi/10.1111/jvs.13070)).
- **Arid/semi-arid savanna:** woody plants are *more* aggregated on drier sites (<400 mm MAP), tied to facilitation and runoff-runon infiltration islands; aggregation measurable via L-function at 1–60 m ([Cramer et al. 2017](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)).

**Conserved pattern (verified repeatedly): conspecific clumping decays with age/size** — juvenile aggregation → adult randomness or mosaic — because distance/density-dependent mortality (Section 4) prunes clumps, and because self-thinning removes clustered small stems. The clumping of juveniles is set by the **seedling shadow** (typically 5–50 m radius, mode near the crown edge), then decays over the stand's life.

### 3.3 Cluster processes for the generator

When Poisson isn't enough (i.e. most of the time for anything young, anything animal-dispersed, anything arid), the standard models are **Neyman–Scott / Thomas cluster processes**: scatter parent points (a Poisson process of intensity $\kappa$), then scatter each parent's offspring as a Gaussian cluster of mean count $\mu$ and spread $\sigma$. The resulting $K$-function has closed form $K(r) = \pi r^2 + \frac{1}{\kappa}\left(1 - e^{-r^2/4\sigma^2}\right)$ — the second term is the aggregation excess, concentrated within ~2σ. Parameters straight from ecology: $\kappa$ = density of seed parents, $\mu$ = mean recruits per parent, $\sigma$ = seedling-shadow radius (Section 4).

**When is Poisson-with-density enough?** Blunt engineering call:

| Situation | Adequate model |
|---|---|
| Mature mixed temperate/tropical stand, all-species pooled | Poisson (verified: "pattern of all tree species combined was random for most scales") |
| Old-growth large trees | Poisson, possibly thinned to mild regularity |
| Any single species, tropical | Thomas cluster (σ ~ 5–40 m) |
| Seedlings/saplings, any biome | Thomas cluster (σ ~ 2–20 m) |
| Arid savanna / desert shrub | Strongly clustered (σ ~ 5–30 m, tied to runon patches) |
| Post-fire serotinous flush | Effectively Poisson at first (seed rain is broad), then self-thinning |
| Plantations / managed even-aged | Regular grid + jitter (this is what our engine currently does everywhere — wrong for everything natural) |

---

## 4. Seed dispersal kernels

The seed dispersal kernel $p(x)$ is the probability density of a seed landing at distance $x$ from the parent. Its **tail** controls colonization speed and the spatial grain of gene flow; its **mode** controls the seedling shadow.

### 4.1 Analytical forms (verified)

- **Wind — WALD (inverse Gaussian), 2D.** Katul et al.'s Wald Analytical Long-distance Dispersal model is a two-parameter inverse Gaussian whose parameters derive *mechanistically* from wind statistics, release height $h_r$, and seed terminal velocity $V_t$ — no fitting to dispersal data needed. Crucially, **WALD's asymptotic tail is a power law with exponent −3/2**, giving unbounded variance — turbulent updrafts make LDD intrinsic, not anomalous: seeds can travel >1 km where a ballistic calculation gives ~20 m ([Katul et al. 2005](https://doi.org/10.1086/432589); [Nathan et al. 2011 review](https://ce.berkeley.edu/sites/default/files/assets/users/thompson/10.%20Nathan%20et%20al%202011.pdf)). Large-eddy simulation confirms the −3/2 tail for light seeds in strong wind ([Bohrer et al. 2008](https://doi.org/10.1111/j.1365-2745.2008.01368.x)).
- **The 2Dt kernel (Clark et al. 1999).** A bivariate Student-t obtained by letting the Gaussian distance parameter itself vary randomly:

$$\boxed{\;p(r) = \frac{1}{2\pi u}\left(1 + \frac{r^2}{2u}\right)^{-3/2}\;}$$

(the 2D radial density with scale parameter $u$). It fits better than Gaussian/exponential for most species across temperate and tropical forests because it is simultaneously convex near the source *and* fat-tailed: less seed beyond 5 m than a Gaussian predicts near the crown, more beyond 30 m ([Clark et al. 1999](https://doi.org/10.1890/0012-9658(1999)080[1475:sdnafp]2.0.co;2)). Note WALD's tail exponent (−3/2) is *shallower* than 2Dt's (−2-class) — Katul et al.'s meta-analysis found measured kernels' exponents bound near the WALD value ([Katul 2005](https://doi.org/10.1086/432589)).
- **Animal dispersal fits inverse-power** forms best; wind-dispersed species fit Gaussian/Student-t — and animal-dispersed species have *longer* mean distances but *lower* fecundities than wind-dispersed ones in the same forest ([Clark, Poulsen et al. 2005, Cameroon](https://doi.org/10.1890/04-1325)). Most seeds, regardless of vector, still fall directly under the parent canopy; events >60 m are a small fraction of the crop but dominate colonization.

### 4.2 Numbers by mode

| Mode | Typical median/mode | Tail | Source |
|---|---|---|---|
| Wind (temperate tree) | ~10–60 m | power-law −3/2 tail; >100 m for light seeds in updrafts; >1 km possible | [Katul 2005](https://doi.org/10.1086/432589), [Nathan 2011](https://ce.berkeley.edu/sites/default/files/assets/users/thompson/10.%20Nathan%20et%20al%202011.pdf) |
| Animal (scatter-hoarding, e.g. jays/rodents) | ~tens of m (tens–hundreds for large vertebrates) | inverse-power; long | [Clark et al. 2005](https://doi.org/10.1890/04-1325) |
| Endozoochory (birds, bats, primates) | tens–hundreds of m | inverse-power | same |
| Ballistic (explosive pods, e.g. *Impatiens*, legumes) | ~1–10 m | thin (near-Gaussian) | standard; consistent with kernel reviews above |
| Water (hydrochory) | along channels, km-scale for buoyant seed | effectively unbounded downstream | [Nathan et al. 2008-era reviews] |

*(Ballistic and water rows carry the weakest specific citations in this document — the 1–10 m ballistic range and km-scale water range are consistent with the kernel literature above but I did not pin a single field-measurement paper; flagging rather than laundering.)*

### 4.3 Janzen–Connell: what spreads trees out again

Janzen (1970) and Connell (1971): seed survival *increases* with distance from the parent (enemies accumulate near adults), while seed density *decreases* — so recruitment peaks at an intermediate distance. Verified status:

- The full hump-shaped **Janzen–Connell pattern is rarer than textbooks suggest.** In a 24-species, 26-year Barro Colorado Island analysis, **Hubbell patterns (monotonic decline of recruit density with distance, but survival increasing) were the most common; true J–C humps were very rare** (non-zero recruit modes near 10 m for only 2 of 24 species); larger-seeded species suffered less distance-dependent mortality ([Marchand et al. 2019](https://doi.org/10.1002/ecy.2926)).
- The *mechanism* is real and measured: insect seed predators, soil pathogens. Whether it produces a hump depends on the ratio of seed dispersal distance to enemy dispersal distance — J–C requires seeds to outrun enemies; when enemies disperse as far as seeds you get McCanny patterns, when comparable, Hubbell ([Nathan & Casagrandi 2004](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00914.x); [Beckman et al. 2012](https://doi.org/10.1111/j.1365-2745.2012.01978.x)). Clumped seed deposition *increases* establishment by satiating predators ([Beckman 2012](https://doi.org/10.1111/j.1365-2745.2012.01978.x)).
- **Net generator rule:** apply a survival multiplier that rises with distance from conspecific adults (saturating by ~20–50 m) and multiply it against the dispersal kernel; the product is your effective recruitment kernel. Expect monotonic-decline-with-longer-tail (Hubbell) for most species, humps only for the animal-dispersed large-seeded ones. This is what *spreads* trees relative to a pure seed-shadow clump — and combined with Section 3.2's age-trend, it is why juvenile aggregation decays into adult mosaics.

---

## 5. Succession: timelines with numbers

**The modern view first:** succession is *assembly with stochasticity*, not a fixed sequence. The 100-year Glacier Bay permanent-plot record shows "stochastic early community assembly and subsequent inhibition have dominated; most species arrived shortly after deglaciation and have remained stable for 50+ years" — **no predictable species sequence or timeline** ([Buma et al. 2019](https://doi.org/10.1002/ecy.2885)). Fastie showed the classic Glacier Bay chronosequence mixes sites with different seed-source distances — up to 58% of variance in early spruce recruitment is distance to seed source ([Fastie 1995](https://doi.org/10.2307/1940722)). **But the time constants are robust** even when the species order is not:

### 5.1 Glacier forelands (primary succession)

- **Years 0–2:** first pioneers appear within **1–8 years** (commonly 1–2) of deglaciation in high-alpine forelands; ground cover <2% to year ~20 ([Fickert 2017](https://doi.org/10.5772/intechopen.69479); [Fickert 2020](https://doi.org/10.3390/d12050191)).
- **Years 20–60:** early-successional stage, ~30 species, cover ~10% at year ~50; dwarf shrubs dominant; **speedup in ground cover after ~half a century** (Fickert).
- **Years 60–160:** later successional; species >40, mean cover ~60%; first conifers appear **~120–150 years after deglaciation** near treeline (Fickert).
- **Years 100–200:** at Glacier Bay: shrub/cottonwood stages at 35–45 yr, spruce dominant >90 yr, near-100% overstory cover >160 yr, western hemlock in understory only >160 yr ([Fastie 1995](https://doi.org/10.2307/1940722)). In Norway, birch woodland develops **within 70 years** below 1,000 m; above ~1,600 m, pioneer vegetation persists essentially unchanged even after 250 years ([Robbins & Matthews 2010](https://doi.org/10.1657/1938-4246-42.3.351)).
- **Old-growth traits: 200+ yr** (Section 1.4's Douglas-fir data; structural leveling "by 400–500 years" per [Spies & Franklin](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)).

The parent's anchor "~10 yr pioneers, ~50–100 yr young forest, ~200+ yr to old-growth traits" is therefore **CONFIRMED as an order-of-magnitude skeleton, with the caveat that it is strongly altitude/latitude-dependent** (70 yr to woodland at low elevation; ≥250 yr and counting at high).

### 5.2 Post-fire (secondary succession)

- **Serotinous conifers: recruitment pulse in the first 1–3 years, full stop.** Lodgepole establishment after the 1988 Yellowstone fires occurred "almost entirely during the first year postfire" and "shapes forest structure and function for centuries" ([Turner et al. 2019 PNAS](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)); northern-Rockies median first-year recruitment 1,292 ha⁻¹ falling to 850 (yr 2) and 417 (yr 3) ([Clark-Wolf 2023](https://par.nsf.gov/servlets/purl/10441749)). Density is set by pre-fire serotiny (nonlinearly, peaking for 70–200-yr fire intervals at low elevation) ([Schoennagel 2003](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Schoennagel_etal2003_Ecology_FireIntervalSerotiny.pdf)).
- **Short-interval reburns collapse the pulse:** 2016 reburns converted stands from a mean 26,700 stems/ha to **6,450 seedlings/ha — a sixfold reduction** — because the canopy seed bank needs 40–70 yr to build ([Turner et al. 2019](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)).
- **Herb/cover recovery:** bare-ground fraction goes from ~1.0 immediately post-fire to 0.63 at 21 months and 0.2 at 33 months in southern California shrubland ([McGuire et al. 2021](https://cw3e.ucsd.edu/wp-content/uploads/2021/04/McGuireetal2021_EEG.pdf)); ground cover ≥60–80% is needed to reduce sediment yield to background (Section 8).

### 5.3 Other disturbance types

- **Post-lava / post-landslide / post-volcanic:** primary succession on the same clock as glacier forelands or slower (no soil, no seed bank; colonization rate set by distance to seed source — Fastie's 58%-variance result is the general law). Mount St. Helens is the canonical modern case: lupine and other nitrogen-fixers engineer the earliest surfaces, analogous to *Dryas*/alder at Glacier Bay (facilitation through N inputs; [Chapin et al. 1994](https://esajournals.onlinelibrary.wiley.com/doi/10.2307/2937039)).
- **Floodplain succession:** typically faster than glacial (soil arrives pre-built): willow/cottonwood on fresh bars within years; these are classic **softwood→hardwood** transitions over 50–200 yr.
- **Old-field:** the fastest — annuals yr 1–3, herbaceous perennials yr 3–20, shrubs yr 10–30, forest canopy 30–100+ yr. (Classic chronosequence literature; treat the *sequence* as stochastic per §5 preamble, the *rates* as robust.)

### 5.4 The time constants that ARE robust

- **Background stem mortality in mature tropical forest: ~1–2%/yr — CONFIRMED.** Pan-tropical best-estimate adjusted stem turnover **1.81 ± 0.16 %/yr** across 65 sites; seven intensively-analyzed stands ranged 0.86–2.02 %/yr mortality ([Baker et al. 2004](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00923.x)). Global synthesis: mean mortality 1.64%/yr vs recruitment 1.62%/yr (not different); tropical mean turnover 1.74%/yr vs temperate 1.19%/yr; within temperate, angiosperm 1.71% > mixed 1.03% > gymnosperm 0.77%/yr; turnover tracks productivity ([Stephenson & van Mantgem-era synthesis](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub3558.pdf)). Amazonian regional mortality ranges 1.3–2.8%/yr, with drought-driven increases ([Esquivel-Muelbert et al. 2020](https://link.springer.com/article/10.1038/s41467-020-18996-3); [McDowell et al. 2018](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)). A generator can use **0.5–1%/yr temperate-boreal, 1–2%/yr tropical, and half the dead trees standing vs broken/uprooted** (Esquivel-Muelbert: 48.4% standing vs 51.2% broken/uprooted pan-Amazonia).

---

## 6. Environmental limits as placement rules (the biome-mask table)

| Limit | Threshold | Notes / source |
|---|---|---|
| **Alpine treeline** | Growing-season mean temperature ≈ **6.7 ± 0.8 °C** (root-zone 10 cm, seasonal mean). Körner's worldwide study: 6.7 °C (±0.8 SD), with temperate/Mediterranean treelines 7–8 °C, equatorial 5–6 °C, subarctic/boreal 6–7 °C ([Körner & Paulsen 2004](https://onlinelibrary.wiley.com/doi/10.1111/j.1365-2699.2003.01043.x); updated network: ~6.5 °C current mean, 5.5–7.5 °C range — [Körner's Basel group](https://duw.unibas.ch/en/koerner/research/high-elevation-treeline/)) | Growing-season *length* ≥ ~3 months required; neither season length, extremes, nor thermal sums predict treeline position globally. Mechanism: crown–air aerodynamic coupling + growth (sink) limitation, not photosynthesis or carbon storage ([Körner 2007](https://www.erdkunde.uni-bonn.de/article/view/2594)). **Parent anchor 6.7 °C: CONFIRMED.** |
| **Maritime vs continental treelines** | Same seasonal mean isotherm, but its elevation climbs **~94 m per 100 km** eastward along a maritime–continental gradient in Central Europe (mass-elevation + continentality) ([Kašpar & Treml 2016](https://doi.org/10.3354/cr01370)) | Implementation: treeline altitude = f(latitude, continentality); oceanic treelines lower and warmer-isotherm, continental higher |
| **Crop-analogue limits** | Date palm/olive northern limits track similar growing-season isotherms (an analogue, not a mechanism — use as sanity check on temperate-subtropical biome masks) | — |
| **Desert tree exclusion** | **Trees essentially absent below ~101 mm MAP** (Sankaran, Africa); desert shrubs persist to ~50–100 mm in runon settings; woody-cover upper bound linear in MAP to 650 mm ([Sankaran 2005](https://edepot.wur.nl/27945)) | **Soil texture dependence (the inverse-texture effect): coarse/sandy soils support MORE woody cover at given low MAP than fine/clay soils** — sand conducts water below the grass rooting zone, and each +1% sand content measurably decouples productivity from precipitation constraint ([Wang et al. 2022](https://iopscience.iop.org/article/10.1088/1748-9326/ac953f)); a global drylands survey found the texture threshold flips at **MAP = 383 mm** — below it, higher water-holding capacity helps shrubs; above it, it hurts ([Eldridge et al. 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11468912/)). Kalahari basal-area gradient: ~2.5 m²/ha per 100 mm MAP above 200 mm ([Scholes](https://the-eis.com/elibrary/sites/default/files/downloads/literature/Trends%20in%20savanna%20structure%20and%20composition%20along%20an%20aridity%20gradient%20in%20the%20Kalahari.pdf)). **Parent anchor "~100–250 mm tree-exclusion": the lower edge (100 mm) is CONFIRMED for Africa; 250 mm is the shrub-dominance threshold, not tree exclusion — a mild disagreement worth encoding as a two-stage rule.** |
| **Swamp/flood oxygen** | Tolerance ordering (flooding duration/depth): submerged-tolerant swamp species (baldcypress, mangroves — months of inundation, lenticels/aerenchyma) > floodplain pioneers (willow/cottonwood — weeks–months) > mesic uplands (days). Mangrove: permanent tidal flooding OK, salinity excludes instead ([Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)) | Peat-swamp forests at <5 dry months/yr; hypermaritime bog woodland shows the stunting endpoint: 15 stems/ha on blanket bog vs 95 on bog woodland ([Hoffman 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)) |
| **Salinity** | Mangroves dominate above ~salinity where tidal flushing maintains nutrients; freshwater swamp species excluded as salinity rises. (Ordering well-established; specific per-species thresholds not pinned in this pass — flagged.) | — |

**The blunt call for the generator:** treeline = a single scalar rule (seasonal-mean 6.7 °C isotherm on your temperature field, raised inland per continentality); desert edge = MAP threshold (101 mm zero-trees; 0.14·MAP−14.2% cover to 650 mm) **modulated by a sand-fraction map** (sandy = +cover in the arid half, −cover in the mesic half, flipping at 383 mm); swamp = a flooding-duration layer with a species-ordered tolerance list.

---

## 7. Non-tree ground cover

| Layer | Biome | Density / cover | Source |
|---|---|---|---|
| Grass tillers, managed temperate sward (tall fescue) | Temperate pasture | **940–2,836 tillers/m²** (measured range across management) | [Scheneiter & Assuero 2010](https://doi.org/10.4067/s0718-16202010000200004) |
| Grass plants, semi-arid African rangeland | Savanna restoration | **10–18 plants/m²**, basal cover 49–73% | [Mligo/Kizinga-era Frontiers 2021](https://www.frontiersin.org/journals/ecology-and-evolution/articles/10.3389/fevo.2020.613835/full) |
| Desert tussock grass (Stipagrostis, Namib escarpment, 120 mm MAP) | Desert | **~2 tussocks/m²**, canopy cover to 50% in wet years | [Wagner et al. 2016](https://doi.org/10.1371/journal.pone.0166743) |
| Shrub density, Mojave/Colorado desert (Ephedra/Larrea) | Desert | sites of 0–166 shrubs each (remote-sensed individuals); order **~100–1,000 shrubs/ha** | [Owen et al. 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11118996/) |
| Chaparral shrubs | Mediterranean | hundreds–thousands/ha, closed canopy; now ~34% grass-invaded | [Park 2020](https://par.nsf.gov/servlets/purl/10303007) |
| Understory cover (PNW conifer forest, all ages) | Temperate conifer | **46–57% total understory cover**; shade-tolerant saplings 84–335/ha | [Spies & Franklin 1991](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf) |
| Shrub-encroached grassland carbon | Savanna (SW China) | vegetation C 0.304 → 1.574 kg/m² as shrub cover 0→10–30% | [Chen et al. 2019](https://www.plant-ecology.com/EN/Y2019/V43/I4/365) |

**The parent anchor "grassland tiller densities ~1,000–10,000/m²" is PARTIALLY CONFIRMED with a real disagreement:** managed swards measure ~1,000–3,000 tillers/m² ([Scheneiter & Assuero](https://doi.org/10.4067/s0718-16202010000200004)) — the lower half of the anchor. The upper anchor (10,000/m²) is plausible for *ungrazed dense lawn-type or stoloniferous* swards (the same source's size–density compensation data and standard pasture ecology), but I did not pin a field measurement at 10,000 for natural grassland; **natural semi-arid rangelands are 2–20 plants/m², two-to-three orders of magnitude sparser than the anchor.** Use 1,000–3,000/m² for lush managed/temperate grass, 10–100/m² for natural dry grassland, ~2/m² for desert tussock grass.

**Moss/lichen:** boreal forest floor and tundra moss-lichen fractions are high (often dominant in ground cover; the northern-Rockies post-fire study measured moss+forb cover as a seedling-facilitation variable with real predictive weight — [Clark-Wolf 2023](https://par.nsf.gov/servlets/purl/10441749)), but I did not pin specific boreal moss-fraction percentages in this research pass — **flagged, not fabricated.** Epiphyte loads in cloud forests: large but not quantified here — flagged.

**Connection to LAI (cross-reference, not re-derivation):** the tree doc's measured oak-forest LAI band is **1.25–3.6**; general biome LAI runs ~1–2 (sparse/dry) to high single digits (dense temperate/tropical). Ground-layer density should be set *by the residual* — what LAI the canopy doesn't capture — rather than independently: savanna LAI 1.27 (savanna peak, dry end) to 6.0 (forest peak) with a *bimodal* distribution above 1,000 mm MAP, i.e. two stable states at the same rainfall ([Yin et al. 2014, West Africa](https://esd.copernicus.org/articles/5/257/2014/esd-5-257-2014.pdf)). That bimodality is a placement rule in itself: above ~1,000 mm MAP in the tropics, a cell is either savanna-like (LAI ~1.3–2.7) *or* forest-like (LAI ~6), rarely in between — pick per-cell with a stochastic flip, not an interpolation.

---

## 8. Vegetation–terrain feedbacks (biotic terrain engineering)

### 8.1 Tree-throw and pit-and-mound microtopography

Wind-thrown trees create pit-and-mound pairs: pits ~0.5–2 m across, mounds of similar footprint and 0.2–0.8 m height (rootwad volumes typically <0.5–several m³). The quantitative anchors:

- **Mound lifetimes: ~2,000+ years.** The oldest still-recognizable tree-throw mound in a Michigan forest is **2,420 years old** (on a 14° slope); a diffusivity of $D = 2.5\times10^{-3}$ m²/yr flattens a 14° mound in ~2,500 years, matching the observation ([Gabet & Mudd 2010](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub), citing Schaetzl & Follmer 1990).
- **Tree-throw contributes 11–18% of total hillslope sediment flux** in the Indiana field area — measurable but not dominant against creep-like processes ([Doane et al. 2021](https://doi.org/10.1029/2021gl094987)). In SE Australia, tree-throw becomes dominant on mid-slopes **only if it occurs at ≥2–3 events ha⁻¹ yr⁻¹** ([Richards et al. 2011](https://doi.org/10.1002/esp.2149)).
- Tree-throw + root fracture produce a **humped soil-production function** (max erosion at intermediate soil thickness) and maintain soil-thickness heterogeneity — the pit-and-mound mosaic *is* the nucleation-point structure for renewed bedrock weathering (Gabet & Mudd 2010).

### 8.2 Creep rates with vs without plants — the order-of-magnitude claim

- Hillslope diffusivity $D$ **increases strongly with moisture among dry sites and less strongly among wet sites; among drier sites $D$ rises from desert → grassland → forest** — the establishment of life substantially accelerates soil creep ([Richardson et al. 2019, Geology](https://doi.org/10.1130/g45305.1)).
- Numbers: forested Oregon Coast Range landscape-scale $D \approx 5\times10^{-3}$ m²/yr (Reneau & Dietrich 1991, quoted in [Gabet & Mudd 2010](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub)); with fire-driven erosion removed, $D \approx 2.5\times10^{-3}$ m²/yr. Bare-soil/arid-site diffusivities are typically 10× smaller (~10⁻⁴–10⁻³ m²/yr across the compiled global dataset — Richardson et al.). **The "order-of-magnitude difference with vs without plants" anchor is CONFIRMED at the dry end and compresses to ~2× among wet, always-vegetated sites.**
- Bioturbation rates by agent (vertical mixing): **ants 0.0045–1.8 mm/yr; earthworms 0.54–10 mm/yr** (compiled by [Gabet et al. 2003](https://doi.org/10.1146/annurev.earth.31.100901.141314)); measured effective soil-reworking rates ~0.028–0.26 mm/yr depending on agent and depth ([ESPL 2020 study](https://www.ovid.com/journals/espl/fulltext/10.1002/esp.4628~bioturbation-and-erosion-rates-along-the-soilhillslope)).

### 8.3 Bank vegetation and rivers

Roots stabilize banks, narrowing channels and permitting meandering; log jams force avulsion and multi-thread plans. The deep-time evidence is the strongest statement available (Section 8.6). For a generator: vegetated-bank channels are ~2–10× narrower and single-thread/meandering; unvegetated reaches braid wide and shallow.

### 8.4 Fire–erosion coupling

- **Post-fire erosion multiplier and recovery:** debris-flow likelihood and volume peak in **post-fire year 1**, drop to **~1/3–1/4 volume by year 2**, and the 15-min rainfall intensity threshold rises from ~15–30 mm/hr (yr 1) to >60 mm/hr by yr 3 ([McGuire et al. 2021](https://doi.org/10.1002/1096-9837(200012)25:13-era summary; direct: EEG 2021](https://cw3e.ucsd.edu/wp-content/uploads/2021/04/McGuireetal2021_EEG.pdf)). Bare-ground fraction: 1.0 → 0.63 (21 mo) → 0.2 (33 mo). **Ground cover of 60–80% is required to reduce sediment yield to background** ([Robichaud et al., ERMiT validation](https://d10.nrfirescience.org/sites/default/files/2025-02/07-2-2-10_jfsp_p07-2-2-10_final_report_pi_robichaud.pdf)). The old rule "an order of magnitude decrease with each post-fire year, background by year 3" holds as a median but large storms in years 4–7 can still produce big yields.
- A *lagged* landslide window exists at **3–5 years** post-fire (infiltration has recovered but roots have decayed; Rice 1982 data: 10 m³/ha unburned, 298 m³/ha at 9 years post-fire) before root strength returns ([Thomas et al. 2020-era Landslides](https://link.springer.com/article/10.1007/s10346-020-01506-3)).
- Long-term share: fire-related processes ≈ **25% of long-term sediment yield** in the western Cascades (Swanson 1981), and **>90% of geologic-time denudation** in some Southwest settings (Orem & Pelletier; both quoted in [McGuire et al. 2016](https://doi.org/10.1002/2016jf003867)).

### 8.5 Beaver meadows

- **Dam densities: <1 to >70 dams per km of stream** (compiled range; [Larsen et al. 2021, Earth-Science Reviews](https://beavertrust.org/wp-content/uploads/2021/06/Larsen_et_al-2021-Earth-Science-Reviews.pdf)); Riding Mountain National Park models **9.5 dams/stream-km** at capacity, median pond area **1,419 m²**, median pond volume ~421 m³ ([Stoll et al. 2024](https://harvest.usask.ca/server/api/core/bitstreams/038f39e4-3681-4aca-8df8-c6322b18a265/content)).
- **Pond/meadow lifecycle:** dams maintained for years–decades; open-water extent up to **9–12× pre-beaver**; ponds fill at **0.26–47 cm/yr** sediment accumulation; **beaver meadows form within ~15 years** of occupation (compiled in [Bradbury et al. 2026](https://doi.org/10.1002/esp.70256)). Active beaver-colony lifespan at a site ~33 years in a Swiss case, sequestering ~10.1 t C/ha/yr long-term — order of magnitude above the counterfactual forest ([Hallberg-Larsen et al. 2026](https://martinezbeavers.org/wp-content/uploads/2026/03/Hallberg-Larsen-et-al.-2026-Beavers-can-convert-stream-corridors-to-persistent-carbon-sinks-Communications-Earth-Environment.pdf)). Pre-European North America: 15–100 million ponds holding 3–50 billion m³ of sediment ([Murray et al. 2023](https://doi.org/10.1029/2022jg007199)).
- Generator rule: valley-bottom cells with low stream power get dam cascades at 1–10 dams/km, pond patches of ~1,000–2,000 m², conversion to wet meadow after ~15–30 years, abandonment, then re-occupation.

### 8.6 Deep time: the Devonian soil/landscape revolution

Before land plants there were **no soils as we know them, no mud in rivers, no meandering channels**: Cambrian–Ordovician landscapes were sheet-braided sand rivers and aeolian tracts, with fines deflated to sea. The Silurian–Devonian evolution of roots → wood → trees (over ~60 Myr) is visibly correlated, step by step, with channelled-braided rivers, meandering rivers, muddy floodplains, fixed narrow channels, vegetated islands, and log jams ([Gibling & Davies 2012, Nature Geoscience](https://web.archive.org/web/20191103013328/https:/www.nature.com/articles/ngeo1376); [Gibling & Davies 2014 synthesis](https://www.sciencedirect.com/science/article/abs/pii/S001678781300120X); [Davies et al. 2021, Svalbard](https://doi.org/10.1144/jgs2020-225)). The mud itself has two verified origins: plant-driven weathering produced it, and — a genuinely fun result — **early bryophyte organics flocculated clay in rivers, raising settling velocities by three orders of magnitude and muddying floodplains *before* deep roots even evolved** ([Zeichner et al. 2021, Science](https://www.science.org/doi/10.1126/science.abd0379); [McMahon & Davies 2018](https://www.science.org/doi/10.1126/science.aan4660)). (A minority view argues the mud/meandering shift predates effective plant engineering — [Santos et al. 2016](https://eprints.whiterose.ac.uk/id/eprint/110462/9/Santos%20et%20al%202016%20JGSL%20reassessing%20the%20impact%20of%20early%20land%20plants%20on%20sedimentation.pdf) — report both.) **Parent anchor CONFIRMED.** For the game: an unvegetated world is a legitimately alien terrain — sandy braided sheets, no soil layer, no floodplain mud — and the engine's biome system would generate it correctly by simply disabling every rule in Sections 1–7.

---

## 9. Global numbers and measurement methods

- **Total tree count: ~3.04 trillion (95% CI ±96 billion)** — Crowther et al. 2015, from **429,775 ground-sourced plot measurements** (trees ≥10 cm dbh) plus 20 remote-sensing/GIS covariates, with trees defined ≥10 cm dbh. Split: ~0.74 T boreal, ~0.66 T temperate, ~1.39 T tropical/subtropical; biome totals table in the paper includes tropical moist 799 B, temperate broadleaf 363 B, boreal 749 B, tundra 95 B ([Crowther 2015](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)). **The anchor "3.0 trillion": CONFIRMED.** Follow-up corrections to note honestly: (a) the number is a modeled extrapolation with per-hectare uncertainty much larger than biome means ("the predictive power of our models is limited at the level of an individual hectare"); (b) the estimate is dominated by the ≥10 cm dbh convention — including smaller stems multiplies it several-fold (Section 1.1's Amazonian ratio); (c) ~15.3 billion trees cut/lost per year, ~46% reduction since human civilization (model-derived, larger error bars). Treat 3.04 T as "the right order of magnitude, defined one particular way."
- **Global forest area: ~4 billion ha** (FAO ~4.06 Gha "forest," being >10% canopy cover, >0.5 ha, >5 m trees — the definition caveats are real: the FAO class includes plantations and fragmented woodland that ecologists would exclude; Pan et al.'s ecological-zone accounting gives 1,354 Mha tropical rainforest + 795 Mha moist deciduous + … summing to a similar ~4 Gha with different sub-totals) ([Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)).
- **Biomass/NPP by biome:** tropical intact live biomass **~152–163 Mg C/ha** (Pan's Table 3: tropical intact live biomass 152.1 Mg C/ha in 2007 vs boreal 46.7 and temperate 60.7 — i.e. tropical ~2.5–3× the per-area live biomass of extratropical forests); temperate rainforests are the per-area champions at **500–2,500 Mg/ha** total biomass with a record eucalypt stand at 2,844 Mg C/ha; African moist forest AGB mean **395.7 Mg dry/ha** ([Lewis 2013](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)); central Amazonian stands 124–346 Mg/ha ([Schietti](https://doi.org/10.1111/1365-2745.12596)). **Desert <5 Mg/ha aboveground — consistent with the anchor** (deserts carry 53 billion trees over ~2 Gha-class area = well under 5 Mg C/ha; the savanna bimodality paper's savanna-peak biomass 1.10–3.56 kg C/m² = 11–36 Mg/ha confirms the dry end of the gradient). Forests hold ~92% of all terrestrial biomass; necromass is 58% of total forest ecosystem carbon (Pan).
- **How measured — one honest paragraph on error bars.** Everything above rests on three instrument classes with genuinely different failure modes. (1) **Plot networks** (FIA ~300k US plots; RAINFOR/ForestGEO tropical plots; Crowther's 430k compilations): direct, but tiny sampling fractions — a 1-ha plot is 10⁻⁸ of the Amazon — and they cluster near roads; tropical stem-density estimates for ≥10 cm stems are good to ~±10%, but the ≥1 cm class doubles-to-quintuples counts with site-level scatter of ±50%. (2) **Passive optical remote sensing** (MODIS LAI/canopy cover, Hansen forest cover): global and cheap, but saturates above ~LAI 4–5, cannot see understory, and its "forest" is a cover-fraction not a stem count — the savanna/forest bimodality analysis had to use two independent products to check itself. (3) **LiDAR** (airborne; GEDI spaceborne; GLAS before it): the only thing that measures vertical structure, hence biomass with ~±15–20% at stand scale, but coverage is sampled transects, not wall-to-wall, and calibration depends on the same limited field plots. The composite result: **biome-mean densities are trustworthy to a factor ~1.3; hectare-level values are trustworthy to a factor ~2–3; and any single headline number (3.04 T) carries a conventions-dependence larger than its formal confidence interval.** A game engine's tolerance is comfortably looser than all of this.

---

## 10. Practical simulation synthesis

### 10.1 What the engine does now

`world/generation/src/tree_placement.cpp` places trees on a **jittered grid**: 8 m cells, ~37% of cells host a candidate (pre-mask), jittered within [2, cell−2) so spacing ≥4 m holds by construction, then masked by height band (`kTreeMinHeight`–`kTreeMaxHeight`) and local slope (`kTreeMaxSlope`), with shape/size/jitter selected from hash bits. What this gets right: determinism, cheapness, slope and altitude masking, guaranteed spacing. What it gets wrong, measured against Sections 1–9:

1. **Density is one global constant** — no biome, no climate, no age axis. Real density spans 0.1–500,000 stems/ha (Section 1). The current ~0.37 trees/64 m² ≈ **58 trees/ha** is a plausible *temperate woodland* number but is applied to every land surface, including what should be desert, boreal, and closed forest.
2. **Uniform spacing is the single most unrealistic choice available.** Jittered-grid is the plantation/orchard model. Real stands: clumped (Thomas cluster, Section 3) for juveniles/single-species/arid; Poisson for pooled mature; the current layout is *more regular than any natural forest ever measured* — its Clark–Evans R would exceed 1 by construction.
3. **No age structure** — all trees are 4–7 m trunks. Real stands mix 300-year emergents with 5-year saplings at ratios of ~1:50+ (Section 1.4); this is the difference between a forest and a tree farm.
4. **No climate fields** — height/slope masking is a crude proxy for treeline (Section 6 shows the real rule is a 6.7 °C seasonal isotherm, not a raw altitude), and there is no moisture axis at all (Section 6: MAP + soil texture are the dominant desert/savanna controls).
5. **No ground layer** — shrubs exist only as a tree-shape variant at tree spacing; grass/forb/moss layers (Section 7) are absent.
6. **Species mix is 3 shapes at fixed ratios** — no conspecific clustering (Section 3), no Janzen–Connell spacing (Section 4).

### 10.2 The minimum fix (highest realism-per-line-of-code, in order)

1. **Drive density from two climate fields.** Even a hand-authored temperature+moisture pair, or better: derive MAP and seasonal-mean temperature from the existing heightmap/noise. Then: trees/ha = f(MAP, T) using Section 6's thresholds and Section 1's table — zero below 101 mm MAP or the 6.7 °C isotherm; 0.14·MAP−14.2% cover (converted to stems) in the arid-savanna band; biome-table values elsewhere. This one change replaces the 37%-of-cells constant with the real six-order-of-magnitude gradient, and makes deserts actually empty — currently the engine's most visible ecological lie.
2. **Replace the jittered grid with per-species Thomas clusters.** Keep the 8 m candidate grid (it's a fine sampler), but accept candidates through a cluster field: generate cluster centers (Poisson, intensity κ from the species' adult density), then accept each candidate with probability from a Gaussian bump of radius σ around the nearest center. σ = 5–20 m for most species, ~2–5 m for ballistic dispersers, tens of meters for wind. Instant clumping; Clark–Evans drops below 1 where it should.
3. **Add a two-class age mixture.** For each cluster, with probability p_old (0.1–0.3), place one "veteran" (tall, wide, from the existing conifer/round params scaled 2–3×) plus a surrounding cohort of small stems; else a even-cohort of mid-size stems. This fakes Sections 1.4 and 5 (old-growth vs. stand-initiation structure) with one extra branch on the shape selector — and it is the single biggest *visual* win available, because varied height is what reads as "natural" to the eye.
4. **Add a moisture-modulated ground layer.** Grass cover fraction = g(MAP) with the bimodal tropical flip (Section 7's LAI note); shrubs as clustered small instances at Section 7 densities in the arid half. If only one thing: make grass cover track the moisture field so dry hillsides read dry.
5. **Fire scars and succession states (optional, cheap version).** A per-region "time since disturbance" field: young cells get the flush densities of Section 1.4 (scaled down for playability), old cells the sparse-large-tree profile. Even a two-state {recently burned, mature} bitmask driven by existing noise adds the dominant source of real-world spatial heterogeneity.

**What to fake, honestly:** individual seed dispersal (use the cluster process, not a kernel simulation); Janzen–Connell dynamics (implicitly captured by using cluster-decay-with-age in the age mixture); self-thinning as a process (precompute the age–density curve from Section 2.2's anchors and interpolate); soil-texture modulation in the first pass (add the 383 mm sand-flip rule only if a soil map ever exists); beaver meadows, tree-throw microtopography, and Devonian mode (pure set-dressing material for later — Sections 8.5, 8.1, 8.6 respectively — high charm, low systems value).

**What NOT to fake:** the treeline (it's a scalar rule, Section 6 — altitude-only masking is quantitatively wrong and trivially fixable); the desert (empty is correct and free); the clumping (one Gaussian bump is one line of probability).

### 10.3 Validation

Cheap acceptance tests against this document's numbers, for the placement system: (1) per-biome stems/ha within the Section 1 table's ranges; (2) Clark–Evans R < 1 for single species, ≈1 for all-species pooled (Section 3.2); (3) L(r) from Poisson within confidence bands for pooled mature stands; (4) nearest-neighbor spacing distribution log-normal-ish, never hard-capped at exactly 4 m (the current cap is detectable in a histogram and would fail any Ripley's K test at r = 4–8 m with a visible regularity dip).

---

## Provenance

Written 2026-XX-XX (subagent for the Earth-terrain merged document, Part 6). Sources: live web search (Exa MCP) — 8 search batches, ~26 queries; every numeric claim carries an inline link to its source. Anchor-disagreement discipline: the parent's "?"-marked anchors were each verified and are marked above as CONFIRMED / PARTIALLY CONFIRMED / with-disagreement; the disagreements found (temperate old-growth nuance, tiller-density upper bound, 250-mm "tree exclusion" vs 101 mm, self-thinning worked-example slope, J–C hump rarity, Crowther conventions) are stated with both values, not silently resolved. Ballistic/water dispersal distances, boreal moss fractions, cloud-forest epiphyte loads, and salinity thresholds carry explicit flags rather than fabricated citations. Cross-references: `tree-motion-growth-and-appearance.md` (LAI 1.25–3.6 oak; Pipe Model §1.2 — used in §2.1, not re-derived), `water-physics-and-wave-simulation.md` (house style), `_terrain_question_bank.md` §F (questions 59–68, all covered in sections 1–9 above).

## Consolidated source list

1. [Crowther et al. 2015 — Mapping tree density at a global scale (PDF)](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)
2. [Pan et al. 2013 — The Structure, Distribution, and Biomass of the World's Forests](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)
3. [Madrigal-González et al. 2023 — Global patterns of tree density (PMC)](https://pmc.ncbi.nlm.nih.gov/articles/PMC9839683/)
4. [Lewis et al. 2013 — Above-ground biomass and structure of 260 African tropical forests](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)
5. [Schietti et al. 2016 — Forest structure along a 600 km Amazonian transect](https://doi.org/10.1111/1365-2745.12596)
6. [Sankaran et al. 2005 — Determinants of woody cover in African savannas](https://edepot.wur.nl/27945)
7. [Cramer et al. 2017 — Patterns in woody vegetation structure across African savannas](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)
8. [Yin et al. 2014 — Bimodality of woody cover and biomass across the precipitation gradient in West Africa](https://esd.copernicus.org/articles/5/257/2014/esd-5-257-2014.pdf)
9. [Loubota Panzou et al. 2020 — Structural diversity and tree density, southern African woodlands](https://nph.onlinelibrary.wiley.com/doi/10.1111/nph.17639)
10. [Scholes et al. — Kalahari aridity-gradient transect](https://the-eis.com/elibrary/sites/default/files/downloads/literature/Trends%20in%20savanna%20structure%20and%20composition%20along%20an%20aridity%20gradient%20in%20the%20Kalahari.pdf)
11. [Park 2020/2022 — Grass invasion into chaparral shrublands](https://par.nsf.gov/servlets/purl/10303007)
12. [Spies & Franklin 1991 — Structure of natural young, mature, and old-growth Douglas-fir forests](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)
13. [Tappeiner et al. 1997 — Density, ages, and growth rates in old-growth and young-growth forests, coastal Oregon](https://doi.org/10.1139/x97-015)
14. [Van Pelt et al. 2015 — Variability of stand structures in old-growth PNW forests](https://www.mdpi.com/1999-4907/6/9/3177)
15. [Hoffman et al. 2021 — Old-growth structure in hypermaritime rainforest, BC](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)
16. [Freund et al. 2015 — Structure of early old-growth Douglas-fir forests](https://www.fs.usda.gov/sites/nfs/files/legacy-media/r06/2%20Freund_Franklin_and_Lutz_2015_Structure_of_early_old-growth_Douglas-fir_forests_in_the_PNW.pdf)
17. [Lonsdale 1990 — The Self-Thinning Rule: Dead or Alive?](https://doi.org/10.2307/1938275)
18. [Pickard 1983 — Three interpretations of the self-thinning rule](https://doi.org/10.1093/oxfordjournals.aob.a086526)
19. [Deng et al. 2008 — Trade-offs between metabolic rate and population density of plants](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.804.4516)
20. [Kikuzawa 1999 — Theoretical relationships between mean plant size, size distribution and self-thinning](https://doi.org/10.1006/anbo.1998.0782)
21. [Stoll et al. 2002 — Size symmetry of competition alters biomass–density relationships](https://doi.org/10.1098/rspb.2002.2137)
22. [Peters et al. 2018 — A new mechanistic theory of self-thinning](https://ideas.repec.org/a/eee/ecomod/v390y2018icp1-9.html)
23. [West et al. 2026 — Dynamic self-thinning lines in Canadian interior spruce](https://link.springer.com/article/10.1007/s00468-026-02787-2)
24. [Wright et al. 2021-era review — plant density responses / constant final yield](https://onlinelibrary.wiley.com/doi/10.1111/pce.13968)
25. [Spatial point-pattern analysis review (Springer 2021)](https://link.springer.com/article/10.1186/s13717-021-00314-4)
26. [Embrapa — Ripley's K of 15 commercial Amazonian species](https://www.alice.cnptia.embrapa.br/alice/bitstream/doc/1182840/1/28140.pdf)
27. [Aakala (Kreutz) — Spatial tree community structure across boreal succession, Silva Fennica](https://www.silvafennica.fi/pdf/1279)
28. [Nuske-era — Spatial relationships between tree species and gaps, broad-leaved woodland](https://onlinelibrary.wiley.com/doi/10.1111/j.1654-1103.2005.tb02400.x)
29. [Arnell et al. 2021 — Fine-scale tree spatial patterns shaped by dispersal limitation](https://onlinelibrary.wiley.com/doi/10.1111/jvs.13070)
30. [MDPI Forests 2024 — Spatial patterns and associations of tree species (scale dependence)](https://www.mdpi.com/1999-4907/15/4/714)
31. [Katul et al. 2005 — WALD mechanistic long-distance wind dispersal](https://doi.org/10.1086/432589)
32. [Nathan et al. 2011 — Mechanistic models of seed dispersal by wind (review)](https://ce.berkeley.edu/sites/default/files/assets/users/thompson/10.%20Nathan%20et%20al%202011.pdf)
33. [Bohrer et al. 2008 — Canopy heterogeneity and wind-driven dispersal kernels](https://doi.org/10.1111/j.1365-2745.2008.01368.x)
34. [Clark et al. 1999 — Seed dispersal near and far: the 2Dt kernel](https://doi.org/10.1890/0012-9658(1999)080[1475:sdnafp]2.0.co;2)
35. [Clark, Poulsen et al. 2005 — Comparative seed shadows of bird-, monkey-, and wind-dispersed trees](https://doi.org/10.1890/04-1325)
36. [Marchand et al. 2019 — Distance-dependent mortality at Barro Colorado Island](https://doi.org/10.1002/ecy.2926)
37. [Nathan & Casagrandi 2004 — Janzen-Connell and beyond](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00914.x)
38. [Beckman et al. 2012 — Clumped seed dispersal and density-dependent mortality](https://doi.org/10.1111/j.1365-2745.2012.01978.x)
39. [Buma et al. 2019 — 100 yr of primary succession at Glacier Bay](https://doi.org/10.1002/ecy.2885)
40. [Fastie 1995 — Multiple pathways of primary succession at Glacier Bay](https://doi.org/10.2307/1940722)
41. [Chapin et al. 1994 — Mechanisms of primary succession following deglaciation](https://esajournals.onlinelibrary.wiley.com/doi/10.2307/2937039)
42. [Fickert 2017 — Glacier forelands: field laboratories for primary succession](https://doi.org/10.5772/intechopen.69479)
43. [Fickert 2020 — Common patterns and diverging trajectories, Eastern Alpine glacier forelands](https://doi.org/10.3390/d12050191)
44. [Robbins & Matthews 2010 — Successional trajectories, Norwegian glacier forelands](https://doi.org/10.1657/1938-4246-42.3.351)
45. [Schoennagel et al. 2003 — Fire interval and serotiny, postfire lodgepole pine density](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Schoennagel_etal2003_Ecology_FireIntervalSerotiny.pdf)
46. [Turner et al. 2016 — 24 years after the Yellowstone fires](https://doi.org/10.1890/15-1585.1)
47. [Turner et al. 2019 — Short-interval severe fire erodes subalpine forest resilience (SI)](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)
48. [Clark-Wolf et al. 2023 — Conifer seedling demography, northern Rocky Mountains](https://par.nsf.gov/servlets/purl/10441749)
49. [Hansen-era / Hansen et al. 2018 — Regeneration failure of two subalpine conifers](https://www.caryinstitute.org/sites/default/files/public/reprints/hansen_etal_2018_ecology.pdf)
50. [Baker et al. 2004 — Tropical forest turnover rates when census intervals vary](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00923.x)
51. [Stephenson-era global turnover synthesis (pub3558)](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub3558.pdf)
52. [McDowell et al. 2018 — Drivers and mechanisms of tree mortality in moist tropical forests](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)
53. [Esquivel-Muelbert et al. 2020 — Tree mode of death and mortality risk across Amazon forests](https://link.springer.com/article/10.1038/s41467-020-18996-3)
54. [Körner & Paulsen 2004 — A world-wide study of high altitude treeline temperatures](https://onlinelibrary.wiley.com/doi/10.1111/j.1365-2699.2003.01043.x)
55. [Körner — High elevation treeline research page (Basel)](https://duw.unibas.ch/en/koerner/research/high-elevation-treeline/)
56. [Körner 2007 — Climatic treelines: conventions, global patterns, causes](https://www.erdkunde.uni-bonn.de/article/view/2594)
57. [Kašpar & Treml 2016 — Thermal characteristics of alpine treelines, Central Europe](https://doi.org/10.3354/cr01370)
58. [Eldridge et al. 2024 — Drivers of woody dominance across global drylands](https://pmc.ncbi.nlm.nih.gov/articles/PMC11468912/)
59. [Wang et al. 2022 — Soil coarsening alleviates precipitation constraint in global drylands](https://iopscience.iop.org/article/10.1088/1748-9326/ac953f)
60. [Nature Comm. 2024 — Vegetation resistance crossing aridity thresholds](https://www.nature.com/articles/s43247-024-01546-w)
61. [Scheneiter & Assuero 2010 — Tiller population density, tall fescue and prairie grass swards](https://doi.org/10.4067/s0718-16202010000200004)
62. [Wagner et al. 2016 — Herbaceous legume encroachment, Stipagrostis tussock density](https://doi.org/10.1371/journal.pone.0166743)
63. [Owen et al. 2024 — Native shrub densities, burrow co-occurrence, California desert](https://pmc.ncbi.nlm.nih.gov/articles/PMC11118996/)
64. [Frontiers 2021 — Morphoecological traits of African rangeland grasses](https://www.frontiersin.org/journals/ecology-and-evolution/articles/10.3389/fevo.2020.613835/full)
65. [Chen et al. 2019 — Shrub coverage effects on grassland carbon, SW China](https://www.plant-ecology.com/EN/Y2019/V43/I4/365)
66. [Gabet et al. 2003 — The effects of bioturbation on soil processes and sediment transport](https://doi.org/10.1146/annurev.earth.31.100901.141314)
67. [Gabet & Mudd 2010 — Bedrock erosion by root fracture and tree throw](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub)
68. [Doane et al. 2021 — Topographic roughness on forested hillslopes: tree-throw flux](https://doi.org/10.1029/2021gl094987)
69. [Richardson et al. 2019 — Influences of climate and life on hillslope sediment transport](https://doi.org/10.1130/g45305.1)
70. [Richards et al. 2011 — Bioturbation on a SE Australian hillslope](https://doi.org/10.1002/esp.2149)
71. [ESPL 2020 — Bioturbation and erosion rates along a soil catena](https://www.ovid.com/journals/espl/fulltext/10.1002/esp.4628~bioturbation-and-erosion-rates-along-the-soilhillslope)
72. [Gabet 2000 — Gopher bioturbation: non-linear hillslope diffusion](https://doi.org/10.1002/1096-9837(200012)25:13)
73. [McGuire et al. 2016 — Raindrop- vs flow-driven postwildfire sediment transport](https://doi.org/10.1002/2016jf003867)
74. [McGuire et al. 2021 — Time since burning and post-fire debris-flow initiation](https://cw3e.ucsd.edu/wp-content/uploads/2021/04/McGuireetal2021_EEG.pdf)
75. [Robichaud et al. — ERMiT post-fire erosion model validation](https://d10.nrfirescience.org/sites/default/files/2025-02/07-2-2-10_jfsp_p07-2-2-10_final_report_pi_robichaud.pdf)
76. [USDA RMRS GTR-240 — Post-fire treatment effectiveness for hillslope stabilization](https://d10.nrfirescience.org/sites/default/files/2023-09/rmrs_gtr240.pdf)
77. [Thomas et al. 2020-era — Landslides after wildfire (Landslides journal)](https://link.springer.com/article/10.1007/s10346-020-01506-3)
78. [Graber et al. 2026 — Multi-year postfire debris-flow hazard with recovery](https://doi.org/10.1130/ges02936.1)
79. [Larsen et al. 2021 — Beaver influences on river corridor hydrology/geomorphology (Earth-Science Reviews)](https://beavertrust.org/wp-content/uploads/2021/06/Larsen_et_al-2021-Earth-Science-Reviews.pdf)
80. [Stoll et al. 2024 — Beaver dam capacity of Canada's boreal plain](https://harvest.usask.ca/server/api/core/bitstreams/038f39e4-3681-4aca-8df8-c6322b18a265/content)
81. [Laurel & Wohl 2019 — Persistence of beaver-induced geomorphic heterogeneity and carbon stock](https://www.beaverinstitute.org/wp-content/uploads/2023/03/Laurel-and-Wohl-The-persistence-of-beaver-induced-geomorphic-heterogeneity-and-organic-carbon-stock-in-river-corridors-ESPL-20I9.pdf)
82. [Murray et al. 2023 — Beaver pond geomorphology and nitrogen retention](https://doi.org/10.1029/2022jg007199)
83. [Bradbury et al. 2026 — Sub-annual sediment dynamics in beaver ponds](https://doi.org/10.1002/esp.70256)
84. [Hallberg-Larsen et al. 2026 — Beavers convert stream corridors to persistent carbon sinks](https://martinezbeavers.org/wp-content/uploads/2026/03/Hallberg-Larsen-et-al.-2026-Beavers-can-convert-stream-corridors-to-persistent-carbon-sinks-Communications-Earth-Environment.pdf)
85. [Gibling & Davies 2012 — Palaeozoic landscapes shaped by plant evolution](https://web.archive.org/web/20191103013328/https:/www.nature.com/articles/ngeo1376)
86. [Gibling & Davies 2014 — Palaeozoic co-evolution of rivers and vegetation](https://www.sciencedirect.com/science/article/abs/pii/S001678781300120X)
87. [Davies et al. 2021 — The Devonian landscape factory, Svalbard](https://doi.org/10.1144/jgs2020-225)
88. [Zeichner et al. 2021 — Early plant organics increased global terrestrial mud deposition](https://www.science.org/doi/10.1126/science.abd0379)
89. [McMahon & Davies 2018 — Evolution of alluvial mudrock forced by early land plants](https://www.science.org/doi/10.1126/science.aan4660)
90. [Santos et al. 2016 — Reassessing the impacts of early land plants on sedimentation](https://eprints.whiterose.ac.uk/id/eprint/110462/9/Santos%20et%20al%202016%20JGSL%20reassessing%20the%20impact%20of%20early%20land%20plants%20on%20sedimentation.pdf)

---

## Appendix A — Forty further questions

**Density and structure**
1. **How does stem density vary with plot size (the perimeter effect)?** Small plots inflate density because trees near edges get counted; Madrigal-González found plot size *not* supported as a determinant in their BIC model, but tropical 1-ha vs 0.1-ha counts differ systematically. *Disposition: use consistent plot windows when validating; not a first-order game concern.*
2. **What is the stem density of tree ferns and palms in tropical montane forest?** Not pinned; they follow different allometry (no secondary thickening) and can dominate thousands per ha. Flagged.
3. **How does plantation forestry density compare (e.g. pine at 1,100–2,500 stems/ha at planting)?** Standard silviculture; roughly the same as natural young-stand densities. *Dispositioned from general forestry practice, not pinned.*
4. **What's the density response to thinning treatments?** Variable-density thinning deliberately recreates clumping; relevant as "authored" landscapes. Not pinned.
5. **Do lianas have a measurable stem density of their own?** Yes — liana loading classes 0–4 are used as mortality predictors ([McDowell 2018](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)); absolute densities not pinned.

**Spatial statistics**
6. **What's the typical lag distance at which tropical conspecific aggregation saturates?** ~10–40 m from the Chinese-temperate and Amazonian data (Section 3.2); tropical BCI-scale studies give tens of meters. Use 10–40 m.
7. **How do you edge-correct Ripley's K on a finite game chunk?** Isotropic/translation edge corrections (Ripley 1977) or guard zones; for validation only, not runtime.
8. **What is the pair-correlation signature of a jittered grid?** A regularity dip at the grid spacing (4–8 m in our engine) — exactly the tell identified in §10.3.
9. **Are shrub patterns (vs tree patterns) more or less aggregated?** More, in drylands — tied to runon/facilitation islands ([Cramer 2017](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)).
10. **Does aggregation increase or decrease with elevation?** Not pinned; high-elevation stands often track microtopography (patchy safe sites), suggesting increase. Flagged.

**Dispersal**
11. **What seed terminal velocities do common game-relevant species have?** ~0.3–1.5 m/s for plumed/winged samaras; >5 m/s for heavy nuts. Consistent with WALD's parameter ranges but not tabulated here. Flagged as per-species art-direction data.
12. **How far can a single storm transport seeds?** Kilometers — WALD's unbounded-variance tail is the point ([Katul 2005](https://doi.org/10.1086/432589)).
13. **What is secondary dispersal (seed moved again after landing)?** Common — water-rewetting, ants, rodents; not pinned. Flagged.
14. **How does clumped seed deposition interact with J–C mortality?** Clumping *helps* by satiating predators ([Beckman 2012](https://doi.org/10.1111/j.1365-2745.2012.01978.x)) — the non-obvious sign.
15. **What fraction of seeds travel beyond 100 m?** Small (most fall under the parent — [Clark 2005](https://doi.org/10.1890/04-1325)) but dominates spread rate; 2Dt puts more >30 m than Gaussian ([Clark 1999](https://doi.org/10.1890/0012-9658(1999)080[1475:sdnafp]2.0.co;2)).

**Succession**
16. **What is the classic climax vs. individualistic (Gleasonian) resolution?** Modern view: assembly rules with stochasticity; no fixed climax ([Buma 2019](https://doi.org/10.1002/ecy.2885)). Answered in §5.
17. **How fast does soil develop on fresh substrate?** Glacier Bay: ~10× soil organic matter increase over 200 yr, N accumulation 2.3–3.6 g/m²/yr under alder ([Fastie 1995](https://doi.org/10.2307/1940722)).
18. **What triggers gap-phase vs. stand-replacing dynamics?** Severity of the disturbance and shade-tolerance of the dominant; PNW data shows both pathways coexist regionally (Section 1.4).
19. **How long do snags stand?** Rot-resistant species (cedar) stand centuries — 90% of standing dead in hypermaritime BC were redcedar/yellow-cedar ([Hoffman 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)).
20. **What is the successional role of N-fixers?** Facilitative for spruce growth but reduces spruce stand density (competition); nets out ambiguous ([Fastie 1995](https://doi.org/10.2307/1940722); [Chapin 1994](https://esajournals.onlinelibrary.wiley.com/doi/10.2307/2937039)).
21. **How does post-fire recruitment depend on distance to live edge?** Sharply — most regeneration within 150 m of unburned edge for non-serotinous species ([Hansen 2018](https://www.caryinstitute.org/sites/default/files/public/reprints/hansen_etal_2018_ecology.pdf)).
22. **What happens on repeated short-interval fire?** Sixfold+ recruitment collapse (Section 5.2; [Turner 2019](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)).
23. **What is nurse-plant facilitation and when does it flip to competition?** Shrubs facilitate seedlings in semiarid settings (Section 5.2's forb-cover effect); competition dominates in mesic zones. The stress-gradient hypothesis; not pinned further.

**Environmental limits**
24. **Why 6.7 °C and not the old 10 °C warmest-month rule?** Seasonal mean tracks treeline globally; warmest-month fails (Körner, Section 6). The old rule was a regional coincidence.
25. **What sets the polar (arctic) treeline vs the alpine one?** Same isotherm, near sea level in the Arctic ([Körner 2007](https://www.erdkunde.uni-bonn.de/article/view/2594)).
26. **Do treelines move with warming, and how fast?** Yes — rapid advances documented ([Körner group 2024 citations](https://duw.unibas.ch/en/koerner/research/high-elevation-treeline/)); rates not pinned here.
27. **What is the flooding-tolerance mechanism?** Aerenchyma/lenticels transporting oxygen; not pinned further — ordering in §6 is the usable result.
28. **What salinity kills which species?** Flagged in §6; per-species thresholds not pinned.
29. **What is a muskeg/palsa peatland tree density?** Blanket bog: ~15 stems/ha ([Hoffman 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)).
30. **Does aspect matter at treeline?** Yes — snowpack and radiation load shift the local limit by tens of meters; the central-European study needed whole-season metrics to see through it ([Kašpar & Treml 2016](https://doi.org/10.3354/cr01370)).

**Feedbacks**
31. **How much does a single tree-throw event move?** Rootwad volumes typically <0.5–several m³ ([ESPL 2020](https://www.ovid.com/journals/espl/fulltext/10.1002/esp.4628~bioturbation-and-erosion-rates-along-the-soilhillslope)); a pit+mound pair displaces roughly that downslope.
32. **What is the frequency of tree-throw?** Needs ≥2–3 events/ha/yr to dominate mid-slope flux ([Richards 2011](https://doi.org/10.1002/esp.2149)); typical forest frequencies are lower.
33. **How deep do roots weather bedrock?** Meters — root fracture drives the soil-production function ([Gabet & Mudd 2010](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub)).
34. **What is the roughness signature of pit-and-mound topography?** Directly measurable from high-res topography; theory links production/decay to a dimensionless flux ratio ([Doane 2021](https://doi.org/10.1029/2021gl094987)).
35. **How much carbon does a beaver meadow sequester?** ~10 t C/ha/yr long-term in one well-studied case, order of magnitude above forest counterfactual ([Hallberg-Larsen 2026](https://martinezbeavers.org/wp-content/uploads/2026/03/Hallberg-Larsen-et-al.-2026-Beavers-can-convert-stream-corridors-to-persistent-carbon-sinks-Communications-Earth-Environment.pdf)); N-removal ~50–450 kg/km² catchment ([Lazar-era, PubMed](https://pubmed.ncbi.nlm.nih.gov/26436285/)).
36. **What did rivers look like on Mars (the no-vegetation endmember)?** Sheet-braided, as Earth's pre-vegetation rivers — the same physics, no banks; supports §8.6's "disable everything" mode.

**Global numbers / methods**
37. **What did GEDI change about biomass estimates?** Spaceborne LiDAR gave the first global canopy-structure sampling; reduced error bars on tropical biomass; not pinned further.
38. **How much has tree count changed since Crowther?** Estimates converge on the same ~3 T order with different conventions; the follow-up literature debates the human-loss term (46%±) more than the total.
39. **What is the error bar on FAO's 4 Gha?** Definitional (canopy threshold, plantation inclusion) rather than statistical — likely ±0.5 Gha depending on convention.
40. **What would a pre-Devonian world's sediment yield be?** Higher per unit rainfall (no retention): Schumm's argument — sediment yield rises with precipitation to a rock-erodibility cap without vegetation ([Davies & Gibling 2010](https://www.sciencedirect.com/science/article/abs/pii/S0012825209001779)).


# Part 7 - Procedural Terrain Generation: The Synthesis

Provenance: written by the algorithms-side research agent for the merged Earth-terrain document, after the six geoscience parts were outlined. Method: local context (question bank §G, `research/micro-voxel-creators-research.md`, `research/grass-rendering-research.md`, house style from `research/water-physics-and-wave-simulation.md`, the engine reality in `research/terrain-fixes-log.md`) absorbed first; then ~27 web queries across 7 batches (Exa search; game/GDC/vendor-doc sources dominate, geomorphology literature where the physics lives). Every "?"-marked parent anchor was treated as unverified and checked; disagreements are reported inline and tallied in the Provenance section — five this time, one of them a 25× arithmetic error worth fixing before the merge.

This is the synthesis layer. Parts 1–6 own the geoscience: Part 1 tectonics/mountains, Part 2 fluvial/landscape evolution, Part 3 glacial/coastal/periglacial, Part 4 caves/karst, Part 5 deserts/wind/climate, Part 6 vegetation. This part owns the **algorithms**: what noise actually is statistically, what erosion solvers actually compute, what shipped engines actually do, and how to check any of it against reality. Cross-references point at "Part N" per that mapping.

---

## 1. Noise-based terrain and its statistical failures

### 1.1 The fBm sum

Every procedural-terrain conversation starts from fractional Brownian motion approximated as a sum of band-limited noise octaves — "rescale and add" in Saupe's phrasing, as documented in Musgrave's dissertation [1][2]:

$$
\boxed{\;h(\mathbf{x}) = \sum_{i=0}^{O-1} A\, g^{\,i}\; N\!\left(f_0\, \lambda^{\,i}\, \mathbf{x}\right)\;}
$$

with octave count $O$, gain $g$ (amplitude falloff per octave, classically $0.5$), lacunarity $\lambda$ (frequency ratio, classically $2.0$), base frequency $f_0$, and $N$ a smooth coherent-noise basis (Perlin, simplex, value noise). In practice $O = 3\text{–}8$: Musgrave notes that more octaves are "unnecessary-to-detrimental" — frequencies below the viewport act only as a slope bias, frequencies above half the sampling resolution alias into stochastic noise [1][2]. The engine's own generator sits exactly in this band: 4 octaves, lacunarity 2, gain 0.5, amplitude 64, FastNoise2 pinned to SSE2, analytic `height_at` with a permanent smoothness regression test (`research/terrain-fixes-log.md` §Group R).

The spectral connection: an fBm with Hurst exponent $H$ has a 1D power spectrum $E(k) \propto k^{-\beta}$ with $\beta = 2H + 1$ (Turcotte's relation $\beta = 2H + 1$, equivalently $H_a = (\beta-1)/2$) [4]. So the classic $g = 0.5$, $\lambda = 2$ fBm is *Brownian motion*: $H = 0.5$, $\beta = 2$.

### 1.2 What real terrain's spectrum actually is

The parent anchor "$E(k) \propto k^{-2}$-ish" is **confirmed for second-order statistics, with two mandatory caveats**:

- **Confirmed:** Turcotte's 1987 spherical-harmonic analysis found Earth topography "a well-defined fractal with D = 1.5 … Brown noise" — $\beta = 2$ along 1D angle-integrated transects [1], reproduced in his 2007 review [4]. Bathymetric studies span $\beta \approx 1.6\text{–}2.5$ by region and method (Berkson & Matthews 1.6–1.8; Fox & Hayes ~2.5; Gibert & Courtillot 2.1–2.3; Balmino ~2) [3].
- **Caveat 1 (dimensions):** those are 1D/angle-integrated exponents; the angle-*averaged* 2D spectrum carries $\beta + 1$ [3]. Validating a generator's 2D FFT against Turcotte's number without this conversion concludes your terrain is an octave too rough.
- **Caveat 2 (the monofractal lie):** Lovejoy, Schertzer, and Gagnon's 2005–2006 analyses of four DEMs spanning 20,000 km down to 50 cm ($>2\times10^8$ pixels) show the $\beta \approx 2$ line holds for *second-order* moments from planetary scales down to ~40 m — but "the multifractal FIF is easily compatible with the data, while the monofractal fBm and fLm are not" [3][5]. Universal multifractal parameters: $\alpha \approx 1.79$ (degree of multifractality; 0 = monofractal), $C_1 \approx 0.12$, and a smoothing exponent $H$ that *differs by setting*: $H = 0.46$ (bathymetry), $0.66$ (continents), $0.77$ (continental margins) [3]. In plain terms: real terrain matches fBm in variance-per-scale, and breaks it in the tails — extreme relief (the Himalaya vs the Indo-Gangetic plain in one tile) is far more common than a Gaussian cascade produces. This is exactly the failure Musgrave was chasing with his multifractal constructions [2][6][7].

### 1.3 The self-similarity failure

Real terrain is not self-similar, in two documented directions:

- **Vertical anisotropy (peaks vs valleys).** In rugged alpine terrain the peaks are more jagged than the valleys (valves fill with detritus and get smoothed by glaciers); in diffusion-dominated sub-alpine terrain the hilltops are rounder than the valleys. Musgrave states this as the motivating asymmetry of his whole research program — "fBm is by design homogeneous and isotropic, while real terrains are neither" [1][2]. A single-$H$ fBm has one roughness everywhere, up-slope and down-slope alike.
- **Horizontal anisotropy (direction-dependent $H$).** Mountain ranges are elongated; the scaling exponent along a range axis differs from across it. Lovejoy's analyses require an anisotropic scaling operator $\mathbf{G}$ beyond the scalar $H$ for precisely this reason, and note that isotropic analysis "washes out different geomorphologies" [3][5]. Games notice this as the "everything looks the same from any direction" property of naive noise.
- **Scale-boundedness.** The $\beta \approx 2$ line breaks below ~40 m (trees, in their data) [3][5], and extended-self-similar analyses find the *local* Hurst exponent varies with scale — Lewis's critique, cited in the ESS work, is that "landscapes are fractal for only a few scales" [8]. Mandelbrot's own famous quip, relayed by Musgrave: the fractal dimension of the Himalayas is approximately that of the JFK runway — only the crossover scale differs (kilometers vs millimeters) [1]. Crossover-scale modulation (where the fractal band sits), not dimension modulation, is the stronger artistic control — Musgrave's empirical conclusion [1][9].

### 1.4 The hypsometric failure

The parent anchor suggested pure fBm has "too little land at mid elevations." **Disagreement — the failure is the opposite shape.** Earth's hypsometric curve is *bimodal*: two primary elevation groupings — continents a few hundred meters above sea level, abyssal plains near −4,300 m — with ~29% of the surface above sea level [10][11]. Pedersen's 2024 analysis adds that the largest concentration of *land* area sits within a few meters of sea level (maximum at +2–5 m), with the curve steepened at mountains and trenches by active tectonics [11]. An fBm heightfield cut by a sea-level plane yields a *unimodal, roughly Gaussian* area-elevation distribution centered mid-range: far too much terrain at mid elevations, no double peak, and a land fraction that is an accident of the plane's placement rather than a consequence of two different crusts (Part 1's oceanic/continental dichotomy — the real cause of the bimodality [10]). The "blobby average everywhere" quality of fBm islands is this statistic, seen with the naked eye.

### 1.5 The drainage failure — the core one

fBm has no flow routing, therefore no coherent rivers, no valley networks, no ridges-between-valleys structure. Musgrave, 1989: "Fractal terrains in general have no global erosion features inherently due to isotropy and stationarity, and practically due to the difficulty in implementation and computation of such global processes, which require global communication" [1]. Every later development — his own hydraulic/thermal passes, World Machine's and Gaea's erosion nodes (§5), the hydrology-correctness industry (§4), and FastScape-style LEMs (§3) — exists to answer this one sentence. The community-facing version (with USGS elevation maps as the visual): real terrain's fractal shapes "are driven by erosion … the fractal pattern emerges from smaller streams merging into larger streams and rivers as they flow downhill" [12].

This is *the* #1 tell of fake terrain, ahead of every spectral subtlety in §1.2: drainage networks are the globally coherent structure that point-evaluated noise cannot produce, because a height sample at $\mathbf{x}$ in pure fBm is statistically independent of everything more than ~one feature-scale away.

### 1.6 When fBm is enough

- **Micro-relief** (centimeter-to-meter roughness on an already-correct macro surface): the spectrum below ~40 m is where multifractal analysis itself gives out into vegetation-dominated noise [3]; low-octave noise displacement is the standard and defensible choice.
- **Distant LOD.** At screen sizes of a few pixels per feature, second-order statistics dominate perception; the tail failures are invisible. Musgrave's Nyquist argument [1] is the formal version.
- **Base continental shape** — the low-frequency *skeleton* that later passes (tectonic stamps, SPIM carving, §3–4) reorganize. fBm as a *first draft* is fine; fBm as the deliverable is the failure.
- **Non-eroded settings:** recent volcanic terrain, dune fields (Part 5), and badlands are closer to scale-limited noise than fluvial terrain is — Musgrave: "All natural terrains, except perhaps recent volcanic ones, bear the scars of erosion" [7].

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

- **Billowed** ($|N|$ or $1-|N|$ without the weight cascade): rounded lumps — clouds, dune swells, and the "cotton-ball" foothills look. The multiplicative multifractal version gives heterogeneous plains-foothills-mountains in one patch [7][9].
- **Terraced / quantized.** Gardner's 1980s terrain quantized altitude to yield "terraced land, such as mesas" [1]. **What real stair-step terrain is (confirmed):** resistant-layer stratigraphy plus differential erosion. The Grand Staircase is the type example: ~6,000 vertical feet of alternating cliffs and plateaus over ~150 miles; "each 'riser' is a cliff … as much as 2,000 feet high and each 'tread' is a plateau, terrace, or flat … as much as 15 miles wide"; hard sandstones/limestones form cliffs and terraces, soft shales/siltstones form the slopes between [17][18][19]. Cosmogenic-erosion work on the same region adds the mechanism detail that matters for generation: strong-over-weak contacts get *undermined* (amplified erosion), weak-over-strong contacts grow protective benches — i.e., the stair-step is an erosion-rate pattern in layered rock, not a height quantization [20]. Procedurally: terrace the heightfield where a stratigraphy mask says so, then erode; Houdini's explicit `HeightField Terrace` SOP exists for exactly this [21]. Flat-top mesa outlines additionally want a *cap-rock* logic (a resistant layer above softer rock, Part 1's volcanic/Sedimentary story).
- **Plane/rigid/warped combinations per landform.** Musgrave's parameter-table doctrine: modulate crossover scale with altitude (foothills→peaks), square-and-weight for ridges, offset for valleys; Michel et al. for folds [1][7][14]. Shipped-game/tutorial parameter tables vs terrain-science measurements: the former are aesthetic (gain 0.5, lacunarity 2, 3–8 octaves — universal across Musgrave, Blender, FastNoise2 defaults); the latter say the *bulk* spectrum matches $\beta \approx 2$ but $H$ should vary $0.46\text{–}0.77$ by setting [3] and crossover scale — not $D$ — is the artistic knob [1]. A generator honest about this exposes per-biome $H$ and crossover, not one global "roughness."

---

## 3. Erosion algorithms

### 3.1 Thermal erosion (the talus pass)

Musgrave's formulation is still the canonical one: a low-pass filter whose fixed point is a slope, not a height — "exactly like a standard low-pass filter, except that the value to which it converges is not a DC level but rather, for instance, a slope of 45 degrees. Slopes less than the angle of repose are unaffected" [9]. Iterate: for each pair of neighbors, if the height difference exceeds `talus` (a slope threshold in height-units per cell), move material downhill. O(n) per pass, trivially parallel, converges to angle-of-repose hillslopes — which is *genuinely correct physics* for soil-mantled and scree slopes (Part 2 §20: real threshold hillslopes; Montgomery's Olympic/Coast-Range slope histograms cluster at threshold values [22]).

**The soap-bubble artifact:** iterated to convergence, thermal erosion minimizes total height variance subject to the slope constraint — every convexity gets planed toward the talus angle, and the terrain ends up a network of planar facets meeting at ridges, the visual signature of a *minimum-energy soap film*, not of rock mass strength contrasts. Real talus fields are patchy (lithology, aspect, vegetation; Part 6 §66); the algorithm is lithology-blind and homogeneous. Mitigation in production tools: mask the pass by rock-softness (Gaea's Selective Processing exposes exactly a Rock Softness bias mask [23]).

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

The stack-ordering idea is the single most stealable algorithm in this document for a game engine: it converts "global hydrology" from an iterative, convergence-sensitive simulation into two linear sweeps. The FastScape library family (fastscapelib-fortran and its C++ successor) packages SPL + sediment transport + hillslope diffusion + marine deposition, all implicit and O(n), plus O(n) depression-resolving flow routing over sinks and implicit O(n) glacial erosion; it is designed to couple to flexural isostasy and full 3D tectonic models, and has run $10^8$-node problems on a laptop [30][31][32]. Flexural-isostasy coupling (erosion unloads the crust, the crust rebounds, see Part 1 §4) is a supported add-on, and in a game context is best treated as a cheap low-pass rebound kernel rather than a physical plate solver.

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

**World Machine:** a world file "doesn't define a terrain, but the steps to create a terrain" — devices wired in a flowchart, continuous previews, build to high resolution on export; resummon the same graph with a new seed for a sibling terrain [49]. Basic flow per the docs and its own marketing lineage: primitive/noise devices → erosion devices (hydraulic + thermal, the flagship) → coastal/masking/export. **Gaea:** same shape, one node per heightfield operation, graphs strictly left-to-right, Portals/Chokepoints for organization [50]. Gaea's Erosion node is the reference commercial hydraulic-erosion implementation: Feature Scale in meters (width of largest valleys/ridges), Real Scale driven by the terrain definition, selective processing by Rock Softness / Erosion Strength / Precipitation masks (slope/altitude bias or custom), and — the two properties a game should copy — **resolution-parity** (a 512² preview preserves major erosion features of a 4K/8K build) and an explicit **Deterministic** toggle (parallel erosion is otherwise nondeterministic in the small; single-core for reproducibility) [23][51]. Data outputs — Wear, Deposits, Flow — are the mask set every downstream system (biomes, vegetation, texturing) consumes [23]. Gaea's docs also carry the sharpest practitioner warnings in the field: flow-line textures make terrains "extremely conspicuous"; real terrains "rarely have clean flow lines" [51].

*Architecture implication:* heightfield-first with erosion as the central transform, everything else as masks. This is the proven pipeline for *artists*, and its graph structure is what a voxel engine's generation passes should mirror internally.

### 5.2 Unreal Landscape (heightmap-consumer)

Landscape is a GPU heightfield with non-destructive Edit Layers and splines [52], imported from external tools (World Machine explicitly called out); world composition streams level tiles, with a **Tiled Landscape Import** that consumes World Machine's tiled heightmap/weightmap export and requires adjacent tiles to share border vertices [53][54]. Section size 63×63 quads recommended; the 505×505 default, component/section LOD structure, and origin shifting for large worlds are all in the docs [53][55]. *Implication:* Unreal does not generate — it consumes. The generation architecture lives upstream (World Machine/Gaea/Houdini), which is exactly the seam where a game with its own procedural pipeline would slot in.

### 5.3 Houdini terrain (heightfields as 2D volumes)

Heightfields are 2D volume primitives (`height` + `mask` layers, default 1000×1000 m at 2 m grid spacing = 500×500 samples) — "it is not possible to work on a terrain's vertical areas" without converting to polygons; masks as second inputs on nearly every node; erosion via `HeightField Erode` (rewritten in Houdini 21) producing `sediment`, `debris`, `flow`, `flowdir` layers, with hydro/thermal sub-node control and the documented stacking workflow: **Massing → Seeding → Lobing → Remapping (elevation passes) → Upsampling → Shaping (Terrace/Clip) → Re-seeding → Erosion**, iterating erode→distort→erode chains [21][56][57][58]. Seeding — "the less smooth the surfaces, the more realistic erosion will be later … obstacles that water and soil must move around" — is the practitioner's version of our §1.3 scale-boundedness point. *Implication:* DCC-grade iteration on the same heightfield-first substrate; the LOD of truth is the 2D grid, 3D comes only by conversion.

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
3. **Terrain shape** = `sloped_cheese` (base 3D density from `depth` × `factor` — roughly $h(x,z)-y$ plus 3D noise) with a `range_choice` split: above the 1.5625 threshold, the surface regime; below, caves. A `jaggedness` noise adds sharp peaks in high mountains [63].
- **Caves — parent anchor confirmed and extended:** Minecraft carves with **three** noise-cave types, not two — **cheese caves** (3D `cave_cheese` noise blobs: "the black part of the noise image becomes stone, white becomes air … resembling cheese with many holes" — large open pockets), **spaghetti caves** (2D-ish noise pair whose *intersection* is air — long tunnels), and **noodle caves** (thinner, squigglier, 1–5 block wide variants), plus `cave_entrances` noise connecting surface to underground, noise pillars, and aquifers governing cave water/lava with per-aquifer fluid levels [62][63][64]. Pre-1.18 "carver caves" (worm-like feature carvers) still exist as a separate feature pass [64].
- **Biomes:** a multi-noise parameter list — each biome is a point in (temperature, humidity, continentalness, erosion, weirdness, depth) space; nearest-neighbor wins. Terrain and biome *share* some noise inputs (the same continents/erosion/ridges fields feed both), creating the implicit link between shape and surface without biome-determines-terrain coupling [62][64].
- **Surface rules:** a separate declarative pass decides surface blocks (grass/sand/etc. by biome + slope + depth + water) after density [62][64].

*Implication:* the most-shipped density-first architecture on Earth, and entirely data-driven — worth copying structurally (declarative density graph; biome as climate-space nearest-neighbor; surface rules as a separate pass), whatever one thinks of blocky output.

### 5.6 Dwarf Fortress (simulation-first)

From Tarn Adams' own descriptions (Gamasutra 2008 interview; GameAIPro ch. 41; PRACTICE 2016) [65][66][67]:

1. **Elevation** by midpoint displacement (its axis-alignment artifacts explicitly why the erosion phase exists next).
2. **Climate fields:** temperature (biased by elevation and latitude), **rainfall later biased with orographic precipitation / rain shadows**, drainage as another fractal, plus salinity, vegetation, and fantasy fields (savagery, good/evil).
3. **Biomes as derived lookup, never laid down directly:** "rainfall ≥ 66/100 and drainage < 50 → swamp" — "the nice thing about having the fractally-generated basic fields is that the biome boundaries all look natural" [65].
4. **Erosion phase:** temporary river paths run out from mountain bases, "digging away at a square if it can't find a lower one"; then real rivers, *forced* to the ocean if they fail; lakes bulged; loop-erasure; flow amounts and tributary structure computed; rivers named [65]. This is a hand-rolled priority-flood-plus-carve — §4's algorithm class, invented independently in 2006-era hobby code.
5. Then vegetation/animal populations, civilizations, ~500 years of history (economy, wars, sites) — "long-term simulation design" [65][67]. Adams' design principles: simulate basic fields and let biomes *arise*; and "base your model on real-world analogs … the world maps improved greatly when rain shadows were taken into consideration … drainage was another nonobvious consideration" [66].

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

Generate a hydrologically-correct heightfield (§4), then voxelize with 3D detail only where needed. **For:** (a) rivers, drainage, and biome fields all need 2D graphs anyway — no one has shipped coherent river networks from pure 3D density fields; (b) NMS — the most voxel-native shipped game — does exactly this (noise-varied sphere radius + a thin 3D shell) [60]; Minecraft's `sloped_cheese` is the flat-world version [63]; MishMash's micro-voxel engine is "generally a height field, however with several stamping and carving passes to create 3d detail" (micro-voxel doc §2.2); (c) the collision/vegetation/LOD systems all want an authoritative analytic surface (the engine's own `height_at` pattern — micro-voxel doc §4.2 makes the same argument). **Against:** the hybrid's honest cost — 3D detail near the surface can contradict the hydrology (a cave breaching a riverbed drains the river; MishMash's documented pond-draining bug). The discipline: treat the 3D shell as *subordinate* — carve caves with a density budget that goes to zero within some distance below the water table, and make the water table a property of the flow graph, not of local geometry. Verdict: hybrid, with the heightfield as the constitution and 3D noise as statute.

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

For this engine — analytic fBm heightfield today, ray-marched 7.8 mm SVO, static deterministic world, generation-time budget (world-ready ~0.6 s on the svo path, so the macro pipeline can afford hundreds of ms regionally):1. **Base continents:** low-frequency fBm + plate-velocity domain warp (§2.1 [13][14]); enforce Earth-like bimodal hypsometry by construction (separate ocean/land crust fields, sea level cut at ~29% land [10][11]) — replaces today's single 4-octave field as the *skeleton* only.
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

1. **Is the β=2 spectrum an attractor of erosion physics, or of deposition?** Partially answered: Turcotte's own lattice-deposition model produces k⁻² surfaces from pure deposition [4]; fluvial erosion topography is self-similar/multifractal under SPIM dynamics with conditions on m, n and uplift variability [Banavar et al., not fetched]. Disposition: worth a dedicated follow-up in Part 2's terms.
2. **What octave count does β=2 require if lacunarity ≠ 2?** The octave-lacunarity-gain trio sets a finite band; outside the band the spectrum rolls off. Answered qualitatively (Musgrave's band-limiting [2]); exact filter response is a one-page derivation — future work.
3. **Can a voxel engine validate its 3D density field (not just heights) against anything?** Open; no equivalent statistics for full 3D terrain exist in the game or GIS literature surveyed. Proposal: extend §9's suite with cave-porosity and passage-orientation statistics against Part 4.
4. **What is the visual, not statistical, detection threshold for drainage wrongness?** Unanswered in the literature I found; user-study territory (the forestry-placement studies [88][89] are the template).
5. **Does priority-flood filling produce geologically wrong lakes (too many, wrong shapes)?** Yes as stated in GIS practice — fills are data-conditioning, not hydrology; games wanting real lakes need depression *hierarchy* and outflow decisions. Disposition: flagged; Barnes' watershed-labeling variant is the starting point [33].
6. **How fast is Priority-Flood in game terms?** 20-line algorithm, O(n) integer [33]; for a 1024² region it is sub-millisecond-scale on modern CPUs. Answered by complexity, not benchmarked here.
7. **Is D8's 45° bias visible at game resolution?** At channel widths ≥3 cells, yes — sawtooth rivers; mitigations: D8-LTD [39] or sub-cell path accumulation. Answered.
8. **What m/n should a game's SPIM use?** θ=m/n≈0.4–0.5 typical, from observed slope–area exponents −0.35…−0.6 with n≤1 [28][29]. Answered.
9. **How many implicit SPIM steps to steady state?** Depends on uplift/erodibility ratio; research codes use hundreds–thousands of steps but Braun-Willett's implicit scheme allows large dt [29]. Disposition: needs an experiment on our heightfield scale; not in sources.
10. **Can orographic P and SPIM oscillate (rain shadow chases the ridge)?** Physically yes over geologic time; at generation-time iteration counts, one LT pass per erosion checkpoint is standard practice in coupled fastscape work [30][72]. Partially answered.
11. **What does the 128 m NMS shell imply for our 7.8 mm engine?** Our shell must be similarly thin — full-resolution voxels only near the camera (already the engine's LOD-radius design); macro relief belongs to the analytic layer. Answered by analogy [60].
12. **Is Minecraft's 1.5625 sloped_cheese threshold meaningful for us?** It's a tuned constant of their density math, not transferable. Answered (negative).
13. **Do aquifers generalize beyond Minecraft's water/lava?** Yes trivially — per-region fluid level + flood/spread/barrier channels is a clean chunk-scale water table [62]. Answered.
14. **What is the real fraction of cave volume vs rock (porosity) by karst maturity?** Part 4's territory; I did not fetch numbers. Disposition: cross-reference, not answered here.
15. **Can the constant-drop test run per-biome?** Yes — drainage density varies 2–12 km/km² by climate [45][46]; per-biome t-tests are the natural extension of TauDEM's global one [43]. Answered.
16. **Does thermal erosion's soap-facet artifact survive under added noise?** Practically masked by re-seeded detail (Houdini's re-seeding step [58] is the documented practitioner answer). Answered.
17. **What droplet count converges to a stable channel network?** No convergence theory exists; Lague's 70k/255² is aesthetic [25]. Disposition: open; treat droplets as non-deterministic decoration.
18. **Is there a deterministic (seed-stable) droplet scheme?** Yes — fixed spawn lattice + deterministic PRNG per droplet; Gaea's Deterministic toggle is the commercial precedent (single-core for reproducibility [23]). Answered.
19. **Should rivers be carved below the heightfield or the heightfield lowered to them?** Carve: SPIM produces the valley and the channel together; post-hoc lowering breaks the slope–area law. Answered by construction [29].
20. **How wide should a game river be per catchment area?** Real hydraulic geometry: w ∝ A^0.5 within basins [28]; use it. Answered.
21. **Where do waterfalls/knickpoints belong in the pipeline?** As SPIM transients on lithology contrasts (Part 2 §19, §29); a stencil on layer boundaries is the cheap version. Answered in outline.
22. **Can domain warp encode real fold *orientation* data?** Yes — Michel et al. drive warp by plate velocity vectors [14]; orientation comes free if the belt skeleton is authored. Answered.
23. **Is a 2D heightfield adequate for anticlinal ridges with breached cores?** Marginally: ridge + carve stencil gets the planform; the water gap through the breach needs flow-graph forcing. Partially answered.
24. **Does anyone ship variogram-based LOD (fractal interpolation between samples)?** Musgrave's QAEB tracing is the historical version [7]; modern engines bake LODs instead. Answered (negative for shipped modern engines surveyed).
25. **What grid resolution should the hydrology pass run at vs the voxel grid?** Decoupled: hydrology at 10–30 m-equivalent (drainage statistics are defined there [42][48]), voxels 7.8 mm near camera. Answered by scale analysis.
26. **Is blue-noise vegetation placement worth it over jittered-hex?** At 5 m tree spacing, no user can tell (angle histograms differ [85]); spend the effort on cluster structure instead. Answered with the study backing [88].
27. **How do you place vegetation on 3D cave walls?** John Lin ray-traces placement queries (sunlight, openness, cave walls) — the documented answer (micro-voxel doc §1.4). Answered by citation.
28. **What's the grass-density equivalent of stems/ha?** Part 6's ground-cover fractions own this; grass-rendering doc §4's shipped blade budgets (83k–100k drawn) are the render-side answer. Answered by cross-reference.
29. **Do ecotone sharpness rules apply underwater?** Unresearched (kelp/seagrass boundaries). Disposition: open; Part 6/Part 3 follow-up.
30. **Can the Whittaker diagram be made hysteretic cheaply?** Yes — order-dependent lookup (last biome biases threshold), implementing alternative stable states; no shipped example found. Partially answered (proposal).
31. **What's the cheapest correct rain shadow?** Upslope model P = Cw·U·∇h [71]; one gradient + dot product. Answered.
32. **When does the LT FFT model beat the upslope model?** When mountain width ~ drift distance (5–25 km): spillover and displaced maxima matter [71][74]. Answered.
33. **Should biome noise fields share octaves with terrain noise (Minecraft-style)?** Yes where correlation is physical (elevation→temperature), no where independence is physical (rainfall vs micro-relief); Minecraft shares continents/erosion/ridges [62]. Answered.
34. **How is the 29% land fraction best enforced?** By two-crust construction (§10.2 step 1) or sea-level quantile matching of the continent field; the second is cheaper, the first is geologically honest (Part 1). Answered with options.
35. **Does the slope-histogram acceptance test distinguish SPIM output from stamped terrain?** Yes — stamped cliffs create bimodal/shouldered histograms absent from Wolinsky-Pratson's observed trend [90]. Answered.
36. **What is the memory cost of a flow graph per region?** D8 receivers + stack order + accumulation: ~3 words/cell at hydrology resolution — negligible vs bricks. Answered by arithmetic.
37. **Can the SPIM stack be computed incrementally as chunks stream?** Not locally — flow graphs are global by nature; but hydrology at 10–30 m resolution fits whole-region computation at generation time (DF does world-scale in seconds [65]). Answered.
38. **Is there a shipped game with published terrain validation statistics?** None found; the §9 suite would be novel. Answered (negative).
39. **What would break in the pipeline first at planet scale?** FFT-based orography (global FFT on a sphere needs HEALPix/spherical harmonics) and the 32-bit float coordinate system (NMS's documented multi-space problem [15][60]). Answered.
40. **Which single §9 test gives the most bug-detection per line of code?** Drainage density (test #4): it catches missing rivers, noise gullies, wrong thresholds, and broken flow routing in one number with a wide real-Earth band [45][46][47][48]. Answered — recommendation.


