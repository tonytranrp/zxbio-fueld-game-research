# 4. Motor Control and Cognition During Locomotion — "How Do They Think"

**Scope:** the sensorimotor control of human walking and running — why feedback alone cannot work, what the nervous system builds instead (internal models, spinal oscillators, phased reflex gates), how balance is maintained through foot placement, how walkers decide among affordances and steer, what attention does to gait, and how the whole system adapts. Mathematics is shown where the literature supports it; numbers are reported as ranges where the literature is a spread, and explicit disagreements between sources are flagged rather than averaged away. Section numbering continues the merged five-part document (this is part 4 of 5).

---

## 0. Provenance

- **Date:** 2026-09-06 (web verification performed this session).
- **Tools:** Exa web search/fetch and direct URL fetches; every URL cited below was retrieved live this session. Local numeric verification with `pwsh` (PowerShell): `[math]::Sqrt(1.0/9.81) = 0.31927...` s; `1.4 * 0.31927 = 0.44699` m; `0.2 * 4.0 = 0.8` m exactly; `[math]::Sqrt(0.9/9.81) = 0.30289` s (sensitivity check for pendulum length 0.9 m).
- **Verification summary:** ~11 search batches (~33 queries), ~60 page/result fetches. All load-bearing claims carry an inline URL verified this session. Where a numeric anchor from the parent brief disagreed with the verified literature (stair-riser maximum, dual-task slowing percentage, head-turn lead time, foot-placement precision), the disagreement is reported in-text rather than silently resolved. Two claims from the brief could not be verified live and are explicitly marked as such (foot-placement execution precision of "~1–2 cm"; prism-adaptation timescales — replaced with the verified mirror-tracing literature).
- **Source count:** 40+ distinct verified primary sources; consolidated list in §4.8.

---

## 4.1 The delay problem: why movement cannot be reactive

### 4.1.1 The latency stack

The sensorimotor loop is slow at every stage, and the stages add. A scoping review of 46 time-delay-estimation studies (2000–2022) found the literature reports a **mean total sensorimotor delay of ~150 ms** for balance control, with trunk-level torque latencies of 100–210 ms, but also found that over 50% of studies did not even clearly define which delay they were estimating — the field is genuinely messy ([Frontiers scoping review, 2024](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2024.1329269/full)).

Component estimates from the verified literature:

