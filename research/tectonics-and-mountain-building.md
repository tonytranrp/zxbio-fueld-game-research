# Tectonics & Mountain Building: A Simulation-Grade Reference

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
