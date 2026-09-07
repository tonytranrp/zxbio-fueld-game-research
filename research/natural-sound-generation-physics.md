# The Physics of Natural Sound Generation: Mechanisms, Math, and Spectra

> **Provenance.** Drafted 2026-09-06 by a delegated research agent using the Exa web-search/fetch MCP tools (17 search queries + 2 verification searches across aeroacoustics, bubble acoustics, vegetation noise, snow mechanics, granular acoustics, dune physics, thunder acoustics, and soundscape literature), plus local numerical verification of the Minnaert, Wood, Strouhal, and Hertz formulas in PowerShell. **~30 distinct sources** are cited inline; every boxed equation was checked against at least one source. Claims without a source are explicitly marked *[unsourced, mechanism-plausible]*. This section feeds game sound design for the voxel engine (terrain, water, grass, trees, footsteps), but the deliverable is the physics itself: *what process converts motion/energy into pressure waves in air, with what frequency content and amplitude envelope, and why.*

---

## 1. A Taxonomy of Sound-Generation Mechanisms

Every natural sound begins as energy in some non-acoustic form — kinetic energy of wind, elastic energy in a bent blade of grass, thermal energy in a lightning channel — and ends as a tiny fraction (often 10⁻⁶ to 10⁻³) of that energy radiated as propagating pressure fluctuations in air. The conversion happens through a surprisingly small number of elementary mechanisms, and each has a characteristic radiation efficiency and spectral signature.

### 1.1 The four canonical sources

**(a) Vibrating solid bodies (modal vibration).** A struck, plucked, or wind-driven solid stores elastic energy in its normal modes and radiates by accelerating its surface. The spectrum is a set of discrete damped resonances (modes) whose frequencies are set by geometry and stiffness and whose decay is set by material damping. This is the mechanism of twigs, trunks, wooden floors, stone, metal, and — at a tiny scale — every leaf behaving as a small plate. Section 5 develops the math (Euler–Bernoulli and Kirchhoff–Love eigenproblems).

**(b) Aeroacoustic sources (turbulence and vortex shedding).** Moving air converts to sound through unsteady momentum flux. Lighthill (1952, 1954) recast the exact equations of fluid motion as an inhomogeneous wave equation whose source is the divergence-free part of the **Lighthill stress tensor**:

$$\frac{\partial^2 \rho}{\partial t^2} - c_0^2 \nabla^2 \rho = \frac{\partial^2 T_{ij}}{\partial x_i \partial x_j}, \qquad T_{ij} = \rho v_i v_j + \left(p - c_0^2 \rho\right)\delta_{ij} - \tau_{ij}$$

