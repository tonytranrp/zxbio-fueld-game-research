# Fixation, Attention, and Perception: The Science of Locking Onto an Object

> **What this is.** Part 4 of the five-part human-eye deep study (companions: [`human-movement-and-perception-research.md`](human-movement-and-perception-research.md) Parts 1–5, [`water-physics-and-wave-simulation.md`](water-physics-and-wave-simulation.md) for house style). Domain: what happens when you fixate and attend to an object — enhancement at the target, the measured suppressive surround around it, the crowding limit beyond it, fixation-centered aftereffect/fading phenomena, transsaccadic memory, and the object/feature structure of selection. GFM LaTeX; ranges not fake values; disagreements reported inline with both sources; every load-bearing claim carries a live-verified URL.

## 0. Provenance

- **Date:** 2026-09-06. **Author:** research agent 4 of 5 (fixation & attention domain).
- **Tools:** Exa search/fetch, `web_fetch`; every cited URL returned live content this session. Local `pwsh` verification: Bouma spacings for $b=0.4/0.5/0.8$; peripheral CS loss in log units; face-feature angular size vs 10° MAR (crowding dissociation); AB window from lag×SOA; MAE √-scaling; fixation-cycle rates; VWM throughput; IOR gradient in cm; flash-lag offset at 10°/s — all reproducible from stated formulas.
- **Verification summary:** 11 search batches (~35 queries) plus fetches; 40 distinct load-bearing sources (§4.9). Parent anchors that disagreed with the literature are reported, not forced (AB onset ~180 ms not 200; Troxler 1–10 s; conjunction slopes span both sides of "10–25"; anger-superiority contested; forward-vs-convergent remapping). Unverifiable items dropped, listed in Appendix A dispositions.
- **Scope boundary:** acuity-vs-eccentricity, cortical magnification, UFOV, inattentional-blindness basics, dual-task walking costs → movement document Parts 3–4 (cross-referenced only). Saccade kinematics, suppression time course, microsaccades, motor glance-timeline → Part 2 (agent 3); here, *perceptual* consequences only.

---

## 4.1 When you fixate: the perifoveal cost structure

### 4.1.1 The fixation benefit and its measured falloff

Acuity-vs-eccentricity (MAR ~linear in eccentricity, $\text{MAR}(E) \approx \text{MAR}_0(1+E/E_2)$, $E_2 \approx 1.0$–2.7° by task; Landolt-C ~20/116 at 10°) is in the movement document Part 3 §3.1. Here: the two assigned falloffs.

