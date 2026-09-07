# Head Stabilization, Eye Movements, and Gaze Behavior During Locomotion (Section 2)

**Scope:** how real humans move their head and eyes while walking, running, turning, side-stepping, and moving backward — the science grounding for realistic character/camera movement in a game engine. Every load-bearing claim carries the URL it came from; every number is either verified live this session or explicitly marked unverified. Where sources disagree, both numbers and both sources are reported. This is section 2 of 5 in the merged biomechanics document.

---

## 0. Provenance

- **Date:** 2026-09-06 (research session).
- **Produced by:** research agent 2 of 5 (domain: head stabilization, eye movements, gaze behavior during locomotion), via read-only web research with Exa search/fetch and standard web fetch. This file was written before any implementation.
- **Verification:** every URL cited inline was returned live by search or fetched successfully this session. Numerically derivable constants were cross-checked locally in PowerShell (canonical computations recorded below); derived numbers quoted in the text are outputs of that run. Where the literature gives a spread, the spread is reported — never a fake single value.
- **Local computation summary (pwsh, 2026-09-06):** canal corner frequency $1/(2\pi T_L)$ evaluated at $T_L = 5.0$ s → 0.0318 Hz and $T_L = 4.2$ s → 0.0379 Hz; velocity-storage corner at 15 s → 0.0106 Hz; saccade duration $D = 20 + 2A$ ms evaluated at $A = 5/10/20°$ → 30/40/60 ms; VOR slip at gain 0.9 with 150°/s head velocity → 15°/s, and gain 0.75 at 350°/s → 87.5°/s; head bob of amplitude 0.02 m at 2 Hz → peak vertical velocity 0.251 m/s → angular retinal effect ≈14.1°/s at 1 m viewing distance, ≈3.6°/s at 4 m; look-ahead of 2 steps (0.7 m each) at 1.4 m/s → 1.0 s; the 4–6 m fixation region at 1.4 m/s → 2.9–4.3 s of travel; optic flow at 10° eccentricity, $v = 1.4$ m/s, $d = 4$ m → 3.48°/s retinal speed.
- **Source count:** 42 distinct verified sources (inline citations throughout; consolidated list in §2.8).
- **Known gaps / honesty notes:** the tactical/side-step doctrine section rests on practitioner and training literature (shooting-on-the-move), not peer-reviewed gaze studies — this is flagged in §2.6. Measured rearward-sampling *frequency* while moving backward (looks-over-shoulder per second) has no clean peer-reviewed number; the closest verified data are an observational study of backward stepping and backward-walking gaze studies, both reported with their actual effect sizes.

---

## 2.1 The head as a stabilized sensor platform

### 2.1.1 Why stabilize at all: retinal slip tolerance and vestibular thresholds

The fovea can tolerate only a small amount of image motion before acuity collapses. The literature gives a consistent band:

