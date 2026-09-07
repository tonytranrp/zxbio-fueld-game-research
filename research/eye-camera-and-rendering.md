# The Eye vs. the Camera vs. the Game Renderer: Exposure, Glare, Motion, Foveation, Focus, and Time

**Scope:** what game renderers do that the eye does, doesn't, or does differently — exposure and adaptation, glare/bloom, motion blur, peripheral degradation and foveated rendering, focus and accommodation, temporal response — and the engineering mapping from the vision science to rendering features. This is Part 5 of the eye research set; it cross-references rather than duplicates the perception material in [`human-movement-and-perception-research.md`](human-movement-and-perception-research.md) (Part 3 §3.7 foveated rendering, §3.4 temporal limits; Part 5 FOV/VR comfort) and goes deeper on the optics/perception numbers that rendering decisions should be derived from. Formulas render as GitHub-flavored LaTeX (`$...$`, `$$...$$`).

---

## 0. Provenance

- **Date:** 2026-09-06. **Author:** research agent 5 of 5 (eye vs. camera vs. renderer).
- **Tools:** Exa web search/fetch, `web_fetch`, local numeric verification with `pwsh` (PowerShell). 12 search batches (~40 queries), ~30 full-content fetches; every URL in the source list returned live content this session.
- **Numeric anchors recomputed locally (pwsh):** pupil area range 8→2.5 mm = **1.01 log units**; EV100→luminance $L = 2^{EV-3}$ (EV15 → 4096 cd/m², matching Wikipedia's $K=12.5$ table); Stiles–Holladay $10/\theta^2$ at 5° → **0.4** cd/m² per lux; 180° shutter at 24 fps = **20.8 ms**, at 60 fps = **8.33 ms**; MPRT on a sample-and-hold display = 1000/Hz (60→16.67 ms, 90→11.11, 120→8.33, 144→6.94, 240→4.17); HDR10 container range 10 000/0.005 nits = **6.3 log units**; Adler-range 10⁻⁴→10⁵ cd/m² = **9 log units**; 63 mm IPD total convergence 7.21° at 0.5 m, 3.61° at 1 m, 1.80° at 2 m.
- **Anchor disagreements found (reported, not forced):** the "~10–12 log units" eye-range anchor holds only *with adaptation over time* — the instantaneous-with-full-adaptation span is ~9 log units ([Adler's/Clinical Tree](https://clinicalpub.com/light-adaptation-in-photoreceptors/)) and the *simultaneous* discriminable range under one adaptation state is only ~3.7 log units ([Reinhard et al. / simultaneous dynamic range, ACM](https://dl.acm.org/doi/10.1145/1836248.1836251)). The "Weber fraction ~0.01–0.02" anchor is low for foveal photopic vision: the standard reference value is **0.02–0.03** for the L/M-cone pathway and **0.14** for rods ([Webvision: Light and Dark Adaptation](https://www.ncbi.nlm.nih.gov/books/NBK11525/)). The "rods saturate ~10 cd/m²" anchor is imprecise: rod saturation is a retinal-illuminance phenomenon beginning near **2.0 log scotopic trolands and extending to ~3.0 log sc td** ([Stockman & Sharpe mesopic review](http://www.cvrl.org/people/Stockman/pubs/2006%20Mesopic%20review%20SS.pdf)); the ~1–10 cd/m² luminance figures floating in secondary sources are pupil-area-dependent conversions. "Phantom array to ~500 Hz+" understates: measured visibility extends from ~100 Hz to **several kHz**, peak sensitivity ~640 Hz ([Martinsons, CIE 2025](https://files.cie.co.at/x051_2025/P0685_ID286ChristopheMARTINSONS_OA_with_cover.pdf)).
- **Dropped as unverifiable this session:** exact per-game auto-exposure constants for shipped titles other than what engine docs state (engine documentation is used instead); any GDC-claimed foveation speedup numbers beyond the published papers.
- **Source count:** 41 distinct verified sources in §5.8; Appendix A adds none.

---

## 5.1 Exposure and adaptation: eye vs. camera

### 5.1.1 The camera model

A camera integrates light for a fixed exposure time $t$ through a fixed aperture $N$ (f-number) at a fixed gain (ISO $S$). Everything about its exposure is captured by one number, the exposure value:

$$\mathrm{EV} = \log_2\!\frac{N^2}{t}, \qquad \mathrm{EV}_{100} = \log_2\!\frac{L\,S}{K} \;\Rightarrow\; L = 2^{\mathrm{EV}-3}\ \mathrm{cd/m^2}$$

with the reflected-light meter constant $K = 12.5$ (Canon/Nikon/Sekonic convention; [Wikipedia: Exposure value](https://en.wikipedia.org/wiki/Exposure_Value), [scantips camera math](https://www.scantips.com/lights/math.html)). Worked example, verified locally: EV 15 ("sunny sixteen", ISO 100) corresponds to $2^{12} = 4096$ cd/m² scene luminance; EV 0 to 0.125 cd/m². A camera's sensor has a **fixed full-well capacity per pixel per frame**: one global (or rolling) shutter integrates for $t$, the well fills linearly, and it either has headroom or clips. The signal chain is linear light in → linear charge → sRGB-ish OETF out; nothing in a camera adapts *within* the frame. Two properties follow that no eye shares: (1) exposure is **global per frame** — every pixel of a photo taken at 1/500 s got exactly 2 ms; (2) dynamic range is fixed by the well depth and read noise, typically 10–14 stops, and cannot be extended except by bracketing.

### 5.1.2 The eye's model

The eye operates over an intensity range "of at least a billion-fold, from around 10⁻⁴ cd/m² under starlight conditions to around 10⁵ cd/m² under intense sunlight" — 9 log units — and "changes in pupil area account for only about 1 log unit of this 9 log unit range" (8 mm → 2.5 mm diameter; verified locally as 1.01 log units) ([Adler's Physiology of the Eye / Clinical Tree: Light Adaptation in Photoreceptors](https://clinicalpub.com/light-adaptation-in-photoreceptors/)). The rest comes from the duplex retina plus per-receptor gain control: each photoreceptor system operates over ≥5 log units, and "cone photoreceptors … manage to escape saturation no matter how intense the steady light" via Weber-law desensitization and, above ~4.3 log photopic trolands, photopigment bleaching ($p = I_0/(I+I_0)$ with $I_0 \approx 4.3$ log td) ([Lamb, *Why rods and cones?*, Eye 2016](https://cgvr.cs.uni-bremen.de/teaching/cg_literatur/Why%20rods%20and%20cones%20-%20TD%20Lamb,%20Eye%202016.pdf); [Rider, Henning & Stockman, PLOS ONE 2019](https://journals.plos.org/plosone/article/file?id=10.1371/journal.pone.0220358&type=printable)). Rods are the exception: they saturate — their circulating current shuts off entirely — beginning near 2–3 log scotopic trolands ([Aguilar & Stiles 1954, via Stockman & Sharpe](http://www.cvrl.org/people/Stockman/pubs/2006%20Mesopic%20review%20SS.pdf)); the "rods saturate ~10 cd/m²" anchor is a luminance-space approximation of this troland-space fact and should not be quoted as a sharp threshold.

**Increment sensitivity follows Weber's law** in the photopic regime: $\Delta L / L = $ constant, with the Weber fraction **0.02–0.03 for the cone pathways and 0.14 for rods** (S-cones ~0.09) ([Webvision: Light and Dark Adaptation](https://www.ncbi.nlm.nih.gov/books/NBK11525/)) — this is *contrast constancy*: the eye encodes local contrast, not absolute luminance. Cornsweet & Pinsker showed the fraction is exactly constant from just above absolute threshold to at least 5 log units up, for briefly-flashed comparison targets ([Cornsweet & Pinsker 1964](https://escholarship.org/content/qt1bg026w7/qt1bg026w7.pdf)) — a useful reminder that the Weber fraction is condition-dependent (their simultaneous-flash design yields the smaller fractions; steady-increment designs yield larger ones at low luminance).

**Time courses.** Dark adaptation is biphasic: the cone branch plateaus at **5–8 min**; the rod–cone break occurs at ~7–10 min; the absolute threshold (~10⁻⁵ cd/m²) is reached at **~40 min**, with the rod S2 recovery slope ~0.24 log units/min ([Webvision](https://www.ncbi.nlm.nih.gov/books/NBK11525/); [Lamb 2016](https://cgvr.cs.uni-bremen.de/teaching/cg_literatur/Why%20rods%20and%20cones%20-%20TD%20Lamb,%20Eye%202016.pdf); ERG confirmation in [Jiang & Mahroo, PMC9796346](https://pmc.ncbi.nlm.nih.gov/articles/PMC9796346/)). Light adaptation (dark→bright) is far faster — cone response speed and gain re-set within ~100 ms to seconds ([Rider et al. 2019](https://journals.plos.org/plosone/article/file?id=10.1371/journal.pone.0220358&type=printable)).

**The crucial distinction the anchor conflates:** the 9–12 log-unit figure is the range the eye covers *by adapting over time*. The range simultaneously present and discriminable in one adapted state is ~**3.7 log units** under favorable conditions (literature range 2–4), and it shrinks with stimulus duration and low background ([A reassessment of the simultaneous dynamic range of the human visual system, ACM](https://dl.acm.org/doi/10.1145/1836248.1836251)). That number — not 12 — is the correct design target for how much scene contrast a display must reproduce at once.

### 5.1.3 The mapping table

| Eye property | Value | Rendering concept it corresponds to |
|---|---|---|
| Photoreceptor adaptation (gain vs. ambient) | 5+ log units per receptor, Weber-law | Auto-exposure (scene luminance → exposure key) |
| Spatial pooling of the adaptation signal (retina, ~6° foveal pool) | [Vangorp et al. 2015](https://www.cl.cam.ac.uk/~rkm38/pdfs/vangorp2015local_adapt.pdf) | Local tone mapping / metering region |
| Pupil (8→2.5 mm) | 1.01 log units | Aperture — affects *DOF and glare* more than exposure range |
| Rod/cone duplex switch + slow rod recovery | 5–8 min cones, ~40 min rods | Nothing — game auto-exposure runs at f-stops/second, i.e. ~1000× faster than rod dark adaptation |
| Weber fraction (contrast constancy) | 0.02–0.03 photopic | Tone curve slope in log-log space (see §5.7a) |
| Photoreceptor temporal integration | continuous, ~10–50 ms effective | *Not* frame accumulation — the eye has no frames (§5.6) |
| Bleaching afterimages (opsin-equivalent background) | seconds–minutes decay | Nothing standard; HDR "flashblindness" effects are artistic imitations |

### 5.1.4 What shipped engines actually do (verified from engine docs)

- **Unreal Engine** auto-exposure ("eye adaptation") offers **Histogram** (64-bin log-luminance histogram; discards pixels below Low Percent and above High Percent, defaults 10/90 since UE 4.25, previously 80/98.3) and **Basic** (average of log luminance) metering, converging the average to **18% grey** since 4.25, plus a **manual** mode with physical ISO/aperture/shutter, an artist **Exposure Metering Mask** (center-weighted screen texture), and adaptation **Speed Up/Down in f-stops per second** ([Epic: Auto Exposure in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine?application_version=4.27); [Epic tech blog on 4.25](https://www.unrealengine.com/en-US/tech-blog/how-epic-games-is-handling-auto-exposure-in-4-25)).
- **Unity** post-processing Auto Exposure: per-frame **compute-shader histogram**, filtering range (default 50–95%), min/max luminance in EV, "Key Value" exposure bias, Progressive/Fixed adaptation with default **speedUp 2 / speedDown 1** (exponential units per second) ([Unity Manual: Eye Adaptation](https://docs.unity3d.com/560/Documentation/Manual/PostProcessing-EyeAdaptation.html); [Unity FPSSample AutoExposure.cs](https://github.com/Unity-Technologies/FPSSample/blob/master/Packages/com.unity.postprocessing/PostProcessing/Runtime/Effects/AutoExposure.cs)).

So the shipped state of the art is: log-luminance histogram → percentile-windowed average → exponential adaptation over ~0.5–2 s. Compare the eye: local (not global) adaptation over a ~6° pool ([Vangorp et al. 2015](https://www.cl.cam.ac.uk/~rkm38/pdfs/vangorp2015local_adapt.pdf)), asymmetric speeds (light adaptation in ~100 ms, rod dark adaptation in tens of minutes). Games deliberately run a *camera-feel* timescale; a faithful eye simulation would separate a fast cone-like channel (sub-second) from a slow rod-like channel (minutes) — almost no game does (HDR "night eye" modes are artistic).

### 5.1.5 HDR display standards vs. the eye's range

SDR reference white is ~100 nits (sRGB/BT.1886 mastering); BT.2100 defines **HDR Reference White at 203 cd/m²** on a 1000 cd/m² PQ or HLG display ([ITU-R BT.2100-3](https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-3-202502-I!!PDF-E.pdf)). HDR10 is PQ (SMPTE ST 2084) + BT.2020 + 10-bit + static metadata (SMPTE ST 2086 mastering-display limits, MaxCLL/MaxFALL); PQ's container ceiling is **10 000 nits**, but "common HDR10 contents are mastered with peak brightness from 1 000 to 4 000 nits" ([HDR10, Wikipedia](https://en.wikipedia.org/wiki/HDR10); [HDR10+ whitepaper](https://hdr10plus.org/wp-content/uploads/2023/11/HDR10_WhitePaper.pdf); [Microsoft DXGI_HDR_METADATA_HDR10](https://github.com/MicrosoftDocs/sdk-api/blob/docs/sdk-api-src/content/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10.md)). Verified: the full PQ container spans 10 000/0.005 ≈ 6.3 log units — *more* than the eye's simultaneous 3.7-log-unit discriminable range, and *less* than the eye's 9-log-unit adaptational range. The engineering conclusion is already implicit in the standards: HDR displays need not reproduce the eye's full adaptational range, only exceed its simultaneous range, because the viewer adapts to the display.

---

## 5.2 Glare, bloom, and scatter in rendering vs. reality

### 5.2.1 Real veiling luminance (the perception side)

Disability glare is intraocular forward scatter; its perceptual effect is an *equivalent veiling luminance* added uniformly across the retinal image, reducing contrast. The classical Stiles–Holladay relation, valid for **1° < θ < 30°** and young eyes:

$$\boxed{L_v = \frac{10\,E}{\theta^2}\ \mathrm{cd/m^2} \quad (E \text{ in lux at the cornea},\ \theta \text{ in degrees})}$$

The CIE age-adjusted version multiplies the constant by $1+(A/70)^4$; the CIE small-angle equation ($10/\theta^3 + \dots$, 0.1°–30°) and general equation (adds pigmentation $p$ and large-angle terms, 0.1°–100°) extend the domain ([Vos, *On the cause of disability glare*, 2003](https://doi.org/10.1111/j.1444-0938.2003.tb03080.x); [CIE 146/147:2002](https://doi.org/10.25039/tr.146/147.2002); [van den Berg straylight review](https://www.sciencedirect.com/science/article/pii/S0939388912001420); [disability-glare measurement review](https://onlinelibrary.wiley.com/doi/full/10.1111/j.1600-0420.2006.00860.x)). Worked example (verified): oncoming headlight delivering 1 lux at the cornea, 5° off-axis, 30-year-old eye: $L_v = 10/25 = 0.4$ cd/m² — enough to lift a 4 cd/m² road-surface contrast by 10%; at 70 years the same geometry at 7° gives 0.41 cd/m², i.e. aging roughly doubles scatter at moderate angles (the $1+(A/70)^4$ factor is 2 at $A=70$... rising steeply beyond 60). **The angular exponent is $\theta^{-2}$ only in the mid-range**: the full CIE glare function shows $\theta^{-3}$ behavior below ~1° and a *flatter* fall-off beyond ~10° due to fundus and iris/sclera trans-illumination in lightly pigmented eyes ([van den Berg review](https://www.sciencedirect.com/science/article/pii/S0939388912001420)) — a game glare model using a single $1/\theta^2$ falloff is wrong in both tails.

### 5.2.2 What game bloom actually simulates

Say it explicitly: **mainstream game bloom is a camera-lens conceit, not an eye model.** The physically-based lens-flare literature renders *ghosts* as internal reflections between the elements of a multi-element *camera* lens (two-bounce paths, Fresnel intensities ~4%/surface, anti-reflection-coating spectra) and *starbursts* as aperture diffraction ([Hullin et al., *Physically-Based Real-Time Lens-Flare Rendering*; implementation notes by Hennessy](https://placeholderart.wordpress.com/2015/01/19/implementation-notes-physically-based-lens-flares/); [jpgrenier/bitsquid implementation](http://bitsquid.blogspot.com/2017/07/physically-based-lens-flare.html); [polynomial-optics follow-up, The Visual Computer 2024](https://link.springer.com/article/10.1007/s00371-024-03625-7); a shipped ray-traced version in [Capcom's RE Engine 2023 slides](https://convert.docswell.com/s/CAPCOM_RandD/ZNR832-RE2023)). The eye has one lens and no aperture-blade ghosts — it *cannot* produce camera lens flare.

The eye's own scatter artifacts are different and well documented: **bloom/veiling luminance** from cornea+lens+retina scatter (roughly equal contributions), the **ciliary corona** (radial streaks from refractive-index fluctuations in the ocular media) and the **lenticular halo** (colored concentric rings from radial lens-fiber diffraction, *constant apparent size independent of source distance*) ([Spencer, Shirley, Zimmerman & Greenberg, *Physically-based glare effects for digital images*, SIGGRAPH '95](http://luthuli.cs.uiuc.edu/~daf/courses/rendering/papers3/spencer95.pdf)). So the premise "the eye never sees its own lens flare" is exactly backwards for *eye* glare — glare IS the eye seeing its own scatter — but correct in the specific sense that the eye never sees *camera* ghost patterns, and viewers nonetheless accept them in games because a century of photography has trained the expectation "bright source ⇒ streaks and ghosts" ([PC Gamer on motion blur and camera effects](https://www.pcgamer.com/why-people-hate-motion-blur-in-videogames/)).

### 5.2.3 The glare-rendering research line

- **Spencer et al. 1995**: psychophysically-parameterized digital PSF (four filter components f₀–f₃ from vision-science fits) convolved with HDR pixels; perceptual test showed added glare *increases apparent brightness* of sources on LDR displays ([SIGGRAPH '95](http://luthuli.cs.uiuc.edu/~daf/courses/rendering/papers3/spencer95.pdf)).
- **Ritschel et al., *Temporal Glare* (EG 2009)**: wave-optics (Fourier/Fresnel) simulation of scatter from a dynamic anatomical eye model on the GPU; glare *flickers* as lens particles and the eye move, and psychophysics confirmed dynamic glare increases perceived brightness/attractiveness ([DTU PDF](https://people.compute.dtu.dk/jerf/papers/TemporalGlare.pdf); [Wiley CGF](https://onlinelibrary.wiley.com/doi/10.1111/j.1467-8659.2009.01357.x)). Related: [Yoshida/± line and perception studies cited therein](https://people.compute.dtu.dk/jerf/papers/TemporalGlare.pdf).
- **Sun & Baranoski 2025**: Fresnel-diffraction glare models break down at night — the dilated pupil invalidates the small-pupil approximations; they use Rayleigh–Sommerfeld diffraction plus photoreceptor thresholds for day-and-night starburst depiction ([ACM](https://doi.org/10.1145/3763356)). **This is why night halos look bigger:** a dilated pupil (7–8 mm vs 2–3 mm) both passes more scattered light and changes the diffraction regime of the lens-fiber structures.
- **Luidolt et al. 2020**: gaze-dependent light perception in VR — glare rendered as a function of *measured gaze* so that the displayed glare behaves like the viewer's own ([IEEE TVCG](https://doi.org/10.1109/tvcg.2020.3023604)).

A cited psychophysical result worth keeping: convolving high-intensity pixels with even *simple* glare filters raises the impression of displayed brightness "by over 20%" ([Yoshida/Ishida line quoted in Temporal Glare](https://people.compute.dtu.dk/jerf/papers/TemporalGlare.pdf)) — bloom is a cheap dynamic-range *illusion amplifier*, which is why it survives in every engine.

---

## 5.3 Motion blur: why the eye doesn't need it (and when it does)

### 5.3.1 Saccades: the smear you never see

Saccades reach 50–500°/s for typical 1–20° amplitudes (durations 20–50 ms; hard ceiling ~100 ms) ([Bahill et al., via Martinsons CIE 2025](https://files.cie.co.at/x051_2025/P0685_ID286ChristopheMARTINSONS_OA_with_cover.pdf)) — up to ~700–900°/s for the largest excursions (cross-ref movement-doc Part 2). The retinal image is genuinely smeared by hundreds of receptor widths, yet perception is stable: **saccadic omission**. The classical account (Campbell & Wurtz 1978) is pre/post-saccadic masking plus central suppression (threshold elevation ~0.5–1 log unit around saccade onset — cross-ref movement-doc Part 3). The modern refinement matters for rendering: replaying the exact retinal stimulus of a saccade during *fixation* reproduces the smear percept almost identically, and "no extra-retinal process was needed" — omission largely falls out of the smear's own low-spatial-frequency content plus temporal integration and post-saccadic masking ([*Saccadic omission revisited*, bioRxiv 2023](https://www.biorxiv.org/content/10.1101/2023.03.15.532538v1)). Intra-saccadic perception is even functional at the right retinal temporal frequencies (15–25 Hz) via the magnocellular pathway ([Sci Rep 2026](https://www.nature.com/articles/s41598-026-39420-8)).

**Implication:** the eye *does* "motion-blur" its own retinal image constantly and deletes it. A game camera that adds motion blur when the *player* turns the view is simulating a camera exposure the player's visual system would have suppressed — the standard player-side complaint, and the reason the gamedev community proposes "saccadic masking" heuristics (drop camera-motion blur, keep object blur) ([GameDev.net thread](https://gamedev.net/forums/topic/714357-how-to-properly-cancel-camera-motion-blur/); [PC Gamer](https://www.pcgamer.com/why-people-hate-motion-blur-in-videogames/)).

### 5.3.2 The smooth-pursuit exception — background smear is real and mostly unnoticeable

During pursuit, the tracked object is stabilized near the fovea while the **background smears on the retina at the pursuit speed** — and humans do perceive that smear, but *less than an equivalent fixation-condition smear*: measured blur extents 10.8 vs 12.0 arcmin (4°/s) and 18.6 vs 20.7 arcmin (8°/s), i.e. perceived blur durations of 44–58 ms ([Bedell et al., *Motion Deblurring During Pursuit Tracking Improves Spatial-Interval Acuity*, PMC3637418](https://pmc.ncbi.nlm.nih.gov/articles/PMC3637418/)). Pursuit compensation for the background is real but *incomplete* and uses a separate, recalibratable reference signal from the one that stabilizes target perception ([Lindner, Haarmeier & Thier, J Vis 2001](https://doi.org/10.1167/1.3.22); [Schütz et al. review, *And yet it moves*](https://www.sciencedirect.com/science/article/pii/S014976341100100X)). So in a first-person game the *correct* eye-simulation would keep the pursuit target (crosshair/enemy) razor sharp and smear the world only when the player is tracking — which is exactly the behavior players demand by turning global motion blur off. The display itself adds the smeared background for free: on a sample-and-hold display, eye tracking across persistent frames produces genuine retinal smear (§5.6).

### 5.3.3 The shutter-angle model, and why games adopted it

Cinematic motion blur is an exposure artifact: a 180° shutter at 24 fps integrates for $t = \frac{180/360}{24} = 20.8$ ms (verified; 8.33 ms at 60 fps). Film *needs* it — 24 discrete sharp frames per second strobe violently (the King Kong stop-motion jerks Glassner diagnosed) — and games inherited both the math and the aesthetic. The empirical player-experience evidence is thin: a controlled MIT/Disney study of *Split/Second* found simulated motion blur had **no significant effect on enjoyment, satisfaction, focus, performance, or perceived speed** — it was detectable, nothing more ([Sharan, Neo, Mitchell & Hodgins, Disney Research PDF](https://la.disneyresearch.com/wp-content/uploads/Presence-of-Motion-Blur-Effect-Does-Not-Improve-Gaming-Experience-Paper.pdf)). Its remaining engineering justification is artifact masking at low frame rates (≤30 fps temporal aliasing/strobing; [GPU Gems 3 via PC Gamer](https://www.pcgamer.com/why-people-hate-motion-blur-in-videogames/)) and, per Blur Busters, masking *stroboscopic* artifacts at any frame rate — for some users GPU motion blur is an "ergonomic/assistive feature" against step-artifact eyestrain ([Blur Busters: stroboscopic effect](https://blurbusters.com/the-stroboscopic-effect-of-finite-framerate-displays/)).

### 5.3.4 VR: motion blur off, persistence low, refresh high

VR engines disable simulated motion blur. The reasoning chain, from the VR-comfort literature: (1) the display is head-tracked, so camera-turn smear should be suppressed like saccadic omission; (2) users *pursuit-track* virtual objects, so the sharp-target/smearing-background split of §5.3.2 must come from the display, not a blur post-process; (3) display-generated smear is attacked at the hardware level instead — low-persistence strobing (Rift CV1 ~2 ms, Vive ~1.9 ms/90 Hz, Vive Pro 2 0.42 ms/120 Hz ≈ 5% duty cycle) with **<20% duty cycle** the measured threshold for effective blur mitigation during pursuit ([Zhao et al., *Spatiotemporal image quality of VR HMDs*, Sci Rep / PMC9691731](https://pmc.ncbi.nlm.nih.gov/articles/PMC9691731/); [VRARWiki: refresh rate](https://vrarwiki.com/wiki/Refresh_rate)); (4) 90 Hz+ and <20 ms motion-to-photon latency are the comfort floor — judder and vection-conflict sickness rise sharply below it ([VRARWiki: latency](https://vrarwiki.com/wiki/Latency); [Stauffert et al. latency review, Frontiers](https://www.frontiersin.org/articles/10.3389/frvir.2020.582204/pdf)). Frame-rate below refresh produces replicated shadow images — visible double images under pursuit, the VR form of the phantom array ([Zhao et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC9691731/)).

---

## 5.4 Peripheral degradation and LOD from vision

Cross-ref: movement-doc Part 3 §3.1 established the acuity falloff (MAR linear in eccentricity, $E_2 \approx 1.0$–2.6°, 20/116 at 10°) and §3.7 sketched foveated rendering. This section is the rendering-side depth.

**The founding result.** Guenter et al. 2012 render three nested eccentricity layers with per-layer sampling rates following the Aubert–Foerster linear model $\omega = \omega_0 + m\,e$, with user-study-fitted **m = 1.32–1.65 arcmin per degree of eccentricity**, achieving 5–6× speedup / 10–15× fewer shaded pixels on 1080p with latency under ~10–20 ms required to avoid foveal "pop" ([Guenter et al., *Foveated 3D Graphics*, ACM TOG 2012](https://doi.org/10.1145/2366145.2366183)).

**Detectability thresholds — the numbers a renderer may rely on:**

- **Contrast, not just resolution, is the limiting factor.** Filtering the periphery reduces contrast and induces "tunnel vision"; a post-process *contrast enhancement* let subjects tolerate **2× larger blur radius** before detecting foveation, and the production system shades up to **70% fewer samples**, coarsening shading up to **30° closer to the fovea than Guenter et al.** without perceivable aliasing/blur ([Patney et al., *Towards Foveated Rendering for Gaze-Tracked VR*, SIGGRAPH Asia 2016](https://anjulpatney.com/docs/papers/2016_Patney_TFR.pdf)).
- **Latency tolerance is looser than feared:** no significant loss of acceptable foveation for added eye-tracking latencies of 20–40 ms; a significant loss at 80–150 ms — total eye-to-image latency of **50–70 ms is tolerable** ([Albert et al., *Latency Requirements for Foveated Rendering in VR*, ACM TAP 2017](https://research.nvidia.com/sites/default/files/pubs/2017-09_Latency-Requirements-for/a25-albert.pdf)). (Meta's shipped Quest Pro ETFR pipeline reports 46–57 ms end-to-end — inside this envelope.)
- **Fixed-foveation detectability:** subjects "barely notice" foveation at eccentricity ≥7.5° with peripheral resolution ≥540p ([Hsu et al. 2017](https://people.cs.nycu.edu.tw/~chuang/pubs/pdf/2017mm.pdf)).
- **Stereoacuity survives foveation:** peripheral blur up to ~15–26.6 arcmin Gaussian σ at 10–20° eccentricity leaves stereoscopic depth thresholds unaffected — roughly **2× stronger foveation than common practice** is safe for depth ([*Towards Understanding Depth Perception in Foveated Rendering*, arXiv 2501.18635](https://arxiv.org/html/2501.18635)).
- Other quality axes (SSAO sample count, tessellation) can be foveated with measurable gains, though with user- and scene-dependence ([Swafford et al. 2016](https://doi.org/10.1145/2931002.2931011)); gaze-contingent *depth-of-field* filtering conceals foveation artifacts so well that foveated+DoF rates "almost on par with full rendering" at >69% sample reduction ([Weier et al., *Foveated Depth-of-Field Filtering in HMDs*, ACM TAP 2018](https://dl.acm.org/doi/10.1145/3238301)).

**Fixed vs. eye-tracked, shipped examples (verified):** Meta Quest 2/3 ship **Fixed Foveated Rendering** (full density center, low-density periphery — works because gaze is usually near screen center); **Quest Pro** ships eye-tracked ETFR via the Vulkan `VK_QCOM_fragment_density_map_offset` tile-offset extension, with automatic fallback to Dynamic-High FFR when eye tracking is denied, and Red Matter 2 used it for a 33% pixel-density increase ([Meta Horizon developers blog](https://developers.meta.com/horizon/blog/save-gpu-with-eye-tracked-foveated-rendering/); [Meta ETFR docs](https://developers.meta.com/horizon/documentation/unreal/unreal-eye-tracked-foveated-rendering/)). **PSVR2** ships per-eye IR eye tracking with a PS5-specific hardware foveated-rendering feature for "smooth resolution changes across the display" ([PS VR2 tech specs](https://www.playstation.com/en-ie/ps-vr2/ps-vr2-tech-specs/); [Sony R&D STEF2022](https://www.sony.com/en/SonyInfo/technology/activities/STEF2022/exhibition_0302/02/); [PlayStation FAQ](https://blog.playstation.com/2023/02/06/playstation-vr2-the-ultimate-faq/)). The tradeoff space: FFR is free but must stay conservative (gaze ≠ center); ETFR is aggressive but adds an eye tracker, ~50 ms pipeline latency budget, and a privacy/permission surface.

**Peripheral defocus and HMD fatigue:** fixed-focus HMD optics leave the periphery optically sharp at the accommodation plane while vergence demands vary — the discomfort channel is the vergence–accommodation conflict treated in §5.5, not blur per se; DoF *simulation* has been measured to reduce visual fatigue in HMDs ([Vinnikov & Allison, via Weier et al.](https://dl.acm.org/doi/10.1145/3238301)).

---

## 5.5 Focus, depth of field, and accommodation in games

**Real DOF.** The eye's depth of focus is ~±0.25–0.3 D at a 3 mm pupil (cross-ref Part 1's optics treatment; the pupil dependence is the standard one — smaller pupils widen the depth of focus geometrically). Real depth of *field* in the world then follows from the dioptric distance: at 2 m focus, ±0.25 D spans roughly 1.75–2.3 m.

**Game DOF** is an artistic camera imitation: pick a focal distance and f-stop, compute a circle of confusion, blur by depth. Two things make it non-ocular: (1) the eye's accommodation is *reflexive and fast* (vergence-accommodation loops run in ~300–500 ms; the eye is almost never looking at a blurred region voluntarily), so "cinematic" wide-aperture bokeh reads as *camera*, never as eye; (2) without gaze tracking the focus distance is guessed (usually screen center or the crosshair), which fights the player's actual attention — the exact failure mode of global motion blur (§5.3).

**The vergence–accommodation conflict (VAC).** A stereoscopic HMD renders disparity-driven vergence at the virtual object's distance while the optics fix accommodation at one plane. Verified geometry (local computation, 63 mm IPD): total convergence is 7.2° at 0.5 m, 3.6° at 1 m, 1.8° at 2 m — while typical HMD focal distances are **1.5 m (HTC Vive Pro) and 2 m (HoloLens)** ([Hussain et al., IEEE TVCG 2023](https://doi.org/10.1109/tvcg.2023.3331902)). Content within ~1 m therefore sits well outside the (±0.25–0.3 D ≈ ±0.1–0.15 m at 1.5 m) Percival zone. The standard account: VAC "hinders visual performance and causes visual fatigue" (Hoffman, Girshick, Akeley & Banks 2008, the canonical reference cited throughout the literature). **The critics, reported:** a 30-minute controlled VAC study found SSQ symptom increases after large-VAC VR play *but no measurable changes in phoria, accommodative lag, fusional vergence, or NPC* — concluding short-term symptoms were "probably related to inappropriate oculo-vestibular relationship," not VAC ([Dymczyk et al., J Optom 2024 / PMC11585873](https://pmc.ncbi.nlm.nih.gov/articles/PMC11585873/)); a reaching study found **no impairment of visually guided movement** from introduced VAC over its durations ([McAnally, Wallis & Grove, Displays 2024](https://doi.org/10.1016/j.displa.2024.102668)); reducing vergence demand ("quasi-3D") cut convergence discomfort ~30% and cybersickness ~12% ([Springer, Virtual Reality 2024](https://link.springer.com/article/10.1007/s10055-023-00923-8)). Honest summary: VAC discomfort is real in aggregate and in strong-vergence near-field tasks; its magnitude at typical >1 m content distances and 30-min exposures is contested.

**Why flat displays can't drive accommodation, and the hardware attempts.** A fixed focal surface presents one accommodation plane, period. The verified varifocal lineage: **Half Dome 1** (2018) — mechanically actuated displays moving on eye tracking, 140° FOV; **Half Dome 2** — smaller/lighter, narrower FOV; **Half Dome 3** (2019) — no moving parts, stacked polarization-dependent liquid-crystal lenses + switchable half-wave plates, 6 LC lens pairs sweeping **64 focal planes** ([Meta/FRL Half Dome updates](https://www.meta.com/blog/half-dome-updates-frl-explores-more-comfortable-compact-vr-prototypes-for-work/); [UploadVR coverage](https://www.uploadvr.com/oc6-half-dome-2-prototype/)); **Butterscotch Varifocal** (2023) — retinal resolution (up to **56 PPD** ≈ 20/20), 0–4 D accommodation range (infinity to 25 cm), matching accommodation dynamics of **≥10 D/s peak velocity, 100 D/s² acceleration** ([Zhao et al., *Retinal-Resolution Varifocal VR*, SIGGRAPH 2023](https://doi.org/10.1145/3588037.3595389); [Road to VR](https://roadtovr.com/meta-prototype-vr-retinal-resoltion-light-field-passthrough/)). Status: all research prototypes; none shipped. Software mitigation that *is* citable: inverse-blurring (Wiener deconvolution approximating the retinal PSF of mis-accommodation) improved depth-judgment tasks 36–48% without an eye tracker ([Hussain et al. 2023](https://doi.org/10.1109/tvcg.2023.3331902)).

---

## 5.6 Temporal rendering vs. the eye's temporal response

**CFF and the 60 Hz myth.** Critical flicker fusion is 50–90 Hz depending on luminance, size, wavelength, eccentricity, and adaptation (cross-ref movement-doc Part 3 §3.4 for the full citation set: [Mankowska et al. 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8537539/), [Sehl & Kohl 2025](https://link.springer.com/article/10.1007/s00421-025-05935-7), [Veridiano et al. 2024](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0298007)). "60 fps looks smooth" is true only of *flicker fusion for moderate foveal stimuli under fixation*. Everything that involves the eye *moving* persists far beyond it:

- **The phantom array** — multiple discrete images of a small source seen during a saccade across a temporally-modulated display — is visible from ~100 Hz up to **several kHz**. Measured: peak sensitivity ~640 Hz at 15% modulation depth for a 400°/s saccade; sensitive subjects and tiny sources extend to 2.6 kHz–15 kHz ([Martinsons, CIE x051 2025](https://files.cie.co.at/x051_2025/P0685_ID286ChristopheMARTINSONS_OA_with_cover.pdf); [IOP 2025 analytic model](https://iopscience.iop.org/article/10.1088/1742-6596/3128/1/012002); [Kim/Kang-line saccade-speed correlation, Sci Rep 2023](https://www.nature.com/articles/s41598-023-38477-z)). Crucially, **saccadic suppression does not suppress the phantom array** — it is precisely the artifact that escapes the eye's own blur-deletion mechanism (§5.3.1).
- **Sample-and-hold vs. impulse.** On a hold-type display, persistence blur under eye tracking equals the frame time: **MPRT (motion picture response time)** is defined as the effective pixel-visibility time, measured as the average extended blurred-edge time of a moving edge with a pursuit camera, and "for the scientifically ideal instant-response sample-and-hold display, MPRT is exactly equal to the time period of one refresh cycle" ([SID/Information Display MPRT article](http://archive.informationdisplay.org/id-archive/2005/october/moving-picture-response-time-and-perceived-motion); [Kurita, SID J.](https://sid.onlinelibrary.wiley.com/doi/10.1889/1.2918077); [TestUFO MPRT](https://testufo.com/mprt)). Verified values: 60 Hz → 16.67 ms, 90 → 11.11, 120 → 8.33, 144 → 6.94, 240 → 4.17 ms. "1 ms MPRT" marketing requires backlight strobing — without it you'd need ~1000 fps at 1000 Hz ([TFTCentral](https://tftcentral.co.uk/articles/why-moving-picture-response-time-mprt-specs-can-be-misleading-and-where-1ms-mprt-is-sometimes-abused)).
- **The fundamental tradeoff:** you cannot simultaneously eliminate hold-type blur (needs shorter persistence) and stroboscopic stepping (needs more frames) at any commercially achievable rate; Blur Busters' summary of the research consensus is that "retina refresh rates are well beyond 1000 Hz" ([Blur Busters](https://blurbusters.com/the-stroboscopic-effect-of-finite-framerate-displays/)).

**Why VR needs 90 Hz+:** motion-to-photon latency <20 ms, judder from dropped frames (Carmack: "dropping a frame in VR is a bad thing"), low persistence <20% duty cycle to kill pursuit smear, and vection-conflict sickness which falls ~half from 60→120 Hz in controlled studies ([VRARWiki refresh rate](https://vrarwiki.com/wiki/Refresh_rate), [latency](https://vrarwiki.com/wiki/Latency); [Zhao et al. 2022](https://pmc.ncbi.nlm.nih.gov/articles/PMC9691731/); [Stauffert et al.](https://www.frontiersin.org/articles/10.3389/frvir.2020.582204/pdf)).

**TAA vs. the eye — plainly.** Temporal antialiasing reprojects and accumulates color from previous discrete frames, jittering the sample position per frame; its softness and ghosting are reconstruction artifacts of that pipeline ([Digital Foundry TAA deep-dive](https://www.digitalfoundry.net/articles/digitalfoundry-2024-temporal-anti-aliasing-a-blessing-or-a-curse); [The Code Corsair: Temporal AA and the Quest for the Holy Trail](https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/); [k-DOP clipping paper, ACM 2024](https://doi.org/10.1145/3681758.3697996)). **Nothing in human vision corresponds to this.** The retina integrates continuously — no frames, no jittered sub-pixel offsets, no history buffer — and photoreceptor integration (~10–50 ms effective, luminance-dependent: rods slower, cones faster — §5.1.2) is a causal temporal filter, not a reprojected sample cache. TAA's motion blur-on-camera-move is therefore a *second, spurious* blur layered on top of the display's genuine persistence smear (MPRT), which is itself already the correct physical analog of the eye's retinal smear. The one defensible perceptual mapping: TAA's ghosting is a temporal alias that the visual system detects the same way it detects the phantom array — as discreteness in time that real-world motion never contains.

---

## 5.7 What the renderer should actually do (engineering recommendation — marked as recommendation)

For this engine (custom Diligent D3D12/Vulkan renderer, TAA + bloom + tonemapping already present), prioritized and tied to the numbers above. *This section is recommendation, not reported science.*

**(a) Tonemap curve — keep ACES/filmic, know what it is.** The ACES Output Transform's stated rationale is *not* a Weber-law imitation: the RRT+ODT splines were "derived through visual testing on a large test set of images … based on feedback from expert viewers" — an empirically-tuned preferred-reproduction curve for an idealized cinema device, with ACES 2 moving to a Hellwig-CAM-derived JMh lightness space for hue preservation ([sdyer on ACESCentral](https://community.acescentral.com/t/odt-tonescale/387); [ACES docs: Output Transforms](https://docs.acescentral.com/system-components/output-transforms/); [ACES 2 rendering](https://docs.acescentral.com/background/about-rendering/)). If you want the *eye's* curve, it is the Naka–Rushton photoreceptor response $V = I/(\sigma(I_a)+I^m)$ or Reinhard's Hood-derived variant — explicitly "inspired by photoreceptor adaptation," *not* the filmic curve ([Reinhard & Devlin 2005](https://pages.cs.wisc.edu/~lizhang/courses/cs766-2012f/projects/hdr/Reinhard2005DRR.pdf); [Reinhard et al. 2002](https://doi.org/10.1145/566654.566575); [Meylan et al. 2007](https://doi.org/10.1364/josaa.24.002807)). Recommendation: ACES for the camera-look default; the slope of any curve near middle grey should keep local log-contrast ≥ the Weber fraction (0.02–0.03) so rendered contrasts stay supra-threshold ([Ferradans et al. 2009](http://hdl.handle.net/11299/180329) does exactly this analysis).

**(b) Auto-exposure → local, fovea-weighted key.** Replace/augment the global histogram average with a metering region (UE's Exposure Metering Mask pattern, [Epic docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine?application_version=4.27)) centered on the crosshair — a poor-man's stand-in for the fovea, since ~2° of detail vision rides there (movement-doc Part 3 §3.1). Justification: the adaptation state that matters perceptually is pooled over ~6° around fixation ([Vangorp et al. 2015](https://www.cl.cam.ac.uk/~rkm38/pdfs/vangorp2015local_adapt.pdf)); a bright sky at screen top shouldn't crush a dark room the player is looking into. Keep two-timescale adaptation (fast up, slow down) as engines do — it accidentally mimics the light-adaptation/dark-adaptation asymmetry, at ~1000× speed.

**(c) Bloom threshold/shape — make it veiling-luminance-aware.** Game bloom should behave like $L_v = 10E/\theta^2$: intensity ∝ source luminance (not a fixed threshold), spatial extent growing as the eye's PSF, and *stronger in dark-adapted scenes* (night halos are bigger — dilated pupil, §5.2.2; [Sun & Baranoski 2025](https://doi.org/10.1145/3763356)). Concretely: scale bloom energy with the *ratio* of source luminance to adaptation luminance so a torch in a cave blooms like the sun outdoors, and widen the kernel as exposure (adaptation) drops. Even the simple Spencer-style filters measurably increase perceived brightness ~20% ([Temporal Glare](https://people.compute.dtu.dk/jerf/papers/TemporalGlare.pdf)) — this is dynamic range you get for free. Do not render camera lens ghosts in first-person "eye" mode; they are a camera tell (§5.2.2).

**(d) Peripheral LOD/mips from the CSF falloff.** The safe numbers: coarsening beyond ~7.5° eccentricity at ≥540p-equivalent is barely noticeable ([Hsu 2017](https://people.cs.nycu.edu.tw/~chuang/pubs/pdf/2017mm.pdf)); contrast-preserving filtering doubles the tolerable blur rate ([Patney 2016](https://anjulpatney.com/docs/papers/2016_Patney_TFR.pdf)); Guenter's m = 1.32–1.65 arcmin/deg is the slope budget. For a monitor game without eye tracking, use *fixed* foveation around the crosshair (the Quest FFR pattern) plus anisotropic mip bias toward screen edges; keep contrast (don't low-pass away edge energy) — the periphery is a motion/contrast sentinel (movement-doc Part 3 §3.2.2).

**(e) Motion blur: OFF for first-person eye-simulation purism; ON only as an optional camera-feel mode.** The reasoning is §5.3 in one line: the eye smears its retinal image during gaze shifts and *deletes it*; the display's persistence already provides the physically-correct smear during pursuit; adding a shutter-angle blur on top simulates a camera the player is not. Empirically it buys no measured player-experience benefit ([Disney/MIT study](https://la.disneyresearch.com/wp-content/uploads/Presence-of-Motion-Blur-Effect-Does-Not-Improve-Gaming-Experience-Paper.pdf)). If kept for a "cinematic" toggle, exclude camera rotation (the saccade analogue) and blur only object motion.

**(f) Vignette is a poor man's acuity falloff — and it's wrong.** Luminance sensitivity and acuity have *different* eccentricity falloffs: acuity is roughly linear in eccentricity (MAR = $\omega_0 + m e$) while contrast sensitivity falls more steeply and with a different shape (movement-doc Part 3 §3.1.3, CSF peak 3–8 cyc/deg photopic). A luminance vignette darkens the periphery, which the periphery is *tuned to detect* (motion/looming sentinel) — it reads as tunnel vision, not as reduced detail. If you want peripheral degradation, degrade *resolution/contrast* (d), not luminance.

**(g) Defer:** eye-tracked foveated rendering (needs HMD-class eye tracking — desktop Tobii-class hardware adds the 50–70 ms latency budget of Albert et al. but no benefit over fixed foveation for a crosshair-centered FPS view); varifocal anything (hardware does not exist in consumer form, §5.5); physically-based glare (Spencer/Ritschel-class) — a validated, cheap approximation per (c) covers the perceptual win.

---

## 5.8 Consolidated summary table + primary sources + Appendix A

### 5.8.1 Summary table

| Quantity | Value | Source / section |
|---|---|---|
| Eye's adaptational range | ~9 log units instantaneous-adapted; 10–12 with time; **simultaneous discriminable ~3.7 log units** | Adler's; §5.1.2 |
| Pupil contribution | 8→2.5 mm = **1.01 log units** | Adler's; pwsh |
| Weber fraction (cone/rod) | **0.02–0.03** / 0.14 (S-cones 0.09) | Webvision; §5.1.2 |
| Rod saturation onset | ~2–3 log scotopic trolands (≈ conventional 1–10 cd/m² luminance) | Stockman mesopic review; §5.1.2 |
| Dark adaptation | cone plateau 5–8 min; rod–cone break 7–10 min; threshold ~10⁻⁵ cd/m² at ~40 min; S2 slope 0.24 log/min | Webvision; Lamb; §5.1.2 |
| Veiling luminance | $L_v = 10(1+(A/70)^4)E/\theta^2$, valid 1–30°; $\theta^{-3}$ below 1°, flatter beyond 10° | Vos 2003; van den Berg; §5.2.1 |
| Saccade speeds | 50–500°/s typical (1–20°), ≤100 ms | Bahill via Martinsons; §5.3.1 |
| Pursuit background blur | real but reduced: 44–58 ms perceived duration vs fixation | Bedell; §5.3.2 |
| 180° shutter | 20.8 ms @24 fps; 8.33 ms @60 fps | pwsh; §5.3.3 |
| Foveation thresholds | m = 1.32–1.65 arcmin/deg; ≥7.5° ecc @540p barely noticed; contrast-enhance → 2× blur tolerance; 50–70 ms latency OK; stereo safe to 2× common practice | Guenter; Hsu; Patney; Albert; arXiv 2501.18635; §5.4 |
| VAC geometry | convergence 7.2° @0.5 m vs fixed 1.5–2 m HMD focus; depth of focus ±0.25–0.3 D | pwsh; Hussain; §5.5 |
| Varifocal state of the art | HD3: 64 focal planes, no moving parts; Butterscotch: 56 PPD, 0–4 D, 10 D/s | Meta; SIGGRAPH 2023; §5.5 |
| CFF | 50–90 Hz (stimulus-dependent) | movement-doc Part 3; §5.6 |
| Phantom array | visible ~100 Hz – several kHz; peak ~640 Hz; escapes saccadic suppression | Martinsons; §5.6 |
| MPRT (sample-and-hold) | = frame time: 16.67/11.11/8.33/6.94/4.17 ms at 60/90/120/144/240 Hz | SID; TFTCentral; pwsh; §5.6 |
| VR comfort floor | ≥90 Hz, <20 ms MTP latency, <20% duty cycle persistence | VRARWiki; Zhao; §5.6 |
| HDR10 container | PQ to 10 000 nits (mastered 1 000–4 000); SDR ref ~100 nits; HDR ref white 203 nits | Wikipedia; BT.2100; §5.1.5 |
| ACES rationale | empirically-tuned preferred reproduction (expert-viewer spline fits), *not* a Weber/Naka–Rushton model | ACESCentral; Reinhard; §5.7a |

### 5.8.2 Primary sources (all verified live this session)

1. Adler's Physiology of the Eye (via Clinical Tree), *Light Adaptation in Photoreceptors* — https://clinicalpub.com/light-adaptation-in-photoreceptors/
2. Lamb, *Why rods and cones?*, Eye 30 (2016) — https://cgvr.cs.uni-bremen.de/teaching/cg_literatur/Why%20rods%20and%20cones%20-%20TD%20Lamb,%20Eye%202016.pdf
3. Webvision (Kalloniatis & Luu), *Light and Dark Adaptation* — https://www.ncbi.nlm.nih.gov/books/NBK11525/
4. Cornsweet & Pinsker, *Luminance discrimination of brief flashes* (1964) — https://escholarship.org/content/qt1bg026w7/qt1bg026w7.pdf
5. Rider, Henning & Stockman, *Light adaptation controls visual sensitivity…*, PLOS ONE 2019 — https://journals.plos.org/plosone/article/file?id=10.1371/journal.pone.0220358&type=printable
6. Stockman & Sharpe, *Into the twilight zone: mesopic vision* (2006) — http://www.cvrl.org/people/Stockman/pubs/2006%20Mesopic%20review%20SS.pdf
7. *A reassessment of the simultaneous dynamic range of the human visual system*, ACM (2011) — https://dl.acm.org/doi/10.1145/1836248.1836251
8. Jiang & Mahroo, *Human retinal dark adaptation tracked in vivo with the ERG*, PMC9796346 — https://pmc.ncbi.nlm.nih.gov/articles/PMC9796346/
9. Wikipedia, *Exposure value* — https://en.wikipedia.org/wiki/Exposure_Value
10. scantips, *Camera math for EV* — https://www.scantips.com/lights/math.html
11. Epic Games, *Auto Exposure in Unreal Engine* — https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine?application_version=4.27
12. Epic Games tech blog, *How Epic Games is handling auto exposure in 4.25* — https://www.unrealengine.com/en-US/tech-blog/how-epic-games-is-handling-auto-exposure-in-4-25
13. Unity Manual, *Eye Adaptation* — https://docs.unity3d.com/560/Documentation/Manual/PostProcessing-EyeAdaptation.html
14. Unity FPSSample, `AutoExposure.cs` — https://github.com/Unity-Technologies/FPSSample/blob/master/Packages/com.unity.postprocessing/PostProcessing/Runtime/Effects/AutoExposure.cs
15. Wikipedia, *HDR10* — https://en.wikipedia.org/wiki/HDR10
16. HDR10+ Alliance whitepaper — https://hdr10plus.org/wp-content/uploads/2023/11/HDR10_WhitePaper.pdf
17. ITU-R BT.2100-3 — https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-3-202502-I!!PDF-E.pdf
18. Microsoft, `DXGI_HDR_METADATA_HDR10` — https://github.com/MicrosoftDocs/sdk-api/blob/docs/sdk-api-src/content/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10.md
19. Vos, *On the cause of disability glare…*, 2003 — https://doi.org/10.1111/j.1444-0938.2003.tb03080.x
20. CIE 146/147:2002, *CIE Collection on Glare* — https://doi.org/10.25039/tr.146/147.2002
21. van den Berg et al., *History of ocular straylight measurement* — https://www.sciencedirect.com/science/article/pii/S0939388912001420
22. *Principles of disability glare measurement* (Adamsons et al.) — https://onlinelibrary.wiley.com/doi/full/10.1111/j.1600-0420.2006.00860.x
23. Spencer, Shirley, Zimmerman & Greenberg, *Physically-based glare effects for digital images*, SIGGRAPH '95 — http://luthuli.cs.uiuc.edu/~daf/courses/rendering/papers3/spencer95.pdf
24. Ritschel et al., *Temporal Glare*, CGF 2009 — https://people.compute.dtu.dk/jerf/papers/TemporalGlare.pdf
25. Sun & Baranoski, *Glare Pattern Deposition*, ACM 2025 — https://doi.org/10.1145/3763356
26. Luidolt, Wimmer & Krösl, *Gaze-Dependent Simulation of Light Perception in VR*, IEEE TVCG 2020 — https://doi.org/10.1109/tvcg.2020.3023604
27. Hennessy, *Implementation Notes: Physically Based Lens Flares* — https://placeholderart.wordpress.com/2015/01/19/implementation-notes-physically-based-lens-flares/
28. jpgrenier/bitsquid, *Physically Based Lens Flare* — http://bitsquid.blogspot.com/2017/07/physically-based-lens-flare.html
29. *Real-time ray transfer for lens flare rendering using sparse polynomials*, The Visual Computer 2024 — https://link.springer.com/article/10.1007/s00371-024-03625-7
30. Capcom R&D, *RayTracingLensFlare* (RE 2023 slides) — https://convert.docswell.com/s/CAPCOM_RandD/ZNR832-RE2023
31. *Saccadic omission revisited* (2023 preprint) — https://www.biorxiv.org/content/10.1101/2023.03.15.532538v1
32. *Eye movement dynamics are a key factor for intra-saccadic motion perception*, Sci Rep 2026 — https://www.nature.com/articles/s41598-026-39420-8
33. Bedell et al., *Motion Deblurring During Pursuit Tracking Improves Spatial-Interval Acuity* — https://pmc.ncbi.nlm.nih.gov/articles/PMC3637418/
34. Lindner, Haarmeier & Thier, *Motion perception during smooth pursuit eye movements*, J Vis 2001 — https://doi.org/10.1167/1.3.22
35. Schütz et al., *And yet it moves: pursuit compensation review* — https://www.sciencedirect.com/science/article/pii/S014976341100100X
36. Sharan, Neo, Mitchell & Hodgins, *Simulated motion blur does not improve player experience in racing games* — https://la.disneyresearch.com/wp-content/uploads/Presence-of-Motion-Blur-Effect-Does-Not-Improve-Gaming-Experience-Paper.pdf
37. PC Gamer, *Why you're right to hate motion blur in games* — https://www.pcgamer.com/why-people-hate-motion-blur-in-videogames/
38. GameDev.net, *How to properly cancel camera motion blur* — https://gamedev.net/forums/topic/714357-how-to-properly-cancel-camera-motion-blur/
39. Blur Busters, *The stroboscopic effect of finite framerate displays* — https://blurbusters.com/the-stroboscopic-effect-of-finite-framerate-displays/
40. Martinsons, *The phantom array effect explained using simple formulas*, CIE x051 2025 — https://files.cie.co.at/x051_2025/P0685_ID286ChristopheMARTINSONS_OA_with_cover.pdf
41. *Saccadic eye movement speed is related to phantom array visibility*, Sci Rep 2023 — https://www.nature.com/articles/s41598-023-38477-z
42. *Modelling the phantom array from the spatial waveform on the retina*, J Phys Conf 2025 — https://iopscience.iop.org/article/10.1088/1742-6596/3128/1/012002
43. TestUFO, *Eye tracking motion blur* — https://testufo.com/eyetracking ; *MPRT* — https://testufo.com/mprt
44. TFTCentral, *Why MPRT specs can be misleading* — https://tftcentral.co.uk/articles/why-moving-picture-response-time-mprt-specs-can-be-misleading-and-where-1ms-mprt-is-sometimes-abused
45. SID Information Display, *Moving-Picture Response Time and Perceived Motion Blur* — http://archive.informationdisplay.org/id-archive/2005/october/moving-picture-response-time-and-perceived-motion
46. Kurita, *Motion-blur characterization on LCDs*, SID J. — https://sid.onlinelibrary.wiley.com/doi/10.1889/1.2918077
47. Guenter et al., *Foveated 3D Graphics*, ACM TOG 2012 — https://doi.org/10.1145/2366145.2366183
48. Patney et al., *Towards Foveated Rendering for Gaze-Tracked VR*, 2016 — https://anjulpatney.com/docs/papers/2016_Patney_TFR.pdf
49. Albert et al., *Latency Requirements for Foveated Rendering in VR*, ACM TAP 2017 — https://research.nvidia.com/sites/default/files/pubs/2017-09_Latency-Requirements-for/a25-albert.pdf
50. Swafford et al., *User, metric, and computational evaluation of foveated rendering methods*, 2016 — https://doi.org/10.1145/2931002.2931011
51. Hsu et al., *Is Foveated Rendering Perceivable in VR?*, 2017 — https://people.cs.nycu.edu.tw/~chuang/pubs/pdf/2017mm.pdf
52. Weier et al., *Foveated Depth-of-Field Filtering in HMDs*, ACM TAP 2018 — https://dl.acm.org/doi/10.1145/3238301
53. *Towards Understanding Depth Perception in Foveated Rendering*, arXiv 2501.18635 — https://arxiv.org/html/2501.18635
54. Meta Horizon OS Developers, *Save GPU with eye-tracked foveated rendering* — https://developers.meta.com/horizon/blog/save-gpu-with-eye-tracked-foveated-rendering/
55. Meta Horizon OS, *Eye Tracked Foveated Rendering (Unreal)* — https://developers.meta.com/horizon/documentation/unreal/unreal-eye-tracked-foveated-rendering/
56. PlayStation, *PS VR2 tech specs* — https://www.playstation.com/en-ie/ps-vr2/ps-vr2-tech-specs/
57. Sony R&D, *New rendering technology for PS VR2* (STEF2022) — https://www.sony.com/en/SonyInfo/technology/activities/STEF2022/exhibition_0302/02/
58. PlayStation Blog, *PS VR2 ultimate FAQ* — https://blog.playstation.com/2023/02/06/playstation-vr2-the-ultimate-faq/
59. Meta/FRL, *Half Dome updates* — https://www.meta.com/blog/half-dome-updates-frl-explores-more-comfortable-compact-vr-prototypes-for-work/
60. UploadVR, *New Half Dome prototypes* — https://www.uploadvr.com/oc6-half-dome-2-prototype/
61. Zhao et al., *Retinal-Resolution Varifocal VR*, SIGGRAPH 2023 — https://doi.org/10.1145/3588037.3595389
62. Road to VR, *Meta prototype VR headsets* — https://roadtovr.com/meta-prototype-vr-retinal-resoltion-light-field-passthrough/
63. Dymczyk et al., *Effect of a VAC induced during a 30-minute VR game…*, J Optom 2024 — https://pmc.ncbi.nlm.nih.gov/articles/PMC11585873/
64. McAnally, Wallis & Grove, *Visually guided movement in VR is tolerant of the VAC*, Displays 2024 — https://doi.org/10.1016/j.displa.2024.102668
65. *Quasi-3D: reducing convergence effort improves visual comfort…*, Virtual Reality 2024 — https://link.springer.com/article/10.1007/s10055-023-00923-8
66. Hussain, Chessa & Solari, *Improving Depth Perception… by Addressing VAC*, IEEE TVCG 2023 — https://doi.org/10.1109/tvcg.2023.3331902
67. Zhao et al., *Spatiotemporal image quality of VR HMDs*, Sci Rep 2022 — https://pmc.ncbi.nlm.nih.gov/articles/PMC9691731/
68. VRARWiki, *Refresh rate* — https://vrarwiki.com/wiki/Refresh_rate ; *Latency* — https://vrarwiki.com/wiki/Latency
69. Stauffert et al., *Latency and Cybersickness: A Review*, Frontiers in VR 2020 — https://www.frontiersin.org/articles/10.3389/frvir.2020.582204/pdf
70. Digital Foundry, *Temporal anti-aliasing: a blessing or a curse?* — https://www.digitalfoundry.net/articles/digitalfoundry-2024-temporal-anti-aliasing-a-blessing-or-a-curse
71. The Code Corsair, *Temporal AA and the Quest for the Holy Trail* — https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/
72. Ikkala et al., *k-DOP Clipping: Robust Ghosting Mitigation in TAA*, ACM 2024 — https://doi.org/10.1145/3681758.3697996
73. ACES Central, *ODT Tonescale* (sdyer) — https://community.acescentral.com/t/odt-tonescale/387
74. ACES Documentation, *Output Transforms* — https://docs.acescentral.com/system-components/output-transforms/
75. ACES Documentation, *ACES 2 Rendering Transform* — https://docs.acescentral.com/background/about-rendering/
76. Reinhard et al., *Photographic tone reproduction for digital images*, 2002 — https://doi.org/10.1145/566654.566575 (TR: https://www-old.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf)
77. Reinhard & Devlin, *Dynamic range reduction inspired by photoreceptor physiology*, 2005 — https://pages.cs.wisc.edu/~lizhang/courses/cs766-2012f/projects/hdr/Reinhard2005DRR.pdf
78. Meylan, Alleysson & Süsstrunk, *Model of retinal local adaptation for tone mapping*, JOSA A 2007 — https://doi.org/10.1364/josaa.24.002807
79. Ferradans et al., *An analysis of visual adaptation and contrast perception for a fast TMO*, 2009 — http://hdl.handle.net/11299/180329
80. Krawczyk, Myszkowski & Seidel, *Perceptual effects in real-time tone mapping*, 2005 — https://doi.org/10.1145/1090122.1090154
81. Mantiuk et al., *Display adaptive tone mapping*, 2008 — https://resources.mpi-inf.mpg.de/hdr/datmo/mantiuk08datm.pdf
82. Vangorp et al., *A Model of Local Adaptation*, 2015 — https://www.cl.cam.ac.uk/~rkm38/pdfs/vangorp2015local_adapt.pdf

---

## Appendix A — Agent-generated questions (40)

Each with a one-line answer or explicit disposition (answered in §X, or could not verify — dropped).

1. **Is the eye's dynamic range really 10–12 log units?** Only with adaptation over time; instantaneous-adapted ~9, simultaneous discriminable ~3.7 — answered §5.1.2.
2. **How much of the range does the pupil cover?** ~1.01 log units (8→2.5 mm) — answered §5.1.2 (pwsh-verified).
3. **What is the exact Weber fraction for cone vision?** 0.02–0.03 (L/M), 0.14 rods, 0.09 S-cones — answered §5.1.2; parent anchor 0.01–0.02 reported as low.
4. **At what luminance do rods saturate?** 2–3 log scotopic trolands (retinal-illuminance, pupil-dependent), not a sharp cd/m² value — answered §5.1.2.
5. **How fast is light vs. dark adaptation in the eye vs. game auto-exposure?** Cones ~100 ms–s up / 5–8 min down; games run f-stops/s (~1000× faster) — answered §5.1.4.
6. **Does any shipped engine model rod/cone duplex adaptation?** Not found in UE/Unity docs; "eye adaptation" is a single-channel camera-feel loop — answered §5.1.4.
7. **What luminance does EV15 correspond to?** 4096 cd/m² at ISO 100, K=12.5 — answered §5.1.1 (pwsh-verified).
8. **How does UE's histogram metering work exactly?** 64-bin log-luminance histogram, percentile window (10/90 default since 4.25), converge to 18% grey — answered §5.1.4.
9. **What is HDR Reference White?** 203 cd/m² (BT.2100) vs SDR ~100 nits; PQ container 10 000 nits — answered §5.1.5.
10. **Does HDR10's 10 000-nit ceiling exceed the eye's needs?** It exceeds the simultaneous range (3.7 log) but not the adaptational range (9 log) — answered §5.1.5.
11. **What is the validity range and exponent of the Stiles–Holladay formula?** $10E/\theta^2$, 1°<θ<30°; $\theta^{-3}$ below 1°, flatter beyond 10° — answered §5.2.1.
12. **How does age change veiling luminance?** Factor $1+(A/70)^4$; rapid growth beyond ~60 years — answered §5.2.1.
13. **Does ocular pigmentation matter for scatter?** Yes, at large angles (CIE general equation, p=0–1) — answered §5.2.1.
14. **What does game bloom physically simulate — eye or camera?** Camera: ghosts = multi-element internal reflection; the eye's own artifacts (corona, lenticular halo) are different structures — answered §5.2.2.
15. **Why are night halos bigger?** Dilated pupil changes the diffraction/scatter regime (Fresnel approximations fail; Rayleigh–Sommerfeld needed) — answered §5.2.2.
16. **Does the eye see its own "lens flare"?** It sees its own scatter (glare IS that); it cannot see camera ghost patterns — answered §5.2.2.
17. **Does adding glare increase perceived brightness of HDR displays?** Yes, >20% in cited psychophysics, even for simple filters — answered §5.2.3.
18. **Is there a real-time physically-based glare model?** Ritschel et al. Temporal Glare (wave-optics, GPU, 2009); Sun & Baranoski 2025 for night starbursts — answered §5.2.3.
19. **Does the retinal image smear during saccades?** Yes, massively (50–500°/s), and omission removes it; partly explainable without extra-retinal suppression — answered §5.3.1.
20. **Is intra-saccadic perception ever functional?** Yes, at 15–25 Hz retinal frequencies (magnocellular) — answered §5.3.1.
21. **Do humans perceive background smear during pursuit?** Yes, but reduced vs. fixation-equivalent (10.8 vs 12.0 arcmin at 4°/s) — answered §5.3.2.
22. **What shutter angle does 180° correspond to in ms?** Half the frame time: 20.8 ms @24 fps, 8.33 ms @60 fps — answered §5.3.3 (pwsh-verified).
23. **Does motion blur measurably improve player experience?** No significant effect in the controlled racing study; detectable only — answered §5.3.3.
24. **Why is motion blur disabled in VR?** Head-tracking + pursuit + low-persistence strobing replace it; the added blur only masks what persistence already handles — answered §5.3.4.
25. **What persistence/duty cycle do VR displays use?** ~0.33–2 ms (5–17% duty); <20% is the measured effectiveness threshold — answered §5.3.4.
26. **What foveation slope did Guenter et al. validate?** m = 1.32–1.65 arcmin/deg eccentricity, 3 layers, 5–6× speedup — answered §5.4.
27. **How much does contrast enhancement extend foveation?** 2× tolerable blur radius; 70% shading reduction — answered §5.4.
28. **What eye-tracking latency can foveation tolerate?** 50–70 ms total eye-to-image — answered §5.4.
29. **At what eccentricity/resolution is fixed foveation barely noticeable?** ≥7.5° eccentricity at ≥540p peripheral resolution — answered §5.4.
30. **Does foveation hurt stereoscopic depth?** No — stereoacuity unaffected up to ~2× common practice blur — answered §5.4.
31. **Which consumer HMDs ship ETFR?** Quest Pro (VK_QCOM_fragment_density_map_offset, SDK v49) and PSVR2 (per-eye IR tracking, PS5 hardware feature); Quest 2/3 are FFR-only — answered §5.4.
32. **What is the eye's depth of focus and its pupil dependence?** ±0.25–0.3 D at ~3 mm; smaller pupils widen it — answered §5.5 (cross-ref Part 1).
33. **What is the vergence demand at typical near-field VR distances vs. HMD focus?** 7.2° convergence at 0.5 m vs. fixed 1.5–2 m optics — answered §5.5 (pwsh-verified).
34. **Is the VAC-discomfort account contested?** Yes — 30-min studies find symptoms without vergence-parameter changes; movement studies find tolerance — reported both sides §5.5.
35. **What is the current state of varifocal displays?** Research prototypes only: Half Dome 3 (LC lenses, 64 focal planes), Butterscotch (56 PPD, 0–4 D, 10 D/s) — answered §5.5.
36. **Is 60 Hz actually "smooth" to the eye?** Only for foveal flicker fusion; phantom arrays persist to ~kHz — answered §5.6.
37. **What is MPRT and its typical values?** Effective pixel-visibility time = frame time on sample-and-hold: 16.67 ms @60 Hz … 4.17 ms @240 Hz — answered §5.6 (pwsh-verified).
38. **Can a display fix both hold-blur and stroboscopics?** Not below ~1000 Hz-class rates; the two needs pull in opposite directions — answered §5.6.
39. **Does TAA's softness correspond to anything in vision?** No — the retina integrates continuously with no frames or history buffer; stated plainly §5.6.
40. **Is the ACES curve a Weber-law/photoreceptor imitation?** No — empirically-tuned expert-preference splines; the Naka–Rushton/Reinhard line is the actual photoreceptor-inspired family — answered §5.7a.
