# Eye Movements and Blinks: The Complete Oculomotor Story — Kinematics, Suppression, Nystagmus, and Blinking

> **What this is.** Part 3 of 5 of the human-eye deep-study for the voxel-engine research repo. This document is about the eye **in the head**: the oculomotor plant and its muscles, the full kinematics of saccades, the microstructure of fixation, smooth pursuit and its failures, the gaze-holding reflexes (VOR/OKN/nystagmus/vection), the assembled timeline of a fast side-glance, and blinking — including the direct answer to "do the human eyes get blocked?" Locomotion-context material (VOR gain/latency during walking, gaze strategy on terrain, saccade basics) lives in [`human-movement-and-perception-research.md`](human-movement-and-perception-research.md) Part 2 and is cross-referenced, not re-derived. House style matches [`water-physics-and-wave-simulation.md`](water-physics-and-wave-simulation.md): derivations shown, worked numeric examples, ranges not fake values, disagreements reported inline with both sources.

---

## 0. Provenance

- **Date:** 2026-09-06. **Author:** research agent 3 of 5 (eye movements and blinks).
- **Tools:** Exa web search + fetch (11 search batches, ~20 queries, all result highlights read; several primary PDFs fetched full-text), `web_fetch`, local numeric verification with `pwsh`.
- **Verification:** every URL in §3.9 returned live content via Exa this session. Numeric anchors recomputed locally:
  - Main sequence $D = 20 + 2.7A$ ms → **22.7 / 33.5 / 47 / 74 / 155 ms** for 1/5/10/20/50° saccades; mean velocities 44→323°/s over that range (peak velocities are ~2–2.5× the mean, consistent with Bahill's and Collewijn's curves).
  - Blink **occlusion duty cycle**: 10–20 blinks/min × 100–150 ms pupil coverage = **1.7–5.0%** of waking time (matches the 2–5% anchor; counting the full 200–400 ms motor event including partial coverage raises the "visually degraded" fraction toward ~5–13%).
  - Chronostasis antedating: Yarrow's 22° data (matched 880 ms, 72 ms saccade) and 55° data (811 ms, 139 ms) both give perceptual onset antedated to **≈ 48–50 ms before saccade onset** — the constant-antedating claim reproduces exactly.
  - Velocity storage: at time constant 20 s, post-rotatory slow-phase velocity is still **47%** of initial at 15 s and **22%** at 30 s; at 11.4 s (Choi's human-tuned value), 26% at 15 s.
- **Anchor disagreements found (reported, not forced):** peak-velocity saturation differs strongly by method — Bahill's photodiode data saturate at ~700–900°/s while Collewijn's search-coil data saturate at **502±32°/s** horizontal (§3.2); drift velocity spans a 10× range between classical instructed-fixation studies and modern natural-viewing measurements (§3.3); the up-phase of a blink is **not** a simple "Bell's phenomenon upward rotation" — normal blinks rotate the eyes **down and nasal** 1–5° (§3.7); chronostasis upper bound "~500 ms" could not be verified (largest verified effect ≈ 190 ms at 55°, §3.2).
- **Deliberately unverified (dropped or flagged):** the exact °/s² magnitude of open-loop pursuit acceleration (verified structurally, not numerically — §3.4); a specific peer-reviewed gaze study of tactical lateral movement was already flagged in the movement doc and not revisited; express-saccade absolute latency bounds are reported qualitatively only.
- **Source count:** 98 numbered entries in §3.9 (some are dual-access copies of the same paper: doi + PMC/PDF); ~80 distinct publications, all verified live. This exceeds the 35-source floor because nearly every numeric claim carries its own primary citation. Document length is ~9,500 words — over the 4,000–6,000 target, driven by the breadth of mandated sections plus Appendix A; density was prioritized over trimming verified content. **Question count:** 40 agent-generated questions with dispositions in Appendix A (plus parent-bank dispositions).

---

## 3.1 The oculomotor plant

### 3.1.1 Six muscles, three axes, one table

Each eye is moved by six extraocular muscles plus the levator palpebrae superioris (eyelid). Rotations occur about three axes: vertical (adduction/abduction), transverse (elevation/depression), anteroposterior (intorsion/extorsion) ([StatPearls: Eye Muscles](https://www.ncbi.nlm.nih.gov/sites/books/NBK470534/)). The actions, verified across three anatomical sources that agree exactly ([Bradley's Neurology in Clinical Practice, ch. 44](https://elsevier-elibrary.com/contents/fullcontent/84861/epubcontent_v2/OEBPS/xhtml/chp0044_1.xhtml); [Adler's Physiology of the Eye, EOM chapter](https://clinicalpub.com/the-extraocular-muscles/); [NCBI Neuroscience, NBK10793](https://www.ncbi.nlm.nih.gov/books/NBK10793/)):

| Muscle | Nerve | Primary | Secondary | Tertiary |
|---|---|---|---|---|
| Medial rectus (MR) | III (inf. div.) | Adduction | — | — |
| Lateral rectus (LR) | VI | Abduction | — | — |
| Superior rectus (SR) | III (sup. div.) | Elevation | Intorsion | Adduction |
| Inferior rectus (IR) | III (inf. div.) | Depression | Extorsion | Adduction |
| Superior oblique (SO) | IV | Intorsion | Depression | Abduction |
| Inferior oblique (IO) | III (inf. div.) | Extorsion | Elevation | Abduction |

Geometry that explains the secondary/tertiary actions: the four recti originate at the Annulus of Zinn and are ~40 mm long; the orbits diverge, so SR/IR run at ~22.5–23° to the sagittal plane — they are **pure vertical movers only when the eye is abducted 23°**, and the obliques (SO redirected by the trochlea, inserting posterior to the equator at a 51° angle) are pure vertical movers at **51° adduction** (Bradley's; Adler's). This is why clinical testing of vertical actions uses 30° adduction/abduction. Yoked pairs under Hering's law (equal simultaneous innervation) with Sherrington reciprocal inhibition: MR↔LR, SR↔IO, IR↔SO (Bradley's Table 44.2). The globe can rotate ~50° but only ~15° is normally used before the head joins in ([StatPearls NBK519565](https://www.ncbi.nlm.nih.gov/books/NBK519565/)) — directly relevant to game cameras that snap the eyes ±60° with a fixed head.

### 3.1.2 Listing's law and the half-angle rule

The eye has three rotational degrees of freedom but uses only two. **Donders' law:** for any gaze direction (H,V), ocular torsion is unique. **Listing's law:** from primary position, any gaze direction is reached by a single rotation about an axis lying in **Listing's plane** (the frontal plane through primary position). In Helmholtz coordinates (torsion, then horizontal, then vertical rotations) the law reads ([Wong, *Listing's law — review*, Surv Ophthalmol 2004](http://individual.utoronto.ca/agneswong/Wongpublications/Wong_2004-Listings_law_review.pdf)):

$$
\boxed{\;T = \frac{HV}{2}\;} \qquad \text{(Helmholtz "false torsion"; in rotation-vector/quaternion coordinates the torsional component is identically zero)}
$$

The **half-angle rule** is the velocity-domain consequence: for the eye to *stay* in Listing's plane during a movement, the instantaneous angular-velocity axis must tilt out of Listing's plane by **half the angle of eye-position eccentricity** from primary. If the eye looks up by α, the horizontal-rotation axis ("velocity plane") pitches up by α/2, leaving it 90°−α/2 from the line of sight (Wong 2004, Fig. 4; [Klier, Meng & Angelaki 2006](https://pmc.ncbi.nlm.nih.gov/articles/PMC6675172/)). Equivalently, the rotation axis taking gaze v̂ to v̂′ lies in the plane perpendicular to the bisector of n̂ (primary) and v̂ ([Handzel & Flash, NeurIPS 1995](https://proceedings.neurips.cc/paper_files/paper/1995/file/d736bb10d83a904aefc1d6ce93dc54b8-Paper.pdf)). This exists because 3D rotations are **non-commutative**: Fick and Helmholtz gimbal parameterizations imply twisted velocity surfaces x = ∓yz, and only Listing's choice is planar.

**Measured imperfection:** the tilt-angle coefficient (TAC = velocity-axis tilt / position eccentricity; 0.5 = perfect) measures **0.57–0.58 for horizontal saccades and 0.34–0.35 for vertical** — so eye positions lie on a *twisted* surface, not a plane, and the law is only approximately obeyed ([Klier et al. 2006](https://pmc.ncbi.nlm.nih.gov/articles/PMC6675172/); [Cheng et al., "3-D kinematics of saccadic eye movements," Ann NY Acad Sci](https://nyaspubs.onlinelibrary.wiley.com/doi/10.1111/j.1749-6632.2011.06129.x)). Listing's law is **violated during the VOR and OKN** (counter-rotation about roughly the head's axis, including pure torsion), where the system settles on a quarter-angle compromise ([Listing's law overview](https://en.wikipedia.org/wiki/Listing's_law); Misslisch-class work cited therein).

### 3.1.3 The pulley system — orbital mechanics, not neural 3D control

The modern view replaces "the brain computes non-commutative 3D commands" with mechanics: each rectus muscle passes through a **fibroelastic pulley** in the orbit. Crucially the muscle is bilaminar: the **orbital layer (OL)** inserts on the *pulley*, and only the **global layer (GL)** continues to insert on the sclera ([Demer, *Current concepts of mechanical and neural factors in ocular motility*, 2006](https://pmc.ncbi.nlm.nih.gov/articles/PMC1847330/); [Demer-group fascicular tracing, PMC1978188](https://pmc.ncbi.nlm.nih.gov/articles/PMC1978188/)). The OL contains 40–60% of all muscle fibers (GL ≈ 10,000–15,000 fibers in humans); the rectus pulley array acts as an inner gimbal that is torsionally rotated by the obliques — and this arrangement **implements Listing's law mechanically**, with human behavioral and monkey physiological evidence favoring peripheral over central implementation (Demer 2006; [Demer 2002, *The Orbital Pulley System*, Ann NY Acad Sci](https://doi.org/10.1111/j.1749-6632.2002.tb02805.x); active-pulley evidence in Demer, Oh & Poukens 2000 cited therein).

### 3.1.4 Muscle fiber types: twitch and tonic

EOMs contain both **singly innervated fibers (SIFs)** — fast-twitch, en plaque endings, like ordinary skeletal muscle — and **multiply innervated fibers (MIFs)** with en grappe endings, slow/tonic, foreign to mammalian limb muscle ([Porter-group, *Biological organization of the extraocular muscles*](https://www.sciencedirect.com/science/article/abs/pii/S0079612305510021); [Hoh 2020, *Myosin heavy chains in EOM fibres*, Acta Physiol](https://doi.org/10.1111/apha.13535)). Human counts ([Wasicky et al., IOVS](https://iovs.arvojournals.org/article.aspx?articleid=2123010); [Kjellgren-class IOVS myosin study](https://iovs.arvojournals.org/article.aspx?articleid=2124289)): global layer ≈ 59% SIF-granular + 21% SIF-coarse + 21% MIF; orbital layer ≈ 83% SIF + 17% MIF. The division of labor matches the pulley anatomy: the fatigue-resistant, oxidative orbital layer holds pulley position (tonic-ish duty), the global layer delivers the twitch force that rotates the globe. This is the muscle-level substrate of the pulse-step below: fast SIF force for the pulse, sustained MIF/tonic support for the step.

### 3.1.5 The neural integrator and gaze-evoked nystagmus

**Why a pure position command fails — derivation.** Neglecting the (tiny) globe inertia, the plant is a first-order viscoelastic system driven by net force ΔF:

$$
B\,\dot\theta + K\,\theta = \Delta F(t) \quad\Rightarrow\quad \tau_p \dot\theta + \theta = \Delta F/K,\qquad \tau_p = B/K
$$

A pure step ΔF₀ = K·A puts the eye at A only exponentially: θ(t) = A(1−e^{−t/τ_p}). With τ_p ≈ 0.2 s the eye needs 3τ_p ≈ **600 ms** to cover 95% of a 10° movement — but real 10° saccades take **47 ms**. Conversely a pure pulse with no step lets the elastic pull swing the eye back to origin (post-saccadic drift / glissade). The motoneuron command is therefore a **pulse-step**: a high-frequency burst (pulse) to beat the viscosity, riding into a tonic position command (step) that holds against the elastic pull. Bahill's classic finding: the burst needs to last only about **half** the saccade because of the "apparent inertia" of series elasticity, muscle viscosity and activation dynamics ([Bahill, Clark & Stark 1975](http://www.visualcognition.ca/spering/reading/Bahill.Clark.Stark.MathBiosci.1975.pdf)).

Mathematically the step *is the integral of the pulse*:

$$
F_{MN}(t) = \underbrace{P(t)}_{\text{burst}} + \underbrace{k\!\int_0^t P(s)\,ds}_{\text{step (neural integrator)}}
$$

The **velocity-to-position neural integrator** that performs this integration resides in the nucleus prepositus hypoglossi + medial vestibular nucleus (horizontal) and the interstitial nucleus of Cajal (vertical/torsional), with the cerebellar flocculus/paraflocculus in a feedback loop that tunes its time constant ([Leigh/Zee-class OMLAB monograph, *Neural Integration in Ocular Motility*](https://omlab.org/Personnel/lfd/Jrnl_Arts/071_Neural_Integration_OM_1989.pdf); [Arikan et al., systematic review, Acta Ophthalmol](https://onlinelibrary.wiley.com/doi/10.1111/aos.13307); [Frontiers, *Cerebellum and Ocular Motor Control*](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2011.00053/full)). The brainstem integrator is inherently **leaky** (model value ~5 s; [Marti, Straumann, Büttner & Glasauer 2008](https://doi.org/10.1007/s00221-008-1396-7)); floccular feedback extends the effective time constant to **~25 s** in the light (Becker & Klein's classic 25 s value, via OMLAB and Arikan; Glasauer-class modeling: gain-10 feedback turns 5 s into ~50 s).

**Failure mode — gaze-evoked nystagmus (GEN):** when the integrator is leaky, the step decays, the eyes drift centripetally with velocity ≈ θ/τ, and corrective quick phases snap them back. Worked example: at 30° eccentricity with τ = 25 s the drift is 30/25 = **1.2°/s** — squarely in the clinical 1–6°/s GEN range. Slow-phase velocity grows with eccentricity from a **null position** (Alexander's law); an over-tuned integrator instead produces *increasing*-velocity drifts and pendular nystagmus — both failure polarities can even coexist ([Shaikh & Ghasia, J Neuro-Ophthalmol 2020](https://journals.lww.com/jneuro-ophthalmology/fulltext/2020/06000/_leaky__and__unstable__neural_integrator_can.15.aspx)). Rebound nystagmus after sustained eccentric gaze is the adaptive set-point mechanism overshooting (Frontiers 2011, above).

---

## 3.2 Saccades: the complete story

### 3.2.1 The main sequence with fitted constants

Three primary sources, three recording methods, one shape:

- **Bahill, Clark & Stark 1975** (infrared photodiode): duration grows non-linearly-but-approximately-linearly with amplitude; peak velocity quasi-linear to **15–20°**, then a soft saturation; their large-saccade data saturate in the **~700–900°/s** band; burst ≈ half the saccade ([PDF](http://www.visualcognition.ca/spering/reading/Bahill.Clark.Stark.MathBiosci.1975.pdf); [doi](https://doi.org/10.1016/0025-5564(75)90075-9)). Their normative database paper (13 subjects, 900+ saccades, 1 kHz) stresses that computed peak velocity depends strongly on velocity-channel bandwidth (flat above ~74 Hz cutoff) ([Bahill-class database PDF](http://sysengr.engr.arizona.edu/publishedPapers/Database.pdf)).
- **Baloh et al. 1975** (DC-EOG, 25 subjects): duration linearly correlated with amplitude with **slope 2.7 ms/deg** over a large range; velocity vs amplitude best fit by an exponential ([Neurology](https://www.neurology.org/doi/10.1212/WNL.25.11.1065)).
- **Collewijn, Erkelens & Steinman 1988** (scleral search coil — the most accurate method): horizontal saccades between continuously visible targets undershoot by only ~0.5°; peak velocity saturates at a mean asymptote of **502 ± 32°/s** for amplitudes ≥ 40°; duration linear up to 50° then rising more steeply ([J Physiol 1988a](https://doi.org/10.1113/jphysiol.1988.sp017284); [Europe PMC](https://europepmc.org/article/MED/3253429)). Vertical saccades: peak velocity still rising monotonically at 513 ± 27°/s for 70° saccades (no distinct asymptote); upward saccades undershoot ~10%, downward overshoot and develop a **second velocity peak** beyond 30° ([J Physiol 1988b](https://doi.org/10.1113/jphysiol.1988.sp017285); [Europe PMC](https://europepmc.org/article/MED/3253430)).

$$
\boxed{\;D \approx 20\ \text{ms} + (2\text{–}2.7)\tfrac{\text{ms}}{\text{deg}}\cdot A\;,\qquad V_{peak} \to \begin{cases} \sim 700\text{–}900°/\text{s} & \text{(Bahill photodiode, large } A\text{)}\\ 502\pm 32°/\text{s} & \text{(Collewijn coil, horizontal, } A\ge 40°\text{)}\\ 513\pm 27°/\text{s at } 70° & \text{(Collewijn coil, vertical, no asymptote)}\end{cases}}
$$

**Disagreement to report, not average away:** the saturation level differs ~1.7× between photodiode and coil recordings (and EOG reads lower still). Some of this is bandwidth/definition (Bahill's own analysis: apparent peak velocity depends on filter cutoff), some is paradigm (target steps vs self-paced gaze shifts between visible targets). The movement doc's Part 2 §2.4.1 already reports "2–2.7 ms/deg, 700–900°/s" for locomotion-context use; the coil numbers are the better anchor for engine work that models the eye itself.

**Vertical vs horizontal:** vertical saccades are slower and less accurate — a television-based study of 43 subjects measured 20° horizontal saccades at ~420°/s peak with **downward slower than upward, and both slower than abduction/adduction**; oblique saccades trade component velocities ([PubMed 6668756](https://pubmed.ncbi.nlm.nih.gov/6668756/)). Combined with Collewijn 1988b, a defensible statement: **vertical peak velocities run ~10–20% below horizontal at matched amplitude, with downward the slowest** — exact ratio varies by study and direction.

**Skewness/asymmetry:** the velocity profile is not symmetric. Collewijn's skewness metric (acceleration time ÷ total duration) falls from **~0.45 for saccades ≤ 10° to ~0.20 at ≥ 50°**: large saccades peak *early* and coast (1988a). This matches Bahill's mechanism: the pulse ends near mid-saccade, so velocity peaks when the burst stops and the plant's apparent inertia carries the eye out. Additional asymmetries: the abducting eye is faster/shorter/more skewed than the adducting eye (transient divergence up to 3° mid-saccade), and centripetal saccades are ~10% faster than centrifugal (Collewijn 1988a) — micro-details a camera model can ignore but an animation system pairing two eyes should know.

**Why saturation at all:** beyond 15–20°, essentially all motoneurons are already firing near maximum during the burst, so only pulse *width* can still grow — amplitude is bought with duration (Bahill 1975). In muscle terms this is the force-velocity relation: extraocular muscle can only shorten so fast, so force saturates and the main sequence bends.

**Ballistic, no mid-flight correction:** the burst ends before the movement does; visual latency (~70–100 ms) exceeds most saccade durations, so corrections can only arrive as separate saccades ~150–250 ms later (Rashbass's reaction times, §3.4). This is why a saccade launched at a moving target's old position lands where the target *was*.

### 3.2.2 Saccadic suppression: time course, magnitude, mechanism

**Time course and magnitude (Diamond, Ross & Morrone 2000, J Neurosci):** contrast thresholds for flashed low-frequency gratings during 12° voluntary saccades: suppression **anticipates the saccade by ~50 ms** (their model treats sensitivity loss as starting ~75 ms before any image motion), is **maximal at saccade onset**, and **outlasts it by ~50 ms**; magnitude **~10-fold (1 log unit)** for luminance-modulated gratings; **no suppression for chromatic gratings**; simulated (mirror) saccades produce weaker, longer, shallower suppression — and adding contrast noise to simulated saccades deepens it, while barely affecting real-saccade suppression ([PMC6773104](https://pmc.ncbi.nlm.nih.gov/articles/PMC6773104/); [doi](https://doi.org/10.1523/JNEUROSCI.20-09-03449.2000)). So: **0.5–1 log unit threshold elevation across a ~±50 ms window, magnocellular-selective, parvocellular spared.**

**Neural time course (macaque MSTd, flashed textures):** response amplitudes suppressed for flashes up to **90 ms before** saccade onset, maximum suppression ~10 ms after onset (mean reduction 81%), then a **postsaccadic enhancement** window: first significant at ~50 ms, peaking **100–130 ms** after onset, persisting 200–450 ms, mean peak **310% of control** — and enhancement appears even for saccades in total darkness, proving an extra-retinal drive ([J Neurosci 28(43):10952](https://www.jneurosci.org/content/28/43/10952)).

**Mechanism — genuinely contested, both sides verified:**
- *Extraretinal/corollary-discharge side:* suppression begins **before** there is any retinal image motion (Diamond 2000; MSTd pre-saccadic suppression with a latency argument: no signal can reach LGN in <16 ms to intercept a flash that is already en route — ≥30% of MSTd cells show suppression of non-retinal origin, J Neurosci 28:10952). Chronostasis and perisaccadic compression share its time course (Diamond 2000).
- *Retinal/visual-only side:* Idrees, Baumann, Franke, Münch & Hafed (2020) showed that **image displacements alone** (simulated saccades, texture jumps, even luminance steps) reproduce the perceptual properties of saccadic suppression, that neural suppression exists **in the retina itself** (mouse/pig RGC flash responses suppressed after texture displacement, modulation index −0.55), and that retinal suppression *outlasts* perceptual suppression — suggesting motor-related signals may act to *shorten*, not cause, suppression; pre-saccadic suppression may be **backward masking** by the upcoming transient ([Nat Commun](https://doi.org/10.1038/s41467-020-15890-w); [PMC7181657](https://pmc.ncbi.nlm.nih.gov/articles/PMC7181657/)).
- Computationally, intrasaccadic suppression is dominated by a **detector gain reduction**, not added noise or spatial uncertainty ([Watson & Krekelberg, J Vis](https://pmc.ncbi.nlm.nih.gov/articles/PMC3704127/)).

**Why no motion blur at 500°/s:** four stacked reasons. (1) At 500°/s retinal speed, contrast sensitivity to anything above very low spatial frequencies collapses — the smear is inherently low-contrast (high-SF components become invisible at saccadic speeds; Diamond 2000 citing Morgan). (2) The suppression that does exist is precisely targeted at that surviving low-SF luminance channel (magno), which is why a *large* luminance change (room light toggling) is still seen during a saccade, but scene smear is not. (3) The pre- and post-saccadic fixations **mask** the intrasaccadic frame both forward and backward (Campbell & Wurtz's "saccadic omission," via Idrees 2020 and Watson & Krekelberg). (4) The postsaccadic enhancement window (above) immediately overwrites the trace with the new, sharp fixation. Net effect: a 20° glance at 74 ms duration is a visual non-event.

### 3.2.3 Chronostasis — the stopped clock

Glance at a clock and the second hand seems frozen. Yarrow, Haggard, Heal, Brown & Rothwell (2001): subjects saccade 22° (mean 72 ms) or 55° (139 ms) to a counter incrementing every second, and judge the first digit's duration. They matched **1 s when they had actually seen the digit for only 880 ms** (22°) or **811 ms** (55°) — overestimates of 120 and 189 ms. The difference between conditions (69 ms) almost exactly tracks the saccade-duration difference (67 ms), and back-computation places the perceptual onset of the target at **~50 ms before the eyes began to move** — the "antedating hypothesis" ([Nature](https://www.nature.com/articles/35104551); [author PDF](https://kielanyarrow.github.io/MyPage/papers/Chronostasis.pdf); my local check reproduces −48/−50 ms). Properties: effect size is constant across stimulus durations from 100–1333 ms (ruling out arousal/clock-speed accounts, [Yarrow, Haggard & Rothwell 2004](https://openaccess.city.ac.uk/id/eprint/330/2/Action_arousal_and_subjective_time.pdf)); equal across self-timed, antisaccade, express and reflexively-cued saccades → a **subcortical (superior colliculus) efferent trigger** ([Yarrow et al. 2004, J Cogn Neurosci](https://doi.org/10.1162/089892904970780)); abolished when the target is *noticeably* displaced mid-saccade — spatial stability of the target is the precondition for temporal extension ([review chapter](https://kielanyarrow.github.io/MyPage/papers/Chronostasis_Review.pdf); [Yarrow et al. 2006, Percept Psychophys](https://doi.org/10.3758/bf03193722)). Magnitude across the verified literature: ~50 ms (small saccades) to ~190 ms (55°); the commonly quoted "~500 ms" upper bound could not be verified this session and is **dropped**. Function: chronostasis is the temporal half of trans-saccadic continuity — the brain prices the saccade's blind gap at zero by predating the target percept.

---

## 3.3 Fixation and its microstructure

"Fixation" is a system, not a state. Three movement classes persist ([Rucci & Poletti 2015, *Control and functions of fixational eye movements*](https://pmc.ncbi.nlm.nih.gov/articles/PMC5082990/); [Martinez-Conde, Otero-Millán & Macknik, *Unchanging visions*, Phil Trans R Soc](https://royalsocietypublishing.org/doi/10.1098/rstb.2016.0204)):

- **Microsaccades:** involuntary small saccades during intended fixation, binocular/conjugate, sharing the saccadic main sequence and saccadic suppression — common generator with saccades. Rate: **1–2 Hz during sustained fixation, ~0.5 Hz within free-viewing fixations, with only ~14% of scene-viewing fixations containing any** (Martinez-Conde et al. 2016). Amplitude: the classical <12′ cutoff collapsed in modern data — sizes reach **up to and sometimes beyond 1°**, though the bulk are <30′ ([Rolfs 2009 review](http://www.martinrolfs.de/Rolfs_MicrosaccadeReview.pdf)). Trained subjects can cut rates from 2 to 0.5 Hz with minimal instruction (Winterson & Collewijn via Martinez-Conde).
- **Drift:** the slow, curvy, random-walk inter-saccadic motion. **Report the disagreement — it is a 10× methodological chasm:** classical instructed-fixation studies (Steinman school) give amplitudes 1.5–4′ and median velocities ~4′/s; natural-viewing measurements give **mean instantaneous drift speed ~50′/s**, an order of magnitude more, because drift is fastest right after saccades and changes direction constantly ([Rucci, *The Unsteady Eye*, TiCS](https://pmc.ncbi.nlm.nih.gov/articles/PMC4385455/)); with adequate (80 Hz) bandwidth, median inter-saccadic speed including tremor reaches **~1.5°/s with an upper tail to 4°/s** ([*Eye movements between saccades: measuring ocular drift and tremor*, Vision Research](https://www.sciencedirect.com/science/article/pii/S0042698916300037)). The parent anchor "0.1–0.3°/s" (6–18′/s) sits inside this band toward the classical end — correct for well-practiced fixation, too low for natural vision.
- **Tremor (ocular microtremor):** the smallest component, amplitude roughly one photoreceptor width. Dominant frequency **70–103 Hz, mean ~84 Hz** (Bolger et al., n=105, via Martinez-Conde 2016 and [Ryle, Vohnsen & Sheridan 2015](https://doi.org/10.1117/1.jbo.20.2.027004)); amplitude estimates scatter from 6″ (Eizenman) to 17.5″ median (Ratliff & Riggs) to 0.1–0.5′ in early studies (Rolfs 2009) — report **~6–30 arcsec at 30–100+ Hz**.

**Net retinal image motion during "fixation":** the fixated target's image wanders over an area **up to ~1 deg²**, carrying the image across dozens to hundreds of photoreceptors (drift+tremor paper above). What happens *without* it: stabilized retinal images fade within seconds (Troxler's 1804 observation; stabilized-retina literature via Martinez-Conde 2016) — the perceptual side belongs to the adaptation/optics companion document and to [`human-movement-and-perception-research.md`](human-movement-and-perception-research.md) §2.4.2; the motor-side fact is here.

**Function debate (report both):** microsaccades **counteract visual fading** — they restore visibility of a faded target (Martinez-Conde, Macknik, Troncoso & Dyar 2006, via Rucci & Poletti 2015) — but they are also locked to attentional shifts and contribute to high-acuity information extraction; and in demanding discrimination tasks they are **partially inhibited**, producing longer drifts punctuated by larger corrective saccades, with 57% more retinal area covered by the fixated target than in passive viewing (AOSLO at 960 Hz, [Bowers, Gautier, Lin & Roorda 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8556553/)). Modern synthesis (Rucci): drift is not noise — its spatiotemporal structure converts spatial gradients into temporal modulations that the visual system exploits for fine detail; both drift and microsaccades *prevent* fading, only microsaccades *reverse* it.

**Fixation stability metrics — BCEA.** The bivariate contour ellipse area is the standard continuous index:

$$
\boxed{\;\mathrm{BCEA} = 2\,k\,\pi\,\sigma_H \sigma_V \sqrt{1-\rho^2}\;}
$$

with σ_H, σ_V the standard deviations of fixation-point positions on the two meridians, ρ their correlation, and k set by the enclosed probability (k = 1.147 for 63.2%, k = 2.996 for 95.4% — the two standard report levels, from 1−e^{−k} for the bivariate normal). Typical values: in 326–358 normal MAIA microperimetry eyes, **BCEA@63% = 0.80 ± 0.68 deg², BCEA@95% = 2.40 ± 2.04 deg²**, with 95% of points within 1° (P1) and 99% within 2° (P2) ([Morales et al., TVST reference database](https://tvst.arvojournals.org/article.aspx?articleid=2585942); [IOVS version](https://iovs.arvojournals.org/article.aspx?articleid=2559026)); lab video-tracker values are similar (±1 SD ≈ 0.67–0.69 deg²; [comparison study](https://pmc.ncbi.nlm.nih.gov/articles/PMC9112722/)). Caveats: BCEA assumes normality (kernel-density ISOA differs), and it grows with epoch length until ~20 s saturation ([Chung, Aga-Oglu & Krishnan, JOV 2018](https://doi.org/10.1167/18.10.1000); [ProgStar methods comparison](https://pmc.ncbi.nlm.nih.gov/articles/PMC6733530/)).

---

## 3.4 Smooth pursuit and its failures

**Initiation latency ~100–150 ms.** Rashbass measured smooth-movement onset "after a reaction time of about 150 msec," with target-matching speed only ~400 ms after motion onset ([J Physiol 1961, full PDF](http://wexler.free.fr/library/files/rashbass%20(1961)%20the%20relationship%20between%20saccadic%20and%20smooth%20tracking%20eye%20movements.pdf)); modern human values run ~100–140 ms. The first ~100 ms of the response is **open-loop** — visual feedback cannot yet influence it (monkey ~100 ms, human ~130 ms; [J Neurosci 39(14):2709](https://www.jneurosci.org/content/39/14/2709); [Tychsen & Lisberger 1986](https://doi.org/10.1152/jn.1986.56.4.953)).

**Open-loop structure (verified, magnitude flagged):** Lisberger & Westbrook's monkey classic and Tychsen & Lisberger's human replication show **two components**: in the first 20 ms, eye acceleration is direction-correct but *independent* of target position, velocity, and background; from ~60–100 ms it depends strongly on all of them — highest for images near the fovea moving **foveopetally**, velocity-tuned with peak effectiveness at **30–60°/s** (dark background: acceleration grows with velocity up to 150°/s) ([Lisberger & Westbrook 1985](https://doi.org/10.1523/JNEUROSCI.05-06-01662.1985); [Tychsen & Lisberger 1986](https://doi.org/10.1152/jn.1986.56.4.953); [Lisberger & Pavelko 1989](https://doi.org/10.1152/jn.1989.61.1.173)). I could **not verify a specific °/s² figure** for human initial acceleration this session (the 40–100, occasionally 200 °/s² anchor) — structure and tuning are verified, the absolute magnitude is **flagged unverified** rather than guessed.

**Velocity-matching limit:** Rashbass found the smooth component linearly related to target velocity **up to 100°/s**; closed-loop gain is near 1 only to ~30°/s, falling to ~0.75 by 60°/s with catch-up saccades appearing above ~30°/s, and pursuit saturates in the ~60–100+ °/s range depending on target size/contrast (all cross-referenced with sources in [`human-movement-and-perception-research.md`](human-movement-and-perception-research.md) §2.4.3 — not re-derived here).

**The Rashbass paradigm — pursuit is driven by velocity, not position.** Displace a target 3° *left* while starting it moving *right*: the eye first accelerates smoothly **right, away from the target's current position**, then makes a small leftward saccade to cancel the lead. Conclusion: smooth movements are stimulated by the target's **retinal velocity** irrespective of position; saccades by **position** irrespective of velocity; the two systems superimpose independently (Rashbass 1961, above). The step-ramp with step ≈ −ramp × latency is now the standard tool for saccade-free pursuit initiation. One modern nuance: a **position input to pursuit exists** — flashes during ongoing pursuit (position error without velocity) evoke smooth movements toward the flash with ~85 ms latency, decaying with ~276 ms time constant, but only if the flash is selected as the target ([Blohm, Missal & Lefèvre 2005](http://www.compneurosci.com/doc/blohm05b.pdf)).

**Catch-up saccades and their trigger.** de Brouwer, Missal, Barnes & Lefèvre (double-step ramp): the governing variable is the **eye-crossing time** T_XE = −PE/RS (time until pursuit alone would foveate). Saccades are *suppressed* inside the **smooth zone T_XE ∈ (40, 180) ms** and triggered outside it (too-soon or too-late crossing); catch-up amplitude takes both position error and retinal slip into account, with the RS contribution **saturating above ~15°/s**; and the system needs ~90 ms minimum to incorporate a target-trajectory change ([de Brouwer et al. 2002a](https://doi.org/10.1152/jn.00621.2001); [de Brouwer et al. 2002b](https://journals.physiology.org/doi/full/10.1152/jn.00432.2001)). Modern refinement: the trigger is better described as a stochastic decision on **predicted position error** — large predicted errors (>10°) give short, low-variability trigger times; blur (uncertainty) lengthens and suppresses triggering ([eNeuro, PMC6964921](https://pmc.ncbi.nlm.nih.gov/articles/PMC6964921/)).

**Why the eye alternates (pursuit vs saccade complementarity):** pursuit is a velocity servo — smooth, low-gain-limited, blind to position; saccades are a position servo — fast, ballistic, blind during flight. Targets faster than ~30°/s, or unpredictable direction changes, inevitably accumulate position error that only a saccade can erase; saccades in turn cost ~200–300 ms of degraded vision (§3.6), so the system prefers pursuit whenever its predictions hold (predictable motion → pure pursuit, Barnes; via de Brouwer 2002b). A game camera that "tracks" like an eye should therefore show velocity-following with periodic positional snaps, not proportional control.

---

## 3.5 Gaze-holding reflexes interacting with vision

### 3.5.1 VOR, frequency response, and cancellation

VOR gain/latency numbers (gain 0.75–1.1 context-dependent, latency 3–10 ms, slip tolerance ~0.2–6°/s) are established in [`human-movement-and-perception-research.md`](human-movement-and-perception-research.md) §2.3 and are not repeated. What Part 2 does not cover:

**The frequency-response problem.** The canal-cupula dynamics are a high-pass filter (torsion-pendulum; time constants T_L ≈ 4.2–5.7 s, T_S ≈ 3–5 ms, movement doc §2.2.1). Left alone, the VOR would stop compensating sustained rotation within a few seconds as the cupula recenters — and would likewise produce a spurious response at rotation *stop*. Two mechanisms fix this: (i) the **velocity storage mechanism (VSM)** — a central integrator in the vestibular nuclei (commissural fibers; inhibited by the cerebellar nodulus/uvula) that extends the effective time constant from the canal's ~4–6 s to a behavioral **15–30 s in healthy subjects** (human-tuned models: ~11–12 s; classic human normative band 15–30 s, unilateral damage 6–12.7 s, bilateral <6 s) ([Diaz Artiles-class, PMC9437187](https://pmc.ncbi.nlm.nih.gov/articles/PMC9437187/); [Karmali, PMC9103412](https://pmc.ncbi.nlm.nih.gov/articles/PMC9103412/); [Choi 2026, e-rvs](https://e-rvs.org/journal/view.php?number=1016)); and (ii) the **optokinetic system**, which takes over at low frequencies where the canals are blind (below). The VSM is a genuine Kalman-filter-style accuracy/precision trade: a longer time constant improves low-frequency velocity estimation but integrates more neural noise — which is why it shortens with age, vestibular damage, and larger stimuli (Karmali 2021, 2022).

**VOR cancellation during gaze shifts** (deeper mechanisms behind movement doc §2.3.2): during combined eye-head gaze shifts the VOR is inhibited 40–96% for a 40° step (Pélisson, Prablanc & Urquizar 1988), is restored with a ~40 ms time constant near saccade end (Lefèvre et al.), and the whole thing behaves as a single gaze-feedback controller rather than separate eye and head loops (Roy & Cullen) — all cited with URLs in movement doc §2.3.2. The design lesson for an engine: don't compose "eye controller + head controller + stabilization reflex" additively; real gaze is one feedback loop with a context-gated reflex.

### 3.5.2 Optokinetic nystagmus

**Structure:** large-field visual motion drives a **slow phase** in the direction of stimulus motion (retinal-image stabilization) punctuated by **quick phases** — saccade-like resets in the opposite direction ([Human OKN stochastic analysis, JOV](https://jov.arvojournals.org/article.aspx?articleid=2192086)). It exists because it is the visual complement of the canal VOR: during sustained self-motion the canals decay (seconds) while the optic flow persists; OKN holds the retinal image still on the timescale of minutes. When the lights go out mid-OKN, **optokinetic after-nystagmus (OKAN)** continues from the charged VSM and decays over tens of seconds, sometimes reversing (OKAN-II) — the visual analogue of post-rotatory nystagmus ([Nooij et al. 2018](https://link.springer.com/article/10.1007/s00221-018-5340-1)).

**Fusion limits / saturation:** slow-phase gain is **below 1.0 even at low stimulus speeds**, and during constant-acceleration rotation the eye tracks the drum accurately only up to **~60°/s** — the "optokinetic fatigue threshold" — above which the eye lags ([Mizukoshi, Fabian & Stahle 1977](https://doi.org/10.3109/00016487709123954); [slow-phase gain analysis, PubMed 3827762](https://pubmed.ncbi.nlm.nih.gov/3827762/)). At higher speeds, retinal slip grows: in one 160-s-per-trial study, retinal slip exceeded 5°/s (acuity-destroying) in **79% of slow phases at 30°/s and 91% at 40°/s** (stochastic OKN paper above) — OKN is a stabilizer, not a perfect one. Stimulus quality matters: gain depends on contrast, spatial frequency, and color, with proposed gain thresholds and onset latencies of ~360–525 ms as contrast falls ([MDPI Appl. Sci. 2022](https://www.mdpi.com/2076-3417/12/23/11991)).

**Look OKN vs stare OKN:** *stare* OKN — passive viewing, "look through" the stimulus — has small-amplitude, frequent slow phases and lower gain; *look* OKN — deliberately tracking individual stripes — has large, infrequent slow phases and higher gain, significantly better at all velocities above the slow range (15–60°/s tested; horizontal > vertical in both) ([prospective look/stare study, PubMed 15785979](https://pubmed.ncbi.nlm.nih.gov/15785979/); [Garbutt-class horizontal/vertical look & stare OKN, IOVS](https://iovs.arvojournals.org/article.aspx?articleid=2164286)). Quantitatively, at 10°/s random-dot motion: slow-phase gain ≈ **1.0 (look), 0.90 (stare), 0.60 (stare with limited dot lifetime)**, and look-OKN quick phases sit on the **same main sequence as visually guided saccades** while stare quick phases are slower and longer for the same amplitude ([Kaminiarz, Königs & Bremmer 2010](https://doi.org/10.1167/9.8.405); [author PDF](https://pdfs.semanticscholar.org/17ff/b83fae45e16a63e74cc1b65edc0b61a96b51.pdf)). In practice "look OKN" is pursuit plus resets; "stare OKN" is the true reflex.

### 3.5.3 Vection

Full-field motion in a stationary observer induces illusory self-motion — vection. Verified properties: onset **latency of seconds** that (a) builds — reports of subjective rotation increase over the **first ~30 s** of drum rotation, with perceived self-acceleration and drum-deceleration trading off over that window ([classic developmental time-course study, Springer](https://link.springer.com/article/10.3758/BF03199537)); (b) **shortens with rising scene velocity** and correlates inversely with the individual's optokinetic weight (larger visual weight → shorter latency) ([springermedizin.de CV model test](https://www.springermedizin.de/optokinetic-circular-vection-a-test-of-visual-vestibular-conflic/25621160)); (c) is individually stable (test–retest r ≈ 0.8) and psychophysically well-behaved (Stevens exponent ≈ 0.95 for circular vection magnitude) ([Kennedy et al. 1996](https://doi.org/10.3233/ves-1996-6502)). Vection strength couples to the VSM: strong-vection stimuli leave a larger charged velocity store (bigger OKAN), and **slow-phase elongation of OKN tracks vection onset** — usable as an objective vection detector in VR ([Nooij et al. 2018](https://link.springer.com/article/10.1007/s00221-018-5340-1); [Manukyan et al. 2021](https://doi.org/10.1016/j.procs.2021.09.054)). This is why VR cares: vection *is* the presence engine of large-field motion, and the same stimulus that sells self-motion also drives visually induced motion sickness (below).

### 3.5.4 Vestibular (post-rotatory) nystagmus — the spins

**Mechanism, step by step.** Spin someone at constant yaw velocity: at spin-up the endolymph lags, the cupula deflects and the canal afferents fire, driving compensatory slow phases (the per-rotatory VOR). The cupula is a spring — it **returns to rest with a time constant of ~3.5–7 s** (canal mechanics; [Diaz Artiles 2022, PMC9437187](https://pmc.ncbi.nlm.nih.gov/articles/PMC9437187/)), so after ~15 s of rotation the raw canal signal is down to e^{−15/5} ≈ 5% of its initial value. But the **velocity storage integrator keeps the percept and the nystagmus alive** with its 11–20+ s time constant. When the spin **stops**, the endolymph keeps moving by inertia, deflecting the cupula the *other* way — a step of opposite sign — producing **post-rotatory nystagmus (PRN)**: slow phases as if the head were still rotating the old way, quick phases beating the old spin direction, decaying with the VSM time constant. Local check: at τ_VSM = 20 s, slow-phase velocity is still **47%** of initial 15 s after stopping and 22% at 30 s; three time constants ≈ 60 s of measurable nystagmus — hence a full ~30–60 s of "the world keeps spinning" after a playground-style spin.

**Why the world appears to spin and balance fails.** The PRN slow phases drag the retinal image across the retina (the world seems to rotate opposite the (fictitious) head rotation); because the eyes are executing compensatory rotations for a rotation that is not happening, every visual object appears to move. Perception shares the same integrator as the reflex: in vestibulo-cerebellar patients the *perceptual* and *reflex* VSM time constants co-vary at r = 0.93–0.95 — one stored velocity, two readouts ([Bertolini-class, PMC3376140](https://pmc.ncbi.nlm.nih.gov/articles/PMC3376140/)). Posturally, the VSM feeds the vestibulopostural reflex: a false rotation estimate is decomposed into a false **inertia** cue, and participants' head posture and reported body pull line up with the velocity-storage model's predicted inertial direction — the integrator is a shared gateway for eyes, perception, and balance, which is why the post-spin stagger is directionally specific ([Choi et al. 2022, PMC10008054](https://pmc.ncbi.nlm.nih.gov/articles/PMC10008054/)).

**Dizziness and nausea.** The standard account is **sensory conflict** (Reason & Brandt 1975): the visually induced/fictitious rotation is not corroborated by the otoliths/body. Its critics, with evidence: the conflict exists only at vection onset, yet symptoms build for minutes after full vection is reached; an alternative places the **VSM as the common denominator** — motion-sickness susceptibility correlates positively with the VSM time constant (Quarck et al.; Guo et al.), labyrinthine-defective patients are immune to visually induced motion sickness, and baclofen (which shortens the time constant) reduces susceptibility ([Nooij et al. 2018](https://link.springer.com/article/10.1007/s00221-018-5340-1); [labyrinthine-function-loss review, PMC11239394](https://pmc.ncbi.nlm.nih.gov/articles/PMC11239394/)). Report both; the field has not settled.

---

## 3.6 The complete timeline of a fast side-glance

The user's core question. Assembled from the verified numbers above — a **20° horizontal glance** (D = 20 + 2.7×20 ≈ 74 ms, peak velocity ~400–500°/s):

| t (ms) | Event | Source |
|---|---|---|
| 0 | Target appears in periphery | — |
| +50–150 | **Covert attention shifts to the target** (pre-saccadic attention: benefits at the saccade-target location before the eyes move; the attention literature is reviewed in the perception companion doc; neurophysiologically, SC/FEF activity ramps from ~100 ms before the movement) | [Marino & Mazer 2016](https://pmc.ncbi.nlm.nih.gov/articles/PMC4743436/); [rostral SC buildup](https://www.jneurosci.org/content/23/10/4333) |
| ~+150–200 | Saccade latency elapses (typical 150–250 ms; express saccades in the gap paradigm can approach the minimal retina→muscle conduction time) | [Rashbass 1961](http://wexler.free.fr/library/files/rashbass%20(1961)%20the%20relationship%20between%20saccadic%20and%20smooth%20tracking%20eye%20movements.pdf); [Munoz-class gap studies](https://pubmed.ncbi.nlm.nih.gov/9334428/) |
| −75 to −50 | **Saccadic suppression begins** (threshold elevation up to 1 log unit, magnocellular-selective; MSTd suppression from up to −90 ms) | [Diamond et al. 2000](https://pmc.ncbi.nlm.nih.gov/articles/PMC6773104/); [J Neurosci 28:10952](https://www.jneurosci.org/content/28/43/10952) |
| −20 | **Superior colliculus saccade-related burst begins** (SC leads saccade by ~20 ms; FEF by ~28 ms — the point of no return) | [Hanes & Paré 2003](https://www.jneurosci.org/content/23/16/6480) |
| 0 | **Saccade onset**. Pulse-step command: motoneuron burst (~half the saccade) + tonic step from the neural integrator | §3.1.5 |
| 0 → +74 | The movement: 20°, peak ~400–500°/s early-skewed; retinal image sweeps 20°; smear is low-SF, low-contrast, masked pre & post, and suppressed | §3.2.1–3.2.2 |
| −50 → +50 (perceptual) | Percept of the **new** target is antedated to ~50 ms before saccade onset (chronostasis) — the gap is priced at zero | [Yarrow et al. 2001](https://www.nature.com/articles/35104551) |
| +50 → +310 | **Postsaccadic enhancement**: flashed-probe responses exceed control from ~50 ms, peak 100–130 ms (mean 310%), settling by ~200–450 ms; the new fixation starts with *elevated* sensitivity | [J Neurosci 28:10952](https://www.jneurosci.org/content/28/43/10952) |
| pre & post | **Spatial map re-calibration / remapping**: LIP/FEF/SC receptive fields shift anticipatorily (Duhamel 1992 forward remapping; modern evidence mixes *convergent* shifts toward the saccade target with *forward* shifts around onset — unified in a CD-gated + attention-modulated circuit model); attention leaves a retinotopic trace at the old location while the spatiotopic locus takes over | [Marino & Mazer 2016](https://pmc.ncbi.nlm.nih.gov/articles/PMC4743436/); [Golomb & Mazer 2021](https://www.annualreviews.org/content/journals/10.1146/annurev-vision-032321-100012); [Wang et al. 2023, PMC10542176](https://pmc.ncbi.nlm.nih.gov/articles/PMC10542176/); [Szinte et al. 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC6328271/) |

**The total bill:** planning latency (~150–250 ms) + movement (20–100 ms by amplitude) + recovery to full sensitivity (~50–150 ms) ≈ **200–400 ms**, of which the movement itself is the smallest part. During this window, detailed perception at the *old* location is severely degraded — attention has already left, suppression is active, and the postsaccadic enhancement is busy elsewhere. We never notice because suppression deletes the smear, omission deletes the gap, chronostasis deletes the *time*, and remapping keeps the world's coordinates glued together. (Cross-ref: the sampling-rate consequence — 2–3 such glances per second as the ceiling on informative fixations — is developed in [`human-movement-and-perception-research.md`](human-movement-and-perception-research.md) §2.7 and §3.4.)

---

## 3.7 Blinking — "do the human eyes get blocked?"

**Direct answer: yes — retinal illumination is interrupted every few seconds for 100–150 ms of full occlusion inside a 200–400 ms motor event, totaling roughly 2–5% of waking time (computed: 10–20 blinks/min × 100–150 ms = 1.7–5.0%) — and you never see it.** The mechanisms below are why.

### 3.7.1 Rates

The canonical spontaneous rate is **~15/min awake** (roughly one blink every 4 s), but the honest picture is task-banded. Doughty's analysis of 75 years of measurements gives 95% confidence bands: **reading posture 1.4–14.4 blinks/min, primary gaze (silent) 8.0–21.0, conversation 10.5–32.5** ([Doughty 2001](https://www.researchgate.net/publication/11653609_Consideration_of_Three_Types_of_Spontaneous_Eyeblink_Activity_in_Normal_Humans_during_Reading_and_Video_Display_Terminal_Use_in_Primary_Gaze_and_while_in_Conversation)). A whole-novel reading study measured **10.3 ± 5.9 blinks/min with mean blink duration 128.8 ms** (30,367 blinks), and showed blinks are *placed*: at punctuation, line ends, and after low-frequency/high-surprisal words — cognitive breaks, not random ([Sci Rep 2025](https://www.nature.com/articles/s41598-025-04839-y)). Screen work reduces rate below an idle control on every display type, with **incomplete blinks rising to 56% on desktop monitors (vs 38% control)** — the dry-eye mechanism ([Talens-Estarelles et al. 2021](https://link.springer.com/article/10.1007/s00417-021-05490-9); [PubMed](https://pubmed.ncbi.nlm.nih.gov/34779906/)). One webcam-monitoring study measured a striking **43 ± 15 blinks/min** in its (conversational) baseline vs 24–25 during trailer-watching/reading ([PMC11744486](https://pmc.ncbi.nlm.nih.gov/articles/PMC11744486/)) — an outlier consistent with Doughty's upper conversational band. Net verified statement: **reading 3–10, primary gaze 8–21, conversation 10–32**; the "3–5 reading" anchor is at the low end of a wide verified band; "10–25 screen" is consistent with reading-band values measured under display conditions. Only ~2 blinks/min are needed for tear-film lubrication — the excess is cognitively/dopaminergically modulated (blink rate tracks central dopamine; Karson/Jongkees literature via the reviews above).

### 3.7.2 Duration and phases

Complete blink, phase by phase (ranges from the source studies, not single values): **down phase (closing) ~50–130 ms** (Stern-class; ~75–100 ms in Costela-class measurements); **closed (contact) phase ~10–50 ms** (longer with dry eye); **up phase (opening) ~100–200 ms — roughly twice the down phase**, because the levator palpebrae must overcome passive orbital forces ([Frontiers 2023 blink review](https://www.frontiersin.org/journals/systems-neuroscience/articles/10.3389/fnsys.2023.1242654/full)). Total blink: **200–400 ms** (Volkmann-class; up to 250–450 ms), of which the pupil is fully occluded only **40–200 ms, typically 100–150 ms** (Riggs/Volkmann/Tsubota/VanderWerf, collated in the Frontiers review and [Riggs-class suppression paper](https://www.sciencedirect.com/science/article/abs/pii/0042698981900122)). Lid kinematics: the upper lid reaches maximum speed (~17–20 cm/s, occasionally >40 cm/s) as it crosses the visual axis; closing-phase peak angular velocities ~285°/s vs opening ~151°/s ([Doane 1980](https://pubmed.ncbi.nlm.nih.gov/7369314/); [blink-detection methods paper](https://journals.sagepub.com/doi/10.1177/1071181312561143)). The blink amplitude–velocity relation is itself a "main sequence" — bigger blinks are proportionally faster (Talens-Estarelles 2021). Partial (incomplete) blinks are a normal subpopulation (see §3.7.1 rates) and dominate on displays.

### 3.7.3 Reflex vs voluntary vs spontaneous

Reflex blinks (corneal air-puff, dazzle, menace) have the **fastest down phases**; voluntary blinks are slower; spontaneous blinks are the slowest — no urgency signal exists for them; **up phases do not differ** by type ([Frontiers 2023 review](https://www.frontiersin.org/journals/systems-neuroscience/articles/10.3389/fnsys.2023.1242654/full), citing Doane/Stern/Evinger). Crucially, **blink suppression is the same for reflex and voluntary blinks** — it rides the common efferent pathway, not the voluntary command (Manning et al. 1983, below).

### 3.7.4 Blink suppression — why no blackouts

Volkmann's classic experiments delivered light **through the roof of the mouth** (fiber optic), bypassing the eyelids entirely: sensitivity to luminance decrements still dropped during blinks — the loss is **neural, not optical** ([Volkmann, Riggs & Moore 1980, Science](https://doi.org/10.1126/science.7355270)). Verified time course and magnitude:

- Sensitivity loss begins **~100 ms before** blink onset, is maximal **~30–40 ms before** the lid begins covering the pupil, and does not return to baseline until **~200 ms after** blink onset — a total suppressive window of ~300 ms around each blink ([Frontiers 2023 review](https://www.frontiersin.org/journals/systems-neuroscience/articles/10.3389/fnsys.2023.1242654/full), collating Volkmann 1980/1982 and Manning 1983; the Springer-facing summary of the same literature states 50–100 ms before and 100–150 ms after — both bands are reported, the difference is criterion placement).
- Magnitude: threshold elevation **0.4–0.9 log unit** (typical ~0.6–0.7, i.e. a ~5× stronger decrement needed during a blink); a real blink is subjectively matched by a Ganzfeld dimming **4–10× weaker** than the physical ~2-log-unit (~100×) illumination cut the eyelid actually produces ([Volkmann, Riggs, Ellicott & Moore 1982](https://doi.org/10.1016/0042-6989(82)90035-9); [Riggs, Volkmann & Moore 1981](https://www.sciencedirect.com/science/article/abs/pii/0042698981900122); [Manning, Riggs & Komenda 1983](https://doi.org/10.3758/bf03202953)).
- The suppression is linked primarily to the **efferent discharge that closes the lid** (it is large during closing/blinking, small or absent during pure opening from a closed eye) — Volkmann 1982's decisive asymmetry.
- fMRI: lateral occipital cortex (incl. V5/MT) is suppressed during blinks more than during matched external darkenings, while a medial parieto-occipital region (V6A homologue) *activates* — suppression plus an active continuity mechanism (via the Frontiers review).

**Continuity is not "filled in" — it is *ignored*.** Irwin's tests of the temporal-antedating and perceptual-maintenance hypotheses both failed: a stimulus interrupted by a blink is judged **117 ms shorter** than its physical duration — almost exactly the measured 114 ms of pupil coverage. The occluded time is simply excluded from duration estimates; the efferent signal tags the gap as self-caused and the perceptual system drops it ([Irwin 2016 PDF](http://wexler.free.fr/library/files/irwin%20(2016)%20perceiving%20a%20continuous%20world%20across%20voluntary%20eye%20blinks.pdf)). Between suppression (deletes the transient), backward/forward masking by the pre- and post-blink scenes, and this time-exclusion, the blackout never reaches awareness — the same trick the system uses for saccades (§3.2), bought with the same currency.

### 3.7.5 What the eyes do during a blink — correcting the Bell's-phenomenon folk claim

The textbook claim is that the eyes rotate **up** (Bell's phenomenon) during every blink. Modern measurements say otherwise: search-coil recordings show voluntary and reflex blinks are consistently accompanied by transient **downward and nasalward rotations of 1–5°**, shorter than the lid movement, little affected by gaze eccentricity up to 15°, and present even when the lids are restrained open — they arise from **co-contraction of the extraocular muscles** (which also retracts the globe up to ~1–6 mm), not from lid rubbing ([Collewijn, van der Steen & Steinman 1985](https://doi.org/10.1152/jn.1985.54.1.11); [Bour, Aramideh & Ongerboer de Visser 2000](https://doi.org/10.1152/jn.2000.83.1.166); [Riggs-class visual-persistence study](http://wexler.free.fr/library/files/riggs%20(1987)%20blink-related%20eye%20movements.pdf); [Doane 1980](https://pubmed.ncbi.nlm.nih.gov/7369314/)). Large tonic rotations (occasionally up to ~17°) appear only in **slow/forceful blinks or prolonged closure**, are highly variable, and go up in only about half of subjects ([Kirchner & Lappe 2022, high-speed MRI](https://www.uni-muenster.de/imperia/md/content/psyifp/ae_lappe/2022.kirchner__et_al.pdf)). Bell's *upward* rotation is real as a **protective reflex when lid closure is impeded or forced** — which is how clinicians elicit it — not as the normal-blink behavior. Eye position typically returns to within ~0.2° of fixation before the lids reopen, with small corrective saccades ~300–400 ms later when it doesn't.

### 3.7.6 Purpose: the tear film

Each blink re-spreads the ~3-µm three-layer tear film over the cornea (optical details belong to the optics companion document; break-up time and refractive shifts between blinks are its §territory). The motor-side facts: the lid descent carries the tear film upward-rewetting the ocular surface at 17–20 cm/s, the ~2 blinks/min required for lubrication versus the ~15/min delivered shows the tear function is not rate-limiting (Sci Rep 2025, §3.7.1), and the *incompleteness* of display-work blinks — not their rate alone — is what starves the exposed strip of cornea (Talens-Estarelles 2021).

---

## 3.8 The fixational "spotlight" and its cost structure

Synthesis. At any instant the fovea delivers full resolution over ~2° (cross-ref movement doc Part 3 §3.1; the optics companion doc owns acuity-vs-eccentricity). Everything outside is coarse — good enough to *detect* and *select*, not to read. Each repositioning of the spotlight costs **~200–300 ms** end-to-end (§3.6: 150–250 latency + 20–100 movement + recovery), during which the old location is perceptually degraded and the new one is not yet online. That cost structure forces the observed sampling strategy:

- **2–3 informative fixations per second** — the hard ceiling (verified rate in movement doc §2.4.2; consistency check: mean reading fixation 304 ± 57 ms + mean saccade 32 ± 4 ms ≈ 336 ms/cycle ≈ 3 Hz, from the [binocular-coordination reading study, JOV](https://jov.arvojournals.org/article.aspx?articleid=2122351)).
- **Look-ahead, not look-down**: fixations are spent on ground that *informs future action* (2–3 steps ahead on terrain, 1–2 s ahead at bends) — cross-ref movement doc §2.5, which has the terrain statistics.
- **Peripherally guided targeting**: the saccade target is selected from the periphery's coarse representation (which is why saccades can be tricked by peripheral illusions but land within a fraction of a degree of the *selected* target), with covert attention pre-deployed to the target ~50–150 ms before launch and the saccadic system correcting residual error only via later corrective saccades (§3.2, §3.6). Blink placement rides the same budget: blinks are parked in syntactic and semantic gaps where the information rate is momentarily low (§3.7.1) — the oculomotor system schedules its two kinds of blindness (saccades and blinks) around the information flow.

The design consequence for a camera system: human-equivalent looking is a **discrete, budgeted sampling process** — a ~2° high-resolution probe, relocated 2–3×/s at 200–300 ms per relocation with suppressed transit, not a smooth pan. A game camera that instantly and continuously "sees" everything is not modeling an eye; it is modeling a radar.

---

## 3.9 Consolidated boxed equations, summary table, and primary sources

$$
\boxed{\;D_{sacc} \approx 20\ \text{ms} + (2\text{–}2.7)\tfrac{\text{ms}}{\text{deg}}\,A;\quad V_{peak}\ \text{saturates}\ \sim 500\text{–}900\,°/\text{s (method-dependent)};\quad \text{burst} \approx D/2\;}
$$

$$
\boxed{\;F_{MN}(t) = P(t) + k\!\int P\,dt \quad\text{(pulse-step; leaky integrator } \tau \approx 25\,\text{s} \Rightarrow \text{gaze-evoked nystagmus)}\;}
$$

$$
\boxed{\;\text{Listing: } T=\tfrac{HV}{2}\ \text{(Helmholtz)};\quad \text{half-angle: velocity axis tilts } \tfrac{\alpha}{2};\quad \mathrm{TAC}_{meas} \approx 0.57\ (\text{horiz}),\ 0.34\ (\text{vert})\;}
$$

$$
\boxed{\;\text{Suppression: } -75\to+50\ \text{ms around saccade},\ 0.5\text{–}1\ \log\ \text{unit, magno-selective};\ \text{enhancement } +50\to+310\ \text{ms}\;}
$$

$$
\boxed{\;\text{Pursuit: latency } \sim\!100\text{–}150\ \text{ms, open-loop } \sim\!100\ \text{ms};\ \text{catch-up saccade unless } T_{XE}=-PE/RS \in (40,180)\ \text{ms}\;}
$$

$$
\boxed{\;\text{Rotation: canal } \tau \approx 5\ \text{s} \to \text{VSM } \tau \approx 11\text{–}20\ \text{s} \Rightarrow \text{post-rotatory nystagmus } \sim\!30\text{–}60\ \text{s}\;}
$$

$$
\boxed{\;\text{Blink: } 200\text{–}400\ \text{ms total},\ 100\text{–}150\ \text{ms occlusion},\ 8\text{–}32/\text{min by task};\ \text{occlusion duty } 1.7\text{–}5\%;\ \text{eye rotates down+nasal } 1\text{–}5°\;}
$$

$$
\boxed{\;\mathrm{BCEA}=2k\pi\sigma_H\sigma_V\sqrt{1-\rho^2},\ k_{63}=1.147,\ k_{95}=2.996;\ \text{normals} \approx 0.8 / 2.4\ \text{deg}^2\;}
$$

**Key numbers at a glance.** Saccade 20°: 74 ms, ~450°/s peak, blind ±50 ms, glance total 200–400 ms. Fixation: microsaccades 0.5–2 Hz (most <30′), drift 4′/s (instructed) to ~50′/s (natural), tremor ~84 Hz at 6–30″. Pursuit: accurate to ~30°/s. OKN: tracks to ~60°/s, gain<1 always. Spin: canal 5 s, VSM 11–20 s, world-drift 30–60 s. Blink: every ~4 s, 100–150 ms of true blackout, 0.4–0.9 log unit suppression, ~2–5% of waking vision deleted — invisibly.

### Primary sources (all verified live via Exa, 2026-09-06)

1. Bahill, Clark & Stark 1975, *The main sequence* — [PDF](http://www.visualcognition.ca/spering/reading/Bahill.Clark.Stark.MathBiosci.1975.pdf), [doi](https://doi.org/10.1016/0025-5564(75)90075-9)
2. Bahill-class normative saccade database — [PDF](http://sysengr.engr.arizona.edu/publishedPapers/Database.pdf)
3. Baloh et al. 1975, *Quantitative measurement of saccade amplitude, duration, and velocity* — [Neurology](https://www.neurology.org/doi/10.1212/WNL.25.11.1065)
4. Collewijn, Erkelens & Steinman 1988a, horizontal saccades — [J Physiol](https://doi.org/10.1113/jphysiol.1988.sp017284), [Europe PMC](https://europepmc.org/article/MED/3253429)
5. Collewijn, Erkelens & Steinman 1988b, vertical saccades — [J Physiol](https://doi.org/10.1113/jphysiol.1988.sp017285), [Europe PMC](https://europepmc.org/article/MED/3253430)
6. *Vertical and oblique saccadic eye movements* (43-subject TV study) — [PubMed 6668756](https://pubmed.ncbi.nlm.nih.gov/6668756/)
7. Diamond, Ross & Morrone 2000, *Extraretinal control of saccadic suppression* — [PMC6773104](https://pmc.ncbi.nlm.nih.gov/articles/PMC6773104/), [doi](https://doi.org/10.1523/JNEUROSCI.20-09-03449.2000)
8. Idrees, Baumann, Franke, Münch & Hafed 2020, *Perceptual saccadic suppression starts in the retina* — [Nat Commun](https://doi.org/10.1038/s41467-020-15890-w), [PMC7181657](https://pmc.ncbi.nlm.nih.gov/articles/PMC7181657/)
9. Watson & Krekelberg, *Intrasaccadic suppression is dominated by reduced detector gain* — [PMC3704127](https://pmc.ncbi.nlm.nih.gov/articles/PMC3704127/)
10. *Saccadic modulation of neural responses (MSTd)* — [J Neurosci 28(43):10952](https://www.jneurosci.org/content/28/43/10952)
11. Yarrow, Haggard, Heal, Brown & Rothwell 2001, chronostasis — [Nature](https://www.nature.com/articles/35104551), [PDF](https://kielanyarrow.github.io/MyPage/papers/Chronostasis.pdf)
12. Yarrow, Haggard & Rothwell 2004, arousal/time — [PDF](https://openaccess.city.ac.uk/id/eprint/330/2/Action_arousal_and_subjective_time.pdf)
13. Yarrow et al. 2004, saccade-type invariance — [doi](https://doi.org/10.1162/089892904970780)
14. Yarrow et al. 2006, perisaccadic timing biases — [doi](https://doi.org/10.3758/bf03193722)
15. Chronostasis review chapter — [PDF](https://kielanyarrow.github.io/MyPage/papers/Chronostasis_Review.pdf)
16. *Cerebellum and ocular motor control* — [Frontiers Neurol 2011](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2011.00053/full)
17. *Neural integration in ocular motility* (OMLAB) — [PDF](https://omlab.org/Personnel/lfd/Jrnl_Arts/071_Neural_Integration_OM_1989.pdf)
18. Marti, Straumann, Büttner & Glasauer 2008, downbeat-nystagmus model — [doi](https://doi.org/10.1007/s00221-008-1396-7)
19. Shaikh & Ghasia-class, leaky+unstable integrator in MS — [J Neuro-Ophthalmol](https://journals.lww.com/jneuro-ophthalmology/fulltext/2020/06000/_leaky__and__unstable__neural_integrator_can.15.aspx)
20. Arikan et al., neural integrator systematic review — [Acta Ophthalmol](https://onlinelibrary.wiley.com/doi/10.1111/aos.13307)
21. Klier, Meng & Angelaki 2006, 3D plant kinematics — [PMC6675172](https://pmc.ncbi.nlm.nih.gov/articles/PMC6675172/)
22. Wong 2004, *Listing's law* review — [PDF](http://individual.utoronto.ca/agneswong/Wongpublications/Wong_2004-Listings_law_review.pdf)
23. Handzel & Flash 1995, geometry of eye rotations — [NeurIPS](https://proceedings.neurips.cc/paper_files/paper/1995/file/d736bb10d83a904aefc1d6ce93dc54b8-Paper.pdf)
24. Listing's law overview — [en.wikipedia.org](https://en.wikipedia.org/wiki/Listing's_law)
25. Demer 2006, mechanical vs neural factors — [PMC1847330](https://pmc.ncbi.nlm.nih.gov/articles/PMC1847330/)
26. Demer 2002, orbital pulley system — [doi](https://doi.org/10.1111/j.1749-6632.2002.tb02805.x)
27. Demer-group, OL/GL fascicular specialization — [PMC1978188](https://pmc.ncbi.nlm.nih.gov/articles/PMC1978188/)
28. Bradley's Neurology in Clinical Practice, ch. 44 — [online](https://elsevier-elibrary.com/contents/fullcontent/84861/epubcontent_v2/OEBPS/xhtml/chp0044_1.xhtml)
29. Adler's Physiology of the Eye, EOM chapter — [clinicalpub](https://clinicalpub.com/the-extraocular-muscles/)
30. StatPearls, eye muscles — [NBK470534](https://www.ncbi.nlm.nih.gov/sites/books/NBK470534/); extraocular muscles — [NBK519565](https://www.ncbi.nlm.nih.gov/books/NBK519565/)
31. NCBI Neuroscience, EOM actions — [NBK10793](https://www.ncbi.nlm.nih.gov/books/NBK10793/)
32. Wasicky et al., human EOM fiber types — [IOVS](https://iovs.arvojournals.org/article.aspx?articleid=2123010)
33. Kjellgren-class, MyHC isoforms in human EOM — [IOVS](https://iovs.arvojournals.org/article.aspx?articleid=2124289)
34. Hoh 2020, MyHC in EOM — [doi](https://doi.org/10.1111/apha.13535)
35. Porter-group, biological organization of EOM — [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0079612305510021)
36. Rucci & Poletti 2015, fixational eye movements — [PMC5082990](https://pmc.ncbi.nlm.nih.gov/articles/PMC5082990/)
37. Martinez-Conde, Otero-Millán & Macknik 2016, ocular stillness — [Phil Trans R Soc](https://royalsocietypublishing.org/doi/10.1098/rstb.2016.0204)
38. Rolfs 2009, microsaccade review — [PDF](http://www.martinrolfs.de/Rolfs_MicrosaccadeReview.pdf)
39. Rucci, *The Unsteady Eye* — [PMC4385455](https://pmc.ncbi.nlm.nih.gov/articles/PMC4385455/)
40. *Eye movements between saccades: drift and tremor* — [Vision Research](https://www.sciencedirect.com/science/article/pii/S0042698916300037)
41. Bowers, Gautier, Lin & Roorda 2021, AOSLO fixation — [PMC8556553](https://pmc.ncbi.nlm.nih.gov/articles/PMC8556553/)
42. Ryle, Vohnsen & Sheridan 2015, OMT sensor — [doi](https://doi.org/10.1117/1.jbo.20.2.027004)
43. Morales et al., MAIA BCEA reference — [TVST](https://tvst.arvojournals.org/article.aspx?articleid=2585942), [IOVS](https://iovs.arvojournals.org/article.aspx?articleid=2559026)
44. Chung, Aga-Oglu & Krishnan 2018, unifying fixation metrics — [JOV](https://doi.org/10.1167/18.10.1000)
45. Schönbach et al., ProgStar fixation metrics — [PMC6733530](https://pmc.ncbi.nlm.nih.gov/articles/PMC6733530/)
46. Fixation-stability methods comparison (phoria) — [PMC9112722](https://pmc.ncbi.nlm.nih.gov/articles/PMC9112722/)
47. Rashbass 1961, saccadic vs smooth tracking — [PDF](http://wexler.free.fr/library/files/rashbass%20(1961)%20the%20relationship%20between%20saccadic%20and%20smooth%20tracking%20eye%20movements.pdf), [doi](https://doi.org/10.1113/jphysiol.1961.sp006811)
48. de Brouwer, Missal, Barnes & Lefèvre 2002a, catch-up saccade analysis — [doi](https://doi.org/10.1152/jn.00621.2001)
49. de Brouwer et al. 2002b, what triggers catch-up saccades — [J Neurophysiol](https://journals.physiology.org/doi/full/10.1152/jn.00432.2001)
50. *Predicted position error triggers catch-up saccades* — [eNeuro/PMC6964921](https://pmc.ncbi.nlm.nih.gov/articles/PMC6964921/)
51. Blohm, Missal & Lefèvre 2005, position input to pursuit — [PDF](http://www.compneurosci.com/doc/blohm05b.pdf)
52. Lisberger & Westbrook 1985 — [doi](https://doi.org/10.1523/JNEUROSCI.05-06-01662.1985)
53. Tychsen & Lisberger 1986, human pursuit initiation — [doi](https://doi.org/10.1152/jn.1986.56.4.953)
54. Lisberger & Pavelko 1989 — [doi](https://doi.org/10.1152/jn.1989.61.1.173)
55. *Eye position error over open-loop pursuit* — [J Neurosci 39(14):2709](https://www.jneurosci.org/content/39/14/2709)
56. Hanes & Paré 2003, SC countermanding — [J Neurosci](https://www.jneurosci.org/content/23/16/6480)
57. Munoz-class, SC gap paradigm — [PubMed 9334428](https://pubmed.ncbi.nlm.nih.gov/9334428/)
58. Rostral SC pursuit/saccade initiation — [J Neurosci 23(10):4333](https://www.jneurosci.org/content/23/10/4333)
59. Marino & Mazer 2016, perisaccadic updating review — [PMC4743436](https://pmc.ncbi.nlm.nih.gov/articles/PMC4743436/)
60. Golomb & Mazer 2021, visual remapping — [Annu Rev Vis Sci](https://www.annualreviews.org/content/journals/10.1146/annurev-vision-032321-100012)
61. Wang, Zhang, Yang, Jin, Goldberg & Zhang 2023, convergent+forward remapping — [PMC10542176](https://pmc.ncbi.nlm.nih.gov/articles/PMC10542176/)
62. Szinte et al. 2018, pre-saccadic remapping & attention — [PMC6328271](https://pmc.ncbi.nlm.nih.gov/articles/PMC6328271/)
63. Mirpour & Bisley 2015, LIP remapping timing — [doi](https://doi.org/10.1093/cercor/bhv153)
64. Diaz Artiles-class, velocity storage & vestibular damage — [PMC9437187](https://pmc.ncbi.nlm.nih.gov/articles/PMC9437187/)
65. Karmali 2021, VSM accuracy/precision — [PMC9103412](https://pmc.ncbi.nlm.nih.gov/articles/PMC9103412/)
66. Choi 2026, VSM model optimization with human data — [e-rvs](https://e-rvs.org/journal/view.php?number=1016)
67. Bertolini-class, vestibulo-cerebellum & perception — [PMC3376140](https://pmc.ncbi.nlm.nih.gov/articles/PMC3376140/)
68. Choi et al. 2022, false inertial cue & posture — [PMC10008054](https://pmc.ncbi.nlm.nih.gov/articles/PMC10008054/)
69. *Labyrinthine function loss, motion sickness immunity, VSM* — [PMC11239394](https://pmc.ncbi.nlm.nih.gov/articles/PMC11239394/)
70. Nooij et al. 2018, vection/VSM/VIMS — [doi](https://link.springer.com/article/10.1007/s00221-018-5340-1)
71. Kennedy et al. 1996, circular vection scaling — [doi](https://doi.org/10.3233/ves-1996-6502)
72. Vection time-course classic — [doi](https://link.springer.com/article/10.3758/BF03199537)
73. CV latency model test — [springermedizin.de](https://www.springermedizin.de/optokinetic-circular-vection-a-test-of-visual-vestibular-conflic/25621160)
74. Manukyan et al. 2021, OKN slow phases as vection marker — [doi](https://doi.org/10.1016/j.procs.2021.09.054)
75. Mizukoshi, Fabian & Stahle 1977, ACV-OKN test — [doi](https://doi.org/10.3109/00016487709123954)
76. *Gain of slow-phase velocity of OKN* — [PubMed 3827762](https://pubmed.ncbi.nlm.nih.gov/3827762/)
77. OKN stochastic analysis — [JOV](https://jov.arvojournals.org/article.aspx?articleid=2192086)
78. Kaminiarz, Königs & Bremmer 2010, OKN main sequence — [doi](https://doi.org/10.1167/9.8.405), [PDF](https://pdfs.semanticscholar.org/17ff/b83fae45e16a63e74cc1b65edc0b61a96b51.pdf)
79. Look/stare OKN prospective study — [PubMed 15785979](https://pubmed.ncbi.nlm.nih.gov/15785979/)
80. Horizontal/vertical look & stare OKN — [IOVS](https://iovs.arvojournals.org/article.aspx?articleid=2164286)
81. OKN stimulus parameters — [MDPI](https://www.mdpi.com/2076-3417/12/23/11991)
82. Volkmann, Riggs & Moore 1980, *Eyeblinks and visual suppression* — [Science](https://doi.org/10.1126/science.7355270)
83. Volkmann, Riggs, Ellicott & Moore 1982 — [doi](https://doi.org/10.1016/0042-6989(82)90035-9), [PDF](http://wexler.free.fr/library/files/volkmann%20(1982)%20measurements%20of%20visual%20suppression%20during%20opening,%20closing%20and%20blinking%20of%20the%20eyes.pdf)
84. Riggs, Volkmann & Moore 1981, blackout suppression — [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/0042698981900122)
85. Manning, Riggs & Komenda 1983, reflex eyeblinks — [doi](https://doi.org/10.3758/bf03202953)
86. *Perceptual consequences and neurophysiology of eye blinks* — [Frontiers 2023](https://www.frontiersin.org/journals/systems-neuroscience/articles/10.3389/fnsys.2023.1242654/full)
87. Irwin 2016, continuity across blinks — [PDF](http://wexler.free.fr/library/files/irwin%20(2016)%20perceiving%20a%20continuous%20world%20across%20voluntary%20eye%20blinks.pdf)
88. Collewijn, van der Steen & Steinman 1985, blink eye movements — [doi](https://doi.org/10.1152/jn.1985.54.1.11)
89. Bour, Aramideh & Ongerboer de Visser 2000 — [doi](https://doi.org/10.1152/jn.2000.83.1.166)
90. Riggs-class 1987, blink-related eye movements — [PDF](http://wexler.free.fr/library/files/riggs%20(1987)%20blink-related%20eye%20movements.pdf)
91. Kirchner & Lappe 2022, MRI of blinks — [PDF](https://www.uni-muenster.de/imperia/md/content/psyifp/ae_lappe/2022.kirchner__et_al.pdf)
92. Doane 1980, eyelid/tear dynamics — [PubMed 7369314](https://pubmed.ncbi.nlm.nih.gov/7369314/)
93. Talens-Estarelles et al. 2021, blink kinematics on displays — [Springer](https://link.springer.com/article/10.1007/s00417-021-05490-9), [PubMed](https://pubmed.ncbi.nlm.nih.gov/34779906/)
94. Doughty 2001, SEBR three conditions — [ResearchGate](https://www.researchgate.net/publication/11653609_Consideration_of_Three_Types_of_Spontaneous_Eyeblink_Activity_in_Normal_Humans_during_Reading_and_Video_Display_Terminal_Use_in_Primary_Gaze_and_while_in_Conversation)
95. Blink timing in reading — [Sci Rep 2025](https://www.nature.com/articles/s41598-025-04839-y)
96. Blink monitoring system — [PMC11744486](https://pmc.ncbi.nlm.nih.gov/articles/PMC11744486/)
97. Blink detection methods (EO vs PS) — [SAGE](https://journals.sagepub.com/doi/10.1177/1071181312561143)
98. Binocular coordination during reading — [JOV](https://jov.arvojournals.org/article.aspx?articleid=2122351)

(All 98 entries above returned live content this session; entries pairing a doi with a PMC/PDF mirror are dual-access copies of one publication — roughly 80 distinct publications total, still well above the 35-source requirement.)

---

## Appendix A — Agent-generated questions (40)

Parent-bank dispositions first (my subset, Q21–Q34): Q21 main sequence → §3.2.1; Q22 no mid-flight correction → §3.2.1/3.4; Q23 suppression → §3.2.2; Q24 pre-saccadic attention → §3.6; Q25 microsaccades/Troxler → §3.3; Q26 perceptual events during fixation → §3.3; Q27 Troxler mechanism → perception companion doc (motor side: §3.3); Q28 glance timeline → §3.6; Q28b chronostasis → §3.2.3; Q29 OKN → §3.5.2; Q30 vestibular nystagmus → §3.5.4; Q31 perisaccadic stimulus lifespan → §3.2.2 (large luminance transients survive; high-SF does not); Q32 fixational statistics in tasks → §3.3/§3.8 + movement doc §2.5; Q33 pursuit initiation → §3.4; Q34 catch-up trigger → §3.4.

1. What is the vertical/horizontal peak-velocity ratio at matched saccade amplitude? — Answered, §3.2.1 (vertical ~10–20% slower; downward slowest; exact ratio study-dependent).
2. Is the motoneuron burst as long as the saccade? — Answered, §3.1.5 (≈ half, for <15°; approaches full duration for large saccades).
3. Why do large saccades have more skewed velocity profiles? — Answered, §3.2.1 (skewness 0.45→0.20; pulse ends near mid-movement; apparent inertia coasts).
4. What enforces the velocity saturation — muscles or motoneurons? — Answered, §3.2.1 (all motoneurons near max rate beyond 15–20°, only pulse width grows; force-velocity limits).
5. Is saccadic suppression retinal or extraretinal? — Answered with the conflict reported, §3.2.2 (Diamond/MSTd vs Idrees retinal evidence; unresolved).
6. Why is color vision unimpaired during saccades? — Answered, §3.2.2 (magno-selective suppression; no chromatic threshold elevation).
7. How early does neural suppression begin in cortex? — Answered, §3.2.2 (MSTd up to 90 ms pre-saccade).
8. How big is postsaccadic enhancement? — Answered, §3.2.2/§3.6 (first +50 ms, peak 100–130 ms, mean 310%, ends 200–450 ms).
9. Does chronostasis scale one-to-one with saccade duration? — Answered, §3.2.3 (67 vs 69 ms — near-exact scaling in Yarrow exp. 1).
10. Does chronostasis occur for reflexive/express saccades? — Answered, §3.2.3 (yes, all categories → subcortical SC trigger).
11. Can a purely intrasaccadic flash be seen? — Answered, §3.2.2 (only large, low-SF luminance transients survive; high-SF content invisible at saccadic speeds).
12. What is the brainstem integrator's time constant without the cerebellum? — Answered, §3.1.5 (~5 s inherent; ~25 s tuned; model spread 25–50 s).
13. Why does gaze-evoked nystagmus obey Alexander's law? — Answered, §3.1.5 (drift velocity ∝ eccentricity from null; leaky-integrator geometry).
14. Do the eyes obey Listing's law during the VOR? — Answered, §3.1.2 (no — violated during VOR/OKN; quarter-angle compromise).
15. Is Listing's law neural or mechanical? — Answered, §3.1.3 (pulley mechanics per Demer; debate noted).
16. What is the measured tilt-angle coefficient? — Answered, §3.1.2 (0.57–0.58 horizontal, 0.34–0.35 vertical — imperfect obedience).
17. What do orbital-layer fibers insert on? — Answered, §3.1.3 (pulleys, not sclera; OL = 40–60% of fibers).
18. What fraction of EOM fibers are tonic (MIF)? — Answered, §3.1.4 (GL ≈ 13–21%, OL ≈ 17%).
19. How fast is drift in natural viewing? — Answered, §3.3 (classical ~4′/s vs modern ~50′/s instantaneous; bandwidth-dependent up to 1.5°/s median).
20. What is tremor's frequency and amplitude? — Answered, §3.3 (mean 84 Hz, range 70–103 Hz; ~6–30″).
21. Do microsaccades have their own main sequence? — Answered, §3.3 (shared with saccades — common generator).
22. Can microsaccades be voluntarily suppressed? — Answered, §3.3 (2→0.5 Hz with minimal instruction).
23. Do microsaccades reverse fading? — Answered with debate, §3.3 (restore yes; drift prevents onset; attention account also supported).
24. What are normal BCEA values? — Answered, §3.3 (0.8 deg² @63%, 2.4 @95%, MAIA normals).
25. Does fixation differ between passive and active tasks? — Answered, §3.3 (microsaccade inhibition, longer drifts, 57% larger covered area in discrimination).
26. What did the Rashbass step-ramp prove? — Answered, §3.4 (pursuit follows target velocity, not position; saccade the converse).
27. What is the catch-up "smooth zone"? — Answered, §3.4 (T_XE ∈ 40–180 ms).
28. How is catch-up amplitude programmed? — Answered, §3.4 (PE + RS both; RS contribution saturates above 15°/s).
29. Is there really no position input to pursuit? — Answered, §3.4 (there is, under target selection: Blohm 2005, 85 ms latency).
30. How long is the human open-loop pursuit interval? — Answered, §3.4 (~130 ms human, ~100 ms monkey).
31. Why does the VOR need OKN? — Answered, §3.5.1–3.5.2 (canals are high-pass, decay ~5 s; OKN covers sustained motion; VSM bridges).
32. Where does velocity storage live? — Answered, §3.5.1 (vestibular nuclei commissural loop; nodulus/uvula inhibit).
33. Why does PRN outlast the cupula signal? — Answered, §3.5.4 (VSM integration: 5 s → 11–20 s+).
34. Does self-motion perception share the VSM with the VOR? — Answered, §3.5.4 (yes; TCs correlate 0.93–0.95).
35. Look vs stare OKN — what differs? — Answered, §3.5.2 (gain, slow-phase size/frequency, quick-phase main sequence).
36. Where does OKN slow-phase gain fall off? — Answered, §3.5.2 (~60°/s "fatigue threshold"; gain <1 even at low speeds).
37. What triggers vection and how fast does it build? — Answered, §3.5.3 (large-field motion; latency of seconds, building ~30 s; speed- and weight-dependent).
38. Is motion sickness purely sensory conflict? — Answered with both accounts, §3.5.4 (conflict standard; VSM-based alternative with evidence).
39. Does the eye rotate up during normal blinks? — Answered (anchor corrected), §3.7.5 (down + nasal 1–5°; Bell's upward rotation only with forced/impeded closure).
40. Is the blink blackout filled in or ignored? — Answered, §3.7.4 (ignored: 117 ms excluded ≈ 114 ms occlusion; antedating/maintenance rejected).

**Dropped/unverified this session:** absolute °/s² values for human open-loop pursuit acceleration (structure verified, §3.4); chronostasis effects approaching 500 ms (largest verified ≈ 190 ms, §3.2.3); a specific inter-study consensus on OKN saturation velocity beyond the ~60°/s classical threshold (§3.5.2 reports the spread).