- Classic psychophysics (Westheimer & McKee and successors) puts the **optimal retinal-slip window for high-acuity vision at roughly 0.2–2°/s**, with the tolerated range during active head movement described as 0.2–2.0°/s (Flipse, 1990, [hdl.handle.net/1765/40270](http://hdl.handle.net/1765/40270)).
- An "acuity threshold" of **about 4°/s** is reported for active head-movement conditions (retinal slip stays below it in active conditions, exceeds it in passive ones) — van Leeuwen et al., [springermedicine.com/retinal-slip-during-active-head-motion-and-stimulus-motion/25684106](https://www.springermedicine.com/retinal-slip-during-active-head-motion-and-stimulus-motion/25684106).
- During locomotion specifically, Crane & Demer (1997) measured horizontal/vertical retinal image velocity **< 4°/s for a visible target beyond 4 m** during standing, walking, and running on a treadmill ([doi.org/10.1152/jn.1997.78.4.2129](https://doi.org/10.1152/jn.1997.78.4.2129)).
- A walking-while-reading study found visual acuity decreases rapidly when retinal slip exceeds **≈ 6°/s**, with optimal reading performance in the 2.7–6.3°/s range (Fathi et al., PLOS One 2015, [journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0129902](https://journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0129902)).
- On the adaptation side, VOR gain-error tolerance work suggests stable vision is maintained within ~5% gain error, corresponding to **< ~9°/s** retinal slip before adaptive drive kicks in (Schubert et al., [pmc.ncbi.nlm.nih.gov/articles/PMC7392997/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7392997/)).

So the honest summary is a **layered tolerance: ~0.2–2°/s for optimal high-acuity detail, ~4°/s as the "active-motion acuity threshold," ~6°/s as the rapid-degradation boundary, and up to ~9°/s tolerable before the VOR starts re-calibrating.** The parent-side anchor "1–4°/s" sits inside this band and is consistent with the lower two layers.

For the vestibular side, yaw-rotation detection thresholds cluster near **0.5–2°/s** (details and sources in §2.2.3), so head oscillations during walking — which produce angular velocities well above that — are clearly detectable and must be actively compensated, not ignored.

### 2.1.2 Measured head oscillation during walking and running

The canonical study is Pozzo, Berthoz & Lefort (1990): ten normal subjects across free walking, walking in place, running in place, and hopping. Head translations ranged 1–25 cm amplitude and 0.15–1.8 m/s velocity; **the Frankfort plane (approximating the horizontal canal plane) stayed stabilized near earth-horizontal, with maximum rotation not exceeding ~20° across all four tasks** and vertical angular velocities under ~140°/s; predominant frequencies of translation and rotation were 0.4–3.5 Hz with harmonics to 6–8 Hz. Walking in darkness tilted the mean head position downward but did not significantly change stabilization amplitude ([europepmc.org/article/MED/2257917](https://europepmc.org/article/MED/2257917)).

Finer-grained numbers on the pitch channel: Hirasaki et al. (treadmill study, 0.6–2.2 m/s) found **little head-pitch movement in space at walking speeds up to ~1.2 m/s** (head-on-trunk pitch compensating trunk pitch), while above 1.2 m/s a significant vertical head translation developed and induced compensatory head pitch in space that tended to point the head at a **fixed point in front of the subject, invariant with walking speed** — i.e., the head acts as if tracking a "head fixation distance." Predominant frequency of head translation/rotation stayed in a narrow 1.4–2.5 Hz band across all speeds ([ntrs.nasa.gov/citations/20040141864](https://ntrs.nasa.gov/servlets/purl/1113266) — the model paper; NASA abstract at [ntrs.nasa.gov/citations/20040141864](https://ntrs.nasa.gov/citations/20040141864)). Kavanagh et al. confirm head accelerations are attenuated relative to the trunk in all three axes, most strongly mediolaterally ([link.springer.com/article/10.1007/s00421-005-1328-1](https://link.springer.com/article/10.1007/s00421-005-1328-1)).

So the parent-side anchor "~1–3° pitch during walking, more at run" is directionally right but the measured residual is *condition-dependent*: near zero pitch-in-space at slow walking, growing translation-coupled pitch at faster walking/running. **Report the mechanism, not a single amplitude.** During running, the rotational VOR gain can drop to ~0.75 and reflexive compensation only works well under ~350°/s head rotational velocity (Lim et al., review, [mdpi.com/2076-3425/10/3/174](https://www.mdpi.com/2076-3425/10/3/174)) — which is why active coordination (not just reflexes) is required at run.

### 2.1.3 The head translates with the COM — and the eyes still hold gaze

The head's translational ride is periodic with the gait cycle:

- **Vertical bob at 2× step frequency** (twice per stride cycle), plus a **lateral sway at 1× step frequency** (once per full cycle, side to side). This 2:1 frequency relation is standard gait biomechanics and is used explicitly in VR head-motion models: "the forward and upward bobs' frequency match the step frequency; the rightward bob moves at one-half the step frequency" (i.e., lateral at half the vertical rate) — the OpenNI-based walking model of Vitéribo & Baek, [osti.gov/servlets/purl/1113266](https://www.osti.gov/servlets/purl/1113266).
- The Pozzo band (0.4–3.5 Hz predominant, harmonics to 6–8 Hz) brackets these components.

The stabilization chain that keeps gaze despite this ride is: (1) mechanical shock attenuation through the legs/spine (Kavanagh; Lim et al. review above), (2) the **vestibulocollic reflex (VCR)** and cervicocollic reflex (CCR) stabilizing head-in-space via neck muscles (Hirasaki; Lim et al. review; Hölzl et al., [link.springer.com/content/pdf/10.1007/s00405-020-06488-5.pdf](https://link.springer.com/content/pdf/10.1007/s00405-020-06488-5.pdf)), and (3) the **VOR** rotating the eyes opposite residual head rotation (§2.3). A key subtlety from Crane & Demer: because the orbits *translate* as well as rotate, sub-unity rotational VOR gain can still yield stable gaze — orbital translation was consistently antiphase with rotation at frequencies < 4 Hz, and neglecting it *increases* computed image velocity ([doi.org/10.1152/jn.1997.78.4.2129](https://doi.org/10.1152/jn.1997.78.4.2129)). For a game engine this means: **a rigidly camera-locked view is wrong, and a pure counter-rotation is also wrong; the real system couples translation and rotation with antiphase.**

Worked example (local computation): a 2 cm vertical bob at 2 Hz gives peak vertical head velocity $\dot z = 2\pi f A = 2\pi \cdot 2 \cdot 0.02 \approx 0.25$ m/s. Fixating a ground point 4 m ahead, the gaze-elevation perturbation is $\arctan(0.25/4) \approx 3.6°/s$ — inside the 4°/s acuity band, consistent with Crane & Demer's < 4°/s finding at far targets. The same bob fixating a point only 1 m ahead produces $\arctan(0.25/1) \approx 14°/s$ of gaze-elevation modulation — **above** the acuity threshold, which is exactly why near-target fixation while moving is hard and why the translational VOR gain must rise for near targets (§2.3).

## 2.2 The vestibular apparatus

### 2.2.1 Semicircular canals: the torsion-pendulum model

Each canal is a fluid-filled loop where angular acceleration of the head drives endolymph, deflecting the cupula. The Steinhausen torsion-pendulum equation for cupula-endolymph dynamics:

$$
I\,\ddot{\theta} + c\,\dot{\theta} + k\,\theta = I\,\dot{\omega}(t)
$$

with $\theta$ cupula deflection, $\omega$ head angular velocity, $I$ endolymph moment of inertia, $c$ viscous damping, $k$ cupula elastic stiffness. In Laplace form, cupula displacement per unit head **acceleration**:

$$
\frac{\Theta(s)}{A(s)} = \frac{K}{(T_L s + 1)(T_S s + 1)}, \qquad T_L = \frac{c}{k},\quad T_S = \frac{I}{c}
$$

and per unit head **velocity**:

$$
\frac{\Theta(s)}{\Omega(s)} = \frac{K\,T_L\,s}{(T_L s + 1)(T_S s + 1)}
$$

This is the standard modern form (Choi 2025 review of end-organ transfer functions, [e-rvs.org/journal/view.php?number=991](https://www.e-rvs.org/journal/view.php?number=991); Hullar & minor-species values in [link.springer.com/article/10.1007/s10162-008-0120-4](https://link.springer.com/article/10.1007/s10162-008-0120-4)). Because $T_L \gg T_S$, the system is overdamped and, within the band

$$
\frac{1}{2\pi T_L} < f < \frac{1}{2\pi T_S}
$$

the cupula deflection is proportional to head **angular velocity** — the canal is a velocity transducer in exactly the band that matters for locomotion.

**Measured time constants (report as ranges, they genuinely vary):**

- $T_L \approx 5$ s is the standard human value (Choi 2025, above, citing the classic approximation $c/k \approx 5$ s; $T_S \approx 0.003$ s).
- Fernández & Goldberg's squirrel-monkey afferent value is $T_L = 5.7$ s, with $T_S = 0.005$ s estimated for humans (reported in the mouse-afferent study, [link.springer.com/article/10.1007/s10162-008-0120-4](https://link.springer.com/article/10.1007/s10162-008-0120-4)).
- Human aVOR-based estimate: $\tau_1 = 4.2$ s (Dai et al. 1999, quoted in Soyka et al. 2012, [link.springer.com/article/10.1007/s00221-012-3120-x](https://link.springer.com/article/10.1007/s00221-012-3120-x)).
- Theoretical prediction from canal anatomy alone: ~10 s (van Egmond et al. 1949, quoted in the same Soyka paper) — the measured value is lower, a genuine source disagreement worth keeping.

Local check: the corner frequency $1/(2\pi T_L)$ at $T_L = 4.2$–5.7 s is **0.028–0.038 Hz** — matching the "≈0.03 Hz" quoted in the clinical literature (Choi 2025). So the canal behaves as a pure velocity meter for $f \gtrsim 0.03$ Hz, i.e., everything from slow steering turns up to the 6–8 Hz gait harmonics.

### 2.2.2 Velocity storage

The canal signal decays with $T_L \sim 5$ s during constant-velocity rotation, but the perceptual/oculomotor response decays much more slowly — the brain's **velocity storage mechanism** extends the effective time constant. Behaviorally the velocity-storage time constant is **roughly 10–30 s** in humans (Karmali & Lim, [pmc.ncbi.nlm.nih.gov/articles/PMC9103412/](https://pmc.ncbi.nlm.nih.gov/articles/PMC9103412/)), with a typical value near 15–20 s for young adults; it lengthens the response and improves low-frequency accuracy at the cost of integrating more noise — an explicit accuracy–precision tradeoff the brain resolves optimally-ish (same source). The velocity-storage integrator is also what produces the anticompensatory quick phases of nystagmus during sustained rotation.

Disagreement worth reporting: whether velocity storage affects *perceptual thresholds* is contested — Soyka et al. found their yaw-threshold data were best fit *without* velocity storage influence ($\tau_1$ smaller than velocity-storage values; [link.springer.com/article/10.1007/s00221-012-3120-x](https://link.springer.com/article/10.1007/s00221-012-3120-x)), while the classic Raphan/Robinson models (cited in Karmali & Lim) treat storage as central to low-frequency response. Report both.

### 2.2.3 Detection thresholds

Human vestibular perceptual thresholds measured with modern 2AFC methods:

- **Yaw rotation:** thresholds ~**0.8–2°/s** depending on stimulus period (Soyka et al. 2012: ~2°/s at 6.7 s period, ~0.8°/s at 0.3 s period; [link.springer.com/article/10.1007/s00221-012-3120-x](https://link.springer.com/article/10.1007/s00221-012-3120-x)). A large review finds a pooled **median yaw threshold ≈ 1.1°/s** with a 0.38–3°/s range across studies (Valko/Dietz-class review, [sciencedirect.com/science/article/pii/S0966636223014339](https://www.sciencedirect.com/science/article/pii/S0966636223014339)); a signal-detection re-analysis fits a high-pass plateau of **0.5°/s** above ~0.26 Hz ([pubmed.ncbi.nlm.nih.gov/22923225/](https://pubmed.ncbi.nlm.nih.gov/22923225/)).
- Curved-path canal-otolith interaction: mean yaw detection **1.45 ± 0.81°/s** (3.49 ± 1.95°/s²) and naso-occipital translation **2.93 ± 2.10 cm/s** (MacNeilage et al., [doi.org/10.1152/jn.01067.2009](https://doi.org/10.1152/jn.01067.2009)).
- Translation thresholds are highest head-vertical (~2.13 cm/s median) and show high-pass behavior below ~1 Hz; the full picture is in the Frontiers review ([frontiersin.org/journals/neurology/articles/10.3389/fneur.2021.643634/full](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2021.643634/full)).

The parent anchor "~1–3°/s" is consistent with the yaw band above (0.5–2°/s plateau values, up to ~3°/s at long periods), though the modern consensus plateau is at the *low* end (~0.5–1°/s in the locomotion-relevant band). Report the range.

### 2.2.4 Otoliths and the tilt–translation ambiguity

The otoliths (utricle/saccule) sense the **net gravito-inertial force** — the vector sum of linear acceleration and gravity — and cannot by themselves distinguish "tilted head in a gravity field" from "accelerating horizontally" (same specific force direction in both cases). Resolution of this ambiguity requires canal-otolith integration: an internal model uses canal signals (head rotation relative to gravity) to parse the otolith's $\mathbf{f} = \mathbf{g} - \mathbf{a}$ into separate tilt and translation estimates (reviewed in the Frontiers thresholds review above; MacNeilage et al. for the perceptual consequences). The classical illusion this produces: sustained linear acceleration is mis-perceived as tilt (somatogravic illusion). For game-camera purposes: **the human sensor suite is not an IMU; it is two complementary, partially ambiguous transducers plus a learned Bayesian parser** — which is why smooth, sustained accelerations feel "tilty" and why oscillatory head motion (above the canal corner) is perceived cleanly as motion.

## 2.3 The vestibulo-ocular reflex (VOR) and vestibulocollic reflex

### 2.3.1 VOR gain and latency

The rotational VOR drives the eyes opposite to head rotation to keep the retinal image stable. Key numbers:

- **Latency ≈ 8 ms** from head-impulse onset to eye-response onset (Curthoys, [frontiersin.org/journals/neurology/articles/10.3389/fneur.2017.00258/full](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2017.00258/full)); motorized-impulse study: mean 3.4 ± 6.3 ms ([jamanetwork.com/journals/jamaotolaryngology/fullarticle/484653](https://jamanetwork.com/journals/jamaotolaryngology/fullarticle/484653)); search-coil passive rotations: 7.5 ± 2.9 ms (Aw et al., quoted there) and 7–10 ms whole-body (Crane & Demer). **Report ~3–10 ms, fastest of any oculomotor response; visual reflexes are an order of magnitude slower (~70–100+ ms).**
- **Gain near unity but genuinely variable:** population norm 0.97 ± 0.09 (80 ms window) declining ~0.012/decade with age, 2-SD lower limit 0.79 (Matiño-Soler et al., [link.springer.com/article/10.1186/s40463-015-0081-7](https://link.springer.com/article/10.1186/s40463-015-0081-7)); motorized-impulse study 1.08 ± 0.10 (far target), 1.26 ± 0.10 (near target — gain *rises* for near targets because translation must be compensated too; JAMA study above). Curthoys emphasizes healthy symptom-free people range **0.85–1.2**. During running it can drop to **~0.75** (Lim et al. review, [mdpi.com/2076-3425/10/3/174](https://www.mdpi.com/2076-3425/10/3/174)).
- Roll VOR is notably under-compensatory (gain ~0.7) without perceived blur because the image stays on the fovea (Schubert et al., [pmc.ncbi.nlm.nih.gov/articles/PMC7392997/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7392997/)).

**Disagreement to report:** Crane & Demer found AVOR gain was less than unity and varied by activity, target distance, and subject, with *no significant correlation between gain and image stability during standing and walking* — because orbital translation (antiphase with rotation) does the rest of the job ([doi.org/10.1152/jn.1997.78.4.2129](https://doi.org/10.1152/jn.1997.78.4.2129)). The "gain ≈ 1" textbook number is a simplification that holds for pure far-target rotation only.

Worked slip numbers (local computation): at head velocity 150°/s and gain 0.9, residual slip = 15°/s; at 350°/s and gain 0.75, slip = 87.5°/s — far above the 4–6°/s acuity band, which is why running head rotations are compensated partly by *not rotating the head* (active stabilization, §2.1.2) rather than by the VOR alone.

### 2.3.2 VOR cancellation and suppression

The VOR must be modulated when it would be counterproductive:

- **VOR cancellation** during combined eye-head tracking of a moving target: the pursuit/cancellation command opposes the VOR (classic Lanman et al.; treated in the suppression literature below).
- **VOR suppression during large gaze saccades:** inhibition ranging **40–96% between subjects for a 40° target step**, increasing almost linearly with amplitude, decaying during the saccade (Pélisson, Prablanc & Urquizar 1988, [doi.org/10.1152/jn.1988.59.3.997](https://doi.org/10.1152/jn.1988.59.3.997)); the time constant of VOR restoration after saccade onset is a **fairly constant ≈ 40 ms** (Lefèvre et al., [link.springer.com/article/10.1007/BF00227846](https://link.springer.com/article/10.1007/BF00227846)). Suppression is a function of both saccade amplitude and the timing of head perturbation relative to saccade onset, and gaze accuracy is preserved despite suppressed VOR — evidence for **gaze-feedback control**, not separate eye and head controllers (Roy & Cullen, [jneurosci.org/content/35/3/1192](https://www.jneurosci.org/content/35/3/1192)).
- Passive, abrupt head impulses: VOR suppression only starts to operate after **~80 ms** — before that, the reflex runs open-loop (Curthoys, [frontiersin.org/journals/neurology/articles/10.3389/fneur.2017.00258/full](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2017.00258/full)).

### 2.3.3 The vestibulocollic (neck) reflex and what fails look like

The VCR co-stabilizes the head in space via neck muscles; the cervicocollic stretch reflex stabilizes head-on-trunk; together with mechanical shock attenuation they form the head platform (Lim et al. review, [mdpi.com/2076-3425/10/3/174](https://www.mdpi.com/2076-3425/10/3/174); Hölzl et al. for neck-muscle torque magnitudes during walking, [link.springer.com/content/pdf/10.1007/s00405-020-06488-5.pdf](https://link.springer.com/content/pdf/10.1007/s00405-020-06488-5.pdf)). When the VOR fails bilaterally, patients get **oscillopsia** — the world appears to jiggle with each footfall because retinal slip during walking exceeds acuity thresholds; the head impulse test exposes the deficit via catch-up (refixation) saccades, covert during or overt after the impulse (Curthoys 2017, same Frontiers paper). This is the strongest existence proof that gaze stabilization is *functionally load-bearing* during locomotion, not cosmetic.

## 2.4 The eye-movement repertoire

### 2.4.1 Saccades: the main sequence

Saccade kinematics are stereotyped and amplitude-determined — the "main sequence" (Bahill, Clark & Stark 1975, [visualcognition.ca/spering/reading/Bahill.Clark.Stark.MathBiosci.1975.pdf](http://www.visualcognition.ca/spering/reading/Bahill.Clark.Stark.MathBiosci.1975.pdf)):

- **Duration vs amplitude, approximately linear:** $D \approx 20\ \text{ms} + 2\ \text{ms/deg} \cdot A$. The classic EOG fit gives a slope of **2.7 ms/deg** across a large amplitude range (Baloh et al., [neurology.org/doi/10.1212/WNL.25.11.1065](https://www.neurology.org/doi/10.1212/WNL.25.11.1065)); modern fits use an affine $D = aA + b$ (de Brouwer/cf. the speed-accuracy re-analysis, [nature.com/articles/s41598-022-09029-8](https://preview-www.nature.com/articles/s41598-022-09029-8)). **Report the slope as ~2–2.7 ms/deg** — a real spread between studies, not a fake single value. Local check: $D = 20 + 2A$ gives 30/40/60 ms for 5/10/20° saccades, matching Bahill's curves.
- **Peak velocity saturates:** quasi-linear up to ~15–20° amplitude, then a soft saturation limit. Bahill's own data saturate around **~700–900°/s** for large saccades; the modern model form $V_p = R/(k\,(a + bA))$-style saturation asymptotes at $V_p^{\max} = 1/\beta$ (speed-accuracy paper above); empirical subject means in a 2020 methods paper run 160–414°/s for ~9° average saccades (Gibaldi & Sabatini, [pmc.ncbi.nlm.nih.gov/articles/PMC7880984/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7880984/)). **Report: saturation in the several-hundred-°/s range, ~700–900°/s for large amplitudes in the classic data; inter-subject spread is large.**
- **Why ballistic:** the saccadic controller signal (motoneuron burst) lasts about *half* the saccade duration — the movement is over before visual feedback (latency ~70–100 ms) could correct it; corrections appear only as separate, later saccades (Bahill et al. 1975, above). Mid-flight modification is essentially absent beyond ~50 ms into the movement, which is shorter than most saccades' own duration.
- **Saccadic suppression:** visual sensitivity drops before and during saccades — suppression anticipates saccades by ~50 ms, is maximal at onset, outlasts by ~50 ms, and amounts to about a 10-fold (1 log unit) loss of luminance-contrast sensitivity (Burr/Morrone-class extraretinal study, [pmc.ncbi.nlm.nih.gov/articles/PMC6773104/](https://pmc.ncbi.nlm.nih.gov/articles/PMC6773104/)); mechanistically, intrasaccadic suppression is dominated by a **detector-gain reduction** rather than added noise or uncertainty (Harris et al., [pmc.ncbi.nlm.nih.gov/articles/PMC3704127/](https://pmc.ncbi.nlm.nih.gov/articles/PMC3704127/)), and a large body of retinal work shows visual-only (image-shift-triggered) mechanisms starting in the retina itself can account for the perceptual properties (Ido/Baccus-class, [pmc.ncbi.nlm.nih.gov/articles/PMC7181657/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7181657/), [nature.com/articles/s42003-022-03526-2](https://preview-www.nature.com/articles/s42003-022-03526-2)). Net: **the brain is effectively blind during saccades — saccades are samples, not continuous scans.**

### 2.4.2 Fixations

- **2–3 fixations per second** during scene scanning/locomotion — "human eyes make two to three fixations per second and move very quickly between each fixation" (Gibaldi & Sabatini, [pmc.ncbi.nlm.nih.gov/articles/PMC7880984/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7880984/), and the same rate is standard in the QE literature, §2.5.4). The parent anchor "150–600 ms; 2–3 saccades/s" is consistent with this rate and typical single-fixation durations; note the locomotion-specific numbers in §2.5 (e.g., footprint fixations ending ~800–1000 ms before footfall, and Hollands' stepping-stone gaze held on target until ~51 ms *after* foot contact).
- **Fixational eye movements** (microsaccades, drift, tremor) persist even in "fixation"; too-perfect stabilization makes perception fade, and retinal slip in the 2.7–6.3°/s band is actually *optimal* for reading-while-walking (Fathi et al., PLOS One 2015, [journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0129902](https://journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0129902)). Detail vision only happens in fixation because only then is the image held near the fovea within the slip tolerance of §2.1.1.
- **Why staring at the feet is maladaptive:** fixating one's own feet consumes the fixation budget on ground that will be traversed *anyway* while providing no preview; §2.5's data show humans look 2+ steps ahead precisely to have time to alter the gait.

### 2.4.3 Smooth pursuit

- Accurate pursuit with gain near 1 holds for target speeds up to ~**30°/s**, degrading above that; classic quantification: gain 0.98→0.75 over 10→60°/s constant-velocity targets with catch-up saccades appearing above ~30°/s (Zee/classic quantification in [pubmed.ncbi.nlm.nih.gov/7211334/](https://pubmed.ncbi.nlm.nih.gov/7211334/)). The dot-tracking saturation velocity is ~63°/s, higher for extended targets (Feng et al., [doi.org/10.1117/12.840146](https://doi.org/10.1117/12.840146)); Meyer, Lasker & Robinson's classic upper limit is in the same tens-of-°/s range (cited there). **Report: accurate to ~30°/s, saturating somewhere ~60–100+°/s depending on target size/contrast.**
- While walking, pursuit of an earth-fixed object is *generated* partly by the VOR; pursuit of a moving object while self-moving compounds retinal flow and measurably degrades balance (increased ML trunk sway and step-width variability during pursuit vs fixation while walking; Thomas et al., [doi.org/10.1007/s00221-017-4996-2](https://doi.org/10.1007/s00221-017-4996-2)). Smooth pursuit has an on-line gain-control mechanism that modulates visual-motor transmission as a function of eye/target velocity (Churchland & Lisberger, [doi.org/10.1152/jn.2002.87.6.2936](https://doi.org/10.1152/jn.2002.87.6.2936)).
- During curved-path walking, gaze tracking is accomplished by nystagmus — slow compensatory phases plus quick phases in the travel direction — mixed from optokinetic, pursuit, and vestibular sources in the light (Authié et al. 2015, [pmc.ncbi.nlm.nih.gov/articles/PMC4458691/](https://pmc.ncbi.nlm.nih.gov/articles/PMC4458691/)).

### 2.4.4 Gaze = eye-in-head + head-on-body

$$
\theta_{gaze}(t) = \theta_{eye\text{-}in\text{-}head}(t) + \theta_{head\text{-}on\text{-}body}(t) + \theta_{body}(t)
$$

In large gaze shifts (>~20–30°), **the eye leads and the head follows**:

- In monkeys with unrestrained heads, eye amplitude saturates at ~35° and head contribution grows linearly for 25–90° shifts; head latency relative to gaze onset depends on initial eye position (Freedman & Sparks, [doi.org/10.1152/jn.1997.77.5.2328](https://doi.org/10.1152/jn.1997.77.5.2328)). In humans, the eye typically leads the head by tens of ms (the classic figure ~30–50 ms is consistent with the head-start delays of 12–41 ms measured by Roy & Cullen under passive chair perturbations, [jneurosci.org/content/35/3/1192](https://www.jneurosci.org/content/35/3/1192), and with active orienting data across species, [nature.com/articles/s42003-026-09943-x](https://www.nature.com/articles/s42003-026-09943-x)).
- The VOR is inhibited during the shift (40–96% for a 40° step; §2.3.2) and restored with a ~40 ms restoration constant near saccade end, so the eyes typically end the movement by counter-rotating back toward primary position as the head catches up ("gaze stabilization on target while the head continues").
- The controller is best modeled as a single **gaze-displacement feedback controller** with a VOR whose gain is modulated as a function of gaze error and head velocity (Lefèvre et al. 1992, cited in [link.springer.com/article/10.1007/BF00227846](https://link.springer.com/article/10.1007/BF00227846); Roy & Cullen above; Pélisson et al. 1988 above).

## 2.5 Gaze strategy during forward locomotion — where do people actually look?

### 2.5.1 Flat-ground walking: travel gaze and intermittent goal fixations

Patla & Vickers (1997) introduced the taxonomy still used: **travel fixation** (TravFix — gaze stable, traveling with the whole body, sampling optic flow), **obstacle/footprint fixation** (target-directed), and **far-region fixation** (Fix4-6, the 4–6 m region). Stepping over obstacles: the obstacle was fixated only ~**20% of travel time**, and **participants did not fixate the obstacle while stepping over it — the planning was done in the steps before**; TravFix duration/frequency stayed constant while Fix4-6 duration rose in the step before/over the obstacle (landing-area search) ([doi.org/10.1097/00001756-199712010-00002](https://doi.org/10.1097/00001756-199712010-00002)).

Patla & Vickers (2003), stepping on 17 footprints over 10 m: **travel fixation dominated (>50% of travel time)**; when participants did fixate the landing target they did so **on average two steps ahead, ~800–1000 ms before the limb is placed on the target** ([pubmed.ncbi.nlm.nih.gov/12478404/](https://pubmed.ncbi.nlm.nih.gov/12478404/)). This is the temporal anchor for "look-ahead ≈ 2 steps ≈ 1 s."

**Honest disagreement:** Pelz et al. (2010) failed to replicate "travel gaze" with a modern eye tracker — they saw ordinary fixations and saccades, and argue the original TravFix was an artifact of temporal averaging of small saccades plus order effects ([doi.org/10.1167/9.8.422](https://doi.org/10.1167/9.8.422)). Both results should be reported; the *functional* conclusion (gaze spends most time ahead on the path/goal, with target-directed fixations ~2 steps ahead when precision is required) survives either way, since Pelz also found path fixations drop off when no markings exist.

Local check: at 1.4 m/s with 0.7 m steps, 2 steps ahead = 1.0 s; the Fix4-6 region (4–6 m) is 2.9–4.3 s of travel. So "1–2 s ahead on flat ground, up to several seconds in the 4–6 m far region" is the verified spread.

### 2.5.2 Rough terrain: foot-placement targets 2–3 steps ahead

The key modern papers (all verified live):

- **Matthis, Yates & Hayhoe (2018), Current Biology** — the definitive natural-terrain study. Walkers tune gaze to terrain difficulty **while maintaining a constant temporal look-ahead window across terrains**: gaze reallocates toward foothold regions as terrain roughens, and walkers **slow down** when terrain demands more visual search ([doi.org/10.1016/j.cub.2018.03.008](https://doi.org/10.1016/j.cub.2018.03.008)).
- **Matthis & Hayhoe (2016, VSS abstract)** describing the same apparatus: in flat terrain subjects looked far down the path and explored; in difficult terrain they performed rapid visual search on regions **around 2–4 steps ahead, often fixating precisely on upcoming footholds**, with saccades between future footholds ([doi.org/10.1167/16.12.766](https://doi.org/10.1167/16.12.766)).
- **Matthis & Fajen (2013), Proc. R. Soc. B:** as little as **two step lengths of visible foreground** lets walkers choose footholds as energetically efficient as with unlimited vision ([doi.org/10.1098/rspb.2013.0700](https://doi.org/10.1098/rspb.2013.0700)); the binocular-vision follow-up (Sci. Rep. 2021) shows gaze-elevation distributions centered near −45° (ground 2–3 footholds ahead) in rough terrain, shifting to near-horizontal (far) in smooth terrain ([nature.com/articles/s41598-021-99846-0](https://preview-www.nature.com/articles/s41598-021-99846-0)).
- **Marigold & Patla (2007), J. Neurophysiol.:** gaze fixation patterns for negotiating complex ground terrain — participants visually fixated areas where they eventually stepped, and fixations were frequently directed at **transition zones** between surfaces (solid→compliant, rocky→slippery, tilted→irregular) (paper at [doi.org/10.1016/j.neuroscience.2006.09.006](https://doi.org/10.1016/j.neuroscience.2006.09.006); the transition-zone finding is summarized in [e-jmd.org](https://www.e-jmd.org/journal/view.php?doi=10.14802%2Fjmd.18018)). Note the parent prompt asked about "Marigold & Patla on gaze during adaptive locomotion" — the verified citations are Marigold & Patla 2007 (complex terrain, J. Neurophysiol./Neuroscience) and Marigold 2008 (peripheral vision in online guidance, [doi.org/10.1097/jes.0b013e31817bff72](https://doi.org/10.1097/jes.0b013e31817bff72)).

**Temporal coupling between fixation and footfall:** Hollands & Marple-Horvat (1995), stepping stones: **68% of saccades toward the next footfall target were completed while the foot to be positioned was still on the ground** (rest within the first 300 ms of swing), and gaze remained on the target **until on average 51 ms after foot contact** ([doi.org/10.1080/00222895.1995.9941707](https://doi.org/10.1080/00222895.1995.9941707)). Their 2001 follow-up shows the saccade-to-swing interval is stable across visual conditions while saccade-onset-away to stance-onset varies — evidence of coupled oculomotor/locomotor control ([doi.org/10.1080/00222890109603151](https://doi.org/10.1080/00222890109603151)). Foulsham's synthesis: "people look approximately two steps ahead, fixating the ground that will be traversed up to 1 s later" ([doi.org/10.1038/eye.2014.275](https://doi.org/10.1038/eye.2014.275)). Elderly fallers look at the *imminent* target instead of two steps ahead (Chapman-class work summarized in [e-jmd.org](https://www.e-jmd.org/journal/view.php?doi=10.14802%2Fjmd.18018)) — the maladaptive pattern the healthy system avoids.

So the verified spread is: **foot-placement fixations 2–3 steps ahead (2–4 in the VSS report), fixated ~800–1000 ms before footfall, gaze leaving the target at or just after (~50 ms) foot contact.**

### 2.5.3 Optic flow and heading: the focus of expansion

For an observer translating with velocity $\mathbf{v}$ past a stationary point at world position $\mathbf{p}$ (relative position $\mathbf{r} = \mathbf{p} - \mathbf{x}$, depth along heading $Z$), the retinal (angular) velocity of the point is:

$$
\dot{\mathbf{r}}_{ang} = \frac{\mathbf{v}_\perp}{Z} = \frac{\mathbf{v} - (\mathbf{v}\cdot\hat{\mathbf{r}})\hat{\mathbf{r}}}{|\mathbf{r}|}
$$

This is zero exactly when $\mathbf{v} \parallel \mathbf{r}$ — i.e., at the **focus of expansion (FOE)**, the retinal locus of the heading direction. The flow field radiates outward from the FOE with speed growing as $\sin(\varepsilon)\,v/Z$ at eccentricity $\varepsilon$; local check: at $\varepsilon = 10°$, $v = 1.4$ m/s, $Z = 4$ m, retinal speed ≈ 3.5°/s — inside the slip tolerance. **Looking at/near the FOE (or the distant goal, which is near the FOE) minimizes retinal flow for the fixated direction**, which is why a far-goal fixation is the visually cheapest place to park gaze during straight locomotion (and why walking retinal slip stays < 4°/s for targets beyond 4 m — Crane & Demer, §2.1.1).

**Heading precision:** the classic result is 75%-correct thresholds of **0.66°** in the best condition and **~1.2° generally** (Warren, Morris & Kalish 1988, [doi.org/10.1037//0096-1523.14.4.646](https://doi.org/10.1037//0096-1523.14.4.646)); with features near the direction of heading, accuracy **< 0.2°** is possible (Warren & Kurtz 1992 / Crowell & Banks 1996, quoted in [pmc.ncbi.nlm.nih.gov/articles/PMC4520383/](https://pmc.ncbi.nlm.nih.gov/articles/PMC4520383/)). Humans can also recover heading from retinal flow containing rotation (i.e., while fixating off-axis) to ~1.5° (van den Berg & Brenner, [doi.org/10.1016/0042-6989(94)90324-7](https://doi.org/10.1016/0042-6989(94)90324-7)). The parent anchor "~1–2°" is verified, with the best-case ~0.2–0.66° and typical ~1–1.5°. Two studies show observers **spontaneously track the FOE with gaze** without instruction (cited in the continuous-psychophysics paper, [pmc.ncbi.nlm.nih.gov/articles/PMC11469512/](https://pmc.ncbi.nlm.nih.gov/articles/PMC11469512/)) — direct evidence for far-goal/FOE gaze anchoring during self-motion.

### 2.5.4 Running and sports: gaze locks onto task-relevant points

- **The "quiet eye" (Vickers):** the final fixation or tracking gaze located on a specific location/object **within 3° of visual angle for a minimum of 100 ms**, onset before the critical movement phase; elite performers have **earlier onset and longer duration** than near-elites (definition and review: Vickers 2016, [doi.org/10.15203/ciss_2016.118](https://doi.org/10.15203/ciss_2016.118); basketball three-point data — elite fixation on hoop ~972 ms on hits vs 806 ms misses: [researchonline.ljmu.ac.uk](https://researchonline.ljmu.ac.uk/id/eprint/11672/1/Vickers%20et%20al.%2C%202019.pdf)). Across ~28 motor tasks, longer QE durations characterize superior performance (Walters-Symons thesis, [hdl.handle.net/10871/28336](http://hdl.handle.net/10871/28336)).
- **Outfielder optical acceleration cancellation (OAC):** fielders run so as to keep the ball's optical elevation angle increasing at a decreasing rate (zero out its acceleration), plus control the horizontal rotation rate to keep fixation — the generalized OAC theory (McLeod, Reed & Dienes, [doi.org/10.1037/0096-1523.32.1.139](https://doi.org/10.1037/0096-1523.32.1.139)); the VR critical test confirms OAC over LOT and trajectory-prediction theories (Fink, Foo & Warren, [doi.org/10.1167/9.13.14](https://doi.org/10.1167/9.13.14)); Chapman 1968 is the origin ([doi.org/10.1119/1.1974297](https://doi.org/10.1119/1.1974297)); the competing linear-optical-trajectory model is McBeath, Shaffer & Kaiser, Science 1995 ([doi.org/10.1126/science.7725104](https://doi.org/10.1126/science.7725104)) — report both.
- **Driving (the best-instrumented steering literature):** drivers fixate the **tangent point** on the inside of the curve **1–2 s before each bend** and keep returning to it through the bend (Land & Lee 1994, [nature.com/articles/369742a0](https://www.nature.com/articles/369742a0)); later work argues future-path points are equally or better supported as gaze targets (Lappi et al., [jov.arvojournals.org — Beyond the tangent point](https://jov.arvojournals.org/article.aspx?articleid=2193829), and the future-path/tangent-point model comparison, [jov.arvojournals.org — Future path and tangent point models](https://jov.arvojournals.org/article.aspx?articleid=2193909)).

The unifying conclusion: **gaze locks onto specific task-relevant points (footholds, tangent points, targets, the FOE) at 1–2 s look-ahead — it does not sweep continuously.** Steering-by-looking ("go where you look") is directly supported by the turning literature (§2.6.1).

## 2.6 Gaze during turning, sideways, and backward movement

### 2.6.1 Turning: eyes lead, head leads, body follows

The steering sequence is robustly top-down and anticipatory:

- **Grasso, Prévost, Ivanenko & Berthoz (1998):** walking 90° corners, gaze points to the final position well before the corner; head/eye anticipation develops **more than 1 s before the corner in forward walking** (less than 1 s in backward walking, with reduced amplitude), present even in darkness ([prevost.pascal.free.fr/public/pdf/Grasso1998.pdf](http://prevost.pascal.free.fr/public/pdf/Grasso1998.pdf)).
- **Grasso et al. (1996), "The predictive brain":** circular walking — head direction systematically anticipates locomotion direction by **~200 ms** (curvature-dependent), with the head deviated toward the inner concavity in the light ([pubmed.ncbi.nlm.nih.gov/8817526/](https://pubmed.ncbi.nlm.nih.gov/8817526/)).
- **Hollands, Patla & Vickers (2002), "Look where you're going!":** gaze behavior during maintained and changing direction of locomotion — gaze (via saccades) is used to align with the endpoint of the required travel path before the body reorients ([pubmed.ncbi.nlm.nih.gov/11880898/](https://pubmed.ncbi.nlm.nih.gov/11880898/)).
- **Bernardin et al. (2012) / gaze-anticipation line:** quantified lead times — **gaze anticipates heading by ~404 ms, head by ~222 ms**, with both increasing with curvature (up to ~600 ms in high-curvature segments) and gaze leading head, head leading trunk/pelvis/feet (the "Gaze anticipation during human locomotion" paper, [academia.edu/13203358](https://www.academia.edu/13203358/Gaze_anticipation_during_human_locomotion); the summary in the Frontiers paper below gives gaze-head ~100–300 ms and head-trajectory similar).
- **Authié et al. (2015):** on limaçon/figure-eight paths, horizontal gaze anticipates head which anticipates trajectory direction, in light *and* darkness; angular anticipation **halves in darkness**; nystagmus persists in both ([pmc.ncbi.nlm.nih.gov/articles/PMC4458691/](https://pmc.ncbi.nlm.nih.gov/articles/PMC4458691/)).
- **Vallis & Patla (2001):** head yaw, trunk yaw, and COM reorientation in the turn are sequenced head/trunk first, COM last; when the head is unexpectedly perturbed, the CNS **delays committing COM motion until it has looked** at the new path ([pubmed.ncbi.nlm.nih.gov/11374079/](https://pubmed.ncbi.nlm.nih.gov/11374079/)).
- **Imai et al. (2001)** and Hollands' later work: saccades toward the turn direction precede head rotation in 90° turning maneuvers (summarized in [e-jmd.org](https://www.e-jmd.org/journal/view.php?doi=10.14802%2Fjmd.18018)).
- Counterpoint to report honestly: **Cinelli & Warren (2012)** showed anticipatory head rotations are **neither necessary nor sufficient** for changing direction when a visible locomotor goal exists (discussed in Authié et al. above). The anticipation is spontaneous and typical, not obligatory.

The parent anchor "head anticipates the turn by ~100–300+ ms" is verified and if anything conservative: the measured spans are **~200 ms (circular walking), ~400 ms gaze / ~220 ms head (complex trajectories), and up to ~1 s (corner transitions)**. The sequence is eyes (saccade) → head → trunk → pelvis → feet → COM, i.e., a strictly top-down reorientation cascade.

Head stabilization vs re-orientation: during the turn, the head *rotates in space* to the new travel direction (the anticipatory synergy), but within the turn, pitch/roll stabilization (§2.1) continues; the two are compatible because yaw re-orientation is a low-frequency (< ~0.2 Hz) command layered over gait-frequency stabilization.

### 2.6.2 Sideways movement

- **Biomechanics/metabolics:** lateral (side-shuffle) movement is the most expensive of the three translations — at 80.45 m/min, VO₂ was FM = 12.42, BM = 15.95, LM = 22.10 mL/kg/min (lateral ≈ 78% more costly than forward at that speed) (Williford et al., [doi.org/10.1249/00005768-199809000-00011](https://doi.org/10.1249/00005768-199809000-00011)).
- **What the body does:** humans stepping laterally rotate the feet/torso toward the travel direction. In evasive side-stepping, the rear foot is moved ~90° to the side and the chest is turned to point in the same direction as the feet (military/martial training description, [mil-sport.ch/en/topics/exercise/3458](https://www.mil-sport.ch/en/topics/exercise/3458)).
- **Tactical/military side-step doctrine** (practitioner literature — flagged as non-peer-reviewed): law-enforcement "shooting on the move" doctrine teaches the sidestep with **weapon and eyes on the threat while the feet move laterally**: large step with the foot on the side of travel, toe-first, then a small trailing step; upper body erect and isolated as a stable firing platform; feet kept pointed in the direction of movement; flanking targets engaged by twisting the upper body toward the target ([alpharubicon.com/leo/shootingonmove.htm](http://www.alpharubicon.com/leo/shootingonmove.htm)). Practitioner analysis of lateral movement under fire emphasizes that moving with the head/eyes locked on the threat means "you can't see where you're going," and recommends moving toward the weak side so the body opens toward the threat with unimpeded peripheral vision of the travel path ([blog.cheaperthandirt.com](https://blog.cheaperthandirt.com/understanding-movement-in-gunfight-fluidity-rotation-angles/)). Training drills for lateral movement while shooting (shuffle step vs cross-step) are documented at [mosqueras.com/lateral-side-step-shooting/](https://mosqueras.com/lateral-side-step-shooting/).
- **Scientific limitation, stated plainly:** I found no peer-reviewed eye-tracking study of gaze during tactical lateral movement. The doctrine sources above are consistent with the verified science (gaze locks to task-relevant threat points per §2.5.4; peripheral vision suffices for the near path per Marigold 2008, [doi.org/10.1097/jes.0b013e31817bff72](https://doi.org/10.1097/jes.0b013e31817bff72)), but the specific "eyes on threat, feet lateral" allocation is doctrine, not measurement. Mark accordingly in any derived design.

### 2.6.3 Backward movement

- **Strong preference for turning around:** backward locomotion is metabolically costlier (backward walking elicits greater VO₂/HR/RPE than forward at matched intensity; Flynn/classic studies summarized in [sciencedirect.com/science/article/abs/pii/S0966636218309342](https://www.sciencedirect.com/science/article/abs/pii/S0966636218309342) and [ajol.info](https://www.ajol.info/index.php/ajbr/article/view/95144)) and slower (mean speeds in Grasso's corner study: 1.15 m/s forward vs 0.96 m/s backward in light, [Grasso 1998 PDF above](http://prevost.pascal.free.fr/public/pdf/Grasso1998.pdf)). The energetics literature treats backward walking as a training/rehabilitation oddity precisely because humans avoid it naturally.
- **When they do move backward, gaze behavior reverses coherently:** Grasso et al. (1996, 1998) and Courtine & Schieppati (2003) — subjects walking backward on circular paths orient head and gaze such that the *opposite* vectors anticipate body motion; the forward orienting synergy is not simply time-reversed (summary and citations in Authié et al. 2015, [pmc.ncbi.nlm.nih.gov/articles/PMC4458691/](https://pmc.ncbi.nlm.nih.gov/articles/PMC4458691/), and Grasso 1998 above: anticipation < 1 s, lower amplitude, some subjects showing little or even reversed anticipation in backward walking).
- **Rearward sampling when the destination is behind:** an observational study of people stepping backward while taking photographs found **87% looked back at least once before or during a backward step, and 83% of backward steps were preceded by or accompanied by a look in the direction of travel** ([journals.sagepub.com/doi/10.1177/1071181312561143](https://journals.sagepub.com/doi/10.1177/1071181312561143)). This is the closest verified number on rear-checking frequency during backward translation; per-second sampling rates are not available in the verified literature (VR backward-locomotion work exists — e.g., deep-learning backward-movement detection for walking-in-place, [doi.org/10.1109/vr50410.2021.00072](https://doi.org/10.1109/vr50410.2021.00072) — but reports no rearward-gaze sampling rate).
- Vehicle-reversing behavior literature exists for collision-avoidance design but I could not verify a specific rearward glance-rate number live this session; **marked unverified and dropped.**

### 2.6.4 The conclusion this section must deliver

**Real humans do not translate sideways or backward with a fixed forward gaze.** The verified data support two, and only two, patterns:

1. **Re-orient to travel direction:** in nearly all voluntary lateral/backward displacement, the body (feet, torso, and where possible the head) rotates toward the direction of travel — the top-down gaze→head→trunk→feet cascade of §2.6.1 run in reverse geometry (Grasso 1996/1998; Bernardin; Vallis & Patla; mil-sport evasive-step doctrine).
2. **Alternation at saccadic timescales:** when the head/torso *cannot* re-orient (threat fixation in tactical side-stepping; moving backward toward a seen goal), gaze alternates between the task-relevant direction and the travel direction in discrete samples — saccade-plus-fixation units at the canonical **2–3 fixations/s** rate with fixation durations in the hundreds-of-ms range (§2.4.2; the 87%/83% rear-check behavior of §2.6.3; QE-class target locking of §2.5.4). Continuous monitoring of both directions simultaneously does not occur; peripheral vision covers the un-fixated direction at reduced acuity (Marigold 2008, above).

Measured alternation rates specific to lateral/backward locomotion are not available in the peer-reviewed literature (verified gap); the saccadic sampling rate ceiling (~2–3/s) and the rear-check incidence (87% of backward steppers) are the closest verified anchors.

## 2.7 Sampling, attention, and the limits of monitoring

- **Fixation rate:** 2–3 fixations/s (Gibaldi & Sabatini, [pmc.ncbi.nlm.nih.gov/articles/PMC7880984/](https://pmc.ncbi.nlm.nih.gov/articles/PMC7880984/)); saccadic suppression (§2.4.1) makes each saccade a functional blink — the visual system samples the world in discrete ~150–600 ms fixations separated by blind jumps, even while the body moves continuously.
- **Look-ahead minimum:** ~2 steps ≈ 1 s (Matthis & Fajen 2013: two step lengths of visual preview suffice for efficient foothold selection, [doi.org/10.1098/rspb.2013.0700](https://doi.org/10.1098/rspb.2013.0700); Patla & Vickers 2003: fixations ~800–1000 ms before footfall). Below this window, walkers slow down and fall back to imminent-target fixation (elderly-faller pattern).
- **Sampling vs continuous monitoring:** the travel-fixation / far-region taxonomy of Patla & Vickers (1997/2003) is precisely a distinction between *sampling optic flow for self-motion* (TravFix/Fix4-6) and *acquiring specific targets* (footprint/obstacle fixations). The Pelz et al. (2010) non-replication (above) sharpens rather than destroys this: the discrete-sampling structure is real; the smooth "travel gaze" may have been an artifact.
- **Urban pedestrian gaze:** mobile eye-tracking of real streets shows gaze concentrated on **other people, obstacles, and the path ahead**, with a strong central bias — walkers keep eye position near the center of the head frame and select targets with *head* movements, eyes staying near a "heading point" slightly above the head-frame center; walkers look at the near path more than video-watchers do, and rarely fixate nearby pedestrians (within a ~3 s crossing window) in the real world (Foulsham, Walker & Kingstone 2010, [doi.org/10.1167/9.8.446](https://doi.org/10.1167/9.8.446); Foulsham & Kingstone 2011, [sciencedirect.com/science/article/pii/S0042698911002392](https://www.sciencedirect.com/science/article/pii/S0042698911002392); Foulsham 2014 synthesis, [doi.org/10.1038/eye.2014.275](https://doi.org/10.1038/eye.2014.275)). Street-edge studies: pedestrians visually engage ground floors more than upper floors, and the walked-side edge more than the opposite side on non-pedestrianized streets (Simpson et al., [MDPI Sustainability 2019](https://mdpi-res.com/d_attachment/sustainability/sustainability-11-04251/article_deploy/sustainability-11-04251.pdf?version=1565092250)); fixations within 10° of view center correlate with luminance/saliency, more strongly at night (Jiang et al., [doi.org/10.1177/1477153520968158](https://doi.org/10.1177/1477153520968158)); AI-segmentation work finds sidewalks/vegetation/built structures attract the most attention and sky/signage the least ([doi.org/10.1016/j.trip.2026.102113](https://doi.org/10.1016/j.trip.2026.102113)).
- **Why staring at the feet fails:** it consumes the fixation budget on already-selected ground (the current foothold was fixated 2 steps ago), destroys preview for the *next* selection, and — because near-target fixation amplifies translational retinal slip (worked example in §2.1.3: 3.6°/s at 4 m vs 14°/s at 1 m for the same bob) — actively degrades acuity.

## 2.8 Consolidated boxed-equation summary + primary-source list

### Boxed equations

$$
\boxed{\ \dot{\mathbf{r}}_{ang} = \frac{\mathbf{v} - (\mathbf{v}\cdot\hat{\mathbf{r}})\hat{\mathbf{r}}}{|\mathbf{r}|} \;=\; 0 \iff \text{gaze at the focus of expansion (heading)}\ }
$$

$$
\boxed{\ \frac{\Theta(s)}{\Omega(s)} = \frac{K\,T_L\,s}{(T_L s + 1)(T_S s + 1)},\quad T_L \approx 4.2\text{–}5.7\ \text{s},\ T_S \approx 0.003\text{–}0.005\ \text{s}\ \Rightarrow\ \text{velocity meter for } f \gtrsim 0.03\ \text{Hz}\ }
$$

$$
\boxed{\ D_{saccade} \approx 20\ \text{ms} + (2\text{–}2.7)\ \frac{\text{ms}}{\text{deg}}\cdot A,\qquad V_{peak} \xrightarrow{\ A\gg 15°\ } \sim 700\text{–}900\ °/\text{s}\ }
$$

$$
\boxed{\ \theta_{gaze} = \theta_{eye\text{-}in\text{-}head} + \theta_{head} + \theta_{body};\quad \text{eyes lead head by } \sim\!30\text{–}50\ \text{ms in large shifts, VOR inhibited } 40\text{–}96\%\ }
$$

$$
\boxed{\ \text{VOR: gain} \approx 0.75\text{–}1.1\ (\text{context-dependent}),\ \text{latency} \approx 3\text{–}10\ \text{ms};\quad \text{slip tolerance: } \sim\!0.2\text{–}2\ °/\text{s optimal},\ \sim\!4\text{–}6\ °/\text{s limit}\ }
$$

$$
\boxed{\ \text{Turn cascade: saccade} \to \text{head} \to \text{trunk} \to \text{COM};\ \text{gaze leads heading by } \sim\!200\text{–}400\ \text{ms (up to } \sim\!1\ \text{s at corners)}\ }
$$

$$
\boxed{\ \text{Look-ahead: } \sim\!2\ \text{steps} \approx 1\ \text{s on flat/rough ground (footholds fixated } 800\text{–}1000\ \text{ms before footfall; gaze leaves target at/}\sim\!50\text{ ms after contact)}\ }
$$

$$
\boxed{\ \text{Sampling ceiling: } 2\text{–}3\ \text{fixations/s with saccadic suppression between; no continuous monitoring of two directions — alternate or re-orient.}\ }
$$

### Primary sources (all verified live this session, 2026-09-06)

1. Pozzo, Berthoz & Lefort (1990), head stabilization in locomotor tasks — [europepmc.org/article/MED/2257917](https://europepmc.org/article/MED/2257917)
2. Crane & Demer (1997), gaze stabilization during natural activities — [doi.org/10.1152/jn.1997.78.4.2129](https://doi.org/10.1152/jn.1997.78.4.2129)
3. van Leeuwen et al. (retinal slip, active vs passive) — [springermedicine.com](https://www.springermedicine.com/retinal-slip-during-active-head-motion-and-stimulus-motion/25684106)
4. Fathi et al. (2015), reading from head-fixed display during walking — [PLOS One](https://journals.plos.org/plosone/article?id=10.1371%2Fjournal.pone.0129902)
5. Schubert et al., retinal image slip tolerance for VOR adaptation — [PMC7392997](https://pmc.ncbi.nlm.nih.gov/articles/PMC7392997/)
6. Flipse (1990), compensatory eye movements in dynamic perception — [hdl.handle.net/1765/40270](http://hdl.handle.net/1765/40270)
7. Lim et al. (2020), locomotor coordination & head stability during running (review) — [MDPI Brain Sciences](https://www.mdpi.com/2076-3425/10/3/174)
8. Kavanagh et al. (2005), head/trunk accelerations during walking — [Springer](https://link.springer.com/article/10.1007/s00421-005-1328-1)
9. Hirasaki et al. (1999), walking velocity & vertical head movements — [NASA NTRS](https://ntrs.nasa.gov/citations/20040141864)
10. Hölzl et al. (2020), neck-muscle control of head oscillation damping — [Springer PDF](https://link.springer.com/content/pdf/10.1007/s00405-020-06488-5.pdf)
11. Karmali & Lim (2021), velocity-storage time constant tradeoff — [PMC9103412](https://pmc.ncbi.nlm.nih.gov/articles/PMC9103412/)
12. Choi (2025), vestibular end-organ transfer functions — [e-rvs.org](https://www.e-rvs.org/journal/view.php?number=991)
13. Hullar et al. (2009), canal afferent transfer functions — [Springer](https://link.springer.com/article/10.1007/s10162-008-0120-4)
14. Soyka et al. (2012), yaw thresholds & canal model — [Springer](https://link.springer.com/article/10.1007/s00221-012-3120-x)
15. MacNeilage et al. (2010), canal-otolith interaction thresholds — [doi.org/10.1152/jn.01067.2009](https://doi.org/10.1152/jn.01067.2009)
16. Grabherr/Merfeld-class signal-detection re-fit — [PubMed 22923225](https://pubmed.ncbi.nlm.nih.gov/22923225/)
17. Karmali/Valko-class vestibular thresholds review — [ScienceDirect](https://www.sciencedirect.com/science/article/pii/S0966636223014339)
18. Frontiers vestibular thresholds review (2021) — [Frontiers in Neurology](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2021.643634/full)
19. Curthoys (2017), the video head impulse test — [Frontiers in Neurology](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2017.00258/full)
20. Motorized head impulse rotator norms (JAMA Otolaryngology 2007) — [JAMA Network](https://jamanetwork.com/journals/jamaotolaryngology/fullarticle/484653)
21. Matiño-Soler et al. (2015), age-dependent normal VOR gain — [Springer](https://link.springer.com/article/10.1186/s40463-015-0081-7)
22. Pélisson, Prablanc & Urquizar (1988), VOR inhibition during gaze saccades — [doi.org/10.1152/jn.1988.59.3.997](https://doi.org/10.1152/jn.1988.59.3.997)
23. Lefèvre et al. (1992), VOR modulation during large gaze shifts — [Springer](https://link.springer.com/article/10.1007/BF00227846)
24. Roy & Cullen (2015), VOR suppression reveals gaze feedback — [J. Neuroscience](https://www.jneurosci.org/content/35/3/1192)
25. Freedman & Sparks (1997), eye-head coordination, unrestrained — [doi.org/10.1152/jn.1997.77.5.2328](https://doi.org/10.1152/jn.1997.77.5.2328)
26. Bahill, Clark & Stark (1975), the main sequence — [PDF](http://www.visualcognition.ca/spering/reading/Bahill.Clark.Stark.MathBiosci.1975.pdf)
27. Baloh et al. (1975), saccade amplitude-duration-velocity — [Neurology](https://www.neurology.org/doi/10.1212/WNL.25.11.1065)
28. Gibaldi & Sabatini (2021), main sequence revised — [PMC7880984](https://pmc.ncbi.nlm.nih.gov/articles/PMC7880984/)
29. Speed-accuracy tradeoffs & the main sequence (2022) — [Sci. Rep.](https://preview-www.nature.com/articles/s41598-022-09029-8)
30. Extraretinal control of saccadic suppression (Burr/Morrone-class) — [PMC6773104](https://pmc.ncbi.nlm.nih.gov/articles/PMC6773104/)
31. Harris et al., intrasaccadic suppression = gain reduction — [PMC3704127](https://pmc.ncbi.nlm.nih.gov/articles/PMC3704127/)
32. Ido/Baccus-class, saccadic suppression starts in the retina — [PMC7181657](https://pmc.ncbi.nlm.nih.gov/articles/PMC7181657/) and [Comm. Biology 2022](https://preview-www.nature.com/articles/s42003-022-03526-2)
33. Quantification of tracking eye movements (smooth pursuit gain) — [PubMed 7211334](https://pubmed.ncbi.nlm.nih.gov/7211334/)
34. Feng, Pelz & Daly (2010), smooth pursuit velocity limit — [doi.org/10.1117/12.840146](https://doi.org/10.1117/12.840146)
35. Thomas et al. (2017), smooth pursuit decreases balance during walking — [Springer](https://doi.org/10.1007/s00221-017-4996-2)
36. Churchland & Lisberger (2002), pursuit gain control — [doi.org/10.1152/jn.2002.87.6.2936](https://doi.org/10.1152/jn.2002.87.6.2936)
37. Patla & Vickers (1997), where/when we look stepping over obstacles — [doi.org/10.1097/00001756-199712010-00002](https://doi.org/10.1097/00001756-199712010-00002)
38. Patla & Vickers (2003), how far ahead for specific stepping locations — [PubMed 12478404](https://pubmed.ncbi.nlm.nih.gov/12478404/)
39. Pelz et al. (2010), travel gaze re-examined — [doi.org/10.1167/9.8.422](https://doi.org/10.1167/9.8.422)
40. Hollands & Marple-Horvat (1995), eye movements during visually guided stepping — [doi.org/10.1080/00222895.1995.9941707](https://doi.org/10.1080/00222895.1995.9941707)
41. Hollands & Marple-Horvat (2001), coordination of eye and leg movements — [doi.org/10.1080/00222890109603151](https://doi.org/10.1080/00222890109603151)
42. Hollands, Patla & Vickers (2002), "Look where you're going!" — [PubMed 11880898](https://pubmed.ncbi.nlm.nih.gov/11880898/)
43. Marigold & Patla (2007/2008), gaze fixation for complex terrain; peripheral vision online — [doi.org/10.1016/j.neuroscience.2006.09.006](https://doi.org/10.1016/j.neuroscience.2006.09.006), [doi.org/10.1097/jes.0b013e31817bff72](https://doi.org/10.1097/jes.0b013e31817bff72)
44. Matthis, Yates & Hayhoe (2018), gaze & foot placement in natural terrain — [doi.org/10.1016/j.cub.2018.03.008](https://doi.org/10.1016/j.cub.2018.03.008)
45. Matthis & Hayhoe (2016), gaze-gait coupling on rough terrain — [doi.org/10.1167/16.12.766](https://doi.org/10.1167/16.12.766)
46. Matthis & Fajen (2013), two step lengths suffice — [doi.org/10.1098/rspb.2013.0700](https://doi.org/10.1098/rspb.2013.0700)
47. Matthis et al. (2021), binocular vision & foot placement — [Sci. Rep.](https://preview-www.nature.com/articles/s41598-021-99846-0)
48. Warren, Morris & Kalish (1988), translational heading from optical flow — [doi.org/10.1037//0096-1523.14.4.646](https://doi.org/10.1037//0096-1523.14.4.646)
49. Spatial integration of optic flow for heading — [PMC4520383](https://pmc.ncbi.nlm.nih.gov/articles/PMC4520383/)
50. van den Berg & Brenner (1994), combining optic flow with depth for heading — [doi.org/10.1016/0042-6989(94)90324-7](https://doi.org/10.1016/0042-6989(94)90324-7)
51. Continuous psychophysics of heading — [PMC11469512](https://pmc.ncbi.nlm.nih.gov/articles/PMC11469512/)
52. Foulkes et al. (2013), heading models vs humans — [Frontiers](https://www.frontiersin.org/articles/10.3389/fnbeh.2013.00053/pdf)
53. Vickers (2016), quiet eye: definition & reply — [doi.org/10.15203/ciss_2016.118](https://doi.org/10.15203/ciss_2016.118)
54. Vickers et al. (2019), quiet eye in the basketball three-point shot — [PDF](https://researchonline.ljmu.ac.uk/id/eprint/11672/1/Vickers%20et%20al.%2C%202019.pdf)
55. Walters-Symons, quiet eye cognitive mechanisms — [hdl.handle.net/10871/28336](http://hdl.handle.net/10871/28336)
56. McLeod, Reed & Dienes (2006), generalized OAC theory — [doi.org/10.1037/0096-1523.32.1.139](https://doi.org/10.1037/0096-1523.32.1.139)
57. Fink, Foo & Warren (2009), catching fly balls in VR — [doi.org/10.1167/9.13.14](https://doi.org/10.1167/9.13.14)
58. McBeath, Shaffer & Kaiser (1995), LOT model — [doi.org/10.1126/science.7725104](https://doi.org/10.1126/science.7725104)
59. Chapman (1968), catching a baseball — [doi.org/10.1119/1.1974297](https://doi.org/10.1119/1.1974297)
60. Land & Lee (1994), where we look when we steer — [nature.com/articles/369742a0](https://www.nature.com/articles/369742a0)
61. Lappi et al., future path & tangent point models — [JOV](https://jov.arvojournals.org/article.aspx?articleid=2193909), [Beyond the tangent point](https://jov.arvojournals.org/article.aspx?articleid=2193829)
62. Grasso et al. (1998), eye-head coordination for steering — [PDF](http://prevost.pascal.free.fr/public/pdf/Grasso1998.pdf)
63. Grasso et al. (1996), the predictive brain — [PubMed 8817526](https://pubmed.ncbi.nlm.nih.gov/8817526/)
64. Authié et al. (2015), gaze anticipation with/without vision — [PMC4458691](https://pmc.ncbi.nlm.nih.gov/articles/PMC4458691/)
65. Bernardin et al. (2012), gaze anticipation during locomotion — [academia.edu](https://www.academia.edu/13203358/Gaze_anticipation_during_human_locomotion)
66. Vallis & Patla (2001), steering with unexpected head yaw — [PubMed 11374079](https://pubmed.ncbi.nlm.nih.gov/11374079/)
67. The relationship between saccades and locomotion (review, 2018) — [e-jmd.org](https://www.e-jmd.org/journal/view.php?doi=10.14802%2Fjmd.18018)
68. Williford et al. (1998), metabolic costs of forward/backward/lateral motion — [doi.org/10.1249/00005768-199809000-00011](https://doi.org/10.1249/00005768-199809000-00011)
69. Graded forward/backward walking, matched intensity — [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0966636218309342)
70. Walking backwards without looking (observational) — [SAGE](https://journals.sagepub.com/doi/10.1177/1071181312561143)
71. Backward preferred transition speed study — [CSUS Scholars](https://scholars.csus.edu/esploro/outputs/journalArticle/Preferred-and-Energetically-Optimal-Transition-Speeds/99257880091201671)
72. Paik et al. (2021), VR backward movement detection — [doi.org/10.1109/vr50410.2021.00072](https://doi.org/10.1109/vr50410.2021.00072)
73. Shooting on the move (law-enforcement doctrine) — [alpharubicon.com](http://www.alpharubicon.com/leo/shootingonmove.htm)
74. Understanding movement in a gunfight (practitioner analysis) — [blog.cheaperthandirt.com](https://blog.cheaperthandirt.com/understanding-movement-in-gunfight-fluidity-rotation-angles/)
75. Lateral side-step shooting (training) — [mosqueras.com](https://mosqueras.com/lateral-side-step-shooting/)
76. Evasive manoeuvre: sideways step (training) — [mil-sport.ch](https://www.mil-sport.ch/en/topics/exercise/3458)
77. Rao reflex shooting method (tactical doctrine) — [majordeepakrao.com](https://www.majordeepakrao.com/rao_reflex_shooting_method)
78. Foulsham, Walker & Kingstone (2010), gaze in video vs real world — [doi.org/10.1167/9.8.446](https://doi.org/10.1167/9.8.446)
79. Foulsham & Kingstone (2011), where/what/when of gaze allocation — [ScienceDirect](https://www.sciencedirect.com/science/article/pii/S0042698911002392)
80. Foulsham (2014), eye movements in everyday tasks — [doi.org/10.1038/eye.2014.275](https://doi.org/10.1038/eye.2014.275)
81. Foulsham & Kingstone (2017), static scenes vs real world — [doi.org/10.1037/cep0000125](https://doi.org/10.1037/cep0000125)
82. Simpson et al. (2019), urban street edges mobile eye-tracking — [MDPI](https://mdpi-res.com/d_attachment/sustainability/sustainability-11-04251/article_deploy/sustainability-11-04251.pdf?version=1565092250)
83. Simpson, Street DNA — [White Rose](https://eprints.whiterose.ac.uk/id/eprint/131652/1/JoLA%20Accepted%20Text%20and%20Visuals.pdf)
84. Jiang et al. (2021), luminance/saliency & pedestrian fixations — [SAGE](https://doi.org/10.1177/1477153520968158)
85. Sultana et al. (2026), AI eye-tracking urban attention — [doi.org/10.1016/j.trip.2026.102113](https://doi.org/10.1016/j.trip.2026.102113)
86. Vitéribo & Baek, walking head-bob model for VR — [OSTI](https://www.osti.gov/servlets/purl/1113266)
87. Kao et al. (2004), head movement during gait transitions — [SAGE](https://journals.sagepub.com/doi/10.2466/pms.99.3f.1217-1229)
88. Head-movement predictability & vestibular suppression (Frontiers 2017) — [Frontiers](https://www.frontiersin.org/journals/computational-neuroscience/articles/10.3389/fncom.2017.00047/full)