**Contrast sensitivity vs eccentricity.** Broad-eccentricity Gabor measurements (20 adults, 0.375–18 cyc/deg; fovea and 10°–60° temporal): mean CS (across SF) falls 145.9 → 57.1 → 37.4 → 15.3 → 5.6 at 0°/10°/20°/40°/60°, i.e. 0.41 / 0.59 / 0.98 / 1.42 log₁₀ units lost (pwsh-verified); CSFs keep the inverted-U but the peak shifts progressively to lower SF; no consistent responses at 80° ([Adams et al. 2015](https://doi.org/10.1167/15.12.95); nasal field drops fast to 20° then *plateaus* at ~10.5 CS units to 60° while temporal declines steadily — [Adams et al. 2016](https://doi.org/10.1167/16.12.226)). Structural facts:

- **The decline is not linear in log units.** A 2025 far-periphery study (Gabors to 56°–84°) found slight decrease near, then rapid decline at far eccentricity/low SF — "not predicted by 10 CSF models," which all overestimate far-periphery sensitivity because they model log-CS as linear in eccentricity ([Kitakami et al. 2025](https://doi.org/10.1002/col.70007)). A single linear-in-log gaze-contingent falloff is too generous beyond ~30°.
- **Near-periphery rate ~0.24 log units/10°**, "relatively greater for higher spatial frequencies" (Adams et al.). Verified: fovea→10° gives 0.41 log (foveal peak included); 10°→20° gives 0.18 — bracketing the rate.
- **The limit is post-receptoral.** Achromatic and isoluminant red-green resolution to 55° both decline "at a faster rate than that dictated by the known optical and/or receptoral properties," chromatic more steeply than luminance, with the naso-temporal asymmetry tracking ganglion-cell density ([Anderson, Mullen & Hess 1991](https://physoc.onlinelibrary.wiley.com/doi/10.1113/jphysiol.1991.sp018781); ideal-observer confirmation: [Banks et al. 1991](https://opg.optica.org/josaa/abstract.cfm?uri=josaa-8-11-1775)).

**The color-periphery question, with numbers.** Mullen, Sakurai & Chu's "sinring" (de-confounds SF, size, eccentricity): L/M (red-green) cone-contrast sensitivity "declines steeply across the human periphery and becomes behaviourally absent by 25–30 deg (in the nasal field)," reaching the stimulus-bound maximum contrast; luminance and S/(L+M) decline more shallowly, so "by 20–25 deg red-green and luminance cone contrast sensitivities are similar," and beyond it luminance is *more* sensitive than either chromatic mechanism ([Mullen et al. 2005](https://www.mvr.mcgill.ca/Kathy/PDF-05-09/Mullen-et-al-2005.pdf)). The two-axis dissociation replicates: S-cone opponency "remains robust within the central 20 degrees" while L-M degrades continuously from 6° to 24° ([McKeefry et al. 2008](https://doi.org/10.15388/psichol.2008.0.2607)).

**Disagreement (locus of the L-M loss):** Mullen & Kingdom attribute it to "hit and miss" random midget-bipolar wiring in peripheral retina (1996, via [Newton & Eskew 2003](https://doi.org/10.1017/s0952523803205058)); McKeefry et al. argue a *cortical* basis — spatial-scaling experiments (Vakrou et al.) show L-M deficits "can be negated simply by appropriate increases in stimulus size," implying preserved retinal circuitry and different cortical magnification for the two mechanisms; Newton & Eskew's 18° masking data support a post-receptoral (possibly cortical) locus with normal L−M inputs. Physiology splits the same way (Martin et al.: little loss to 40°; Diller et al.: reduced opponent quality — both via McKeefry et al.). Engineering: **saturate-able red-green discrimination is a central-<25° faculty; peripheral hue shifts persist even when sensitivity is size-scaled; low-contrast color must never be the sole peripheral information carrier.**

### 4.1.2 Surround suppression: what happens to the things around the attended object

Two converging literatures — stimulus-driven (high-contrast center suppresses its surround) and attention-driven (attending a location suppresses the surround of the focus).

**Stimulus-driven.** A target in a high-contrast surround appears lower in contrast and is harder to detect; suppression is strongest for iso-oriented surrounds, declining with orientation difference ([Zenger-Landolt & Heeger 2003](https://www.jneurosci.org/content/23/17/6884), citing Cannon & Fullenkamp 1991, Xing & Heeger 2000/01). Quantitative facts:

- **Extent scales with eccentricity, not target size.** Petrov & McKee varied surround geometry, separation, eccentricity: "suppression amplitude remains constant with stimulus eccentricity [but] the lateral extent of suppression scales in proportion to the eccentricity" — and, surprisingly, not with stimulus size or spatial frequency; they propose the mechanism "selects salient targets for subsequent saccades" ([Petrov & McKee 2006](https://pmc.ncbi.nlm.nih.gov/articles/PMC1472811/)). V1 physiology rule of thumb: surround effects extend ~3× the classical RF diameter (Maffei & Fiorentini 1976; Angelucci et al. 2002; via [Zenger-Landolt & Heeger 2003](https://www.jneurosci.org/content/23/17/6884)) — at 4.5°–7.8° eccentricity with 0.5°–1° RFs that is **~1.5°–3°**, matching the parent's "~1–3°" for near-peripheral stimuli (growing proportionally beyond).
- **Time course.** Macaque V1 suppression latency ~61 ms vs ~52 ms center onset — the lag motivating Bair et al.'s fast-feedback account ([Bair et al. 2003 via Schallmo et al. 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC6464404/)); human MEG/EEG sees suppression in the earliest occipital responses (~80–130 ms), later components most perception-linked (Haynes et al. 2003, via Schallmo et al.). A surround lagging 375 ms after the target eliminates both psychophysical masking and most of the fMRI reduction — temporally tight ([Zenger-Landolt & Heeger, JOV abstract](https://doi.org/10.1167/2.7.128)).
- **Site and mechanisms.** V1 BOLD suppression matches psychophysics with one free parameter (96.5% of variance); V2/V3 suppression is "too strong to agree with psychophysics" ([Zenger-Landolt & Heeger 2003](https://www.jneurosci.org/content/23/17/6884)). Two mechanisms: low-level (monocular, broadly tuned, adaptation-resistant — feedforward LGN antagonism) and higher-level (binocular, orientation-selective, strongly reduced by 30 s adaptation — horizontal + feedback connections) ([Webb et al. 2005; Schallmo & Murray 2016; synthesis in Schallmo et al. 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC6464404/)).

**Attentional surround suppression.** Attending a stimulus "both enhances processing in the attended location and suppresses the processing of adjacent locations" (Cutzu & Tsotsos 2003; Mounts 2000 — via [Fang et al. 2019](https://pmc.ncbi.nlm.nih.gov/articles/PMC6868316/)), producing a **Mexican-hat (difference-of-Gaussians) attentional profile** ([Yoo, Tsotsos & Fallah 2018](https://www.frontiersin.org/journals/neuroscience/articles/10.3389/fnins.2018.00710/full), formalizing Tsotsos's Selective Tuning). Measured properties:

- **Suppression is non-monotonic — worst at intermediate distance.** VWM delayed-estimation, six colors, one cued: costs at ±1, ±2, ±3 items ($d = 0.98$–$2.49$), *worst at ±2* (planned contrast ±2 vs ±3: $d = 1.25$), for both precues and retrocues ([Fang et al. 2019](https://pmc.ncbi.nlm.nih.gov/articles/PMC6868316/)).
- **The surround scales with attended-location eccentricity, not target size** — peripheral targets produced wider surrounds (tracking V1 RF size); size had no effect ([Yoo et al. 2018](https://www.frontiersin.org/journals/neuroscience/articles/10.3389/fnins.2018.00710/full)): the attentional analog of Petrov & McKee. The parent's "~1–3°" is fovea-scaled; at 10° eccentricity the annulus is proportionally several degrees wide.
- **Attention modulates the suppression itself.** Gabor target in a hexagon of surround Gabors: unattended targets detectable only by popout; attention reduces thresholds "more than four-fold" (~80% → ~20% threshold contrast at 60% surround contrast) — attention partially *cancels* stimulus suppression at the target ([Zenger & Sagi-lineage 2000](https://www.sciencedirect.com/science/article/pii/S0042698900002182)). Conversely, feature-based attention enhances the attended dimension's contribution to suppression, and attending a *flanker* instead of the center flips iso-oriented suppression into surround *enhancement* ([Flevaris & Murray 2015](https://doi.org/10.1167/15.1.29); [Flevaris et al., J Neurosci 2015](https://courses.washington.edu/viscog/publications/Flevaris_JNeurosci_2015.pdf)). SSVEP: attention focuses tighter when the target is contiguous with vs segmented from an annular surround ([context-dependence study](https://pmc.ncbi.nlm.nih.gov/articles/PMC6621517/)).

$$\boxed{\text{Attention profile} \sim \text{DoG: } g(r) = A_c e^{-r^2/2\sigma_c^2} - A_s e^{-r^2/2\sigma_s^2},\quad \sigma_s \propto \text{RF size} \propto \text{eccentricity}}$$

**Answer to "what happens to the things around it":** demoted twice — by the attended high-contrast object's stimulus-driven suppression of its neighbors (iso-oriented worst) and by the attentional Mexican-hat surround. Neither is subtle: the attentional effect alone can be a four-fold threshold change.

### 4.1.3 The attentional spotlight: zoom lens, minimum size, split debate, gradient edges

- **Zoom lens.** Resources distribute over a continuum from the whole effective field (parallel, low resolution) down to "an area as small as a fraction of a degree" (LaBerge 1983's one-letter focus, cited in [Eriksen & St. James 1986](https://doi.org/10.3758/bf03211502)): smaller field → higher density → faster, finer extraction.
- **Minimum size.** Eriksen & Eriksen 1974: even fully prepared, "incompatible noise within 1° of angle of the target position had a significant impairing effect" — "visual attention is not capable of infinitely fine selectivity. Rather, there is a minimal channel size" (quoted in [Moore et al. 2020](https://doi.org/10.3758/s13414-020-02094-z)). Floor ~**1°** for letters; interference measured at 3° (Fournier & Eriksen 1990) and 5° (Miller 1991) — report **~1–5° by stimulus/task**.
- **Gradient edges, verified.** Eriksen & St. James measured the border directly: slopes of −9.3, −11.1, −9.1 ms per position outside the cued area for one/two/three-position cues ($F<1$ — **gradient invariant with focus size**). "The edge of the attentional focus is not a discontinuity, but is, rather, a graded dropoff... a focus, a margin, and a fringe" ([Eriksen & St. James 1986](https://doi.org/10.3758/bf03211502)). Diffusion-model "shrinking spotlight" implementations formalize this as a Gaussian narrowing linearly over tens-to-hundreds of ms, distant flankers escaping first ([via Deakin 2023](https://doi.org/10.31234/osf.io/4maqs)).
- **Split-spotlight debate — both sides.** *For:* Awh & Pashler's partial report (two 80%-valid cues in a 5×5 array; invalid targets *between* the cues) found "a strong accuracy advantage at cued locations compared with intervening ones," mechanism primarily "suppression of interference from stimuli at unattended locations" ([Awh & Pashler 2000](https://doi.org/10.1037//0096-1523.26.2.834)); sustained SSVEPs show division "between spatially separated locations (excluding interposed locations)... at an early stage of visual-cortical processing" ([Müller et al. 2003](https://preview-www.nature.com/articles/nature01812)). *Against:* Jans, Peters & De Weerd found "no studies in the current literature that pass" four criteria for a *stable* division; division "may not be easily achievable by naive human observers... a skill that may be acquired only through training" ([Jans et al. 2010](https://cris.maastrichtuniversity.nl/ws/files/76027702/Peters_2010_Visual_spatial_attention_to_multiple.pdf)). Resolution: **splitting is real but hemifield-gated** — the attended-vs-intermediate gain difference appears only across hemifields; within-hemifield splits fail because the two foci's Mexican-hat surrounds overlap (different-hemifield advantage; [Walter et al. 2015](https://doi.org/10.1162/jocn_a_00883)). The same DoG machinery of §4.1.2 explains both.

---

## 4.2 Attention as selection: the classic effects with numbers

### 4.2.1 Posner cueing, validity benefits, inhibition of return

Peripheral cueing: valid-cue targets faster for SOAs <~250–300 ms; longer SOAs reverse the effect — IOR (Posner & Cohen 1984; named by Posner et al. 1985; overview in [Panis & Schmidt 2022](https://doi.org/10.1515/psych-2022-0005)).

- **Validity benefit:** tens of ms — e.g., uninformative-cue IOR of 15 ms (384 vs 399 ms, $F(1,42)=27.08$), growing from −8.5 ms at 100 ms SOA to −30.1 ms at 931 ms ([Hayward & Ristic 2013](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2013.00205/full)); the parent's "30–60 ms" for *predictive* endogenous cueing is consistent with the broader Posner literature (invalid−valid of 30–60+ ms in choice RT — [computational replication](https://www.frontiersin.org/journals/computational-neuroscience/articles/10.3389/fncom.2015.00081/full)). Report **~10–60 ms** exogenous→endogenous.
- **IOR window and extent.** Facilitation <~250 ms; inhibition emerges 200–500 ms and **lasts ~1–1.5 s** for the sensory component (Posner & Cohen's own estimate), with oculomotor IOR measurable to **~3 s** ([Panis & Schmidt 2022](https://doi.org/10.1515/psych-2022-0005), dissociating sensory from oculomotor IOR after Hilchey et al. 2014). Within-trial hazard analysis: IOR is time-locked to *target* onset, present in responses 160–280 ms post-target, with co-occurring facilitatory and inhibitory components. **Spatial gradient:** "diameter... estimated to be about 6–10° (visual angle)," extending into the opposite visual field; no-cue baselines put spatiotopic/retinotopic IOR at 55/43 ms at short interval ([Sun & Thomas-lineage 2015](https://www.nature.com/articles/srep16586)). Parent's "up to 6°+" confirmed and slightly conservative: **~6–10°**, shrinking over ~1.4 s. Verified: 6–10° = 6–10 cm at 57 cm viewing.
- **Covert vs overt.** Covert attention modulates gain but cannot overcome the acuity gradient — it does not move photoreceptors. Premotor theory survives only in a limited exogenous form; endogenous covert shifts are dissociable from saccade plans and redirectable within ~30 ms (movement document §3.3.4; [PMC 11534328](https://pmc.ncbi.nlm.nih.gov/articles/PMC11534328/)).
- **Function:** the foraging account — an inhibitory tag on inspected locations that "facilitate[s] visual search... based on memorizing each previously attended location" (Klein 2000, via Panis & Schmidt); note the alternative "attentional momentum" account (Pratt et al. 1999).

### 4.2.2 The attentional blink

Raymond, Shapiro & Arnell 1992, RSVP ~10 items/s, identify white T1 then detect probe "X": dual-task probe detection "dropped below 60% for the posttarget interval from 180 to 450 ms" while the ignore-T1 control stayed ≥85%; deficit at lags 2–5, gone by lag 6–8 ([PDF](https://psych.hanover.edu/classes/cognition/papers/raymond%20et%20al%201992%20attn%20blink.pdf); [DOI](https://doi.org/10.1037//0096-1523.18.3.849)).

- **Window:** impairment within ~**180–500 ms** of T1 — "T2 accuracy is reduced when T2 is presented within approximately 500 ms of a first target" ([MacLean & Arnell 2012](https://doi.org/10.3758/s13414-012-0338-4)). Parent's "200–500" confirmed with earlier onset (~180 ms).
- **Magnitude:** original — 100% at lag 0 to *just below 50% at lag 3* (control ~91%): ~41 points absolute, ~45% of control. The 2020 registered replication reproduced magnitude and duration closely but shifted the function ~one lag earlier and found smaller lag-1 sparing (original Δ=30% accuracy; replication 18% and 14%) ([Grassi et al. 2020](https://doi.org/10.3758/s13428-020-01457-6)). Report **T2 detection dropping to ~40–60% of its no-T1 baseline**.
- **Lag-1 sparing:** T2 immediately after T1 often spared (~80%); neither necessary nor sufficient for an AB, modulated by task switching (Visser et al. 1999, via MacLean & Arnell), smaller in the replication.
- **The blink is attentional, not sensory:** the identical-stimulus ignore-T1 control shows no deficit (both papers). Minority task-switching account of the original (McLaughlin et al. 2001, via Grassi et al.) reported for completeness.

**Meaning:** the system consolidating a target into reportable memory is refractory for a few hundred ms. Two important events within ~0.5 s → the second is missed ~40–50% of the time if the first captured attention.

### 4.2.3 Flanker effects

Flanker congruency effect (FCE): responses to a central target are slower/less accurate with response-incongruent flankers ([Eriksen & Eriksen 1974 lineage; review in Moore et al. 2020](https://doi.org/10.3758/s13414-020-02094-z)).

- **Extent:** significant interference from incompatible letters within **~1°** (classic); spread to 3° and 5° under other conditions (§4.1.3 sources). Cohen & Ivry locate proximity limits for unattended feature migration at ~1° too (§4.7).
- **Resolution time:** FCE shrinks with cue-target SOA — early, both near and distant flankers interfere; as the zoom lens narrows, distant flankers drop out first, near flankers persist ([Eriksen & St. James 1986](https://doi.org/10.3758/bf03211502); replications in [Deakin 2023](https://doi.org/10.31234/osf.io/4maqs)); the selection gradient tightens over roughly the first **100–200 ms** of a trial.
- **Caveat:** Moore et al. argue target-flanker similarity effects "reflect image segmentation, not perceptual grouping" — color/spacing changes alter the input representation's quality. Rendering: **segmentation cues between attended object and neighbors reduce flanker interference**, consistent with §4.1.2.

### 4.2.4 Visual search: feature, conjunction, guided search

- **Feature** slopes "near (but typically a little greater than) 0 ms/item" — pop-out; even basic feature searches grow with the *log* of set size, so "completely flat 0 ms/item slopes" do not exist ([Wolfe 2021](https://doi.org/10.3758/s13423-020-01859-9); [PDF](https://search.bwh.harvard.edu/new/pubs/Wolfe2021_GS6.pdf)).
- **Conjunction:** ~**20–40 ms/item present** for hard searches (T-among-L), **~90 ms/item absent** for 2-among-5s; when acuity+crowding force serial foveation (TLT triplets), **~250–350 ms/item absent / half present** — set by the eyes' 3–4 fixations/s ([Wolfe 2021](https://doi.org/10.3758/s13423-020-01859-9)). The parent's "10–25 ms/item" is the easy end: honest range **~5–40 present / 10–90 absent** by guidance — Wolfe, Cave & Franzel 1989 found many subjects' conjunction slopes "virtually flat" (0.4–5.2 ms/item in the best conditions; [Guided Search 1](https://doi.org/10.1037/0096-1523.15.3.419)).
- **Guided search** resolves the FIT puzzle: parallel preattentive feature maps *guide* serial deployment — a red-vertical target among red-horizontal + green-vertical requires attending only the red subset (halving effective set size); triple conjunctions can be nearly flat (0.4 ms/item) because three guides beat two ([Wolfe et al. 1989](https://doi.org/10.1037/0096-1523.15.3.419); [Wolfe 2021](https://doi.org/10.3758/s13423-020-01859-9)).
- **Guidance takes time:** full effectiveness of even basic color guidance takes 200–300 ms (Palmer et al. 2019, via Wolfe 2021). If selection were item-serial, 20–40 ms/item implies ~20 Hz selection — faster than the 150–300 ms per-object recognition time — so GS6 proposes pipeline parallelism: items *enter* processing every 50–60 ms but each takes several hundred ms to be recognized. Real-world search efficiency depends on scene statistics and guidance quality, not a fixed slope. The ~25 guiding attributes are enumerated in [Wolfe & Horowitz's review](https://pmc.ncbi.nlm.nih.gov/articles/PMC9879335/).

---

## 4.3 What fixation does NOT give you: crowding and the periphery's real limits

### 4.3.1 Bouma's law

Bouma 1970: lowercase letters (x-height ~0.2°) at 2–7° eccentricity flanked by x's; identification required "no other letter... within 0.5 times the eccentricity" ([re-analysis in Coates et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC8556556/)). Modern statement:

$$\boxed{\hat{s} = b\,(\varphi + \varphi_0),\quad b \approx 0.4{-}0.5,\ \varphi_0 \approx 0.5^\circ\ \text{(Bouma law, mk 2)}}$$

($\hat s$ = center-to-center critical spacing, $\varphi$ = eccentricity, $b$ = Bouma factor; [Kurzawski et al. 2023](https://doi.org/10.1167/jov.23.8.6).) Verified locally (pwsh): critical spacing at 2.5°/5°/10°/20° eccentricity = 1.0/2.0/4.0/8.0° for $b=0.4$; 1.25/2.5/5.0/10.0° for $b=0.5$; 2.0/4.0/8.0/16.0° for $b=0.8$.

**The modern debate — the spread:** Bouma first said 0.5, later 0.4–0.5 (Andriessen & Bouma 1976, via Kurzawski et al.). Pelli et al. 2004's task review lists "roughly between 0.3 and 0.7"; Strasburger & Malania's compilation spans **0.13–0.7**; tangential crowding runs 0.1–0.2; Chung et al. 2001 ~0.3 ([all via Strasburger 2020](https://doi.org/10.1177/2041669520913052) and [Coates et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC8556556/)). The largest modern survey (50 observers × 13 spacings × 2) fits the two-parameter law to **82% of variance** cross-validated, with standardized radial Bouma factor **0.24 (this study) vs 0.35 (Bouma 1970) vs 0.30 (geometric mean of five modern studies)** — "good agreement across labs, including Bouma's" once criteria and flanker placement are standardized ([Kurzawski et al. 2023, PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC10408772/)). Its asymmetries (crowding-distance ratios): **0.55 tangential:radial, 0.62 horizontal:vertical, 0.79 lower:upper, 0.78 right:left, 0.78 Sloan:Pelli font** — the radial/tangential anisotropy (~2× worse radially) is the largest and most replicated. Engineering statement: **$b \approx 0.3$–0.5 radial / ~0.15–0.25 tangential**, with 0.5×E the conservative bound.

### 4.3.2 Why crowding is cortical, not optical or retinal

- **Size and contrast don't matter; spacing does.** Critical spacing is independent of target size (fivefold change → <15% critical-distance change: Tripathy & Cavanagh 2002, via [Strasburger 2020](https://doi.org/10.1177/2041669520913052)) — optical blur would predict size-dependence.
- **Radial-tangential anisotropy, inner/outer asymmetry.** Crowding zones are "egg-shaped towards the retinal periphery" (Bouma's own words, via [Pelli & Tillman revision, JOV](https://jov.arvojournals.org/article.aspx?articleid=2212997)): the *outer* (more peripheral) flanker has much more effect — expected, since crowding distance grows with eccentricity (Banks et al. 1977; Bex & Dakin 2005; via Kurzawski et al.). No optical/photoreceptor mechanism produces an egg pointing outward; an eccentricity-scaled cortical grouping field does.
- **Constant cortical distance.** $b \approx 0.4$ corresponds to "a constant length of approximately 6 mm of cortex in V1," matching "the length of horizontal connections in V1" (Gilbert & Wiesel) — though the relation holds only above 5–10° eccentricity ([Pelli & Tillman 2008 via Coates et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC8556556/); [Strasburger 2020](https://doi.org/10.1177/2041669520913052)).
- **Dissociation from acuity.** Crowding's $E_2$ = **0.45°** vs acuity's **2.72°** — more than 5× different, "inconsistent with a common cause" ([Kurzawski et al. 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC10408772/), citing Latham & Whitaker 1996). Crowding grows ~30× steeper with eccentricity than MAR (Strasburger 2020).
- **Scale-free window, with the modern revision:** critical spacing is conserved across object kind and size for one-part objects (Bouma law mk 2); the current revision (mk 3) states critical spacing is "equal across *parts*, not objects" — flipping a lopsided flanker reduces crowding without changing center-to-center spacing ([Pelli & Tillman, JOV](https://jov.arvojournals.org/article.aspx?articleid=2212997)).

### 4.3.3 The face-at-10° dissociation, worked numerically

Why can't you recognize a face at 10° even though acuity there is ~20/60–20/116 (movement document §3.1.3)? Worked with pwsh:

- A 15-cm face subtending 5° sits at ~1.72 m; the ~5-cm eye–nose spacing subtends **1.67° = 100 arcmin**.
- MAR at 10° ≈ 5.8 arcmin (20/116). The facial feature is **~17× the local resolution limit** — *resolvable*.
- But critical spacing at 10° is $b \times 10° = 4$–5°, and facial features sit ~0.5–2.3° apart. **Every feature of the face falls inside every other feature's crowding zone.** Features are resolved but cannot be *bound* — the signature of crowding as a grouping, not resolution, failure.

Documented directly: crowded-face/house identification thresholds at 10° are ~2.7° of required target size — "6× larger than first-order single-letter acuity" there ([Chung et al., via Coates et al.'s table](https://pmc.ncbi.nlm.nih.gov/articles/PMC8556556/)). Design rule: **beyond ~5–10°, players can detect that something is there (contrast, motion, color patches) but cannot identify which member of a similar set it is** unless neighbors are separated by more than $0.5 \times$ eccentricity.

### 4.3.4 Peripheral gist vs identification failure

- **Gist is extracted astonishingly fast.** RSVP of 6–12 novel scenes at 13–80 ms/picture: detection of a named target above chance at **13 ms/picture**, even when the name came only *after* the sequence ([Potter et al. 2014](https://mollylab-1.mit.edu/sites/default/files/documents/FastDetect2014withFigures.pdf)). Scene-property thresholds for 75% correct: **19–67 ms**, asymptote ~100 ms (63–123) — natural-vs-manmade at 19 ms, basic-level categories at 30–67 ms ([Greene & Oliva 2009](https://pmc.ncbi.nlm.nih.gov/articles/PMC2742770/)). Thorpe-lineage animal detection: ERP divergence ~150 ms, above-chance masked detection from ~12 ms SOA, asymptote ~44 ms (Bacon-Macé et al., via Potter et al.). Parent's "100–150 ms" is the *completion* time; the modern above-chance range is **~13–80 ms**, thresholds 19–67 ms.
- **But gist is attention-loadable.** Li et al. (2002) claimed near-absent-attention gist; that fails for complex scenes: categorization of scenes with **four** foreground objects is "greatly impaired when attention resources are limited... even when scenes are presented for 500 ms"; single-object gist is only mildly impaired ([JOV study](https://jov.arvojournals.org/article.aspx?articleid=2122328)). Report both.
- **Gist memory decays in seconds** unless consolidated — "about 500 ms to think about the scene"; conceptual masking at SOAs up to 500+ ms ([Potter 2012](https://doi.org/10.3389/fpsyg.2012.00032)).
- **The periphery as change/motion detector feeding target selection** is movement document Part 3 §3.2.2 (far periphery as "sentinel"; [Vater et al. 2022 review](https://pmc.ncbi.nlm.nih.gov/articles/PMC9568462/) cited there). The crowding numbers are the *why*: peripheral identification bandwidth is a few coarse low-SF channels plus motion/luminance transients — ideal for *where/what-changed*, useless for *which-one*.

---

## 4.4 The fixation-centered visual field phenomena

### 4.4.1 Troxler fading

Troxler (1804): steady fixation makes peripheral stimuli fade. Conditions: **low contrast + periphery + stationary retinal image** (blurred edges, low luminance, optical stabilization accelerate; faster on horizontal than vertical meridians; faster under scotopia than rod density predicts) ([Proudlock et al. 2006](https://www.sciencedirect.com/science/article/pii/S004269890600438X)).

- **Time to fade:** the honest range is wide — classic low-contrast peripheral targets fade over **~5–10 s** of strict fixation (parent's anchor, consistent with Clarke's log-time recovery curves: an early rapid phase = recovery from Troxler proper, a later slow phase = ordinary light adaptation; [Clarke 1960-lineage](https://doi.org/10.1080/713826370)); with very low contrast or blurred edges fading can begin within **1–2 s**; under free viewing it rarely completes.
- **Mechanism — the debate:** (a) eye-movement/RF account (Clarke & Belcher 1962): fixational movements refresh cells at stimulus edges more effectively centrally because central RFs are smaller relative to drift; (b) ganglion-cell account (Bachy & Zaidi 2014): RGC adaptation time constants show **no eccentricity effect between 2 and 12°** electrophysiologically — the eccentricity dependence comes from eye-movement interactions with magno/parvo/konio RF structure; flicker (simulating eye movements) slows peripheral fading for the chromatic axes ([Bachy & Zaidi 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC4411984/)); (c) cortical surface-representation account: fading and filling-in "are better understood as the result of active cortical processes related to surface representation" (Komatsu 2006; De Weerd et al., via Proudlock et al.), with parietal lesions *accelerating* and frontal lesions *preventing* fading (Mennemeier et al.), and attention to a target increasing its fading likelihood (Lou 1999; De Weerd et al.). Synthesis: **peripheral static percepts fade because nothing in the adapted channel signals change; the fovea is protected by small RFs relative to fixational drift; filling-in is active cortical completion.**
- **Why we don't fade in daily life:** fixational eye movements (drift ~0.1–0.3°/s, microsaccades, tremor — agent 3's domain). Verified: 5–10 s of 0.1–0.3°/s drift sweeps the image across 30–180 arcmin — many peripheral RFs, an endless supply of transients. An engine rendering a perfectly still low-contrast peripheral texture is *manufacturing Troxler conditions*.

### 4.4.2 Motion-induced blindness (MIB)

A high-contrast static target surrounded by a moving pattern (rotating grid of "+"s) disappears and reappears intermittently — reported invisibility commonly ~30–60% of inspection time within the first seconds, time-to-first-fade of a few seconds ([PLOS ONE study](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0092894)). Parent's "~50% within seconds" sits inside the reported band.

**The MIB-vs-Troxler dissociation:** varying target contrast 8–80% produced *opposite* trends — increasing contrast **doubled** the disappearance rate in MIB but **halved** it in Troxler; mean invisible periods decreased equally with contrast in both ([PLOS ONE 2014](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0092894)). Interpretation: both share a contrast-adaptation component, but MIB is additionally governed by **neuronal competition between target- and mask-processing populations** — "similar to binocular rivalry," a bistable phenomenon — whereas Troxler is adaptation-dominated. Report the debate: Gorea & Caetta proposed MIB fully reducible to adaptation + prolonged inhibition of static by moving stimuli; the opposite contrast trends refute the pure gain-control version (both accounts in the PLOS ONE paper). Perceptually: Troxler = adaptation swallowing an unchanging signal; MIB = a salient static object losing a winner-take-all contest against a moving surround. Rendering-relevant: a stationary enemy silhouette against moving foliage/water can vanish from awareness even at high contrast.

### 4.4.3 Aftereffects

- **Motion aftereffect (MAE, waterfall illusion).** Purkyně 1820 (cavalry parade); Addams 1834 at the Falls of Foyers — after ~30–60 s of fixation on descending water, the rocks "appeared to be in motion upwards, and with an apparent velocity equal to that of the descending water" ([history: Wade et al. 2018](https://journals.sagepub.com/doi/10.1177/0301006618774645)). Mechanism: direction-selective neurons adapted by motion reduce both evoked and spontaneous firing, tilting the balance among direction channels toward the opposite (Barlow & Hill 1963: rabbit RGC firing declined over 15–20 s of motion then fell *below baseline* ~30 s after — the single-cell correlate; via [Mather, Verstraten & Anstis 1998](https://anstislab.ucsd.edu/files/2012/12/1998-the-motion-aftereffect.pdf)). **Duration ~10–60 s** confirmed: MAE duration is a *square-root function of adaptation duration* (verified: 60 s vs 15 s adaptation → 2.0× MAE duration) and shows **storage** (closing the eyes between adaptation and test preserves it) ([Mather et al. 1998](https://anstislab.ucsd.edu/files/2012/12/1998-the-motion-aftereffect.pdf)). Multiple cortical sites (up to five, V1→MST); TMS over V5/MT shortens both MAE and its storage ([Mather et al. reloaded](https://pmc.ncbi.nlm.nih.gov/articles/PMC3087115/)); static MAE is lower-level (partly monocular, ~30% interocular transfer), dynamic MAE higher-level (binocular).
- **Tilt aftereffect (TAE).** Adapting to an oriented grating shifts a subsequent test's perceived orientation *away* (direct/repulsion, 0–50° differences) or *toward* (indirect/attraction, >50°). Direct effect peaks at 10–20° separation at **~4°** (Mitchell & Muir 1976); indirect peaks at 75–80° at **~0.5°** (up to ~2.5° between subjects) ([canonical numbers in the Glass-pattern TAE paper](https://preview-www.nature.com/articles/srep23567); Mitchell & Muir data in [Bednar's thesis](https://nn.cs.utexas.edu/web-pubs/bednar.thesis/node29.html)). Parent's "1–3°" is best reported as **direct ~2–7° at peak, indirect ~0.5–2.5°**. The simultaneous tilt *illusion* behaves homologously (repulsion ~5°+ at 15° separation; attraction at large separations), and both shrink when center and surround are segmented by contrast/depth cues ([segmentation study, JOV](https://jov.arvojournals.org/article.aspx?articleid=2193837)).
- **Size/depth aftereffects** exist in the same family (adaptation to large/small stimuli shifts perceived size; disparity-defined depth aftereffects) — the class is read as gain-control/calibration, "if it adapts, it's there" (Mollon, via Mather et al.): adaptation decorrelates and recenters responses rather than fatiguing them.
- **Physical afterimages vs cortical aftereffects:** retinal afterimages come from photopigment bleaching (positive then negative complement; seconds to tens of seconds) and are *retinotopic* — they move with the eye. Cortical aftereffects (MAE, TAE, contrast, face aftereffects) survive retinal repositioning to a degree set by the adapted population's RF size, can show interocular transfer (proving post-retinal sites), and follow adaptation dynamics rather than photochemistry. Bachy & Zaidi's color-afterimage method exploits exactly this to measure adaptation speed along the three ganglion-cell classes ([Bachy & Zaidi 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC4411984/)).

### 4.4.4 The flash-lag effect

A flash physically aligned with a moving object is perceived as *lagging* behind it ([Nijhawan 1994](https://doi.org/10.1038/370256b0): the strobed moving rod). **Magnitude:** the moving object's perceived lead corresponds to **~50–100 ms** of its travel (integration/latency windows estimated from ~10–20 ms to ~80 ms in the postdiction account, ~50 ms to ~500 ms in the averaging account — the range documented in [Hogendoorn 2020](https://www.jneurosci.org/content/40/30/5698)); verified locally, 50–100 ms at 10°/s = **0.5–1.0°** of spatial offset — clearly visible.

**Three accounts, all live:** (1) *motion extrapolation* (Nijhawan): the system predicts moving objects forward to compensate ~70 ms retino-cortical delay; a 2020 review argues behavioral, computational, and neuroimaging evidence now converges on real extrapolation mechanisms causally involved in the FLE ([Hogendoorn 2020](https://www.jneurosci.org/content/40/30/5698)); (2) *differential latency* (Whitney, Murakami & Cavanagh): moving objects processed faster than flashes — but temporal-order judgments show flashes processed at least as fast, and flash *lead* occurs with high-luminance flashes, data the simple version cannot explain ([Science exchange](https://papers.cnl.salk.edu/PDFs/Flash-Lag%20Effect_%20Differential%20Latency,%20Not%20Postdiction%202000-3149.pdf)); (3) *postdiction / temporal averaging* (Eagleman & Sejnowski): "the percept attributed to the time of the flash is a function of events that happen in the approximately 80 milliseconds after the flash" — the flash resets an integration window and position information *after* the flash is assigned backward ([Eagleman & Sejnowski 2000](https://doi.org/10.1126/science.287.5460.2036)).

**Meaning for temporal reconstruction:** the visual system does not stamp percepts with arrival time. It back-dates or forward-extrapolates using a window of surrounding context — why the world appears simultaneous despite ~70+ ms processing delay, and why chronostasis (§4.5.3) works the same way in the time domain. Rendering: fast projectiles and flashes rendered *exactly* aligned will be perceived misaligned in the direction of motion.

---

## 4.5 Transsaccadic perception and continuity

### 4.5.1 Transsaccadic memory is sparse

"A visually veridical sensory image of a scene is not retained and fused across saccades" ([Henderson & Hollingworth 2003](http://jhenderson.org/vclab/PDF_Pubs/Henderson_Hollingworth_PP_2003.pdf), citing Irwin 1991/92; Pollatsek & Rayner 1992). What survives:

- **Contours do not integrate:** complementary object contours across two fixations were "at best inconsistently detected," and contour changes had "no influence on object identification" ([Psych Sci 1997](https://journals.sagepub.com/doi/10.1111/j.1467-9280.1997.tb00543.x)).
- **Capacity 3–4 integrated objects, same as VWM.** Delayed comparison across an intervening saccade vs within fixation: identical accuracy; statistical-model fit puts "the estimated numerical memory capacity in both the fixation and saccade tasks... three to four items," with matching storage durations and mask resistance — "transsaccadic memory and VWM share essentially the same storage mechanisms" ([Prime et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC3030828/)).
- **But abstract visual representations of attended objects do survive:** token substitutions (one exemplar for another of the same category, during a saccade) detected well above chance (48.8% correct, 1.9% false alarms, fixating toward the object; 37.4% even when away) — "visually specific representations can be preserved across saccades" for attended objects ([Henderson & Hollingworth 2003](http://jhenderson.org/vclab/PDF_Pubs/Henderson_Hollingworth_PP_2003.pdf)).
- **Global image information does not survive even when attended:** reversing scene strips with gray occluders during saccades was "almost impossible to detect" — **global transsaccadic change blindness**; "point-by-point visual representations are not functional across saccades" ([Henderson & Hollingworth, Psych Sci 2003](https://journals.sagepub.com/doi/10.1111/1467-9280.02459)).
- **Function:** VSTM holds the saccade target's features to "rapidly reacquire [it] after an errant saccade" — memory-based gaze correction is "accurate, fast, automatic, and largely unconscious," disrupted by concurrent VSTM but not verbal load ([Hollingworth, Richard & Luck 2008](https://pmc.ncbi.nlm.nih.gov/articles/PMC2784885/)). Transsaccadic memory exists to keep the fixation cycle running, not to build a picture.

### 4.5.2 Spatiotopic remapping and its perceptual consequences

Duhamel et al. (1992): LIP neurons respond to stimuli in their **future fields** — the post-saccadic RF location — from 100–200 ms *before* saccade onset; remapping since found in V2/V3/V3A, V4, FEF, SC, MST and human imaging ([Visual Remapping review](https://pmc.ncbi.nlm.nih.gov/articles/PMC9255256/); [Marino & Mazer 2016](https://pmc.ncbi.nlm.nih.gov/articles/PMC4743436/)).

- **Forward vs convergent — the live disagreement.** Zirnsak et al. (2014), dense FEF mapping: pre-saccadic RFs shift *toward the saccade target* (convergent), not by the full vector (forward), arguing forward remapping was an under-sampling artifact, with convergent remapping manifesting as unspecific attentional spread around the target. The 2023 delayed-saccade study resolves it temporally: delay-period shifts toward the target (attention-driven, convergent), perisaccadic shifts "predominantly... toward the post-saccadic RF locations" with amplitudes approaching the saccade vector (corollary-discharge-driven, forward) — and networks trained to update retinal positions grow *both* connection types automatically ([Wang et al. 2023](https://doi.org/10.1101/2023.09.23.558993)).
- **Perceptual consequence 1 — predictive remapping of attention.** Discrimination benefits appear at the *remapped* (post-saccadic) location of a pre-saccadic cue, parallel and opposite to the saccade vector (Rolfs et al. 2011, via the reviews). But remapping is *time-gated*: attention maps show it only when the cue preceded saccade onset by **more than ~175 ms** — compatible with the ~100 ms minimum attentional deployment ([Szinte et al., eLife 2018](https://elifesciences.org/articles/37598)). Remapping is also incomplete: a *retinotopic attentional trace* — facilitation at the old retinotopic location — persists *after* the saccade, so briefly there are **two spotlights** (the dual-spotlight theory; the trace costs incorrect foci, feature-binding errors, poorer spatial memory/reaching — [Golomb-lineage review](https://pmc.ncbi.nlm.nih.gov/articles/PMC9255256/)).
- **Perceptual consequence 2 — feature transfer.** Form adaptation transfers from current fixation to future gaze position *before* the eyes move, affecting intermediate positions too — a mechanism for the "smooth transition, with no temporal delay, of visual perception across glances" ([Melcher, Nat Neurosci 2007](https://www.nature.com/articles/nn1917)).

### 4.5.3 Saccadic chronostasis (perception side)

Agent 3 owns the motor timeline; the perceptual mechanism is postdiction. Glance at a ticking clock and the second hand seems frozen: observers "extend the percept of the saccadic target backwards in time to just before the onset of the saccade" ([Yarrow et al., Nature 2001](https://www.nature.com/articles/35104551)). For 22° saccades (mean 72 ms) and 55° saccades (139 ms), the first post-saccadic digit was matched to 1 s when actually seen for 880 and 811 ms — overestimate grows ~1:1 with saccade duration, back-dating the percept to **~50 ms before saccade onset** (up to ~120 ms in some conditions) ([Yarrow et al. 2001](https://www.nature.com/articles/35104551); [review](https://kielanyarrow.github.io/MyPage/papers/Chronostasis_Review.pdf)). The effect is independent of stimulus duration (100–1333 ms), survives pre-saccadic attention shifts, occurs across all saccade categories (self-timed, antisaccades, express — implying a subcortical/SC efferent trigger; [Yarrow et al. 2004](https://doi.org/10.1162/089892904970780)), and **disappears if the target visibly jumps during the saccade** — it requires the target's apparent spatial continuity. Interpretation: temporal back-dating fills the gap saccadic suppression opens; the time-domain twin of flash-lag postdiction.

### 4.5.4 Change blindness as the transsaccadic-sparseness assay

Saccade-contingent change paradigms (no transient at change): **gist and layout survive; details don't.** Identity changes within an attended object are detected well above chance; token and global image changes are missed at high rates (numbers above; plus the flicker/mudsplash literatures in the movement document §3.3.2 — Rensink's review there puts the parallel-attention limit at "4–5 items at a time"). Once detected, a change is "thereafter very apparent" — the signature that the limit is representational, not sensory. Game terms: **anything that can change during a saccade, blink, or 80 ms occlusion without a transient can change invisibly.**

---

## 4.6 Attention to objects vs. space

### 4.6.1 Object-based attention: the same-object advantage

Egly, Driver & Rafal (1994): two rectangles, cue one end (75% valid); on invalid trials the target appears at the far end of the *cued* rectangle or the equidistant far end of the *uncued* rectangle. RTs: cued end < uncued end of cued object < uncued object — the last difference, between *equidistant* locations, is the object effect ([summarized in Law & Abrams 2002](https://doi.org/10.3758/bf03194753)). **Magnitude:** ~**11–15 ms** across Law & Abrams' five experiments (15 ms uncued; 11–13 ms under exogenous, endogenous, and partial-object cues; disappearing only at very short durations before segmentation completes); the broader literature reports up to ~30–80 ms depending on task, with documented strategy-driven reversals (mental rotation/translation can produce a *different-object* benefit — [Acta Psychol 1999](https://www.sciencedirect.com/science/article/abs/pii/S0001691899000219)). Report **~10–30 ms robust, up to ~80 ms in some tasks**, matching the parent's anchor at its upper end. Mechanism partly disengage-cost: between-object shifts require object-based disengagement that within-object shifts skip ([2007 study](https://pubmed.ncbi.nlm.nih.gov/17727114/)).

### 4.6.2 Tracking multiple objects (MOT)

Pylyshyn & Storm (1988): flash a subset of identical moving objects, track them through random motion. Observers "can accurately track approximately four objects and... once this limit is exceeded, accuracy declines precipitously" ([ERP review citing the canon](https://pmc.ncbi.nlm.nih.gov/articles/PMC2927139/); [Scholarpedia MOT](http://scholarpedia.org/article/Multiple_object_tracking)). FINST theory: ~4 pre-attentional indexes that "stick" to objects independent of properties; identity is *not* tracked well ([Pylyshyn 2004](https://www.tandfonline.com/doi/abs/10.1080/13506280344000518)). **The revision:** the 4-object limit is not architectural — "At slow speeds it is possible to track up to 8 objects, and yet there are fast speeds at which only a single object can be tracked": capacity is set by a flexibly allocated resource whose per-object share determines selection resolution; more/faster objects → coarser windows → distractor confusions ([Alvarez & Franconeri 2007](https://jov.arvojournals.org/article.aspx?articleid=2121950)). Honest statement: **~4 typical, 1–8 by speed and spacing** — the parent's "~4–5" confirmed as the standard-condition value. Attention is *optional* for light-load tracking but recruited for hard tracking ([ERP study](https://pmc.ncbi.nlm.nih.gov/articles/PMC2927139/)).

### 4.6.3 Animacy, threat, face detection — with the disagreements

- **Ultra-rapid face detection is real:** choice saccades to a face among two pictures "as short as 100 ms, with a mean time of 140 ms" (Crouzet et al. 2010, via [Potter 2012](https://doi.org/10.3389/fpsyg.2012.00032)).
- **Anger-superiority ("face in the crowd") is contested — both sides.** *For:* Hansen & Hansen 1988's pop-out claim (~60 ms/item slope for happy among angry vs ~2 ms for angry among happy) was traced to a dark-spot artifact (Hampton et al. 1989; Purcell et al. 1996 — history in [the schematic-face study](https://pmc.ncbi.nlm.nih.gov/articles/PMC1839771/)). With controlled schematic faces, angry targets among neutral crowds *are* found faster with lower slopes — fast but not pop-out (same source); with real photographs the asymmetry favors angry faces, driven by the mouth, surviving thatcherization — a *perceptual* basis ([PubMed](https://pubmed.ncbi.nlm.nih.gov/16768552/)); eye-tracking shows fewer distractors fixated before the first fixation on angry targets (target-orienting, not distractor-processing; [PLOS ONE 2014](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0093914)). *Against:* faces from the *same database* split by stimulus set — happiness superiority for one set of posers, anger superiority for another, with no average image statistic explaining it ([Savage, Becker & Lipp 2015](https://doi.org/10.1080/02699931.2015.1027663)); neural-network simulations suggest a purely perceptual bias acts *against* anger detection, and a parsimonious perceptual account may explain the results without threat modules ([2008](https://pubmed.ncbi.nlm.nih.gov/19128799/)). **Synthesis:** faces are detected pre-attentively within ~150 ms; whether *threatening* faces enjoy genuine attentional priority is stimulus-set-dependent — the safe claim is a robust face-detection advantage and an unreliable, low-level-feature-mediated anger advantage.
- **Gaze cueing.** Friesen & Kingstone 1998: a schematic face's *nonpredictive* gaze shift speeds detection/localization/identification at the gazed-at location — significant at 105, 300, 600 ms SOA, gone by 1005 ms, with benefits and *no costs* (neutral = uncued), the exogenous signature ([Friesen & Kingstone 1998](https://doi.org/10.3758/bf03208827)). **Magnitude:** ~10–20 ms cued-vs-uncued at peak SOAs (e.g., 311 vs 323 ms at 300 ms SOA, detection). The effect persists under *counterpredictive* gaze — observers orient to the gazed-at location even when the target is 4× more likely elsewhere, concurrently with voluntary orienting to the predicted location ([counterpredictive study](https://web.uvic.ca/~dbub/Cognition_Action/Topics2021_files/Attentional%20Effects%20of%20Counterpredictive%20Gaze%20and%20Arrow%20Cues.pdf)) — and is *more strongly reflexive than arrows* (arrows lose the reflexive component when counterpredictive; split-brain evidence: gaze orienting lateralized to the face-processing hemisphere, arrow orienting bilateral; [Ristic et al. 2002](https://doi.org/10.3758/bf03196306)). Gaze cues characteristically produce facilitation *without* IOR ([Frischen, Bayliss & Tipper 2007](https://doi.org/10.1037/0033-2909.133.4.694)). **Duration disagreement:** classic accounts call gaze cueing short-lived, but with long-duration cues small validity effects (5–10 ms) persist to 600–1000+ ms — cue duration, not just SOA, sets the time course ([PMC 3981892](https://pmc.ncbi.nlm.nih.gov/articles/PMC3981892/)).

---

## 4.7 The binding problem and feature integration

- **Feature integration theory (FIT).** Features (color, orientation, SF, motion direction) are "registered early, automatically, and in parallel across the visual field"; objects are identified later, requiring focused attention; "focal attention provides the 'glue' which integrates the initially separable features into unitary objects" ([Treisman & Gelade 1980](https://www.cs.princeton.edu/courses/archive/spring08/cos598B/Readings/TreismanGelade1980.pdf)).
- **Illusory conjunctions — conditions and rates.** With attention diverted (digit-report priority) or overloaded: features migrate and recombine. Treisman & Schmidt-style displays (four shapes varying color/size/format, flanked by digits, post-cued): conjunction errors in **~18%** of trials vs ~6% feature intrusions (3:1 against guessing); with colored letters, **more than 30%**; with a pre-cue allowing focused attention, binding errors dropped to the intrusion rate (~10% vs 12%) — "spatial attention plays a role specifically in the binding process" ([Treisman 1998](https://princetonuniversity.github.io/NEU-PSY-502/_static/pdf/Class%2018/Treisman1998.pdf)). Neuropsychological existence proof: Balint's patient R.M. made binding errors on **more than 35%** of two-letter trials even at 10 s exposures, feature errors <10% (same source). Structural findings: conjunction rates unaffected by inter-object similarity; instance-count barely matters (1.5:1 observed vs 3:1 token prediction — features migrate as *types*, not tokens); spatial proximity matters (Cohen & Ivry: unattended-item conjunctions mainly within ~1° — [Cohen & Ivry 1989](https://doi.org/10.1037//0096-1523.15.4.650)).
- **The attention-binding link is not airtight — the dissent.** Strong attentional manipulations (valid/invalid cueing of the target location) left conjunction rates unaffected, and mixed attended/unattended conjunctions were as frequent as unattended-only — "inconsistent with the feature-integration theory" ([Tsal, Meiran & Lavie 1994](https://doi.org/10.3758/bf03207605)). Non-attentional binding evidence: McCollough-effect integration without attention (Houck & Hoffman 1986), organizational/linguistic modulation (Prinzmetal-lineage, via Tsal et al.). Modern consensus: FIT is approximately right as a first law — attention strongly reduces but does not uniquely gate conjunctions; grouping statistics do part of the work.
- **Binding-by-synchrony and its critics.** The Singer & Gray (1995) 40-Hz phase-synchronization hypothesis is now substantially undercut: "enhanced neuronal firing rates bind features into coherent object representations, whereas oscillations and synchrony are unrelated to binding" ([Neuron 2023 review](https://www.sciencedirect.com/science/article/pii/S089662732300212X)). Report both the hypothesis and its current status.
- **VWM constrains what we keep from each fixation.** Luck & Vogel 1997: change detection near-perfect for 1–3 items, declining from 4 to 12; capacity "roughly four items" for colors, same for orientations; and four color+orientation *conjunction* objects are retained as well as four single features — **up to 16 features across 4 objects** — "visual working memory stores integrated objects rather than individual features" ([Luck & Vogel 1997](https://awhvogellab.com/files/pdfs/luck_1997_capacity-features-conjuctions.pdf)). Controls ruled out verbal memory, encoding time, and decision limits. The 3–4 modal / 1–5 spread estimate is defended in the movement document §3.3.3 (Cowan 2001). Verified locally: at 3 Hz fixation rate and $k = 3.4$, the cycle delivers **~10 attended objects per second** — everything else contributes gist and transients only.

---

## 4.8 Synthesis: the fixation cycle as the fundamental unit of vision

A representative scene-viewing fixation lasts **~250–330 ms** (mode ~230; range <50 to >1000 — movement document §3.4.1), the saccade ~20–50 ms: a **2–3.3 Hz cycle** (verified: 250 + 50 ms → 3.3 Hz). One cycle:

1. **Pre-saccadic (last ~50–100 ms):** attention has already shifted to the next target (movement document §3.3.4); remapping begins — LIP/FEF future-field responses, attention benefits at the remapped location only if the cue preceded saccade onset by >~175 ms ([Szinte et al. 2018](https://elifesciences.org/articles/37598)); form adaptation transfers to the future gaze position ([Melcher 2007](https://www.nature.com/articles/nn1917)).
2. **Saccade (~20–50 ms):** sensitivity suppressed 0.4–1.1 log units (movement document §3.4.1); percepts back-dated (chronostasis) and moving objects biased to appear leading (flash-lag).
3. **Fixation (~250–330 ms):** the fovea delivers ~1–2° of high-acuity sampling on the attended object; a suppressive surround (stimulus-driven ~1–3° near the fovea, eccentricity-scaled beyond; attentional Mexican hat) demotes the neighbors (§4.1.2); crowding sets the identification horizon at $b \times E \approx 0.3$–0.5× eccentricity (§4.3.1) — at 10°, nothing closer than 4–5° to another object is individuable; whole-scene gist extracted within the first 13–80 ms (§4.3.4); 3–4 integrated objects at most enter VWM (§4.7); and attentional-blink refractoriness means a second attentional target within ~180–500 ms of the first is ~40–50% likely to be missed (§4.2.2).
4. **Competition ends the fixation:** the next target is selected by the priority map — peripheral motion/looming transients (movement document §3.2.2), feature-guided search (§4.2.4), gaze cues (~10–20 ms reflexive benefits, §4.6.3), IOR tags on inspected locations (~6–10° gradient, 200 ms–3 s, §4.2.1) — and the cycle repeats.

**The complete answer to "what happens when you lock onto an object":**
- **At the target:** foveal acuity + attentional gain (contrast thresholds can drop four-fold with attention against a suppressive surround); binding of its features into an object token that survives the next saccade; entry into the 3–4-slot VWM.
- **Around it:** a measured suppressive annulus — stimulus-driven contrast suppression extending ~3× local V1 RF diameter (iso-oriented worst) plus the attentional Mexican-hat surround whose width scales with eccentricity. The neighbors are not merely unenhanced; they are actively demoted.
- **Beyond it:** crowding, not resolution, is the binding limit — features 17× the local MAR cannot be individuated inside each other's 0.4–0.5×-eccentricity crowding zones. The periphery contributes gist (13–80 ms), motion/transient detection, and target candidates — not identification.
- **Consequences of the fixation-centric architecture:** Troxler fading of static low-contrast periphery; MIB of static targets against moving surrounds; adaptation aftereffects bleeding across the next fixations; postdictive temporal reconstruction; change blindness for everything unattended.

For an engine: render the foveal 1–2° at full quality; spend the surround budget on motion, luminance transients, and coarse gist (the periphery's real currency); never rely on peripheral identification of similar objects; treat the player's ~10 objects/s attended throughput as the true information bottleneck; and remember the perceptual "now" is reconstructed 50–100 ms late and back-dated.

---

## 4.9 Consolidated boxed-equation summary + primary sources + Appendix A

### Boxed equations

$$\boxed{\text{Peripheral CS: } \sim 0.24\ \log_{10}\text{ units per } 10^\circ\ (\text{near periphery, mean over SF; nonlinear beyond } \sim 30^\circ)}$$

$$\boxed{\text{L/M (red-green) opponency behaviorally absent by } 25{-}30^\circ\ \text{(nasal); S/(L+M) robust to } \sim 20^\circ}$$

$$\boxed{\text{Attention profile} = \text{DoG};\ \sigma_{\text{surround}} \propto \text{RF size} \propto \text{eccentricity};\ \text{suppression latency} \approx 61\ \text{ms (V1)}}$$

$$\boxed{\text{Bouma: } \hat s = b(\varphi + \varphi_0),\ b \approx 0.3{-}0.5\ \text{radial},\ \varphi_0 \approx 0.5^\circ;\ E_2^{\text{crowd}} = 0.45^\circ \text{ vs } E_2^{\text{acuity}} = 2.72^\circ}$$

$$\boxed{\text{IOR: facilitation} < 250\ \text{ms} \to \text{inhibition } 200\ \text{ms}{-}3\ \text{s},\ \text{gradient} \approx 6{-}10^\circ}$$

$$\boxed{\text{AB: T2} \downarrow \text{ to } \sim 40{-}60\%\ \text{for lags } 2{-}5\ (\sim 180{-}500\ \text{ms});\ \text{lag-1 spared}}$$

$$\boxed{\text{Gist: above chance at } 13\ \text{ms};\ 75\%\text{-threshold } 19{-}67\ \text{ms};\ \text{asymptote} \approx 100\ \text{ms}}$$

$$\boxed{\text{Transsaccadic memory} \approx \text{VWM} \approx 3{-}4\ \text{objects};\ \text{cycle } 2{-}3.3\ \text{Hz} \Rightarrow \sim 10\ \text{attended objects/s}}$$

$$\boxed{\text{Chronostasis: back-dated} \sim 50\ \text{ms before saccade};\ \text{flash-lag} \equiv 50{-}100\ \text{ms} = 0.5{-}1.0^\circ\ \text{at } 10^\circ/\text{s}}$$

### Primary-source list (40+ distinct verified sources; all live this session)

1. [Adams et al., Peripheral CS, broad eccentricities, JOV 2015](https://doi.org/10.1167/15.12.95)
2. [Adams et al., CS nasal/temporal fields, JOV 2016](https://doi.org/10.1167/16.12.226)
3. [Kitakami et al., Luminance CSF in peripheral vision, 2025](https://doi.org/10.1002/col.70007)
4. [Anderson, Mullen & Hess, Peripheral resolution achromatic/chromatic, J Physiol 1991](https://physoc.onlinelibrary.wiley.com/doi/10.1113/jphysiol.1991.sp018781)
5. [Banks, Sekuler & Anderson, Peripheral spatial vision limits, JOSA A 1991](https://opg.optica.org/josaa/abstract.cfm?uri=josaa-8-11-1775)
6. [Mullen, Sakurai & Chu, L/M opponency in periphery, 2005](https://www.mvr.mcgill.ca/Kathy/PDF-05-09/Mullen-et-al-2005.pdf)
7. [Newton & Eskew, Peripheral chromatic detection/discrimination, 2003](https://doi.org/10.1017/s0952523803205058)
8. [McKeefry et al., Chromatic stimuli in peripheral retina, 2008](https://doi.org/10.15388/psichol.2008.0.2607)
9. [Zenger-Landolt & Heeger, V1 response suppression = surround masking, J Neurosci 2003](https://www.jneurosci.org/content/23/17/6884)
10. [Zenger-Landolt & Heeger, V1 explains lateral masking, JOV](https://doi.org/10.1167/2.7.128)
11. [Petrov & McKee, Spatial configuration of surround suppression, 2006](https://pmc.ncbi.nlm.nih.gov/articles/PMC1472811/)
12. [Schallmo, Murray & Bartlett, Time course of surround suppression, JOV 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC6464404/)
13. [Zenger & Sagi-lineage, Attentional effects with surround masks, Vis Res 2000](https://www.sciencedirect.com/science/article/pii/S0042698900002182)
14. [Fang, Ravizza & Liu, Attentional surround suppression in VWM, 2019](https://pmc.ncbi.nlm.nih.gov/articles/PMC6868316/)
15. [Yoo, Tsotsos & Fallah, Attentional Suppressive Surround, Front Neurosci 2018](https://www.frontiersin.org/journals/neuroscience/articles/10.3389/fnins.2018.00710/full)
16. [Flevaris & Murray, Feature-based attention modulates surround suppression, JOV 2015](https://doi.org/10.1167/15.1.29)
17. [Flevaris et al., Attention determines enhancement vs suppression, J Neurosci 2015](https://courses.washington.edu/viscog/publications/Flevaris_JNeurosci_2015.pdf)
18. [Task-dependent selectivity and surrounding context (SSVEP)](https://pmc.ncbi.nlm.nih.gov/articles/PMC6621517/)
19. [Eriksen & St. James, Zoom lens model, 1986](https://doi.org/10.3758/bf03211502)
20. [Moore et al., Flanker similarity = segmentation not grouping, 2020](https://doi.org/10.3758/s13414-020-02094-z)
21. [Deakin & Heinke, Noisy flanker tasks, 2023](https://doi.org/10.31234/osf.io/4maqs)
22. [Panis & Schmidt, When does IOR occur, 2022](https://doi.org/10.1515/psych-2022-0005)
23. [Hayward & Ristic, Posner paradigm and target probabilities, 2013](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2013.00205/full)
24. [Sun & Thomas-lineage, Environment/eye-centered IOR, Sci Rep 2015](https://www.nature.com/articles/srep16586)
25. [Computational models of the Posner tasks, 2015](https://www.frontiersin.org/journals/computational-neuroscience/articles/10.3389/fncom.2015.00081/full)
26. [Raymond, Shapiro & Arnell, Attentional blink, 1992](https://psych.hanover.edu/classes/cognition/papers/raymond%20et%20al%201992%20attn%20blink.pdf)
27. [Grassi et al., Two replications of Raymond et al., 2020](https://doi.org/10.3758/s13428-020-01457-6)
28. [MacLean & Arnell, AB measurement framework, 2012](https://doi.org/10.3758/s13414-012-0338-4)
29. [Wolfe, Guided Search 6.0, 2021](https://doi.org/10.3758/s13423-020-01859-9)
30. [Wolfe, Cave & Franzel, Guided Search 1, 1989](https://doi.org/10.1037/0096-1523.15.3.419)
31. [Wolfe & Horowitz, Five factors that guide attention](https://pmc.ncbi.nlm.nih.gov/articles/PMC9879335/)
32. [Strasburger, Seven Myths on Crowding, 2020](https://doi.org/10.1177/2041669520913052)
33. [Coates et al., Generality of critical spacing for crowded optotypes](https://pmc.ncbi.nlm.nih.gov/articles/PMC8556556/)
34. [Kurzawski et al., Bouma law in 50 observers, JOV 2023](https://doi.org/10.1167/jov.23.8.6)
35. [Pelli & Tillman-lineage, Bouma law revised: parts not objects, JOV](https://jov.arvojournals.org/article.aspx?articleid=2212997)
36. [Potter et al., Detecting meaning at 13 ms/picture, 2014](https://mollylab-1.mit.edu/sites/default/files/documents/FastDetect2014withFigures.pdf)
37. [Greene & Oliva-lineage, Briefest of Glances, 2009](https://pmc.ncbi.nlm.nih.gov/articles/PMC2742770/)
38. [Potter, Recognition/memory for briefly presented scenes, 2012](https://doi.org/10.3389/fpsyg.2012.00032)
39. [Ultra-rapid categorization requires attention: multi-object scenes, JOV](https://jov.arvojournals.org/article.aspx?articleid=2122328)
40. [Bachy & Zaidi-lineage, Troxler fading and RGC properties, 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC4411984/)
41. [Proudlock et al., Peripheral fading mono/binocular, Vis Res 2006](https://www.sciencedirect.com/science/article/pii/S004269890600438X)
42. [Clarke, Visual recovery after local adaptation (Troxler)](https://doi.org/10.1080/713826370)
43. [MIB and Troxler fading: common and different mechanisms, PLOS ONE 2014](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0092894)
44. [Mather, Verstraten & Anstis, Motion aftereffect, 1998](https://anstislab.ucsd.edu/files/2012/12/1998-the-motion-aftereffect.pdf)
45. [Mather et al., Motion after-effect reloaded](https://pmc.ncbi.nlm.nih.gov/articles/PMC3087115/)
46. [Wade et al.-lineage, Waterfall illusion paradoxes, 2018](https://journals.sagepub.com/doi/10.1177/0301006618774645)
47. [Tilt aftereffect after Glass-pattern adaptation, Sci Rep 2016](https://preview-www.nature.com/articles/srep23567)
48. [Segmentation decreases the tilt illusion, JOV](https://jov.arvojournals.org/article.aspx?articleid=2193837)
49. [Eagleman & Sejnowski, Postdiction, Science 2000](https://doi.org/10.1126/science.287.5460.2036)
50. [Whitney et al. / Eagleman & Sejnowski, Differential latency exchange, 2000](https://papers.cnl.salk.edu/PDFs/Flash-Lag%20Effect_%20Differential%20Latency,%20Not%20Postdiction%202000-3149.pdf)
51. [Hogendoorn, 25 years of flash-lag debate, J Neurosci 2020](https://www.jneurosci.org/content/40/30/5698)
52. [Henderson & Hollingworth, Changes to saccade targets, 2003](http://jhenderson.org/vclab/PDF_Pubs/Henderson_Hollingworth_PP_2003.pdf)
53. [Henderson & Hollingworth, Global transsaccadic change blindness, 2003](https://journals.sagepub.com/doi/10.1111/1467-9280.02459)
54. [Transsaccadic memory and integration, Psych Sci 1997](https://journals.sagepub.com/doi/10.1111/j.1467-9280.1997.tb00543.x)
55. [Prime et al., Cortical mechanisms for trans-saccadic memory](https://pmc.ncbi.nlm.nih.gov/articles/PMC3030828/)
56. [Hollingworth, Richard & Luck, Function of VSTM, 2008](https://pmc.ncbi.nlm.nih.gov/articles/PMC2784885/)
57. [Visual Remapping review](https://pmc.ncbi.nlm.nih.gov/articles/PMC9255256/)
58. [Marino & Mazer, Perisaccadic updating, 2016](https://pmc.ncbi.nlm.nih.gov/articles/PMC4743436/)
59. [Wang et al., Perisaccadic/attentional remapping LIP & FEF, 2023](https://doi.org/10.1101/2023.09.23.558993)
60. [Szinte et al., Pre-saccadic remapping and attention dynamics, eLife 2018](https://elifesciences.org/articles/37598)
61. [Melcher, Predictive remapping of features, Nat Neurosci 2007](https://www.nature.com/articles/nn1917)
62. [Yarrow et al., Chronostasis, Nature 2001](https://www.nature.com/articles/35104551)
63. [Yarrow et al., Chronostasis across saccade categories, 2004](https://doi.org/10.1162/089892904970780)
64. [Yarrow, Chronostasis review](https://kielanyarrow.github.io/MyPage/papers/Chronostasis_Review.pdf)
65. [Law & Abrams, Object-based selection, 2002](https://doi.org/10.3758/bf03194753)
66. [Strategic effects on object-based selection, Acta Psychol 1999](https://www.sciencedirect.com/science/article/abs/pii/S0001691899000219)
67. [Alvarez & Franconeri, How many objects can you track, JOV 2007](https://jov.arvojournals.org/article.aspx?articleid=2121950)
68. [Pylyshyn, MOT without identity tracking, 2004](https://www.tandfonline.com/doi/abs/10.1080/13506280344000518)
69. [Role of visual attention in MOT: ERPs](https://pmc.ncbi.nlm.nih.gov/articles/PMC2927139/)
70. [Pinkham et al., Angry faces detected efficiently?](https://pmc.ncbi.nlm.nih.gov/articles/PMC1839771/)
71. [Search asymmetries with real faces, 2006](https://pubmed.ncbi.nlm.nih.gov/16768552/)
72. [Savage, Becker & Lipp, Stimulus-set effects in emotional search, 2015](https://doi.org/10.1080/02699931.2015.1027663)
73. [Eye tracking the face-in-the-crowd task, PLOS ONE 2014](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0093914)
74. [Neural computation: perceptual vs emotional (anger superiority), 2008](https://pubmed.ncbi.nlm.nih.gov/19128799/)
75. [Friesen & Kingstone, The eyes have it, 1998](https://doi.org/10.3758/bf03208827)
76. [Frischen, Bayliss & Tipper, Gaze cueing review, 2007](https://doi.org/10.1037/0033-2909.133.4.694)
77. [Ristic, Friesen & Kingstone, Are eyes special, 2002](https://doi.org/10.3758/bf03196306)
78. [Counterpredictive gaze and arrow cues](https://web.uvic.ca/~dbub/Cognition_Action/Topics2021_files/Attentional%20Effects%20of%20Counterpredictive%20Gaze%20and%20Arrow%20Cues.pdf)
79. [Reflexive orienting to short/long-duration gaze cues](https://pmc.ncbi.nlm.nih.gov/articles/PMC3981892/)
80. [Treisman & Gelade, Feature integration theory, 1980](https://www.cs.princeton.edu/courses/archive/spring08/cos598B/Readings/TreismanGelade1980.pdf)
81. [Treisman, Feature binding, attention, object perception, 1998](https://princetonuniversity.github.io/NEU-PSY-502/_static/pdf/Class%2018/Treisman1998.pdf)
82. [Tsal, Meiran & Lavie, Attention in illusory conjunctions, 1994](https://doi.org/10.3758/bf03207605)
83. [Luck & Vogel, VWM capacity, Nature 1997](https://awhvogellab.com/files/pdfs/luck_1997_capacity-features-conjuctions.pdf)
84. [Binding by rate coding, not synchrony, Neuron 2023](https://www.sciencedirect.com/science/article/pii/S089662732300212X)
85. [Awh & Pashler, Split attentional foci, 2000](https://doi.org/10.1037//0096-1523.26.2.834)
86. [Müller et al., Sustained division of the spotlight, Nature 2003](https://preview-www.nature.com/articles/nature01812)
87. [Jans, Peters & De Weerd, Attention to multiple locations, 2010](https://cris.maastrichtuniversity.nl/ws/files/76027702/Peters_2010_Visual_spatial_attention_to_multiple.pdf)
88. [Walter, Keitel & Müller, Splits within vs across hemifields, 2015](https://doi.org/10.1162/jocn_a_00883)
89. [Eagleman BBS commentary, Prediction and postdiction](https://eagleman.com/papers/EaglemanCommentonNijhawanBBS2008.pdf)
90. [Bednar thesis, TAE angular function (Mitchell & Muir data)](https://nn.cs.utexas.edu/web-pubs/bednar.thesis/node29.html)
91. [Scholarpedia: Multiple object tracking](http://scholarpedia.org/article/Multiple_object_tracking)
92. [Smith & Schenk-lineage / movement-doc §3.3.4 coupling study, PMC 11534328](https://pmc.ncbi.nlm.nih.gov/articles/PMC11534328/)

The 40 sources carrying load-bearing claims are items 1–40 plus the directly-cited classics among 41–92; companion papers are listed for traceability.

---

## Appendix A — Agent-generated questions (40)

1. How much does mean CS fall per 10° eccentricity, and is it linear? — ~0.24 log/10° near periphery; nonlinear (slow then rapid) beyond ~30°. §4.1.1
2. Does the CS peak frequency shift with eccentricity? — Yes, progressively lower SF; inverted-U retained. §4.1.1
3. At what eccentricity does red-green discrimination become behaviorally absent? — 25–30° nasal (sinring), with size-scaling caveats. §4.1.1
4. Does blue-yellow degrade like red-green? — No; S/(L+M) robust to ~20°. §4.1.1
5. Is peripheral chromatic loss retinal or cortical? — Contested; both camps with evidence. §4.1.1
6. What is the spatial extent of stimulus-driven surround suppression and what does it scale with? — Scales with eccentricity, NOT size or SF (Petrov & McKee). §4.1.2
7. What is the time course of surround suppression? — V1 latency ~61 vs 52 ms center; human MEG ~80–130 ms; 375 ms-lagged surround has no effect. §4.1.2
8. Does attending a location suppress its surround? — Yes; Mexican-hat, worst at intermediate offsets (±2 items). §4.1.2
9. Does the attentional suppressive surround scale with size or eccentricity? — Eccentricity only (Yoo et al.). §4.1.2
10. How much can attention rescue a suppressed target? — Up to four-fold threshold reduction. §4.1.2
11. Can attending a flanker flip suppression to enhancement? — Yes (Flevaris fMRI). §4.1.2
12. What is the minimum spotlight size? — ~1° letters; interference measurable to 3–5°. §4.1.3
13. Are spotlight edges sharp or graded? — Graded; ~9–11 ms/position slopes, invariant with focus size. §4.1.3
14. Can the spotlight split? — Yes across hemifields (Awh & Pashler; SSVEP); contested within (Jans et al.; Walter et al.). §4.1.3
15. Why is within-hemifield splitting hard? — Overlapping DoG suppressive surrounds / competitive content maps. §4.1.3
16. How big is the exogenous cueing validity benefit? — ~10–60 ms exogenous→endogenous. §4.2.1
17. What is the full IOR window and gradient? — Facilitation <250 ms; inhibition 200 ms–3 s; gradient 6–10°, spatiotopic AND retinotopic. §4.2.1
18. Is IOR sensory or motor? — Two components: sensory ~1–1.5 s; oculomotor longer. §4.2.1
19. How deep is the attentional blink? — T2 to ~40–60% of control at lags 2–5. §4.2.2
20. Does lag-1 sparing replicate at full size? — No: 30% (1992) vs 14–18% (2020 replication). §4.2.2
21. How far does flanker interference extend? — ~1° letters to 3–5° other stimuli. §4.2.3
22. How fast is flanker interference resolved? — Zoom-lens narrowing over ~100–200 ms; distant flankers escape first. §4.2.3
23. What are the real conjunction-search slopes? — ~5–40 ms/item present, 10–90 absent; 250–350 absent when foveation forced. §4.2.4
24. Is anything truly 0 ms/item? — No; even feature searches grow with log set size. §4.2.4
25. What did Bouma 1970 actually measure and say? — Letter triplets, gap spacing, "0.5×E," egg-shaped zone; later 0.4–0.5. §4.3.1
26. What is the honest Bouma-factor range? — 0.3–0.5 radial (full spread 0.13–0.7), ~0.1–0.25 tangential; standardized 0.24–0.35. §4.3.1
27. Why is crowding cortical, not optical? — Size-invariance; radial-tangential anisotropy; outer-flanker asymmetry; 6 mm V1 correspondence; E₂ dissociation. §4.3.2
28. Why can't you recognize a face at 10° despite ~20/60 acuity? — Features ~17× MAR but inside each other's 4–5° crowding zones. §4.3.3
29. How fast is gist extraction really? — Above chance at 13 ms; 75%-thresholds 19–67 ms; asymptote ~100 ms. §4.3.4
30. Does gist survive attentional load? — Single-object gist mostly; four-object scenes impaired even at 500 ms. §4.3.4
31. How long until Troxler fading, under what conditions? — ~5–10 s typical (1–2 s with blur/low contrast); needs low contrast + periphery + stationary image. §4.4.1
32. What distinguishes MIB from Troxler mechanistically? — Opposite contrast-dependence (doubling vs halving disappearance rate): competition vs adaptation. §4.4.2
33. How long does the MAE last and scale? — ~10–60 s; duration ~ √(adaptation duration); storage with eyes closed. §4.4.3
34. What is the TAE magnitude and angular profile? — Direct ~4° (2–7°) at 10–20° separation; indirect ~0.5° (to 2.5°) at 75–80°. §4.4.3
35. What is the flash-lag magnitude and which account is winning? — ~50–100 ms (0.5–1° at 10°/s); extrapolation/differential-latency/postdiction all live; 2020 evidence favors real extrapolation. §4.4.4
36. What survives a saccade in memory? — 3–4 integrated objects + gist + attended-object tokens; NOT contours or point-by-point images. §4.5.1
37. Is remapping forward or convergent, and what does it do perceptually? — Both in sequence; predictive attention remapping time-gated at >175 ms; dual-spotlight retinotopic trace after the saccade. §4.5.2
38. How much does the stopped-clock illusion overestimate? — Back-dating to ~50 ms pre-saccade (up to 120 ms), growing 1:1 with saccade duration. §4.5.3
39. How big is the same-object advantage and when does it reverse? — ~11–15 ms robust (10–30 typical, to ~80); reversible by rotation/translation strategies. §4.6.1
40. Is the MOT limit a fixed 4? — No; 1–8 by speed and spacing — flexible resource with resolution trade-off. §4.6.2

**Additional dispositions (raised during research, dropped):** (a) per-observer V2/V3 suppression percentages — only the qualitative "too strong" statement verifiable, dropped; (b) MIB invisibility-percentage per mask speed — trends verifiable but per-speed percentages not extractable, so only the doubling/halving trend is quoted; (c) Bouma's raw per-eccentricity thresholds beyond the three anchor points — treated as re-derived by Coates et al. and cited only through them; (d) the exact lag at which the 2020 AB replication crossed 50% — not extractable; only the ~one-lag shift is reported.

*End of Part 4.*