At low Mach number the Reynolds-stress term dominates, $T_{ij} \approx \rho_0 v_i v_j$: turbulence acts as a volume distribution of **quadrupoles** — two equal-and-opposite force pairs that deform a fluid element without changing its volume ([Lighthill 1954, Proc. Roy. Soc. A](https://doi.org/10.1098/rspa.1954.0049); [Routh & Musielak 2025](https://doi.org/10.3390/fluids10060156)). Because the source is a *double* divergence of a second-rank tensor, each spatial derivative contributes a factor $\sim M = U/c_0$ (compactness), so radiated **power** scales as

$$\boxed{\;W_{\text{quad}} \;\sim\; K\,\frac{\rho_0}{c_0^5}\,L^2\,U^{8}\;}$$

the famous **eighth-power law** ([Lighthill's eighth power law — Wikipedia](https://en.wikipedia.org/wiki/Lighthill%27s_eighth_power_law); [Mašović & Sarradj 2020](https://doi.org/10.3390/acoustics2030035) re-derive it in an "acoustic spacetime" formalism). Acoustic efficiency is of order $10^{-4}M^5$ for jets (Lighthill 1954). The brutal $U^8$ scaling is why gentle breezes are nearly silent as pure aeroacoustic sources and why jets scream.

**(c) Hydrodynamic / bubble sources (monopole volume pulsation).** A gas bubble in liquid oscillating in volume is the **most efficient radiator found in nature**: it changes the fluid volume directly, with no derivative-induced $M$ penalty, and the air–water density contrast (1:800) gives it enormous effective compressibility contrast. Minnaert's 1933 discovery that a bubble is a mass–spring oscillator (Section 3.1) explains most of the sound of streams, rain, splashes, and surf.

**(d) Impact and friction transients.** Sudden contact or fracture injects a broadband force pulse into a structure (Hertz contact, Section 5.1); stick-slip friction (bowing, squeaking snow, glass harmonica) injects a sawtooth force rich in harmonics. These are *excitation* mechanisms — the radiated spectrum is the excitation spectrum filtered by the structural modes of the body involved.

### 1.2 Monopole / dipole / quadrupole: the efficiency hierarchy

The multipole order determines how "efficiently" a source of size $L$ at Mach number $M$ couples to the acoustic field. Each successive multipole order adds one factor of $kL \sim M$ to amplitude and two to power:

| Source type | Physical meaning | Power scaling | Example in nature |
|---|---|---|---|
| Monopole | Fluctuating volume/mass injection | $\propto U^4$ | Pulsating bubble (splash, rain, stream) |
| Dipole | Fluctuating force on fluid | $\propto U^6$ | Vortex shedding from a wire/branch (aeolian tone) |
| Quadrupole | Fluctuating stress, no net force or volume change | $\propto U^8$ | Free turbulence (waterfall roar minus bubbles) |

This hierarchy (monopole $\propto U^4$, dipole $\propto U^6$, quadrupole $\propto U^8$) is standard aeroacoustics; see [Mašović & Sarradj 2020](https://doi.org/10.3390/acoustics2030035), which states the quadrupole law alongside "its relation with the other two main types of sources—monopole and dipole", and [Landa & McClintock 2010](https://doi.org/10.1088/1751-8113/43/37/375101) for the aeolian dipole. At the low Mach numbers of almost all natural sound, a monopole of given flow energy can be many orders of magnitude louder than a quadrupole. This single fact organizes the whole of Section 3: *water would be almost silent without bubbles*.

The dipole aeolian-tone formula from Curle's analogy, confirmed experimentally over $360 < Re < 3\times10^4$:

$$p^2_{\text{rms}}(r,\theta) = 0.037\,\frac{\sin^2\theta}{r^2}\,\frac{\rho^2 U_\infty^{6}}{c_0^{2}}\,\mathrm{St}^2\,L\,d$$

where $L$ is cylinder length and $d$ diameter ([Casalino et al. 2003](https://acoustique.ec-lyon.fr/publi/casalino_jsv03b.pdf); refined as $LL_c C_L^2/16$ with spanwise correlation length $L_c$ in [Fourès et al. 2024](https://ar5iv.labs.arxiv.org/html/2404.11434)). Note the built-in $U^6$.

---

## 2. Grass and Foliage

### 2.1 Mechanism: impulsive leaf-on-leaf contact plus friction

Wind moves foliage; the sound we call *rustle* is generated almost entirely by **contacts between leaves** — micro-collisions and short sliding-friction episodes — plus a secondary contribution from vortex shedding around twigs and branches. In the leafed state "the dominating rustling sound at frequencies around 4 kHz is generated by contacts between leaves in the foliage" ([Bolin et al., Acta Acustica](https://dael.euracoustics.org/bin/EAA/aaua_dl?document_id=64838), building on Fégeant's wind-tunnel work). Each individual contact is a short broadband impulse: two thin plates touching for tens of microseconds excite their bending modes and radiate a click. The aggregate of thousands of random micro-impacts per second, amplitude-modulated by gusts, is what we hear.

The best-validated quantitative model (Fégeant, extended by Bolin and used in Heutschi's EMPA auralizer) expresses vegetation sound power per cubic meter and per Hz as:

$$\Delta W(f) = C_R\, S\, u^{\chi}\, c\, \Gamma(f)$$

with $S$ = leaf area density (m²/m³), $u$ = wind speed inside the canopy, $\chi \approx 1.5$ (a fitted wind-speed exponent; regression inclination $\approx 29\log u$ dB), and a normalized spectrum for foliated deciduous trees of

$$\Gamma(f) = C_1 f^{-1} + C_2 \exp\!\left[-C_3 (f - f_d)^2 / f_d^2\right]$$

— a **pink-noise floor from mechanical collisions plus a Gaussian bump at $f_d \approx 4$ kHz from leaf rustle** ([Heutschi 2014, "Auralization of Wind Turbine Noise"](https://www.dora.lib4ri.ch/empa/dload/empa:6079/PDF/Heutschi-2014-Auralization_of_wind_turbine_noise-(published_version).pdf); [Bolin et al.](https://dael.euracoustics.org/bin/EAA/aaua_dl?document_id=64838)). For conifers the model substitutes a vortex-shedding (dipole) term around needles with peak frequencies $f_s = 0.2u/d_n$ — Strouhal shedding from the needle diameter $d_n$ — and for leafless deciduous trees the spectrum falls as $f^{-2}$ with a Gaussian bump tied to twig diameter (≈5 mm).

### 2.2 What sets the spectrum: leaf size and stiffness

Each leaf is a small plate; its bending modes scale (Section 5.2) as $f \propto t\sqrt{E/\rho}\,/\ell^2$ for a plate of thickness $t$ and lateral dimension $\ell$. A large flexible leaf (hosta, banana) has low bending modes (hundreds of Hz) and damps quickly; a small stiff dry grass blade or a crispy dead leaf has modes up in the several-kHz range and stores/releases energy in sharper impulses. The measured rustle spectrum is broadband roughly **1–12 kHz with the energy maximum near 4 kHz** for typical deciduous canopies (Bolin/Heutschi above); the fruit-tree canopy study of [Li et al. 2024](https://www.sciencedirect.com/science/article/abs/pii/S0168169924004538) measured spectral centroids of wind-excited canopy audio concentrated at **1500–3400 Hz** (mean 2501 Hz) — consistent with the Fégeant/Bolin bump position varying with leaf properties.

Dry vs. wet: wet leaves stick together and are plasticized by surface water, so contacts become damped pulls rather than brittle snaps — lower amplitude, fewer impulses, darker spectrum. Dry leaves rattle: higher contact velocities, more impulses, more high-frequency content. *[The wet/dry contrast is widely reported but I found no dedicated measured spectrum; mechanism-plausible from contact mechanics.]*

### 2.3 Amplitude statistics: wind-gust intermittency

Rustle loudness tracks wind speed with a power law near $u^{1.5}$ in sound *power* (≈ $29 \log_{10} u$ in dB, from Bolin's fit above), applied to the wind *inside* the canopy, which decays exponentially with penetration depth: $u(x) = U_{\text{open}}\,e^{-0.04x}$ (Heutschi 2014, simplifying Fégeant/Bolin). Because $u$ is proportional to power-law-amplified fluctuations, the perceived loudness is strongly gust-modulated: Heutschi's auralizer models this with an asymmetric exponential average of the wind time series — **0.2 s time constant when the wind rises, 1.0 s when it falls** — reflecting leaf/branch inertia making loudness attack faster than it decays. Only the upwind strip of depth ≈ 2 tree heights actually contributes: inner forest regions are wind-shielded and silent.

Classical vegetation acoustics: [Bullen & Fricke 1982](https://doi.org/10.1016/0022-460x(82)90387-x) (sound propagation through vegetation) and [Fricke 1984, "Sound attenuation in forests"](https://doi.org/10.1016/0022-460x(84)90380-8) established the empirical base; [Schomer & Beck 2010](https://doi.org/10.3397/1.3371961) measured wind-induced pseudo-noise vs. leaf rustle and found that at 2–5 m/s the leaf-rustle ambient exceeds the microphone pseudo-noise of a 20–30 cm windscreen — i.e., rustle is the dominant natural ambient in vegetated landscapes at those wind speeds.

### 2.4 Walking through grass: plant-tissue fracture

The *crunch* of footsteps in dry grass is fracture noise: bending a dry blade beyond its failure strain snaps cell walls in a burst of micro-cracks, each radiating an elastic transient that couples to air. Fracture of brittle biological tissue is an **avalanche process** — stress redistributes from each broken element to its neighbors, triggering cascades (the same "crackling noise" physics as paper tearing and magnetic Barkhausen avalanches). *[A direct measured grass-fracture spectrum appears not to exist in the literature; mechanism-plausible by analogy with well-studied avalanche crackling dynamics.]* The audible signature is a dense random pulse train with per-pulse duration of order the contact/fracture time (µs–tens of µs) — broadband to several kHz — with overall envelope following the foot-roll force history (heel-strike rise of a few ms, rolloff through toe-off). Wet grass: turgor-pressurized cells fail plastically and coherently instead of brittlely — the crunch becomes a muffled "swish."

---

## 3. Water

### 3.1 Bubbles: the atom of natural water sound

**Minnaert's derivation (1933).** A spherical gas bubble of equilibrium radius $R_0$ in an infinite liquid of density $\rho$ at ambient pressure $P_0$ behaves as a mass–spring oscillator: the gas provides the spring, the inertia of the surrounding liquid provides the mass. Minnaert's original energy argument ([Minnaert 1933, Phil. Mag.](https://www.uio.no/studier/emner/matnat/math/MEK4480/v22/beskjeder/xvi-on-musical-air-bubbles-and-the-sounds-of-running-water.pdf)): compress the bubble adiabatically by $x = R_0 - R$; the excess internal pressure is $\Delta p = 3\gamma P_0 x / R_0$, so the potential energy at maximum compression is

$$V_{\max} = \int_0^a \! \Delta p \cdot 4\pi R^2 \, dR \;=\; 6\pi \gamma P_0 R_0 a^2$$

for radius amplitude $a$. The kinetic energy of the radially-moving incompressible liquid (which decays as $R^4/\bar{R}^2$ with distance $\bar{R}$, integrated to infinity) is $2\pi \rho \omega^2 a^2 R_0^3$ with $\omega = 2\pi N$. Equating energies gives the pulsation frequency. The modern textbook route linearizes the **Rayleigh–Plesset equation**

$$R\ddot{R} + \tfrac{3}{2}\dot{R}^2 = \frac{1}{\rho}\Big[P_B(t) - P_\infty(t)\Big], \qquad P_B = \left(P_0 + \tfrac{2\sigma}{R_0}\right)\!\left(\frac{R_0}{R}\right)^{3\kappa} - \frac{2\sigma}{R} - \frac{4\mu \dot R}{R}$$

about $R = R_0$, keeping the gas polytropic stiffness (e.g. [Brennen's fluid mechanics notes, Caltech](http://brennen.caltech.edu/fluidbook/multiphase/bubblegrowthandcollapse/bubblenaturalfrequencies.pdf); [Fitzpatrick 2018, ETH thesis](https://doi.org/10.3929/ethz-b-000287325), which derives Minnaert by linearizing Rayleigh–Plesset). The result, neglecting surface tension and viscosity:

$$\boxed{\;f_0 \;=\; \frac{1}{2\pi R_0}\sqrt{\frac{3\kappa P_0}{\rho}}\;}$$

with $\kappa$ the polytropic exponent (1 isothermal … $\gamma = 1.4$ adiabatic; real bubbles sit between — [vibrationdata tutorial after Leighton](http://www.vibrationdata.com/tutorials/bubble.pdf)). Including surface tension (Brennen): $\omega_n^2 = \frac{1}{\rho R_0^2}\left[3\kappa(\bar p_\infty - p_V) + 2(3\kappa-1)\frac{S}{R_0}\right]$ — surface tension matters only for $R_0 \lesssim 10\ \mu$m.

**Numerical verification** ($P_0 = 101{,}325$ Pa, $\rho = 1000$ kg/m³, $\gamma = 1.4$; computed 2026-09-06):

| Bubble radius | Minnaert frequency |
|---|---|
| 0.1 mm | 32.8 kHz (ultrasonic edge; drizzle hiss) |
| 0.5 mm | 6.57 kHz |
| **1 mm** | **3.28 kHz** ✓ (target ≈ 3.3 kHz) |
| 5 mm | 657 Hz |
| **1 cm** | **328 Hz** ✓ (target ≈ 330 Hz — the "glug" of an emptying bottle) |

The astonishing property: a **1 mm** air bubble in water resonates at ~3.3 kHz with a wavelength in water of ~22 cm — a sub-wavelength resonator 200× smaller than the sound it emits, radiating as an efficient monopole ([Gontier, "Minnaert resonance in bubbly media"](https://www.ceremade.dauphine.fr/~gontier/Presentations/2018_01_16_Ceremade.pdf) — note his example: $R = 0.5$ mm → $\omega_M = 42{,}000$ rad/s = 6.7 kHz ✓). This is why essentially all audible water sound is bubble sound.

**The sound of one bubble:** a damped sinusoid (a "chirp" if nonlinear). Damping has three parts — viscous, thermal, and acoustic radiation — each expressible as an effective viscosity (Chapman & Plesset 1971, reproduced in [Brennen's notes](http://brennen.caltech.edu/fluidbook/multiphase/bubblegrowthandcollapse/bubblenaturalfrequencies.pdf)); millimetric bubbles have $Q$ of order 10–30, so a single entrained bubble rings for a few ms to tens of ms at its Minnaert frequency. Each splash/ripple event entrains bubbles of a distribution of sizes → the event's spectrum is a comb of damped sinusoids at $f_0(R)$ for each bubble, weighted by excitation.

### 3.2 Splash physics and the dripping-tap "plink"

A drop falling on a free surface produces a well-mapped sequence: impact → cavity → crown/recoil → **bubble entrainment** → jet. The landmark experimental result is [Phillips, Agarwal & Jordan 2018, *"The Sound Produced by a Dripping Tap is Driven by Resonant Oscillations of an Entrapped Air Bubble"*, Sci. Rep. 8:9515](https://www.nature.com/articles/s41598-018-27913-0) (Cambridge; [popular summary](https://www.cam.ac.uk/research/news/what-causes-the-sound-of-a-dripping-tap-and-how-do-you-stop-it)). Their 30,000 fps video + synchronized microphone/hydrophone show:

- The impact, the cavity formation, the capillary waves, the crown, and the jet are **all effectively silent**. Capillary waves travel at ~0.5 m/s — far below sound speed — and cannot radiate; the ejected droplets oscillate in *shape* not *volume*, i.e. quadrupoles, "extremely inefficient acoustic generators."
- The sound begins at the exact frame the bubble detaches from the cavity bottom; recorded frequencies align with the Minnaert prediction for the entrained bubble size (their case: ~0.7 mm stabilized diameter → ≈ 4–5 kHz).
- Suppressing entrainment (with a thin rod or a drop of surfactant) **eliminates the plink entirely** — the causal proof.
- The airborne plink is not the underwater field leaking through the interface (that path loses ~40 dB): the oscillating bubble sits just below the cavity bottom and **drives the cavity surface like a piston in a baffle**, an efficient monopole coupling to air.

Design consequence: a single drip sound = one damped sinusoid packet at $f_0(R)$, ms-scale, with the cavity acting as a small piston radiator.

### 3.3 Rain on water: drop-size acoustics

The ocean-acoustics community has turned rain noise into a rain gauge because **raindrop size classes have distinct acoustic signatures** underwater. [Nystuen, "The sound of rainfall at sea" (2003)](https://doi.org/10.1121/1.4780502) and [Nystuen & Barry 2002](https://doi.org/10.1121/1.4779024) summarize: bubbles generated by raindrop splashes produce a loud, distinctive underwater sound used to detect and measure rainfall at sea, with "distinctive acoustic signatures for at least four raindrop sizes" enabling inversion of the drop-size distribution. Key results from that literature (Medwin/Nystuen school: [Medwin et al. 1992, "The anatomy of underwater rain noise"](https://doi.org/10.1121/1.403902); [Nystuen & Ostwald 1992](https://doi.org/10.1121/1.403551); [Barry, Nystuen & Lien 2005](https://doi.org/10.1121/1.1910283)):

- **Small drops (drizzle, ~0.8–1.2 mm)** do not entrain on primary impact; their "hiss" comes from **bubbles entrained by the crown/recoil of the crater** at normal incidence — narrow bubbles near 0.1–0.3 mm → **10–20+ kHz** peak (the famous drizzle spectral peak near 15–20 kHz; light rain in wind shifts with incidence angle — [Nystuen 1993](https://doi.org/10.1007/978-94-011-1626-8_49)).
- **Large drops (2–4.5 mm)** entrain a large bubble on cavity collapse → the lower-pitched "plunk," broadband with energy from ~1–15 kHz; laboratory signatures for drops 2.2–4.6 mm at terminal velocity form the basis of acoustic rain inversion ([Nystuen & Ostwald 1992](https://doi.org/10.1121/1.403551)).
- The [Barry–Nystuen–Lien 2005](https://doi.org/10.1121/1.1910283) semi-empirical model predicts ambient spectra 0.5–50 kHz from rainfall rate (2–200 mm/h) and wind speed (2–14 m/s) across five mechanism regimes.

Abovewater, the same physics radiates through the surface: drizzle = high "sizzle," heavy rain = fatter, lower roar because the bubble population shifts to larger radii.

### 3.4 Streams vs. rivers vs. waterfalls

**Turbulence alone is nearly silent.** Free turbulence radiates as Lighthill quadrupoles at efficiency $\sim M^5$; a stream at $U \sim 1$ m/s has $M \approx 3\times10^{-3}$ → efficiency $\sim 10^{-13}$. Practically all the audible sound of moving water comes from **air entrained as bubbles** — at drops, steps, breaking eddies, foam — each bubble ringing at its Minnaert frequency when excited by turbulence or by detachment. A stream's "babble" is literally a stochastic chorus of damped millimetric-bubble tones in the 1–8 kHz range. *[Spectral attribution per-bubble-size is mechanistically solid; per-stream measured decompositions are sparse.]*

**Bubbly mixture acoustics — Wood's equation.** Once bubbles are present in volume fraction $\alpha$, the mixture's acoustic properties change drastically: the mixture compressibility is the volume-weighted sum of phase compliances, giving (Wood 1930; modern treatment [Kieffer 1977](https://geology.illinois.edu/~skieffer/papers/SoundSpeed_JGR1977.pdf)):

$$\boxed{\;\frac{1}{\rho_m c_m^{2}} \;=\; \frac{1-\alpha}{\rho_w c_w^{2}} \;+\; \frac{\alpha}{\rho_a c_a^{2}}\;}$$

Verified numerically: $\alpha = 10^{-4}$ drops $c_m$ from 1480 m/s to ~920 m/s; $\alpha = 1\%$ → ~118 m/s; the minimum near $\alpha \approx 0.5$ is **≈ 23.5 m/s**, matching Kieffer's reported minimum of 24 m/s at 1 bar. Below individual-bubble resonance the low-frequency sound speed can fall below both pure-phase speeds ([Caflisch et al. 1985, JFM](https://www.math.ucla.edu/~caflisch/Pubs/Pubs1980-1989/Bubbles1JFM1985.pdf): with $\gamma = 1$, $R_0 = 10^{-3}$ cm, $C \approx 100$ m/s at modest void fractions; [Wilson 2005](https://doi.org/10.1121/1.1903024) measured ~5% dispersion in "Wood's dispersionless" regime). Consequences: bubbly clouds are strongly scattering and dispersive; acoustic energy below the bubble-resonance band is trapped and re-radiated slowly — contributing to the sustained "wash" after a breaking-wave crest.

**Waterfall structure.** A tall fall is a two-regime source: (i) the impact foam zone — a dense bubble cloud spanning mm–cm radii — emits a broadband roar whose low-frequency emphasis comes from the population of large bubbles (1 cm → 328 Hz!) plus collective cloud oscillations; (ii) the free-falling shear layer is a quadrupole source. The low-frequency rumble of big falls traces to **large turbulent scales**: in the Kolmogorov picture, turbulent kinetic energy cascades from the energy-containing eddies (size $L$, frequency $\sim U/L$) through the inertial range with the celebrated energy spectrum

$$E(k) \;\propto\; k^{-5/3}$$

down to the dissipation scale ([Kolmogorov 1941; standard treatment in any turbulence text — see e.g. the turbulence discussion in the granular-acoustics context of Bachelet et al. 2022, which notes vortices "grow through coalescence until they reach the thickness of the flow, then break up into smaller vortices, transferring flow energy towards smaller scales [Kolmogorov, 1941]"](https://doi.org/10.1002/essoar.10512904.1)). The acoustic imprint of turbulence is its low-pass-filtered counterpart: large eddies modulate pressure coherently at low frequency (the seconds-scale surge of a big fall), while the high-frequency content is fed by small scales and by bubbles. *[The direct acoustic-spectrum↔Kolmogorov mapping for waterfalls specifically is qualitative; the −5/3 law itself is standard.]*

### 3.5 Waves on a beach

A breaking wave is a **line source of bubble entrainment** sweeping shoreward: the plunging lip entrains a sheet of bubbles (broadband 500 Hz–5 kHz "crash"), then the swash — the thin sheet rushing up and draining back through sand or gravel — contributes friction-dominated hiss. On a **sand** beach, swash noise is shear flow through pore spaces and over individual grains: broadband hiss dominated by grain-scale impacts, low per-event amplitude, dense statistics. On a **pebble beach**, each wave is a chorus of **granular impacts** — hard stone-on-stone Hertz collisions (Section 5.1) with contact times of tens of µs → sharp clatter extending well above 10 kHz — the classic clatter sound. The backwash sorts and mobilizes grains, so the clatter statistics track flow energy. The perception literature notes beach swash is among the most preferred natural sounds (Galbrun & Ali below — water generally, with "large temporal variations" preferred).

### 3.6 Perceptual note: why water sounds restore and mask

Soundscape research gives game-audio-relevant constraints: [Galbrun & Ali 2013, JASA](https://doi.org/10.1121/1.4770242) found water sounds preferred at a level **similar to or not more than 3 dB below** road-traffic noise; **stream > fountain > waterfall** in preference; preferred features were **low sharpness and large temporal variation**; and water sounds *mismatch* traffic spectra (traffic 250 Hz–2 kHz vs. water 500 Hz–8 kHz) except high-flow waterfalls — water features work as **informational** rather than energetic maskers ([Galbrun & Calarco 2014](https://doi.org/10.1121/1.4897313); [Jeon et al. 2012](https://doi.org/10.1121/1.3681938) identify sharpness as the dominant psychoacoustic factor). For a game: stream loops should be mid-frequency, time-varying, and roughly level-matched to the ambient they replace.

---

## 4. Wind Sounds

### 4.1 Aeolian tones: the Strouhal law

Flow past a bluff body (wire, branch, blade of grass) separates and sheds alternate vortices — the **Kármán vortex street** — at a frequency governed by the **Strouhal number**. Strouhal measured it in 1878 by spinning wires; Rayleigh (1879, 1896) connected the tone to vortex shedding, and Phillips (1956) gave the aeroacoustic prediction via Lighthill/Curle (history in [Casalino et al. 2003](https://acoustique.ec-lyon.fr/publi/casalino_jsv03b.pdf)). For a circular cylinder:

$$\boxed{\;\mathrm{St} = \frac{f D}{U} \;\approx\; 0.2 \quad (250 \lesssim Re \lesssim 2\times10^5)\;}$$

with $f = \mathrm{St}\,U/D$. The regimes (from the [NASA Langley vortex-shedding computation study](https://ntrs.nasa.gov/api/citations/20040110243/downloads/20040110243.pdf) and [NovaSolver's engineering summary](https://novasolver.jp/en/tools/strouhal-vortex-shedding.html)): no shedding below $Re \approx 47$; Roshko fit $St \approx 0.21(1 - 21.2/Re)$ for $47 \le Re < 250$; flat ≈ 0.2 through the subcritical range; scattered through the supercritical drag-crisis regime; ≈ 0.27–0.3 transcritical.

**Worked example — the singing telephone wire** ($D = 2$ cm, $U = 5$ m/s, $\nu_{\text{air}} \approx 1.5\times10^{-5}$): $Re \approx 6700$ (subcritical, St ≈ 0.2) →

$$f = 0.2 \times \frac{5.0}{0.020} = 50\ \text{Hz}$$

— a low hum, exactly the familiar bass drone of wires in a gale. Harvard's demonstration ([Vortex Shedding in Air](https://sciencedemonstrations.fas.harvard.edu/presentations/vortex-shedding-air)) confirms the linear law with 1 mm wire: $f = 5/ (5 \times 10^{-3})$ → 1 kHz at 5 m/s, and shows lock-on to wire harmonics ("sings at 845 Hz, the 7th harmonic, starting ~4.75 m/s").

Radiation mechanism: each vortex pair imposes an **alternating lift force** on the body → **dipole** radiation with the $U^6$ power scaling of §1.2; if the shedding frequency coincides with a structural resonance of the wire/branch, amplitude builds dramatically ("lock-on"/stall flutter — [Landa & McClintock 2010](https://doi.org/10.1088/1751-8113/43/37/375101), who note the classical interpretation conflicts with long ropes sheared at different velocities per section, and model it as a self-oscillation). The shedding from real 3-D bodies decorrelates over a spanwise coherence length $L_c \approx 5d$, which broadens the tone.

### 4.2 Wind through gaps: edge tones and Helmholtz resonance

**Edge tone.** A jet impinging on a wedge sets up a feedback cycle: jet instability waves travel downstream, interact with the edge creating a dipole, whose field perturbs the jet origin, closing the loop. The classical stage formula of [Crighton 1992, JFM](https://doi.org/10.1017/s002211209200082x):

$$\frac{\omega b}{U_0} = \left(\frac{b}{h}\right)^{3/2}\left[4\pi\left(N - \tfrac{3}{8}\right)\right]^{3/2}$$

($b$ = half jet width, $h$ = stand-off distance, $N$ = stage number), though modern simulations support an exponent $n \approx 1$ in $f \propto U h^{-n}$ rather than the classical $n = 3/2$ ([Vaik et al. 2007](https://www.sciencedirect.com/science/article/abs/pii/S0142727X07000586); [Kwon 1998](https://doi.org/10.1121/1.423722) derives the practical low-speed law):

$$\mathrm{St} = \frac{f d}{U_0} = \frac{d}{h}\,\frac{N - 1/4}{1/0.6 + M_0}$$

— convection speed ≈ 0.6 $U_0$, phase criterion $h = \lambda(N + \varepsilon)$ with $\varepsilon = 1/4$ (Curle 1953). Frequencies jump between "stages" as $U$ or $h$ varies, with hysteresis. This is the whistle of wind across a gap, a slit, or a fence slat — and the sound-production element of flutes and organ pipes ([Fletcher 1979] via the edge-tone literature; see also [Howe/Howe-type resonator analysis in "Edge, cavity and aperture tones at very low Mach numbers", JFM 1997](https://www.cambridge.org/core/journals/journal-of-fluid-mechanics/article/abs/edge-cavity-and-aperture-tones-at-very-low-mach-numbers/014E221E77C245F3E06591032EFF2275), which shows shear-layer oscillations identify with poles of a Rayleigh-conductivity response and that an attached acoustic resonator can "speak" by extracting energy from the mean flow).

**Helmholtz resonance.** Wind gusting across a cavity mouth (bottle, cave, hollow tree) couples to the cavity's Helmholtz mode, $f_H = \frac{c}{2\pi}\sqrt{A/(V L_{\text{eff}})}$ — a monopole-class resonator that, once excited by shear-layer feedback, radiates efficiently. *[The cavity/shear-layer coupling at the mouth follows the same feedback physics as the edge tone above; the bottle-resonator frequency formula is standard acoustics.]*

### 4.3 Broadband wind noise

Direct turbulence pressure fluctuations on the ear or microphone ("pseudo-noise," [Schomer & Beck 2010](https://doi.org/10.3397/1.3371961)) set the noise floor of any open-air measurement and of the in-ear experience of standing in wind: low-frequency dominated (gust scale), level rising steeply with speed.

---

## 5. Solid Objects: Impacts and Resonance

### 5.1 Impact theory: the force pulse and its spectrum

A struck body is excited by the force-time curve of the contact. For elastic spheres, **Hertz contact theory** gives the force–deformation law $F = k\,\alpha^{3/2}$, from which the contact duration and peak force follow ([McLaskey & Glaser 2010, JASA, "Hertzian impact: Experimental study of the force pulse and resulting stress waves"](https://doi.org/10.1121/1.3466847); [McLaskey 2009, SPIE](https://courses.cit.cornell.edu/mclaskey/pubs/SPIE2009.pdf)):

$$t_c = 4.53\left(\frac{5 m_1 v_0^2}{8 k_1}\right)^{2/5}\frac{1}{v_0}, \qquad k_1 = \frac{4}{3}E^*\sqrt{R}, \qquad f_{\max} \propto \frac{1}{2 t_c} \;\propto\; v_0^{-0.2}\, R^{-1}\, E^{*-2/5}$$

The exponent structure — **contact time $\propto v_0^{-0.2}$** — is the scaling the task asked for: faster impacts excite *slightly* higher frequencies, but material and size dominate. Hunter (1957) approximated the pulse as a half-sine; Reed (1985) corrected it to a "sin^{3/2}" shape, and McLaskey & Glaser verified this via the zeros of the radiated-wave spectra, which sit at $f_{\text{zero},n} \approx (n + 0.75)/t_c$. **Frequencies up to ~$1/t_c$ are excited.**

**Numerical verification** (Hertz formula, identical-material pairs):

| Case | Contact time | Spectral cutoff ~1/(2t_c) |
|---|---|---|
| 1.58 mm steel ball, 31 mm drop on steel (measured by McLaskey & Glaser) | 5.5 µs measured; **6.8 µs computed** ✓ | ~90 kHz |
| 10 mm steel sphere on steel, 1 m/s | 41 µs | ~12 kHz |
| 10 mm wooden sphere on wood, 1 m/s | 45 µs | ~11 kHz |
| 20 mm wooden sphere on wood, 1 m/s | 91 µs | ~5.5 kHz |

Steel-on-steel radiates energy into the hundreds of kHz (McLaskey & Glaser measured appreciable energy flux above 700 kHz for µs-scale contacts); wood-on-wood of the same size cuts off an order of magnitude lower and couples less into air. Peak radiated stress-wave amplitude scales with **change of momentum**, not energy — McLaskey & Glaser stress that impulse, not kinetic energy, is the source-strength quantity (the seismic-moment analogy). For two colliding steel balls, measured peak sound pressure $\propto v_0^{1.2}$ (Nishimura & Takahashi, via [Takahashi/Kona impact-sound study](https://www.jstage.jst.go.jp/article/kona/7/0/7_1989004/_pdf/-char/en)).

**Plate impact:** the same impulse applied to a thin plate excites plate modes; the plate-impact literature ([J. Acoust. Soc. Japan impact-sound study](https://www.jstage.jst.go.jp/article/ast1980/1/2/1_2_121/_pdf)) confirms: shorter impact duration → higher modes excited and higher sound pressure level; a steel ball on a plate excites all modes up to 20 kHz, a rubber ball (long contact) excites only the lowest.

### 5.2 Modal vibration of bars and plates

**Euler–Bernoulli beam.** Transverse bending waves are dispersive, $\omega = \beta^2\sqrt{EI/\rho A}$ (with wavenumber $\beta$), so finite-beam mode frequencies are set by the roots $\beta_n$ of the boundary-condition equation:

$$\omega_n = (\beta_n L)^2 \sqrt{\frac{EI}{\rho A}}\,\frac{1}{L^2}$$

Standard configurations ([Reinert, vibrationdata beam tutorial](https://www.vibrationdata.com/tutorials2/beam_rev_U.pdf)): pinned–pinned $(\beta_n L)^2 = n^2\pi^2$ (harmonic! $f_n = n^2 f_1$); free–free $(\beta_n L)^2 \in \{22.37, 61.67, 120.9, \dots\}$ → mode ratios ≈ 1 : 2.76 : 5.40 : 8.93. **This inharmonicity is why an untuned wooden bar sounds "woody" rather than string-like**; marimba makers carve an undercut in the bar to detune the free–free modes to the musical ratios **1 : 4 : 10** (fundamental, two octaves, three octaves + major third) — see [Beaton & Scavone, ISMA 2019](https://pub.dega-akustik.de/ISMA2019/data/articles/000040.pdf) and [Suits 2001, Am. J. Phys.](https://doi.org/10.1119/1.1359520), who works through the bar physics and finite-element tuning.

**Kirchhoff–Love plates.** The plate equation is fourth-order in space (biharmonic): $D \nabla^4 w + \rho h\, \ddot w = 0$ with flexural rigidity $D = E h^3 / 12(1-\nu^2)$ ([Manzanares-Martínez et al., "Flexural vibrations of a rectangular plate"](https://www.sciencedirect.com/science/article/abs/pii/S0022460X10004001), which quotes the equation and $D$; the definitive historical treatment of free rectangular plates is [Warburton 1954](https://journals.sagepub.com/doi/10.1243/PIME_PROC_1954_168_040_02) and [Leissa 1973](https://doi.org/10.1016/s0022-460x(73)80371-2)). Free-edge plates produce the **Chladni patterns** — Kirchhoff (1850) showed these are eigenmodes of the biharmonic operator with free boundary conditions; the rectangular free-plate problem "resisted attack" (Rayleigh) until Ritz (1909); a modern pedagogical/numerical treatment with the full eigenvalue tables is [Gander & Kwok, "Chladni Figures and the Tacoma Bridge"](https://giref.ulaval.ca/~fkwok/docs/chladni-print.pdf).

Mode-density argument: a plate has *two* mode indices $(m, n)$, so modal density grows linearly with frequency (vs. the sparser bar spectrum), and the free-plate modes are strongly **inharmonic** — a struck plate (cymbal, shell, wood plank) sounds "crashy" because dozens of unrelated frequencies are excited at once, whereas a bar's sparse low modes read as a pitch. Semi-exact closed-form plate frequencies by phase-closure: [Leamy 2015, J. Appl. Mech.](https://doi.org/10.1115/1.4032183).

### 5.3 Material acoustic fingerprints: damping loss factor

Material damping is quantified by the **loss factor** $\eta$ (fraction of stored energy dissipated per radian; $Q = 1/\eta$; $\eta = 2\xi$ where $\xi$ is the viscous damping ratio — [Irvine, "Damping Properties of Materials"](https://www.vibrationdata.com/tutorials_alt/damping.pdf)). Representative values compiled from engineering tables ([COMSOL damping overview](https://www.comsol.com/blogs/damping-in-structural-dynamics-theory-and-sources); [Irvine](https://www.vibrationdata.com/tutorials_alt/damping.pdf); [Challenging Glass proceedings](https://proceedings.challengingglass.com/index.php/cgc/article/download/74/74/685)):

| Material | Loss factor η (typical range) | Perceptual character |
|---|---|---|
| Aluminum | 10⁻⁴–10⁻² | bright, metallic |
| Steel | 10⁻⁴–10⁻² (mostly ~10⁻³) | "ring" (Q ~ 500–1000) |
| Glass | 10⁻⁵–2×10⁻³ (some sources to 6×10⁻⁴) | sharp snap + faint ring |
| Concrete | 0.02–0.05 | dull "clack" |
| Plywood / wood | 0.01–0.03 (structural timber higher) | "thud" / short "tock" |
| Rubber / polymers | 0.05–2 | dead |

So: metal rings (η ~ 0.001 → tens to hundreds of visible cycles); wood's cellular, anisotropic structure (parenchyma cells, ray cells, moisture) puts it at η ~ 0.01–0.03 → a few cycles to tens — "thud" and "crack"; stone/concrete sits between but with high density and impedance, giving hard broadband clicks; glass combines very low damping with brittle fracture — a pane hit hard enough fails catastrophically, and the fracture itself is a step-release force (a sharp snap) followed by ringing of the remaining fragments.

### 5.4 Footsteps on surfaces

The **heel-strike force pulse** has a rise time of order a few ms (ground-reaction literature; *[exact rise-time figure unsourced here — mechanism-plausible]*), so the direct airborne component is a broadband thump rolling off above a few hundred Hz; the surface then filters and re-radiates.

- **Grass / soil (soft):** the force is applied to a layered, lossy, porous medium — damped low-frequency thump; the grass layer itself adds the fracture/brush noise of §2.4. Fresh snow is famously absorptive (porous layer, multiple scattering in the ice matrix).
- **Gravel:** each grain rearrangement is a **granular collision chain** — Hertzian contacts (above) with force transmission through **force chains**. Granular-flow acoustics ([Bachelet et al. 2022](https://doi.org/10.1002/essoar.10512904.1), laboratory dense flows) shows: high-frequency (1–50 kHz) waves originate in **inter-particle collisions** (Hertzian, radiated elastic power scaling with the 8/5 power of granular temperature), while low frequencies (20–60 Hz) come from coherent dilation/compression oscillations of the packing. A footstep on gravel = superposition of hundreds of small impact pulses, each broadband, statistics set by grain size/shape/roundness — the classic randomized clatter.
- **Wood floors:** the foot force excites **panel modes** of the floorboards/sheeting (Kirchhoff plate modes, above) — a resonant, slightly pitched "boink/thud" colored by the joist system; airborne transmission plus structure-borne radiation.
- **Concrete/hard pavement:** short contact, high stiffness → broadband click; the *room/space* reflections then dominate the perceived character (outdoors: single click; indoors: reverberant tail).

### 5.5 The snow squeak/crunch

The literature: [Colbeck 1992, "A review of the processes that control snow friction"](http://hdl.handle.net/11681/2667) attributes cold-snow crunch to **fracture of ice asperities**: "Ice is more readily deformed at higher temperatures, especially above about −8 °C. Below that temperature fracture of the asperities might be more likely… (Fracture of bonds at lower temperatures may explain the crunching sounds made by walking on cold snow.)"

The popular-physics consensus (University of Wisconsin [WxWise "Squeaking Snow"](http://cimss.ssec.wisc.edu/wxwise/squeak.html); [Scientific American 2015 interview with MIT's W. Craig Carter](https://www.scientificamerican.com/article/why-does-snow-squeak-when-stepped-on/); [Live Science 2024](https://www.livescience.com/physics-mathematics/why-does-snow-squeak-when-you-walk-on-it)):

- **Above about −10 °C (14 °F):** pressure + frictional heating produces a liquid-like layer; grains slide and deform plastically — nearly silent or a soft crumple.
- **Below −10 °C:** no lubricating melt; the **sintered ice "necks"** (Carter's Rice-Krispies-treat picture: nanometer-scale welds grown between crystals over hours) and the grains themselves **fracture brittlely** under the concentrated stresses of the boot — sequential fracture of thousands of bonds ("like dragging a fingertip across the teeth of a comb" — Carter).
- Temperature grades the timbre: a secondary source ([library.ug review, "Snow crackling as a natural phenomenon"](https://library.ug/m/articles/view/Snow-crackling-as-a-natural-phenomenon) — treat as a tertiary compilation, not primary data) reports: 0 to −6 °C almost no crackle (plastic), −6 to −15 °C low-frequency crackle intensifying, below −15 °C high-frequency sharp crackle ("like crushed polystyrene"), consistent with ice Young's modulus and brittleness rising as temperature falls.

Mechanism summary for the engine: snow crunch = a *temperature-parameterized* avalanche of brittle micro-fractures — colder → more impulses per step, higher frequencies, higher amplitude.

---

## 6. Fire and Crackle

**Wood crackle** is an acoustic-emission phenomenon of the fuel, not the flame: "the crackling sound from burning a log of wood originating from evaporation of small pockets of trapped water in the material" ([Springer fire-detection review 2022](https://link.springer.com/article/10.1007/s10694-022-01307-1)). A heated moisture pocket in a wood cell flash-boils; the steam pressure spike ruptures the surrounding cell structure — a miniature explosion whose elastic transient radiates through the char and couples to air. Per-event spectrum is broadband with content set by the fracture size; the fire-detection literature measured **crackle impulse energy concentrated in 6–15 kHz**, with the band correlating with **plant species and water stress** ([Yedinak et al. 2017, "Vegetation effects on impulsive events in the acoustic signature of fires", JASA](https://doi.org/10.1121/1.4974199)) — dry, resinous fuels crackle sharper and denser. Crackling is thus a *moisture meter*: wet wood spits occasionally and low; dry wood crackles continuously and bright.

**The "whoomph" of ignition / flame noise** is combustion-generated turbulence noise: unsteady heat release → unsteady volumetric expansion (a monopole-class source in the reacting region) plus plume turbulence. Laboratory scaling: for premixed flames, overall acoustic power scales with heat release and the peak frequency follows a Strouhal law $f_{\text{peak}} = \mathrm{St}\, U_{\text{ave}}/L_f$ (flame length) ([Rajaram, Gray & Lieuwen 2006](https://doi.org/10.2514/6.2006-2612)); for open solid-fuel fires, measured sound levels peak in the **infrasound** regime and roll off through the audible range, tracking heat-release rate ([Szoke et al. 2025, JASA](https://doi.org/10.1121/10.0041123); [Guan, Fang & Jiang 2013](https://doi.org/10.2991/icssr-13.2013.184) measured wood combustion audio below ~40 dB with dominant content < 20 Hz). The ignition "whoomph" is the transient of that monopole source switching on — a low-frequency pressure surge. For game audio: fire = continuous low-level infrasonic-to-low roar (amplitude ∝ intensity) + stochastic crackle impulses (rate & brightness ∝ dryness, spectrum 6–15 kHz).

---

## 7. Singing / Booming Sand Dunes

Avalanches on the slip face of certain dunes emit a loud, coherent drone — 70–110 Hz with harmonics and a characteristic tremolo — audible for minutes. The modern literature (extensively reviewed in [Hunt & Vriend 2010, Annual Review of Earth and Planetary Sciences, "Booming Sand Dunes"](https://doi.org/10.1146/annurev-earth-040809-152336) and [Andreotti 2012, "Sonic sands", Rep. Prog. Phys.](https://doi.org/10.1088/0034-4885/75/2/026602)):

- **Measured frequencies 70–110 Hz** plus higher harmonics (Hunt & Vriend: "a dominant frequency between 70 and 110 Hz, as well as higher harmonics"); amplitudes reach ~100–105 dB near the avalanche core (Andreotti 2004).
- **Source mechanism:** the avalanche is a cm-thick granular shear flow over static sand; the shear band at the interface converts sliding energy into **coherent surface elastic (Rayleigh-like, elliptically polarized) waves** localized in the top ~10 cm of dry sand, which radiate into air as the surface oscillates ([Andreotti 2004, PRL, "The Song of Dunes as a Wave-Particle Mode Locking"](https://www.phys.ens.psl.eu/~foldingslidingstretchinglab/papers/A20_PhysRevLett_93_238001.pdf)).
- **Frequency selection — two competing theories:** (i) the shear-rate/collision-rate scaling $f \sim \sqrt{g/d}$ (Andreotti 2004 measured $\dot\gamma \approx 100\ \mathrm{s^{-1}}$ matching $f \approx 100$ Hz for $d = 180\ \mu$m; the $\sqrt{g/d}$ scaling with the collision rate in the shear band); and (ii) the **waveguide resonance** of the dry surface layer between the atmosphere and a higher-velocity substrate (Vriend's field measurements: booming frequency fixed by the depth of the dry, loose surficial layer, phase velocities 200–350 m/s, waves at the critical angle interfering constructively — [Vriend/Andreotti et al. 2007, GRL "Solving the mystery of booming sand dunes"](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2007GL030276)). Andreotti & Bonneau's later linear-stability analysis ([PRL 2009, "Booming Dune Instability"](https://doi.org/10.1103/physrevlett.103.238001)) shows friction at the shear band makes the granular surface flow **linearly unstable to growing elastic waves** — an acoustic amplifier: energy pumped from shearing into coherent modes, saturating when surface-grain acceleration reaches $g\cos\theta$ (grains take off).
- Power budget (Andreotti 2004): a 4 m × 1 m avalanche extracts ~3 kW from gravity, ~10 W goes into elastic waves, ~150 mW into airborne sound — a coherent-acoustic output of remarkably low efficiency but high loudness due to source size.
- Requirements: dry, well-sorted, smooth-surfaced (often silica-gel-coated) grains, sufficient layer depth and dune size; moisture kills it — explaining seasonality and site specificity (Hunt & Vriend; Vriend et al. 2007).

For the engine: dune boom is a *sustained* 70–110 Hz tone + harmonics with slow tremolo (the low-frequency beating of multiple unstable modes, per Andreotti & Bonneau 2009), gated by avalanche initiation.

---

## 8. Thunder: The Capstone Synthesis

Thunder is the one natural sound where *all* the preceding physics meets a single event: a thermal energy pulse → shock wave → N-wave → multipath propagation → spectral filtering by the atmosphere.

**Source.** The lightning return stroke heats a ~cm-scale channel to ~30,000 K in microseconds; the channel overpressurizes and expands as a **cylindrical strong shock** which decays to a weak shock and then an acoustic N-wave ([Few 1969; review in Anderson et al. 2023, "Acoustical Power of Lightning Flashes", JGR](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2023JD038714); near-field triggered-lightning measurements confirm each return stroke begins with an **N-shaped shock waveform** followed by low-frequency oscillations — [Wang et al. 2022, Phys. Fluids](https://doi.org/10.1063/5.0110866)). Spectral confirmation of the cylindrical shock theory at 70 m: [Depasse 1994, "Lightning acoustic signature", JGR](https://doi.org/10.1029/94jd01986).

**Characteristic frequency vs. energy.** Few's relation links the peak frequency of the thunder power spectrum to the energy per unit length $\mathcal{E}_l$ of the channel:

$$f_m \;=\; 0.63\, c_0 \left(\frac{P_0}{\mathcal{E}_l}\right)^{1/2}$$

(quoted in both the [thunder-feature frequency analysis](https://www.researchgate.net/publication/309982045_Frequency_Analysis_of_Thunder_Features) and [Anderson et al. 2023](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2023JD038714)) — more energetic strokes radiate *lower* peak frequencies; the historical "dominant 200 Hz peak" ([Few et al. 1967](https://doi.org/10.1029/jz072i024p06149)) reflects typical stroke energies.

**Audible spectrum.** Measured fundamentals of audible thunder lie below ~250 Hz; the subjective classes map to frequency bands and channel geometry ([Frequency Analysis of Thunder Features](https://www.researchgate.net/publication/309982045_Frequency_Analysis_of_Thunder_Features)): **peals ≈ 40–100 Hz** (direct arrivals from energetic channel sections), **claps ≈ 40–160 Hz** (near, high-amplitude), **rumbles ≈ 25–80 Hz** (mean fundamental 63 Hz) — reflections from tortuous channel segments and distant bolts. The 2018 reconstruction study ([Lacroix, Farges, Marchiano & Coulouvrat, JGR 2018](https://doi.org/10.1029/2018jd028814)), analyzing 27 natural flashes over 0.1–180 Hz, shows thunder **infrasound originates dominantly from return strokes**, with spectral variability dominated by propagation distance.

**Why distant thunder rumbles low.** Three mechanisms compound:

1. **Air absorption** — classical and molecular relaxation absorption rises steeply with frequency; over kilometers, the >100 Hz content is preferentially removed, leaving the 25–80 Hz rumble. *(This ties to the separate "air absorption" section of the host document: the same physics that makes distant grass rustle inaudible makes distant thunder a pure bass drone.)*
2. **Channel tortuosity** — the flash is a kinked, branching path kilometers long; each element is a source at a different distance, so the arrival train extends over many seconds (the "string-of-pearls" model — Few 1969; Ribner & Roy 1982; modern treatments in Anderson et al. 2023).
3. **Multiple return strokes and ground/ionosphere reflections** — each subsequent stroke re-excites the channel; reflections stretch the tail further ([Lacroix et al. 2018](https://doi.org/10.1029/2018jd028814); the frequency-analysis study above).

Near thunder = a sharp crack (the directly-arriving N-wave from the nearest channel segment, with broadband content from the steep shock front) decaying into the extended rumble. The full perceptual arc — crack → peals → claps → low rumble — is a single source filtered by distance, geometry, and atmospheric absorption: the complete taxonomy of Section 1 playing out in one event.

---

## 9. Synthesis for Game Sound Design (informal corollary)

The physics above maps cleanly onto synthesis parameters:

- **Bubbles first:** any water event's pitch content is a comb of Minnaert tones $f_0 = 3.28\ \text{kHz}\cdot(\text{mm}/R)$ — drop size distribution → spectrum directly. Damped sinusoid packets, Q ~ 10–30, excited at detachment events.
- **Rustle:** filtered noise with a 4 kHz Gaussian bump on a pink floor, amplitude driven by $u^{1.5}$ with asymmetric 0.2 s/1.0 s attack/decay smoothing; only upwind canopy edges radiate.
- **Wind tones:** $\ f = 0.2 U/D$ per obstacle diameter; broadband turbulence noise scales as $U^6$–$U^8$ — a strong justification for level automation on wind speed.
- **Impacts:** contact-time spectral cutoff $1/(2t_c)$ (steel ~90 kHz, wood ~5–11 kHz for cm-scale); loss factor per material sets ring length ($Q = 1/\eta$).
- **Snow/gravel/grass footsteps:** avalanche/cascade stochastic impulse trains whose rate, brightness, and amplitude are parameterized by temperature (snow), grain size (gravel), and dryness (grass).
- **Fire:** continuous low-frequency roar scaling with intensity + stochastic 6–15 kHz crackle impulses scaling with dryness.
- **Dunes:** sustained 70–110 Hz tonal drone with tremolo, gated by avalanche state and dryness.
- **Thunder:** broadband crack convolving to a 25–80 Hz rumble over distance via air absorption + source extent.

---

### Source register (primary URLs)

1. Lighthill 1954 — https://doi.org/10.1098/rspa.1954.0049
2. Lighthill's eighth power law (overview) — https://en.wikipedia.org/wiki/Lighthill%27s_eighth_power_law
3. Mašović & Sarradj 2020 — https://doi.org/10.3390/acoustics2030035
4. Routh & Musielak 2025 — https://doi.org/10.3390/fluids10060156
5. Minnaert 1933 — https://www.uio.no/studier/emner/matnat/math/MEK4480/v22/beskjeder/xvi-on-musical-air-bubbles-and-the-sounds-of-running-water.pdf
6. Brennen, bubble natural frequencies — http://brennen.caltech.edu/fluidbook/multiphase/bubblegrowthandcollapse/bubblenaturalfrequencies.pdf
7. Leighton-based bubble tutorial — http://www.vibrationdata.com/tutorials/bubble.pdf
8. Gontier, Minnaert resonance — https://www.ceremade.dauphine.fr/~gontier/Presentations/2018_01_16_Ceremade.pdf
9. Fitzpatrick 2018 (ETH) — https://doi.org/10.3929/ethz-b-000287325
10. Phillips, Agarwal & Jordan 2018 — https://www.nature.com/articles/s41598-018-27913-0 (+ https://www.cam.ac.uk/research/news/what-causes-the-sound-of-a-dripping-tap-and-how-do-you-stop-it)
11. Nystuen 2003 — https://doi.org/10.1121/1.4780502
12. Barry, Nystuen & Lien 2005 — https://doi.org/10.1121/1.1910283
13. Nystuen & Ostwald 1992 — https://doi.org/10.1121/1.403551
14. Medwin et al. 1992 — https://doi.org/10.1121/1.403902
15. Kieffer 1977 — https://geology.illinois.edu/~skieffer/papers/SoundSpeed_JGR1977.pdf
16. Caflisch et al. 1985 — https://www.math.ucla.edu/~caflisch/Pubs/Pubs1980-1989/Bubbles1JFM1985.pdf
17. Wilson 2005 — https://doi.org/10.1121/1.1903024
18. Bolin et al., vegetation noise prediction — https://dael.euracoustics.org/bin/EAA/aaua_dl?document_id=64838
19. Heutschi 2014 — https://www.dora.lib4ri.ch/empa/dload/empa:6079/PDF/Heutschi-2014-Auralization_of_wind_turbine_noise-(published_version).pdf
20. Schomer & Beck 2010 — https://doi.org/10.3397/1.3371961
21. Li et al. 2024 (leaf-density audio) — https://www.sciencedirect.com/science/article/abs/pii/S0168169924004538
22. Fricke 1984 — https://doi.org/10.1016/0022-460x(84)90380-8
23. Bullen & Fricke 1982 — https://doi.org/10.1016/0022-460x(82)90387-x
24. Casalino et al. 2003 (aeolian tones) — https://acoustique.ec-lyon.fr/publi/casalino_jsv03b.pdf
25. NASA vortex-shedding computation — https://ntrs.nasa.gov/api/citations/20040110243/downloads/20040110243.pdf
26. Fourès et al. 2024 — https://ar5iv.labs.arxiv.org/html/2404.11434
27. Harvard vortex-shedding demo — https://sciencedemonstrations.fas.harvard.edu/presentations/vortex-shedding-air
28. Landa & McClintock 2010 — https://doi.org/10.1088/1751-8113/43/37/375101
29. Crighton 1992 (edge tone) — https://doi.org/10.1017/s002211209200082x
30. Vaik et al. 2007 — https://www.sciencedirect.com/science/article/abs/pii/S0142727X07000586
31. Kwon 1998 — https://doi.org/10.1121/1.423722
32. Howe, edge/cavity/aperture tones (JFM 1997) — https://www.cambridge.org/core/journals/journal-of-fluid-mechanics/article/abs/edge-cavity-and-aperture-tones-at-very-low-mach-numbers/014E221E77C245F3E06591032EFF2275
33. McLaskey & Glaser 2010 — https://doi.org/10.1121/1.3466847 (+ https://courses.cit.cornell.edu/mclaskey/pubs/SPIE2009.pdf)
34. Kona impact-sound study — https://www.jstage.jst.go.jp/article/kona/7/0/7_1989004/_pdf/-char/en
35. Impact-sound plate study (Acoust. Sci. Tech.) — https://www.jstage.jst.go.jp/article/ast1980/1/2/1_2_121/_pdf
36. Reinert beam tutorial — https://www.vibrationdata.com/tutorials2/beam_rev_U.pdf
37. Suits 2001 (xylophone/marimba bars) — https://doi.org/10.1119/1.1359520
38. Beaton & Scavone 2019 — https://pub.dega-akustik.de/ISMA2019/data/articles/000040.pdf
39. Manzanares-Martínez et al. (plate modes) — https://www.sciencedirect.com/science/article/abs/pii/S0022460X10004001
40. Gander & Kwok (Chladni) — https://giref.ulaval.ca/~fkwok/docs/chladni-print.pdf
41. Warburton 1954 — https://journals.sagepub.com/doi/10.1243/PIME_PROC_1954_168_040_02
42. Leamy 2015 — https://doi.org/10.1115/1.4032183
43. COMSOL damping — https://www.comsol.com/blogs/damping-in-structural-dynamics-theory-and-sources
44. Irvine damping tables — https://www.vibrationdata.com/tutorials_alt/damping.pdf
45. Glass damping (Challenging Glass) — https://proceedings.challengingglass.com/index.php/cgc/article/download/74/74/685
46. Bachelet et al. 2022 (granular acoustics) — https://doi.org/10.1002/essoar.10512904.1
47. Colbeck 1992 (snow friction) — http://hdl.handle.net/11681/2667
48. UW WxWise squeaking snow — http://cimss.ssec.wisc.edu/wxwise/squeak.html
49. SciAm squeak interview — https://www.scientificamerican.com/article/why-does-snow-squeak-when-stepped-on/
50. Live Science snow squeak — https://www.livescience.com/physics-mathematics/why-does-snow-squeak-when-you-walk-on-it
51. library.ug snow crackle — https://library.ug/m/articles/view/Snow-crackling-as-a-natural-phenomenon
52. Yedinak et al. 2017 (fire crackle) — https://doi.org/10.1121/1.4974199
53. Springer fire detection 2022 — https://link.springer.com/article/10.1007/s10694-022-01307-1
54. Szoke et al. 2025 — https://doi.org/10.1121/10.0041123
55. Guan et al. 2013 — https://doi.org/10.2991/icssr-13.2013.184
56. Rajaram et al. 2006 (combustion noise) — https://doi.org/10.2514/6.2006-2612
57. Hunt & Vriend 2010 (booming dunes) — https://doi.org/10.1146/annurev-earth-040809-152336
58. Andreotti 2012 (sonic sands) — https://doi.org/10.1088/0034-4885/75/2/026602
59. Andreotti 2004 — https://www.phys.ens.psl.eu/~foldingslidingstretchinglab/papers/A20_PhysRevLett_93_238001.pdf
60. Vriend et al. 2007 — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2007GL030276
61. Andreotti & Bonneau 2009 — https://doi.org/10.1103/physrevlett.103.238001
62. Anderson et al. 2023 (thunder power) — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2023JD038714
63. Wang et al. 2022 — https://doi.org/10.1063/5.0110866
64. Depasse 1994 — https://doi.org/10.1029/94jd01986
65. Lacroix et al. 2018 — https://doi.org/10.1029/2018jd028814
66. Few et al. 1967 — https://doi.org/10.1029/jz072i024p06149
67. Thunder features frequency analysis — https://www.researchgate.net/publication/309982045_Frequency_Analysis_of_Thunder_Features
68. Galbrun & Ali 2013 — https://doi.org/10.1121/1.4770242
69. Galbrun & Calarco 2014 — https://doi.org/10.1121/1.4897313
70. Jeon et al. 2012 — https://doi.org/10.1121/1.3681938
71. Van Renterghem et al. (vegetation belts) — https://users.ugent.be/~tvrenter/publicaties/vegbelts.pdf
