# Glacial, Coastal & Periglacial Terrain: How Ice and Oceans Reshape Land

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
