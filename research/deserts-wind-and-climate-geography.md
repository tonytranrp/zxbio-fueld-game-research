# Deserts, Wind & Climate Geography: Aeolian Physics, Dune Morphodynamics, and Why Deserts Are Where They Are

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