- **Retinal/cortical visual processing:** retinal information reaches primary visual cortex via the LGN within ~30 ms of stimulus onset, peaking ~60 ms; posterior parietal cortex (movement-error computations) activates at ~80 ms and peaks after 100 ms; "100–400 ms is commonly needed for information processing prior to response output in humans" (Foxe & Simpson, quoted in [PMC7884356](https://pmc.ncbi.nlm.nih.gov/articles/PMC7884356/)). This brackets the classic simple-reaction-time (~200–250 ms) and choice-reaction-time (~350–500 ms) values rather than contradicting them; the anchor values sit inside the verified 100–400 ms processing envelope.
- **Visuo-manual feedback corrections:** trajectory corrections to a visually displaced target appear within **100–150 ms** in reaching — the plausible *minimum* for a visual feedback correction ([PMC7884356](https://pmc.ncbi.nlm.nih.gov/articles/PMC7884356/), summarizing Brenner & Smeets, Saunders & Knill, Franklin & Wolpert).
- **Proprioceptive-to-visual loop delays:** 80–150 ms ([McNamee & Wolpert 2019](https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/McNWol19.pdf)).
- **Electromechanical delay (EMD):** a genuine disagreement. The scoping review cites EMD at "around 10 ms" (attributed to Winter & Brookes 1991) ([Frontiers, 2024](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2024.1329269/full)), while the classical textbook range is 30–50 ms; a thesis reviewing the balance literature similarly reports time-to-peak-twitch of 90–150 ms for the triceps surae — the *full* force development, not just onset ([Liu, UBC 2024](https://doi.org/10.14288/1.0438574)). Read honestly: signal-to-force *onset* is ~10–30 ms; useful *force* takes ~100 ms.
- **Gait-specific corrections are slower.** Perturbing one leg's treadmill stiffness produces a contralateral-leg kinematic response at ~280 ms (hip/knee) and ~420 ms (ankle) — supraspinally mediated inter-leg coordination is *not* fast ([Artemiadis & Krebs lab, Frontiers 2015](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2015.00014/full)). Visual responses during gait are also late (~200 ms post-perturbation, same source). Total lower-limb sensorimotor delay is often quoted as up to ~160 ms ([Liu 2024](https://doi.org/10.14288/1.0438574)).

So the verified spread for a *visually guided* gait correction is roughly **150–420 ms** depending on what is measured (EMG onset vs. joint kinematics vs. inter-limb coordination) — consistent with the parent anchor's 150–260 ms at the fast end, but longer tails are real and must be modeled.

**The worked example that forces prediction.** At running speed $v = 4$ m/s, a 200 ms loop delay corresponds to

$$\Delta x = v\,\Delta t = 4.0 \ \text{m/s} \times 0.200 \ \text{s} = 0.8 \ \text{m}$$

of blind travel — verified arithmetically (`0.2*4.0 = 0.8` in pwsh). At a comfortable walking speed of 1.4 m/s the same delay is 0.28 m, still about 40% of a step length. A controller that only reacts to sensed error is computing corrections for where the body *was*; with the body's own inverted-pendulum instability (time constant $\sim\sqrt{l/g} \approx 0.32$ s, §4.3), a delayed feedback loop amplifies rather than damps error. A dramatic experimental demonstration: impose a 250 ms delay between ankle torque and body motion in a robotic balance simulator and most participants fall repeatedly within the first five minutes ([Liu 2024](https://doi.org/10.14288/1.0438574)).

### 4.1.2 Internal models: the fix

The computational answer, formalized by Wolpert and colleagues, is that the CNS maintains **forward models** — internal simulations of the body's dynamics that map motor commands to predicted sensory consequences, running *ahead* of reality instead of behind it.

- **Forward vs. inverse models:** "Systems that model aspects of this transformation are known as 'forward internal models' because they model the causal relationship between actions and their consequences… inverse internal models implement the opposite transformations, from desired consequences to actions" ([Wolpert & Ghahramani 2000](https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/WolGha00.pdf)).
- **Efference copy and reafference:** a copy of the outgoing motor command is used to predict and cancel the sensory effects of self-motion (von Holst/Sperry lineage; Helmholtz's original insight) — the reason a self-applied tickle is not ticklish, and the mechanism for distinguishing self-produced from external sensory events ([Wolpert 2001 review](http://wexler.free.fr/library/files/wolpert%20(2001)%20motor%20prediction.pdf); [Wolpert & Flanagan](https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/WolFla09.pdf)).
- **State estimation as an observer/Kalman filter:** the feedforward branch predicts the next state from efference copy plus a forward model of dynamics; the feedback branch compares predicted to actual sensory input and corrects the estimate with a Kalman gain. "The major objectives of the observer are to compensate for the delays in the sensorimotor system and to reduce the uncertainty in the state estimate" ([Wolpert 1997](https://psychology.nottingham.ac.uk/staff/srj/int/6.%20Forward%20models%20&%20Movement%20control%20mechanisms/Wol97.pdf); the same framework in [Miall & Wolpert 1996](http://www.liralab.it/teaching/ROBOTICA/docs/miall.worlpert.1996.pdf) and, updated, [McNamee & Wolpert 2019](https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/McNWol19.pdf)).
- **Cue-fusion weighting:** the multisensory estimate is not democratic. Because proprioceptive delays are tens of milliseconds shorter than visual ones, "during feedback control, the brain relies more heavily on proprioceptive information than on visual information (independent of the respective estimation variances), consistent with an optimal state estimator based on multisensory integration" ([McNamee & Wolpert 2019](https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/McNWol19.pdf)) — i.e., the weighting tracks *latency*, not just noise. For a game engine: vestibular/proprioceptive-like channels should dominate short-horizon state estimation; vision dominates when the forward model's prediction disagrees with what is seen.
- **Gait evidence:** the framework holds during locomotion specifically — when a learned visual cue predicts a treadmill-stiffness change, gait adapts *before* the perturbation (anticipatory control), and when the cue is shown but the perturbation withheld, a late (~630 ms) anticipatory kinematic response still appears ([Artemiadis lab, Frontiers 2015](https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2015.00014/full)).

---

## 4.2 The control architecture of locomotion

### 4.2.1 Central pattern generators: the evidence

The basic locomotor rhythm does not require the brain. The evidence stack:

1. **Spinal animals step.** Sherrington reported spinal stepping in transected dogs a century ago; Graham Brown (1911–1914) showed deafferented spinal cats produce alternating rhythmic discharges and proposed mutually inhibitory "half-centres" on each side of the cord; Grillner's spinalized kittens performed weight-bearing hindlimb stepping on a treadmill, adjusted to split belts, and even changed gait to a hindlimb gallop at high belt speeds ([BWSTT animal-studies review, PMC5421634](https://pmc.ncbi.nlm.nih.gov/articles/PMC5421634/); [transspinal stimulation review, Sci Rep 2024](https://preview-www.nature.com/articles/s41598-024-56579-0)). Trained adult spinal cats recovered stepping without external stimulation in 4–5 weeks — "the spinal cord learned to step" ([PMC5421634](https://pmc.ncbi.nlm.nih.gov/articles/PMC5421634/)).
2. **Humans have spinal locomotor circuitry.** Epidural stimulation (25–60 Hz over L2) of the functionally isolated lumbar cord in motor-complete paraplegics elicits rhythmic, alternating stance/swing-like EMG ([Dimitrijevic et al. 1998](https://nyaspubs.onlinelibrary.wiley.com/doi/10.1111/j.1749-6632.1998.tb09062.x)). The pattern generator is organized as flexibly combined **burst generators** rather than a single hard-wired half-centre ([Danner et al. 2015, Brain](https://pmc.ncbi.nlm.nih.gov/articles/PMC4408427/)).
3. **Human spinal stepping with training.** Body-weight-supported treadmill training derived directly from the cat work elicits locomotor activity in humans with complete SCI — the human spinal cord "did not need the brain" to use sensory input to generate locomotor output ([PMC5421634](https://pmc.ncbi.nlm.nih.gov/articles/PMC5421634/)). The existence and *contribution* of a human CPG to normal bipedal walking nevertheless remains debated — the honest current position is that human CPGs "can be defined by the activity they produce" and likely contribute a feedforward rhythm component plus phase-specific activation, simplifying supraspinal control of cycle frequency ([Minassian et al. 2017](https://journals.sagepub.com/doi/10.1177/1073858417699790)).
4. **Infant stepping** assembles the same two-pattern primitive structure (an 'extension' and a 'swing' burst pattern) seen in toddlers, monkeys, cats, rats and guinea fowl, tuned rather than replaced during development ([Dominici et al. 2011, cited in Danner 2015](https://pmc.ncbi.nlm.nih.gov/articles/PMC4408427/)).

**The half-centre model in brief.** In the classical abstraction (Graham Brown's half-centres; the modern two-level version with a rhythm generator driving pattern-formation circuitry is due to McCrea & Rybak, as summarized in [Danner et al. 2015](https://pmc.ncbi.nlm.nih.gov/articles/PMC4408427/)), flexor and extensor half-centres are two neural populations with mutual inhibition and activity-dependent fatigue:

$$
\tau \dot{u}_E = -u_E + \sigma\!\left(e - w\,u_F - \beta\,z_E\right), \qquad
\tau \dot{u}_F = -u_F + \sigma\!\left(e - w\,u_E - \beta\,z_F\right),
$$

where $u_{E,F}$ are population activations, $e$ tonic drive, $w$ mutual inhibition, $\sigma$ a saturating nonlinearity, and $z_{E,F}$ slow adaptation/fatigue variables ($\tau_z \gg \tau$). The mechanism: whichever centre wins inhibition silences the other; but the winner slowly self-fatigues ($z$ grows), inhibition weakens, and the other centre escapes — producing alternating bursts at a period set mainly by the adaptation time constant and the drive $e$. Coupling left/right and flexor/extensor oscillators of this type reproduces gaits from walk to gallop as phase-locking patterns, which is exactly what the spinal-cat speed-series shows ([PMC5421634](https://pmc.ncbi.nlm.nih.gov/articles/PMC5421634/)).

### 4.2.2 Supraspinal modulation

- **Cortex — precision foot placement.** In cats walking on flat ground vs. over barriers vs. on a horizontal ladder, 61–72% of motor-cortex cells significantly change their discharge (usually increasing), and modulation deepens as barriers are placed closer together; walking a ladder with flat rungs changes 61% of cells' mean rates by >20% ([Beloozerova & Sirota 1993, J Physiol](https://pubmed.ncbi.nlm.nih.gov/8350259/)). With matched mechanics (ladder crosspieces biomechanically equivalent to flat walking), motor-cortex activity still changes dramatically with accuracy demand while most of 229 mechanical variables do not — "the activity of motor cortex reflects… integration of visual information with ongoing locomotion" ([Beloozerova et al. 2010, J Neurophysiol](https://doi.org/10.1152/jn.00360.2009)). Unexpected constraints recruit *more* neurons (69% respond, most during swing) than predictable ones ([Stout et al. 2015](https://pmc.ncbi.nlm.nih.gov/articles/PMC4626377/)); premotor cortex shows the same task-dependence ([premotor cortex study, 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC10642938/)). In humans, corticospinal drive is disproportionately important for distal flexors even in level walking ([Yang & Gorassini 2006](https://doi.org/10.1177/1073858406292151)). Engine translation: ramp cortical "involvement" with terrain uncertainty — flat ground can run open-loop; precision targets engage a slower, vision-coupled correction layer.
- **Cerebellum — timing and adaptation.** Split-belt walking in decerebrate cats drives dramatic increases in Purkinje complex-spike firing during early adaptation, and blocking cerebellar plasticity (nitric-oxide deprivation) abolishes the adaptive gait change entirely; humans with cerebellar damage show marked impairments of acquisition and storage of locomotor adaptations ([Bastian lab review, PMC2816031](https://pmc.ncbi.nlm.nih.gov/articles/PMC2816031/)). Cerebellar ataxia is the clinical readout: poorly timed, poorly calibrated stepping.
- **Basal ganglia — action selection.** Freezing of gait in Parkinson's disease is modeled as response conflict driving subthalamic-nucleus activity that inhibits basal-ganglia output, effectively "shutting down" brainstem locomotor regions until a single motor plan wins ([Shine et al., frontostriatal FOG model](https://www.frontiersin.org/journals/systems-neuroscience/articles/10.3389/fnsys.2013.00061/full); updated network view in [Frontiers Neurol. 2026](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2026.1713795/full)). Freezing is *triggered* by dual-tasking, transitions, and narrow spaces — selection failure under load, not muscle failure.

### 4.2.3 Phase-dependent reflex modulation

Reflexes are not fixed circuits; they are gated by the step cycle:

- **H-reflex gating.** The soleus H-reflex is strongly modulated over the gait cycle — largest during stance (where it supports the body against gravity), and nearly abolished in swing (where a stretch reflex would oppose dorsiflexion). At matched stimulus strength and EMG, the reflex is up to **3.5× larger in standing than walking** — task- and phase-dependent central gain control, not passive motoneuron excitability ([Capaday & Stein 1986, J Neurosci](https://www.jneurosci.org/content/6/5/1308)). The modulation persists even when soleus is voluntarily activated during swing, ruling out antagonist co-contraction as the cause and pointing at presynaptic inhibition of Ia afferents ([Schneider et al. 2000](https://pubmed.ncbi.nlm.nih.gov/8224081/)).
- **Cutaneous reflex reversal.** Middle-latency (50–90 ms) cutaneous reflexes in tibialis anterior *reverse sign* with phase: the same stimulus that excites TA at swing onset suppresses it at swing end, with facilitation of antagonists — a within-muscle reversal that cannot be elicited in standing ([Yang & Stein 1990, J Neurophysiol](https://bishtref.com/articles/10.1152/jn.1990.63.5.1109); [Duysens et al. 2004](https://doi.org/10.1139/y04-071)). Functionally: skin stimulation late in swing pushes the foot *down* to contact ground sooner — the reflex implements landing, not a fixed withdrawal.

---

## 4.3 Dynamic balance and foot-placement planning

### 4.3.1 Walking as controlled falling: the XcoM

The linearized inverted-pendulum model of balance in the horizontal plane, with CoM ground projection $x$, CoP position $u_x$, and effective pendulum length $l$:

$$\ddot{x} = \omega_0^2 (x - u_x), \qquad \omega_0 = \sqrt{g/l}$$

Hof's move ([Hof et al. 2005, J Biomech 38:1–8](https://doi.org/10.1016/j.jbiomech.2004.03.025); full PDF at [braceworks.ca](https://braceworks.ca/wp-content/uploads/2016/05/hof-condition-for-dynamic-stability.pdf)) is to define the **extrapolated center of mass**:

$$\boxed{\;X_{\text{CoM}} = x + \frac{v}{\omega_0} = x + v\sqrt{\frac{l}{g}}\;}$$

**Derivation sketch.** Differentiate the definition: $\dot\xi = \dot x + \ddot x/\omega_0$ with $\xi \equiv x + v/\omega_0$. Substitute the pendulum equation: $\dot\xi = v + \omega_0(x - u_x)$. Also $\dot x = v = -\omega_0 (x - \xi)$ by rearranging the definition. Combining gives the decoupled pair ([Hof 2008, Hum Mov Sci](https://doi.org/10.1016/j.humov.2007.08.003)):

$$\dot x = -\omega_0 (x - \xi), \qquad \dot\xi = \omega_0 (\xi - u_x)$$

This is the payoff: the *CoM* subsystem is now a stable first-order tracker of $\xi$ (it always chases the XcoM with time constant $1/\omega_0$), and the *XcoM* subsystem is a pure integrator of the CoP offset. Stability reduces to keeping $\xi$ within the base of support — a static-looking condition applied to a dynamic quantity. The **margin of stability** is $b = \min |u_{\max} - (x + v/\omega_0)|$, the minimum distance from XcoM to the BoS boundary, and is proportional to the impulse $m\,\omega_0\, b$ needed to unbalance the walker ([Hof 2005](https://doi.org/10.1016/j.jbiomech.2004.03.025)).

**Worked example (numbers verified in pwsh).** With $l = 1.0$ m, $g = 9.81$ m/s²: $\omega_0 = \sqrt{9.81} = 3.132$ rad/s, so $1/\omega_0 = \sqrt{l/g} = 0.3193$ s. At walking speed $v = 1.4$ m/s the XcoM leads the CoM by

$$v\sqrt{l/g} = 1.4 \times 0.3193 = 0.447 \ \text{m}.$$

So a walker at 1.4 m/s is dynamically "leaning" nearly half a meter ahead of their center of mass — they are perpetually falling forward and must place each foot to intercept the XcoM, not the CoM. Empirically, during walking the XcoM–CoM separation reaches ~4 cm in the mediolateral direction, and the CoP sits only ~2.5 cm lateral to the XcoM at foot contact — walking balance is *far* more marginal than CoM-based analysis suggests ([Hof 2005, Fig. 6](https://braceworks.ca/wp-content/uploads/2016/05/hof-condition-for-dynamic-stability.pdf)). Robustness caveat: only ~30% of the physical BoS is *effectively* usable for recovery because CoP displacement is rate-limited by muscle activation dynamics ([Hof & Curtze 2016, discussed in Curtze et al. 2024](https://doi.org/10.1016/j.jbiomech.2024.112045)); and the MoS should not be read as a global stability metric — it is an instantaneous state variable ([Curtze et al. 2024](https://doi.org/10.1016/j.jbiomech.2024.112045); scaling discussion in [Nguyen et al. 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC10842449/)). The same quantity appears in the robotics literature as the **capture point**; Hof derives it from and relates it to Pai & Patton's stability work rather than the robotics lineage ([Hof 2005](https://braceworks.ca/wp-content/uploads/2016/05/hof-condition-for-dynamic-stability.pdf)).

Hof's control rule, verified against perturbation experiments: *place the CoP a fixed distance behind and outward of the XcoM at foot contact*; then a velocity disturbance $\Delta v$ is corrected by shifting the next foot placement by $\Delta v / \omega_0$ in the same direction ([Hof 2008](https://doi.org/10.1016/j.humov.2007.08.003)). At 1.4 m/s a lateral push of 0.3 m/s needs only $0.3 \times 0.319 \approx 9.6$ cm of extra step width — one step, no feedback latency in the loop at all.

### 4.3.2 Foot placement as the control variable

- **Variability is structured, not random.** In the $[z_L, z_R]$ plane of left/right foot placements, human steps form strongly anisotropic clouds aligned with the constant-step-width goal-equivalent manifold: people heavily prioritize regulating *step width* (lateral balance) over lateral *position* — $\sigma(z_B)/\sigma(w) \gg 1$ ([Dingwell & Cusumano 2019 framework; Desmet et al. 2022](https://journals.plos.org/ploscompbiol/article/file?id=10.1371%2Fjournal.pcbi.1010035&type=printable); [Dingwell lab, winding-path experiments](https://www.biorxiv.org/content/10.1101/2024.07.11.603068v2)). This is the formal version of the "long-step/short-step correlation" structure: deviations are tolerated along dimensions that don't threaten balance and corrected (within 1–2 steps) along dimensions that do. Lateral maneuvers confirm it: lane changes take ~4 steps — a large transition step plus smaller preparatory and recovery steps — even though two is the geometric minimum ([Desmet et al. 2022](https://journals.plos.org/ploscompbiol/article/file?id=10.1371%2Fjournal.pcbi.1010035&type=printable)).
- **How far ahead is terrain planned?** With a moving visibility window over randomly distributed obstacles, collisions increase and speed drops when walkers can see **less than ~2 step lengths ahead**; beyond two step lengths, behavior is asymptotically like full vision — because only with two steps of look-ahead can the walker control both the previous foot placement and the push-off that set the ballistic CoM arc into the target foothold ([Matthis & Fajen 2013, JEP:HPP](https://doi.org/10.1037/a0033101); biomechanical mechanism in [Bonnen et al. 2013, Proc R Soc B](https://pmc.ncbi.nlm.nih.gov/articles/PMC3673057/)). But look-*ahead* extends further than the control horizon: gaze fixations cluster 2–3 steps ahead, centered on future footholds (~3 steps, ranging 1–5) ([Matthis et al. 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC5937949/); [Muller et al. 2023, eLife](https://doi.org/10.7554/elife.91243)), path choice reflects terrain ~5 steps ahead (avoiding height changes, accepting detours), and *speed* is modulated with a planning horizon of ~8 steps to minimize energy ([Kuo et al., summarized in Muller et al. 2023](https://doi.org/10.7554/elife.91243)). Walkers also maintain a constant temporal look-ahead window across terrains, trading off speed for information ([Matthis et al. 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC5937949/)).
- **Safe-zone selection.** When the normal landing zone is blocked, alternate placements are selected to minimize deviation from the ongoing gait trajectory while preserving dynamic stability, with a preference hierarchy (shorten the step > lengthen > widen > cross over) modulated by spatial and temporal constraints ([Patla et al., AMAM 2000](http://adaptivemotion.org/AMAM2000/papers/B09-patla.pdf); [Moraes et al., cited therein](https://doi.org/10.1007/s00221-004-1888-z)).
- **Execution precision — reported with a caveat.** The parent anchor's "executed with ~1–2 cm precision" could **not** be independently verified this session; the verified literature reports that trained cats step on ladder rungs with "less spatial variability" as accuracy demands rise ([Beloozerova et al. 2010](https://doi.org/10.1152/jn.00360.2009)) and that human gaze fixations on future footholds scatter with a standard deviation of 25–30 cm ([Muller et al. 2023](https://doi.org/10.7554/elife.91243)) — the tight ~1–2 cm figure should be treated as unverified until a primary source is found.

---

## 4.4 Decision-making in motion: affordances and steering

### 4.4.1 Affordances: action boundaries are body-scaled

Warren's stair-climbing study is the quantitative foundation. Riser height $R$ scaled by leg length $L$:

- **Perceived climbable/unclimbable boundary:** $R/L = 0.88$ — predicted from a geometric gait analysis (the bipedal→quadrupedal transition) and confirmed by perceptual judgments in both short and tall observers ([Warren 1984, JEP:HPP](https://doi.org/10.1037//0096-1523.10.5.683); [Warren 1982 dissertation](https://digitalcommons.lib.uconn.edu/dissertations/AAI8309263)). **Disagreement with the parent anchor:** the brief's "~50% max" is *not* what Warren found — 0.88 is the verified critical value (roughly 88% of leg length; an 88 cm riser for a 1 m leg). About half of leg length is where stairs merely become *uncomfortable*, not where climbing fails.
- **Preferred (energetically optimal) riser:** minimum metabolic cost per vertical meter occurred at $R/L = 0.26$ on a stairmill, and visual preference converged on $R/L = 0.25$–0.24 — the verified basis for the "~25% of leg length preferred" anchor ([Warren 1984](https://doi.org/10.1037//0096-1523.10.5.683); [Warren 1982](https://digitalcommons.lib.uconn.edu/dissertations/AAI8309263)).
- **The scalar is not always leg length.** Older adults' boundaries diverge from the 0.88 ratio (tall ~0.73, short ~0.62) and are better fit by leg strength and hip flexibility — affordances scale to *capability*, not just anatomy ([Konczak et al. 1992](https://doi.org/10.1037/0096-1523.18.3.691); review of the follow-ups in [MDPI Stair Design 2024](https://www.mdpi.com/2673-8945/4/3/36); eyeheight-scaled alternatives in [Mark 1987](https://doi.org/10.1037//0096-1523.13.3.361)). Aperture traversal is body-scaled analogously (Warren & Whang 1987, cited in [Mark 1987](https://doi.org/10.1037//0096-1523.13.3.361)).
- **Barrier/opening margins in motion.** Walkers passing through apertures rotate the body to maintain a minimal spatial margin of ~6–8 cm on one side; ducking under a barrier uses a *smaller* vertical margin than lateral margins (vertical sway doesn't threaten the base of support); circumventing a *moving* obstacle demands ~2 m in front and 0.5 m to each side ([Higuchi 2013 review, Front. Psychol.](https://www.frontiersin.org/articles/10.3389/fpsyg.2013.00277/pdf)). The margin is a control parameter, and its magnitude is task-geometry-dependent.

### 4.4.2 Obstacle avoidance: the choice structure and minimal toe clearance

- **Choices are planned at least one step ahead.** Limb trajectories for stepping over an obstacle are essentially unchanged when the obstacle is hidden one step before crossing — the crossing movement is already programmed ([Patla 1998, discussed in Higuchi 2013](https://www.frontiersin.org/articles/10.3389/fpsyg.2013.00277/pdf)). With two obstacles 1 m apart, take-off placement before the first shifts to improve the position for the second — modification for obstacle #2 begins before crossing obstacle #1 ([Krell & Patla 2002, in Higuchi 2013](https://www.frontiersin.org/articles/10.3389/fpsyg.2013.00277/pdf)).
- **Step-over vs. around vs. on.** The verified literature establishes the *determinants* of alternate foot placement selection (minimize gait-trajectory change, preserve stability, under spatial and temporal constraints — [Patla 2000](http://adaptivemotion.org/AMAM2000/papers/B09-patla.pdf); [Moraes et al. 2004/2007, cited therein](https://doi.org/10.1007/s00221-004-1888-z)); a specific *probability-vs-height curve* for "over vs. around vs. on" was not verified this session and is not asserted.
- **Toe clearance is minimal, not generous — the counterintuitive finding.** Minimum toe clearance over level ground in young women is ~34.5 mm; over a flat 0-height obstacle strip it rises to only ~43.5 mm — conscious attention to the obstacle adds under 1 cm ([quantitative toe-clearance study, 2004](https://www.jstage.jst.go.jp/article/rika/19/2/19_2_101/_article/-char/en)). Across obstacle heights up to 30% of leg length, young adults maintain a roughly **constant** leading-foot clearance margin (for obstacles taller than ~79 mm) and constant trailing-foot clearance at all heights — the swing foot tracks the obstacle top closely rather than being thrown high ([Chen et al. 2004](https://doi.org/10.4015/s1016237204000219)). Consistent with the anchor's ~1–2 cm clearance *margin* interpretation: the foot crosses obstacles with centimeters to spare, because raising it higher costs energy and reduces landing stability. Elderly and cautious walkers adopt the opposite — a "conservative strategy" of extraordinarily high foot elevation correlated with *reduced* motor flexibility ([Higuchi lab, UCM analysis](https://researchmap.jp/read0124255/published_papers/45892573?lang=en)).

### 4.4.3 Pedestrian collision avoidance

**The minimum-predicted-distance (MPD) strategy.** At time $t$, MPD is the predicted future distance of closest approach if both walkers hold their current velocity vectors:

$$\text{MPD}(t) = \min_{\tau \ge 0} \left\| \big(\mathbf{p}_1 + \mathbf{v}_1 \tau\big) - \big(\mathbf{p}_2 + \mathbf{v}_2 \tau\big) \right\|$$

Walkers adapt their motion only when MPD at first sight is low — below ~1 m for two adults (0.9 m threshold in a replication; lower when children are involved) — and the interaction then shows three phases: observation (low MPD), reaction (MPD raised linearly to an acceptable value, R² = 0.99, from ~0.38 m to ~0.86 m over a ~1.4 s reaction phase starting ~3 s before crossing), and regulation (maintenance) ([Olivier et al. 2012, Gait & Posture](https://doi.org/10.1016/j.gaitpost.2012.03.021); [Olivier et al. 2013](https://inria.hal.science/hal-00821854/document); child–adult extension in [Rapos et al. 2019](https://pubmed.ncbi.nlm.nih.gov/31132592/)). Avoidance is *collaborative but asymmetric*: the walker giving way contributes ~57% of the MPD increase vs. ~43% for the one passing first, both reorienting their path but the giver-way also slowing ([Olivier et al. 2013](https://inria.hal.science/hal-00821854/document)). Crowd-level confirmation: visually distracting *some* pedestrians with phone tasks delays lane self-organization and degrades avoidance for everyone — mutual anticipation, not local repulsion, is what makes crowds flow ([Murakami et al., Sci Adv](https://www.science.org/doi/10.1126/sciadv.abe7758)).

**The social-force model.** Helbing & Molnár model each pedestrian $i$ as subject to motivational "forces" ([Helbing & Molnár 1995, Phys Rev E 51:4282](https://link.aps.org/doi/10.1103/PhysRevE.51.4282); [arXiv:cond-mat/9805244](https://arxiv.org/abs/cond-mat/9805244)):

$$
m_i \frac{d\mathbf{v}_i}{dt} = m_i \frac{\mathbf{v}_i^0\, \mathbf{e}_i^0 - \mathbf{v}_i}{\tau_i}
+ \sum_{j \ne i} \mathbf{f}_{ij} + \sum_{W} \mathbf{f}_{iW}
$$

with the acceleration term toward desired speed $v_i^0$ on a relaxation time $\tau_i$, boundary/obstacle terms, and pairwise repulsion of the standard exponential form

$$
\mathbf{f}_{ij} = A \exp\!\big((r_{ij} - d_{ij})/B\big)\,\hat{\mathbf{n}}_{ij}
$$

where $d_{ij}$ is the inter-pedestrian distance, $r_{ij}$ the sum of radii, and $A$, $B$ set strength and range. These nonlinearly coupled Langevin equations reproduce self-organized lane formation, arching at bottlenecks, and other collective effects ([Helbing & Molnár 1995](https://link.aps.org/doi/10.1103/PhysRevE.51.4282)). Note the model's status honestly: it is a *phenomenological* model of repulsion, not of cognition; MPD-style anticipation explains behavior the bare social-force model misses (per [Murakami et al.](https://www.science.org/doi/10.1126/sciadv.abe7758)).

**The fundamental diagram.** Free walking speeds cluster at **1.2–1.4 m/s**: mean ~1.34 m/s (Weidmann's value, used across the field), European-study averages ~1.41 m/s, US ~1.35 m/s ([Daamen & Hoogendoorn, free-speed distributions](https://www.pedbikeinfo.org/cms/downloads/Free%20Speed%20Distributions%20for%20Pedestrian%20Traffic.pdf); review in [Springer ETRR 2017](https://link.springer.com/article/10.1007/s12544-017-0264-6)). Speed falls with density in regimes: nearly free below $\rho \approx 0.7$ m⁻², roughly linear decline to $\rho \approx 2.3$ m⁻², a plateau to ~4.7 m⁻², then rapid decline ([Seyfried et al., single-file experiments](https://arxiv.org/pdf/physics/0506170)). In single-file walking the required length per pedestrian is linear in speed: $d = 0.36 + 1.06\,v$ meters — the microscopic origin of the diagram ([Seyfried et al.](https://arxiv.org/pdf/physics/0506170)). Handbook values for capacity and jam density genuinely disagree ($J_{s,\max}$ from 1.2 to 1.8 (ms)⁻¹; $\rho_0$ from 3.8 to 10 m⁻²) — a real, documented spread, partly methodological ([Seyfried et al. 2008](https://ar5iv.labs.arxiv.org/html/0810.1945)).

---

## 4.5 Steering and heading control

### 4.5.1 The behavioral-dynamics steering law (Fajen & Warren)

For an agent at constant speed $s$, heading $\phi$ (allocentric), goal at bearing $\psi_g$ and distance $d_g$, obstacle at $\psi_o$ and $d_o$, the fitted second-order model of human walking is ([Fajen, Warren, Temizer, Bajcsy — model PDF](https://www.ini.rub.de/upload/file/1682237671_cfd96eb5e09ac7ca59e9/FajenEtAl2003.pdf); [Warren et al. 2010 JOV abstract](https://doi.org/10.1167/1.3.184)):

$$
\ddot\phi = -b\,\dot\phi
\;-\; k_g (\phi - \psi_g)\big(e^{-c_1 d_g} + c_2\big)
\;+\; k_o (\phi - \psi_o)\, e^{-c_3 |\phi - \psi_o|}\, e^{-c_4 d_o}
$$

Structure, term by term:

- **Damping** $-b\dot\phi$: bodies have angular inertia; turning rate cannot jump.
- **Goal attraction:** grows linearly with goal angle, decays exponentially with goal distance but *asymptotes to a floor* ($c_2$ term) so distant goals still steer.
- **Obstacle repulsion:** decays exponentially with *both* obstacle angle and distance; the fitted exponentials imply **only obstacles within about ±30° of heading and less than ~4 m ahead appreciably influence steering** ([Fajen et al. 2003](https://www.ini.rub.de/upload/file/1682237671_cfd96eb5e09ac7ca59e9/FajenEtAl2003.pdf)). Summing terms linearly scales the model to complex scenes and predicts left/right route choices around obstacles without explicit path planning (goals as attractors, obstacles as repellors in the heading phase plane; $R^2 \approx 0.97$ for turning-rate fits, [Warren et al. 2010](https://doi.org/10.1167/1.3.184)).

**The bearing-angle form.** Defining the goal bearing angle $\beta \equiv \phi - \psi_g$, the first-order reduction of the goal term is exactly a bearing-nulling law:

$$\dot\beta = -k\,\beta \quad (\text{with } k \text{ inflated near, deflated far from the goal})$$

i.e., humans null the bearing angle — turn until the target is dead ahead — rather than following a curved intercept path; paths to goals are approximately linear after the initial turn, contradicting pure-pursuit-style curved-intercept predictions (Lee, Wann & Swapp, discussed and rejected in [Warren et al. 2010](https://pdfs.semanticscholar.org/657b/214778df346965d3dbbf72df400da53a8c68.pdf)). Evidence that the *visual* variables used include both egocentric goal direction and optic-flow-specified heading ([Warren et al., cited in the 2010 paper](https://doi.org/10.1167/1.3.184)).

### 4.5.2 Anticipatory head orientation and turn kinematics

- **Head/gaze leads the turn.** Walking planned circular trajectories, head direction anticipates walking-direction changes by ~200 ms (curvature-dependent); head orientation deviates toward the inner concavity — a "go where you look" strategy, step-by-step predictive ([Grasso et al. 1996, Neuroreport](https://pubmed.ncbi.nlm.nih.gov/8817526/)). On 90° corner trajectories, head and eyes systematically deviate toward the future travel direction with an anticipation lead of **about 1 s** — and the same anticipatory pattern occurs in *darkness* and reverses appropriately during backward walking, so it is not visually driven but endogenous ([Grasso et al. 1998, Neurosci Lett](https://www.sciencedirect.com/science/article/abs/pii/S0304394098006259)). Curved walking shows head-yaw anticipation of ~220 ± 90 ms ahead of trunk movement, with the head progressively turned toward the circle center (Courtine & Schieppati, via [Robins thesis, LJMU](https://researchonline.ljmu.ac.uk/id/eprint/4415/1/158208_2015RobinsPhD.pdf)). **Anchor note:** the verified lead times span ~0.2–1 s depending on turn sharpness — the brief's "~1–2 steps (~0.5–1.5 s)" is consistent with the corner-turning (1 s) figure and an overestimate for shallow curves.
- **Top-down reorientation sequence.** Turns follow a stereotyped eye → head → trunk → feet sequence; head yaw leads trunk yaw by ~25°; immobilizing the head makes the *trunk* reorient earlier (the system preserves anticipation), and opposing head perturbations delay COM redirection until the new path has been looked at ([Hollands et al. 2001; Vallis & Patla 2001, via Robins thesis](https://researchonline.ljmu.ac.uk/id/eprint/4415/1/158208_2015RobinsPhD.pdf); [Vallis & Patla, PubMed](https://pubmed.ncbi.nlm.nih.gov/11374079/)). Suppressing eye movements delays gait initiation and lowers stepping frequency — the anticipatory nystagmus is part of the motor synergy, not an epiphenomenon ([Robins thesis](https://researchonline.ljmu.ac.uk/id/eprint/4415/1/158208_2015RobinsPhD.pdf)).
- **Curvature vs. speed.** Verified pieces rather than one clean law: on curved paths the CoM must be deflected every contact phase to compensate for straight-line flight phases — an "over-deflection" during stance, with the inner limb deflecting more and the outer limb accelerating forward more; mechanical work rises by up to 25% vs. straight running depending on speed and radius ([Mesquita et al. 2024, PLoS ONE](https://journals.plos.org/plosone/article/file?id=10.1371%2Fjournal.pone.0298790&type=printable)). Cutting maneuvers reorient direction within a 1–3 step transition (cutting literature, cited in the same paper). On a curved treadmill, dynamic stability (margin-of-stability) *decreases* with smaller radius at slow speeds but can *increase* at running speeds — the speed–curvature interaction is not monotone ([Kim et al. 2017](https://doi.org/10.1142/s0219519417501056)). A crisp universal speed–curvature limit was not verified this session; what is established is the mechanism (stepwise lateral redirection of CoM velocity with inner/outer limb role asymmetry).

---

## 4.6 Attention and dual-task costs

### 4.6.1 "Stops walking when talking" and dual-task gait costs

The seminal observation: nursing-home residents who stopped walking when engaged in conversation had a markedly elevated fall risk — an early, purely observational dual-task screen ([Lundin-Olsson, Nyberg & Gustafson, Lancet 1997](https://doi.org/10.1016/s0140-6736(97)24009-2)). A systematic review of 15 studies put the pooled odds ratio for falling at **5.3 (95% CI 3.1–9.1)** for changed gait or task performance under dual tasking, with predictive power strongest in frail older adults and some studies failing to replicate ([Lundin-Olsson 2009, EJN review](https://onlinelibrary.wiley.com/doi/10.1111/j.1468-1331.2009.02612.x)); large prospective cohorts confirm walking-while-talking measures predict falls (pace domain hazard ratio 1.31, p = 0.002 — though raw dual-task *cost* in speed did not) ([Ayers et al. 2013](https://pmc.ncbi.nlm.nih.gov/articles/PMC3944080/)), and the predictive validity of dual-task cost is strongest in robust elderly, not the frailest ([Japanese 1,038-subject cohort](https://agsjournals.onlinelibrary.wiley.com/doi/10.1111/j.1532-5415.2010.03206.x)).

**Magnitude of slowing — population-dependent (anchor disagreement reported).** The brief's "~5–10%" is a young/healthy-adult figure and the small end of the spread:

- Healthy older adults: 1.21 → 1.02 m/s under dual task (~16%) in the meta-analysis comparison quoted by [Smith et al., via the PD meta-analysis](https://pmc.ncbi.nlm.nih.gov/articles/PMC8487457/).
- Parkinson's disease: gait-speed declines of −0.11 to −0.18 m/s under dual task (from ~1.07 m/s single-task), SMD = −0.68, regardless of task type ([PD dual-task meta-analysis, 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8487457/)).
- Cognitive-load costs scale with executive demand: arithmetic/language/motor secondary tasks all impair gait speed in PD ([same meta-analysis](https://pmc.ncbi.nlm.nih.gov/articles/PMC8487457/)).

### 4.6.2 Phone and texting gait — specific measured changes

Meta-analysis of 22 mobile-phone studies (592 participants): significantly decreased gait velocity (SMD = −1.45), step length (−1.01), stride length (−0.90); significantly *increased* step time (+0.77), stride time (+0.87), **step width (+0.79)**, and double-support (+2.32% of gait cycle) — resource-intensive tasks (texting/reading) drive the effect; simple calling/talking does not ([Frontiers meta-analysis, 2023](https://www.frontiersin.org/journals/physiology/articles/10.3389/fphys.2023.1163655/full); corroborating meta-analysis in [Bruyneel et al. 2023](https://doi.org/10.1016/j.gaitpost.2023.01.009)). Concrete single-study numbers: comfortable-speed step length falls from ~70.6 to ~65.5 cm and step width widens from ~8.9 to ~9.8 cm while texting ([Sajewicz & Dziuba-Słonima 2023](https://doi.org/10.3390/ijerph20054590)); head flexion while two-handed texting averages **38.5°** (31.1° one-handed browsing) ([Han & Shin, via Sajewicz 2023](https://doi.org/10.3390/ijerph20054590)). A key control: merely *assuming* the texting posture with no cognitive task already slows gait and shortens steps — the blocked lower visual field and constrained arms produce a cautious gait pattern independent of cognitive load ([TPNT study, PLoS ONE](https://pmc.ncbi.nlm.nih.gov/articles/PMC7549775/); lower-visual-field dependence of foot-placement control per Marigold, cited therein).

### 4.6.3 Priority structure and the two-visual-systems division of labor

- **Locomotion is automated until perturbed; conscious takeover is slow.** The architecture of §4.2 is the mechanism: spinal oscillators + phased reflexes run open-loop; cortex engages when terrain demands precision; basal-ganglia selection failure under load produces freezing. Dual-tasking delays *turn onset* but leaves eye–body coordination during the turn intact — the steering synergy itself is automatic and subcortically organized ([Robins thesis](https://researchonline.ljmu.ac.uk/id/eprint/4415/1/158208_2015RobinsPhD.pdf)). The interference model of freezing — compensatory cortical recruitment overwhelming limited executive resources ([Frontiers Neurol. 2026](https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2026.1713795/full)) — is the extreme end of the same tradeoff.
- **Dorsal/ventral division (the verified core of the ambient/focal dichotomy).** Goodale & Milner's two-visual-systems account: the ventral stream builds scene-based, relational, conscious perception ("what"); the dorsal stream performs egocentric, absolute-metric, real-time visuomotor transformation ("how") ([Goodale & Milner 1992](https://cnbc.cmu.edu/braingroup/papers/goodale_milner_1992.pdf); updated [Goodale 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC4024294/)). The locomotion-relevant proof: patient D.F., with profound visual-form agnosia, "could step over obstacles during locomotion as well as controls, even though her perceptual judgments about the height of these obstacles were far from normal" ([Goodale, CVI framework paper](https://onlinelibrary.wiley.com/doi/10.1111/dmcn.12299)). Online foot placement runs on dorsal-style processing that conscious perception cannot access; the older "ambient (peripheral, posture/heading) vs. focal (central, identification)" phrasing is the perceptual analogue of the same dissociation. Gaze studies add the allocation rule: more terrain irregularity → more fixations near the walker, 2–3 steps ahead, with speed sacrificed to keep the look-ahead time constant ([Matthis et al. 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC5937949/)).

---

## 4.7 Adaptation and learning

**Split-belt adaptation — minutes, with aftereffects and partial savings.** Walking with the legs at different belt speeds (e.g., 1.5 vs. 0.5 m/s) produces: (i) immediate reactive asymmetries that never adapt away; (ii) *adaptive* recalibration of interlimb coordination (step length, double support, phasing) over ~10–20 minutes of exposure; (iii) negative aftereffects (reverse asymmetry) on tied belts; and (iv) *savings* — faster re-adaptation on re-exposure ([Bastian lab review, PMC2816031](https://pmc.ncbi.nlm.nih.gov/articles/PMC2816031/)). The aftereffect partially transfers to overground walking — motor overground transfer measured at **16.3% (CI 12.0–21.7%)** of the treadmill aftereffect in a 2024 decomposition, with ~83% of the remaining treadmill aftereffect washed out by overground walking; a perceptual bias (~0.4 m/s leg-speed mis-estimate) transfers similarly. The split is interpretable as two learning substrates: a context-general forward-model recalibration and a treadmill-specific stimulus–response component ([Roemmich et al., npj Sci Learn 2024](https://www.nature.com/articles/s41539-024-00258-2); original transfer result in [Reisman et al. 2009](https://doi.org/10.1177/1545968309332880)). Cerebellum is required (§4.2.2). Generalization is narrow by construction: partial, context-gated transfer is the rule, not the exception.

**Visual-distortion adaptation (the verified stand-in for prism adaptation).** The brief's "prism adaptation, minutes to hours" was not separately verified this session; the directly verified neighbouring literature is mirror-tracing (Snoddy 1926, the founding power-law dataset) and its modern re-examination: over five days of practice plus one-week and one-month retention, *individual* performance is best fit by an exponential with two timescales — a fast warm-up component and a slow persistent-change component — while *group-averaged* data still fit a power law ([Stratton et al. 2007](https://doi.org/10.3200/jmbr.39.6.503-516); [Newell & Rosenbloom, law of practice](https://doi.org/10.1184/r1/6607196)). Readaptation-after-break is faster than initial learning (warm-up/savings), consistent with the split-belt savings phenomenon.

**Long-term skill acquisition — the power law of practice.** Across perceptual-motor and cognitive tasks alike, performance time follows

$$\boxed{\;T(N) = A + B (N + E)^{-a}\;}$$

with $N$ trials, asymptote $A$, prior experience $E$, and learning rates $a$ clustering around 0.2–0.3 (seldom outside 0.1–0.5) — improvement is rapid at first and each further factor of improvement costs multiplicatively more practice ([Newell & Rosenbloom 1981](https://doi.org/10.1184/r1/6607196); [CMU tech report version](http://iiif.library.cmu.edu/file/Newell_box00032_fld02190_doc0001/Newell_box00032_fld02190_doc0001.pdf)). Two honest caveats from the literature itself: individual-subject curves may be exponential rather than power-law (the power law can be an artifact of averaging — [Heathcote et al., cited in Stratton 2007](https://doi.org/10.3200/jmbr.39.6.503-516)), and the treadmill-training literature shows a categorical distinction between *adaptation* (minutes, aftereffects, cerebellum) and *training-induced learning* (10–12 sessions, no aftereffects) ([Bastian lab review](https://pmc.ncbi.nlm.nih.gov/articles/PMC2816031/)). For a game engine: short-timescale recalibration (minutes, with aftereffects when the distortion is removed) and long-timescale skill (power/exponential-law gains over sessions) are genuinely different systems and should be modeled separately.

---

## 4.8 Consolidated boxed-equation summary

$$\boxed{\;X_{\text{CoM}} = x + v\sqrt{l/g}\;} \quad
\sqrt{1.0/9.81} = 0.319\ \text{s};\ \ v\sqrt{l/g}\big|_{v=1.4} = 0.447\ \text{m}\ (\text{Hof 2005})
$$
$$\boxed{\;\Delta x = v\,\Delta t = 4.0 \times 0.200 = 0.8\ \text{m at running speed}\;} \quad\text{(delay arithmetic)}
$$
$$\boxed{\;\ddot\phi = -b\dot\phi - k_g(\phi-\psi_g)(e^{-c_1 d_g}+c_2) + k_o(\phi-\psi_o)e^{-c_3|\phi-\psi_o|}e^{-c_4 d_o}\;} \quad\text{(Fajen \& Warren steering)}
$$
$$\boxed{\;\text{MPD}(t) = \min_{\tau\ge0}\big\|(\mathbf p_1+\mathbf v_1\tau)-(\mathbf p_2+\mathbf v_2\tau)\big\|\;} \quad \text{act when MPD} \lesssim 1\ \text{m (Olivier et al.)}
$$
$$\boxed{\;m_i\dot{\mathbf v}_i = m_i\frac{\mathbf v_i^0\mathbf e_i^0 - \mathbf v_i}{\tau_i} + \sum_j \mathbf f_{ij} + \sum_W \mathbf f_{iW}\;} \quad\text{(Helbing \& Molnár social force)}
$$
$$\boxed{\;R/L = 0.88\ \text{(climbable)},\quad R/L \approx 0.25\ \text{(preferred)}\;} \quad\text{(Warren 1984 — note disagreement with the 0.5 anchor)}
$$
$$\boxed{\;T(N) = A + B(N+E)^{-a},\quad a \approx 0.2\text{–}0.3\;} \quad\text{(power law of practice)}
$$
$$\boxed{\;\tau\dot u_{E,F} = -u_{E,F} + \sigma(e - w\,u_{F,E} - \beta z_{E,F})\;} \quad\text{(half-centre CPG abstraction)}
$$

**Numeric quick-reference (verified):** visually guided correction 100–150 ms (reaching) to ~280–420 ms (inter-leg gait); mean balance-loop delay ~150 ms; simple RT within the 100–400 ms processing envelope; H-reflex up to 3.5× larger standing than walking; step-width ~8–12 cm (8.9–9.8 cm measured); toe clearance ~3.5 cm over ground, constant small margin over obstacles; look-ahead 2 step lengths (control), 2–3 steps (gaze), ≥5 steps (path), ~8 steps (speed); crowd free speed 1.2–1.4 m/s; single-file spacing $d = 0.36 + 1.06v$ m; head-anticipation 0.2–1 s by turn sharpness; split-belt overground transfer ~16%; texting: velocity SMD −1.45, step width +0.79, head flexion ~38.5°.

### Primary sources (all verified live this session)

1. Time-delay scoping review — https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2024.1329269/full
2. Sensorimotor delays in tracking — https://pmc.ncbi.nlm.nih.gov/articles/PMC7884356/
3. Sensorimotor control of gait (treadmill stiffness) — https://www.frontiersin.org/journals/human-neuroscience/articles/10.3389/fnhum.2015.00014/full
4. Liu, adapting to sensorimotor delay (thesis) — https://doi.org/10.14288/1.0438574
5. Wolpert, computational approaches to motor control — https://psychology.nottingham.ac.uk/staff/srj/int/6.%20Forward%20models%20&%20Movement%20control%20mechanisms/Wol97.pdf
6. Wolpert & Flanagan, forward models review — https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/WolFla09.pdf
7. Wolpert & Ghahramani, computational principles — https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/WolGha00.pdf
8. McNamee & Wolpert, internal models in biological control — https://wolpertlab.neuroscience.columbia.edu/sites/wolpertlab.neuroscience.columbia.edu/files/content/papers/McNWol19.pdf
9. Miall & Wolpert 1996 — http://www.liralab.it/teaching/ROBOTICA/docs/miall.worlpert.1996.pdf
10. Wolpert 2001, motor prediction — http://wexler.free.fr/library/files/wolpert%20(2001)%20motor%20prediction.pdf
11. Hof et al. 2005, condition for dynamic stability — https://doi.org/10.1016/j.jbiomech.2004.03.025 (PDF: https://braceworks.ca/wp-content/uploads/2016/05/hof-condition-for-dynamic-stability.pdf)
12. Hof 2008, XcoM control of walking — https://doi.org/10.1016/j.humov.2007.08.003
13. Curtze et al. 2024, notes on the margin of stability — https://doi.org/10.1016/j.jbiomech.2024.112045
14. Nguyen et al. 2023, scaling the MoS — https://pmc.ncbi.nlm.nih.gov/articles/PMC10842449/
15. Minassian et al., human CPG — https://journals.sagepub.com/doi/10.1177/1073858417699790
16. Transspinal stimulation & locomotor centers — https://preview-www.nature.com/articles/s41598-024-56579-0
17. BWSTT animal studies review — https://pmc.ncbi.nlm.nih.gov/articles/PMC5421634/
18. Danner et al. 2015, burst generators — https://pmc.ncbi.nlm.nih.gov/articles/PMC4408427/
19. Dimitrijevic et al. 1998, spinal CPG in humans — https://nyaspubs.onlinelibrary.wiley.com/doi/10.1111/j.1749-6632.1998.tb09062.x
20. Yang & Gorassini 2006, spinal and brain control of human walking — https://doi.org/10.1177/1073858406292151
21. Capaday & Stein 1986, H-reflex modulation — https://www.jneurosci.org/content/6/5/1308
22. Schneider et al. 2000, H-reflex modulation origin — https://pubmed.ncbi.nlm.nih.gov/8224081/
23. Yang & Stein 1990, reflex reversal — https://bishtref.com/articles/10.1152/jn.1990.63.5.1109
24. Duysens et al. 2004, gait gates foot reflexes — https://doi.org/10.1139/y04-071
25. Drew 2013, sparse synergies & precision walking — https://www.frontiersin.org/articles/10.3389/fncom.2013.00083/pdf
26. Beloozerova & Sirota 1993, motor cortex & accuracy — https://pubmed.ncbi.nlm.nih.gov/8350259/
27. Beloozerova et al. 2010, accurate vs nonaccurate stepping — https://doi.org/10.1152/jn.00360.2009
28. Stout et al. 2015, known vs unexpected constraints — https://pmc.ncbi.nlm.nih.gov/articles/PMC4626377/
29. Premotor cortex & visually guided stepping — https://pmc.ncbi.nlm.nih.gov/articles/PMC10642938/
30. Matthis & Fajen 2013, visual control of foot placement — https://doi.org/10.1037/a0033101
31. Bonnen et al. 2013, exploiting bipedal biomechanics — https://pmc.ncbi.nlm.nih.gov/articles/PMC3673057/
32. Muller et al. 2023, foothold selection (eLife) — https://doi.org/10.7554/elife.91243
33. Matthis et al. 2018, gaze & foot placement — https://pmc.ncbi.nlm.nih.gov/articles/PMC5937949/
34. Patla et al., local path planning — http://adaptivemotion.org/AMAM2000/papers/B09-patla.pdf
35. Higuchi 2013, anticipatory adaptive locomotion — https://www.frontiersin.org/articles/10.3389/fpsyg.2013.00277/pdf
36. Chen et al. 2004, 3-D obstacle crossing — https://doi.org/10.4015/s1016237204000219
37. Toe-clearance quantitative analysis — https://www.jstage.jst.go.jp/article/rika/19/2/19_2_101/_article/-char/en
38. Warren 1984, stair affordances — https://doi.org/10.1037//0096-1523.10.5.683
39. Warren 1982 dissertation — https://digitalcommons.lib.uconn.edu/dissertations/AAI8309263
40. Konczak et al. 1992, climbability in aging — https://doi.org/10.1037/0096-1523.18.3.691
41. Mark 1987, eyeheight-scaled affordances — https://doi.org/10.1037//0096-1523.13.3.361
42. Stair design & affordances (MDPI 2024) — https://www.mdpi.com/2673-8945/4/3/36
43. Olivier et al. 2012, MPD metric — https://doi.org/10.1016/j.gaitpost.2012.03.021
44. Olivier et al. 2013, role-dependent strategies — https://inria.hal.science/hal-00821854/document
45. Murakami et al., mutual anticipation in crowds — https://www.science.org/doi/10.1126/sciadv.abe7758
46. Rapos et al. 2019, children & adults MPD — https://pubmed.ncbi.nlm.nih.gov/31132592/
47. Helbing & Molnár 1995, social force model — https://link.aps.org/doi/10.1103/PhysRevE.51.4282 (arXiv: https://arxiv.org/abs/cond-mat/9805244)
48. Fundamental diagrams review — https://link.springer.com/article/10.1007/s12544-017-0264-6
49. Seyfried et al., single-file fundamental diagram — https://arxiv.org/pdf/physics/0506170
50. Daamen & Hoogendoorn, free speed distributions — https://www.pedbikeinfo.org/cms/downloads/Free%20Speed%20Distributions%20for%20Pedestrian%20Traffic.pdf
51. Seyfried et al. 2008, enhanced empirical data — https://ar5iv.labs.arxiv.org/html/0810.1945
52. Fajen et al. 2003, dynamical steering model — https://www.ini.rub.de/upload/file/1682237671_cfd96eb5e09ac7ca59e9/FajenEtAl2003.pdf
53. Warren et al. 2010, behavioral dynamics of steering — https://doi.org/10.1167/1.3.184 (PDF: https://pdfs.semanticscholar.org/657b/214778df346965d3dbbf72df400da53a8c68.pdf)
54. Grasso et al. 1996, predictive head direction — https://pubmed.ncbi.nlm.nih.gov/8817526/
55. Grasso et al. 1998, eye-head anticipatory synergy — https://www.sciencedirect.com/science/article/abs/pii/S0304394098006259
56. Robins thesis, eye–body coordination in turning — https://researchonline.ljmu.ac.uk/id/eprint/4415/1/158208_2015RobinsPhD.pdf
57. Vallis & Patla, head yaw perturbations — https://pubmed.ncbi.nlm.nih.gov/11374079/
58. Mesquita et al., curved-running CoM mechanics — https://journals.plos.org/plosone/article/file?id=10.1371%2Fjournal.pone.0298790&type=printable
59. Kim et al. 2017, curved treadmill stability — https://doi.org/10.1142/s0219519417501056
60. Lundin-Olsson et al. 1997, stops walking when talking — https://doi.org/10.1016/s0140-6736(97)24009-2
61. Lundin-Olsson 2009, dual-task falls review — https://onlinelibrary.wiley.com/doi/10.1111/j.1468-1331.2009.02612.x
62. Ayers et al. 2013, walking while talking & falls — https://pmc.ncbi.nlm.nih.gov/articles/PMC3944080/
63. PD dual-task meta-analysis — https://pmc.ncbi.nlm.nih.gov/articles/PMC8487457/
64. Mobile-phone gait meta-analysis — https://www.frontiersin.org/journals/physiology/articles/10.3389/fphys.2023.1163655/full
65. Bruyneel et al. 2023, texting meta-analysis — https://doi.org/10.1016/j.gaitpost.2023.01.009
66. Sajewicz & Dziuba-Słonima 2023, texting gait parameters — https://doi.org/10.3390/ijerph20054590
67. Smartphone posture vs cognitive load — https://pmc.ncbi.nlm.nih.gov/articles/PMC7549775/
68. Goodale & Milner 1992, two visual systems — https://cnbc.cmu.edu/braingroup/papers/goodale_milner_1992.pdf
69. Goodale 2014, vision for perception vs action — https://pmc.ncbi.nlm.nih.gov/articles/PMC4024294/
70. Goodale, separate visual systems (CVI) — https://onlinelibrary.wiley.com/doi/10.1111/dmcn.12299
71. Reisman et al. 2009, split-belt transfer — https://doi.org/10.1177/1545968309332880 (PMC: https://pmc.ncbi.nlm.nih.gov/articles/PMC2811047/)
72. Split-belt generalization mechanisms (npj 2024) — https://www.nature.com/articles/s41539-024-00258-2
73. Bastian lab, locomotor adaptation review — https://pmc.ncbi.nlm.nih.gov/articles/PMC2816031/
74. Newell & Rosenbloom, law of practice — https://doi.org/10.1184/r1/6607196
75. Stratton et al. 2007, Snoddy revisited — https://doi.org/10.3200/jmbr.39.6.503-516
76. Desmet et al. 2022, lateral maneuvers (GEM) — https://journals.plos.org/ploscompbiol/article/file?id=10.1371%2Fjournal.pcbi.1010035&type=printable
77. Dingwell lab, winding paths (bioRxiv) — https://www.biorxiv.org/content/10.1101/2024.07.11.603068v2
78. Shine et al., frontostriatal FOG model — https://www.frontiersin.org/journals/systems-neuroscience/articles/10.3389/fnsys.2013.00061/full
79. PFC & freezing of gait review — https://www.frontiersin.org/journals/neurology/articles/10.3389/fneur.2026.1713795/full
80. Dual-task walking in robust elderly — https://agsjournals.onlinelibrary.wiley.com/doi/10.1111/j.1532-5415.2010.03206.x
