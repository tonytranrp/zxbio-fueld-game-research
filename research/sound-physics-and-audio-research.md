# Sound: Physics, Perception, Signals, and Interactive Audio Systems

> **Provenance.** Written 2026-09-06 for the voxel-engine research repository. Five delegated research agents ran in parallel, each with live Exa web search/fetch, each producing one of the five parts below; every part carries its own provenance note and per-claim source URLs, and ~180 distinct sources are cited across the whole document. All key numeric constants were cross-verified two independent ways (agent re-derivation plus local PowerShell anchor computations) and agree: $c = \sqrt{\gamma R T / M} = 343.19$ m/s at 20 °C (vs. the 331.4 + 0.6T fit, 343.4); the Sabine constant 24·ln10/c = 0.1611 s·m⁻¹; Minnaert $R = 1$ cm → 328 Hz, $R = 1$ mm → 3283 Hz; ear-canal quarter-wave resonance $c/4L = 3430$ Hz at $L = 2.5$ cm; 16-bit quantization SNR = $6.02N + 1.76 = 98.08$ dB (not the folk "96 dB"); Woodworth max ITD $(a/c)(1+\pi/2) = 0.656$ ms at $a = 8.75$ cm; mel anchor $2595\log_{10}(1+1000/700) = 1000.0$; ISO 9613-1 air absorption 4.7 / ~30 / ~159 dB/km at 1/4/10 kHz (20 °C, 50 % RH); pink noise −3.01 dB/oct; two equal incoherent sources +6.02 dB. Overlapping claims between parts (Minnaert/Wood between Parts 1 and 3, ERB/Bark between Parts 2 and 4, Sabine/reverb between Parts 1, 2 and 5, air absorption between Parts 1 and 5) were re-checked for consistency during the merge and agree. Where sources disagree, the disagreement is reported inline rather than averaged away — e.g. the 22×/25×/34× middle-ear pressure-gain disagreement in Part 4, and Sony's platform loudness targets spanning −24 to −18 LUFS across eras in Part 5.
>
> **What this is.** A self-contained reference on how sound works in practice — the mathematics of acoustic waves and propagation, the mathematics of sound as a signal, the physics of how natural materials generate their sounds, the biomechanics and psychophysics of human hearing, and the engineering of real-time game audio — written at the density level of the repo's other research documents (see [`water-physics-and-wave-simulation.md`](water-physics-and-wave-simulation.md) and [`grass-rendering-research.md`](grass-rendering-research.md) for the house style: derivations shown, key results boxed, ranges where the literature gives ranges, honest about disagreements). Formulas render as GitHub-flavored LaTeX via `$...$` and `$$...$$`.
>
> **Reading order.** Part 1 is the physical substrate (waves, impedance, dB, absorption, outdoor propagation, rooms, Doppler, nonlinearity). Part 2 is what a sound *is* once it is a signal (Fourier, sampling, quantization, noise, filters, modulation, synthesis, perceptual scales, reverb math). Part 3 asks why specific things in the world sound the way they do (grass, water, wind, impacts, fire, dunes, thunder). Part 4 is the receiver (ear mechanics, cochlear hydrodynamics, loudness, masking, spatial hearing). Part 5 closes with the interactive-audio systems that turn all of the above into a game soundscape, and an engineering recommendation for this engine. Parts are self-contained and cross-referenced; each part's internal numbering stands on its own.

## Contents

| Part | Title | What it covers |
|---|---|---|
| 1 | [Acoustic wave physics and propagation](#part-1--acoustic-wave-physics-and-sound-propagation-in-environments) | Wave equation derived from conservation laws; speed of sound; impedance; SPL/dB; air absorption (ISO 9613-1); reflection, transmission, diffraction; outdoor propagation (ISO 9613-2); room acoustics (Sabine/Eyring, modes, Schroeder frequency); Doppler; nonlinear steepening |
| 2 | [The mathematics of sound as a signal](#part-2--the-mathematics-of-sound-as-a-signal-fourier-analysis-sampling-noise-and-synthesis) | Fourier series/transform and the convolution theorem; sampling and Nyquist; quantization noise; noise colors and 1/f generation; biquad filters; AM/FM and Bessel functions; additive/subtractive/physical-modeling synthesis; mel/Bark/ERB/LUFS scales; Schroeder/FDN/convolution reverb |
| 3 | [The physics of natural sound generation](#part-3--the-physics-of-natural-sound-generation-mechanisms-math-and-spectra) | Multipole source hierarchy (Lighthill); grass rustle; water (Minnaert bubbles, rain, dripping); wind and edge tones; impacts and resonance (Hertz contact, plate modes); fire crackle; booming dunes; thunder |
| 4 | [How the human ear works and how humans perceive sound](#part-4--how-the-human-ear-works-and-how-humans-perceive-sound) | Ear canal resonance; middle-ear transformer; cochlear traveling wave and the Greenwood map; outer hair cells and the Hopf amplifier; loudness (ISO 226, sones); critical bands, Bark and ERB; masking; ITD/ILD, HRTFs, Haas effect, distance perception |
| 5 | [Real-time game audio systems](#part-5--real-time-game-audio-spatialization-propagation-and-procedural-sound) | Middleware architectures; pan laws, VBAP, Ambisonics, HRTF rendering; distance/occlusion/diffraction; parametric and convolution reverb; Doppler; voice management and LUFS loudness; procedural audio practice; a minimum-viable custom audio stack and upgrade path |

---

# Part 1 — Acoustic Wave Physics and Sound Propagation in Environments


## Provenance

Researched 2026-09-06 via Exa web search/fetch (16 query batches, ~60 sources examined, ~40 cited below). Every formula and numeric constant was checked against at least one live web source, and the key constants were independently recomputed: √(1.4·8.314·293.15/0.02897) = **343.19 m/s** (exact ideal-gas value at 20 °C, matching [Wikipedia](https://en.wikipedia.org/wiki/Speed_of_sound) and [LibreTexts](https://phys.libretexts.org/Bookshelves/Waves_and_Acoustics/Acoustics/01%3A_Fundamentals/1.04%3A_Sound_Speed) to within 0.2 m/s); **331.4 + 0.6·20 = 343.4 m/s** (linear fit, matching the exact value to 0.06 %); the Sabine constant **24·ln10/c = 55.26/343 = 0.1611** s·m⁻¹ ([AcousPlan derivation](https://acousplan.com/blog/reverberation-time-formula-derivation)); and the ISO 9613-1 attenuation coefficients at 20 °C, 50 % RH, 1 atm computed from the standard's Annex equations ([SONAR.m docs](https://gorbatschow.github.io/SonarDocs/sound_absorption_air_iso.en/), [Sengpiel](https://sengpielaudio.com/AirdampingFormula.htm)) giving **4.7 dB/km at 1 kHz, ~30 dB/km at 4 kHz, ~159 dB/km at 10 kHz**. Where literature gives ranges (forest attenuation, RT60 targets), ranges are reported. Numbers I could not verify from a source are flagged inline. Summary: §§1–2 build linear acoustics from conservation laws and thermodynamics; §§3–4 develop harmonic-wave quantities and the decibel system; §§5–7 treat losses and environmental propagation (the engineering core, ISO 9613-1/-2); §8 treats room acoustics statistically and modally; §§9–10 close with Doppler kinematics and nonlinear steepening. Game-engine implications appear as brief asides — the physics is the deliverable.

---

## 1. What Sound Is, and the Acoustic Wave Equation

### 1.1 Sound as a longitudinal mechanical wave

Sound is a **mechanical pressure wave** — a traveling disturbance of pressure, density, and particle motion through a material medium (gas, liquid, or solid). Unlike electromagnetic waves it cannot exist in vacuum: it *is* the ordered motion of the medium's own matter. In fluids the wave is **longitudinal**: particles oscillate back and forth *along* the propagation direction, producing alternating **compressions** (above-ambient pressure/density) and **rarefactions** (below-ambient). The medium is treated as a **continuum** — a field with local pressure $P(x,t)$, density $\rho(x,t)$, and velocity $\vec u(x,t)$ — valid whenever the wavelength greatly exceeds the molecular mean free path (in air at STP, ~68 nm, so this holds to ~GHz) ([Wikipedia: Acoustic wave equation](https://en.wikipedia.org/wiki/Acoustic_wave_equation)).

Four field variables describe a sound wave ([MIT OCW 18.325, Demanet](https://ocw.mit.edu/courses/18-325-topics-in-applied-mathematics-waves-and-imaging-fall-2015/42852dfe83c5197f19ce740818fb92a1_MIT18_325F15_Chapter1.pdf)):

- $p(\vec x,t)$ — **acoustic pressure**: deviation from ambient pressure, $p = P - P_0$ [Pa]. This is what microphones measure.
- $\vec u(\vec x,t)$ — **particle velocity**: local velocity of the fluid element [m/s]. Not the sound speed.
- $\vec\xi(\vec x,t)$ — **particle displacement**: $\vec u = \partial\vec\xi/\partial t$ [m]. Amplitudes are tiny — ~10 µm for conversation-level sound.
- $\rho'(\vec x,t)$ — **density perturbation**: $\rho' = \rho - \rho_0$ [kg/m³].

No vacuum, no pressure restoring force, no wave: in space nobody hears anything, because there is no continuum to compress.

### 1.2 The three governing laws

The wave equation follows from **three** physical statements combined and linearized about a quiescent equilibrium $(P_0, \rho_0, \vec u = 0)$ ([Wikibooks: Engineering Acoustics](https://en.wikibooks.org/wiki/Engineering_Acoustics/Acoustic_wave_equation), [Illinois ECE 473 notes, Allen](https://jontallen.ece.illinois.edu/uploads/473.F18/Lectures/Chapter_5a_old.pdf)).

**(a) Continuity (mass conservation).** Mass flux divergence depletes density:

$$\frac{\partial \rho}{\partial t} + \nabla\cdot(\rho\vec u) = 0.$$

Substitute $\rho = \rho_0 + \rho'$, drop the second-order product $\rho'\vec u$, and note $\rho_0$ is constant:

$$\frac{\partial \rho'}{\partial t} + \rho_0\,\nabla\cdot\vec u = 0. \tag{1}$$

Equivalently in displacement form, $\rho' = -\rho_0\nabla\cdot\vec\xi$ — compression is convergence of displacement ([Euphonics §4.1.1](https://euphonics.org/4-1-1-the-wave-equation/)).

**(b) Euler's equation (momentum conservation).** Newton's second law on a fluid element with pressure gradient as the only force (inviscid):

$$\rho\left(\frac{\partial \vec u}{\partial t} + \vec u\cdot\nabla\vec u\right) = -\nabla P.$$

Linearize ($\rho \to \rho_0$, drop the convective term as second order, $P \to P_0 + p$ with $\nabla P_0 = 0$):

$$\rho_0\frac{\partial \vec u}{\partial t} = -\nabla p. \tag{2}$$

**(c) Equation of state — and why adiabatic.** We need $p$ in terms of $\rho'$ to close the system. For an ideal gas, $P = \rho R T$ per unit mass. **Newton (Principia, 1687)** assumed the compressions stay **isothermal** ($T$ constant), giving $dP = RT\,d\rho$, i.e. an isothermal bulk modulus $K_T = P_0$. His predicted speed was $\sqrt{P_0/\rho_0} \approx 280\text{–}298$ m/s — roughly **18–20 % below** the measured ~340 m/s, a discrepancy that embarrassed physics for 130 years ([Finn, "Laplace and the Speed of Sound," Isis 55(1)](https://www3.nd.edu/~pforces/ame.20231/finn1964.pdf) — note the live mirror at [academicweb.nd.edu](https://academicweb.nd.edu/~powers/ame.20231/finn1964.pdf); [mathpages](https://www.mathpages.com/home/kmath109/kmath109.htm)).

**Laplace's correction (1816):** acoustic compressions are far too fast for heat to flow out of the compressed region during a cycle, so the process is **adiabatic** (isentropic), not isothermal. Compressing a gas adiabatically raises its temperature, which raises the pressure *above* the isothermal value, stiffening the medium. The adiabatic law for a perfect gas is $P\rho^{-\gamma} = \text{const}$ with $\gamma = c_p/c_v \approx 1.4$ for air. Differentiating:

$$\frac{dp}{d\rho}\bigg|_s = \frac{\gamma P_0}{\rho_0} = c^2. \tag{3}$$

So the adiabatic bulk modulus is $K_s = \gamma P_0$, a factor $\gamma$ stiffer than Newton's — and $\sqrt{1.4} = 1.183$, exactly the ~18 % Newton was missing. Laplace's own 1823 computation using Gay-Lussac–Welter's measured specific-heat ratio 1.3748 gave 337.7 m/s against the observed 340.9 m/s — agreement at last ([Laplace, trans. in "On the Speed of Sound in Air and Water"](https://personal.lse.ac.uk/ROBERT49/ebooks/PhilSciAdventures/img/Laplace_VitesseEN.pdf)). Biot (1802) and Poisson (1807) had earlier quantified the adiabatic heating; full confirmation waited on calorimetry.

### 1.3 Linearization and the wave equation

Linearize (3): $p = c^2\rho'$ with $c^2 = (\partial P/\partial\rho)_s$ evaluated at equilibrium. Insert into (1):

$$\frac{\partial p}{\partial t} + \rho_0 c^2\,\nabla\cdot\vec u = 0. \tag{4}$$

Now take $\partial/\partial t$ of (4) and use the time derivative of (2)'s divergence ($\rho_0\,\partial_t(\nabla\cdot\vec u) = -\nabla^2 p$):

$$\frac{\partial^2 p}{\partial t^2} = c^2\,\nabla^2 p, \qquad\text{i.e.}\qquad \boxed{\;\nabla^2 p - \frac{1}{c^2}\frac{\partial^2 p}{\partial t^2} = 0\;}$$

The same equation holds for $\rho'$ and for each component of $\vec\xi$ and (with suitable gauge) a velocity potential $\Phi$ with $\vec u = \nabla\Phi$, $p = -\rho_0\partial_t\Phi$ ([MIT OCW](https://ocw.mit.edu/courses/18-325-topics-in-applied-mathematics-waves-and-imaging-fall-2015/42852dfe83c5197f19ce740818fb92a1_MIT18_325F15_Chapter1.pdf); [LibreTexts Staelin §13.1](https://phys.libretexts.org/Bookshelves/Electricity_and_Magnetism/Electromagnetics_and_Applications_(Staelin)/13%3A_Acoustics/13.01%3A_Acoustic_Waves); [Feynman-derived form in Wikipedia](https://en.wikipedia.org/wiki/Acoustic_wave_equation)). In a **variable medium** ($\rho_0(\vec x)$, $K_s(\vec x)$) the generalization is $\partial_t^2 p = K_s\nabla\cdot(\rho_0^{-1}\nabla p)$ — the basis for treating refraction in layered atmospheres ([MIT OCW](https://ocw.mit.edu/courses/18-325-topics-in-applied-mathematics-waves-and-imaging-fall-2015/42852dfe83c5197f19ce740818fb92a1_MIT18_325F15_Chapter1.pdf)). The linearization is valid while $|p| \ll P_0$ and $|\rho'| \ll \rho_0$ — excellent for everything below ~140 dB SPL, breaking at jet/blast levels (§10).

## 2. Speed of Sound

### 2.1 Newton–Laplace in general media

For any homogeneous medium, the derivation above generalizes to ([Wikipedia](https://en.wikipedia.org/wiki/Acoustic_wave_equation), [HyperPhysics](https://hyperphysics.gsu.edu/hbase/Sound/souspe2.html)):

$$\boxed{\;c = \sqrt{\frac{K_s}{\rho_0}}\;}\qquad K_s = \rho_0\left(\frac{\partial P}{\partial\rho}\right)_s$$

with $K_s$ the **isentropic bulk modulus**. Faster media are *stiffer*, not merely *lighter* — steel is 10× denser than water but sound is 4× faster in it because it is vastly stiffer ([HyperPhysics](https://hyperphysics.gsu.edu/hbase/Sound/souspe2.html)).

### 2.2 Ideal gas

Substituting $P_0 = \rho_0 RT/M$ (universal gas constant $R = 8.3145$ J·mol⁻¹K⁻¹, molar mass $M = 0.028965$ kg/mol for dry air; specific gas constant $R_* = R/M \approx 287.05$ J·kg⁻¹K⁻¹) into $c^2 = \gamma P_0/\rho_0$:

$$\boxed{\;c = \sqrt{\frac{\gamma R T}{M}} = \sqrt{\gamma R_* T}\;}$$

([NASA Glenn](https://www.grc.nasa.gov/WWW/BGH/sound.html), [LibreTexts](https://phys.libretexts.org/Bookshelves/Waves_and_Acoustics/Acoustics/01%3A_Fundamentals/1.04%3A_Sound_Speed)). **Numerical check (verified):** $\sqrt{1.4 \times 8.3145 \times 293.15 / 0.028965} = \sqrt{117{,}703} = $ **343.1 m/s** at 20 °C ([Wikipedia](https://en.wikipedia.org/wiki/Speed_of_sound) tabulates 343.21 m/s with γ = 1.400; [LibreTexts](https://phys.libretexts.org/Bookshelves/Waves_and_Acoustics/Acoustics/01%3A_Fundamentals/1.04%3A_Sound_Speed) gives 343.4). Note the pressure **cancels**: at fixed temperature, $c$ is independent of ambient pressure — density and stiffness scale together. The speed depends on temperature alone, plus tiny humidity corrections (~0.1–0.6 m/s for moist vs dry air) ([LibreTexts](https://phys.libretexts.org/Bookshelves/Waves_and_Acoustics/Acoustics/01%3A_Fundamentals/1.04%3A_Sound_Speed)).

### 2.3 The linear approximation and its accuracy

Expanding $c(\theta) = \sqrt{\gamma R_*(\theta + 273.15)}$ about $\theta = 0$ °C with the binomial theorem ([Wikipedia derivation](https://en.wikipedia.org/wiki/Speed_of_sound)):

$$c \approx 331.3 + 0.606\,\theta \;\;\text{m/s}, \qquad \theta \text{ in °C}.$$

The commonly quoted engineering form is:

$$c \approx 331.4 + 0.6\,\theta \ \ \text{m/s}$$

**Verified:** at 20 °C it gives **343.4 m/s**, 0.06 % above the exact 343.2. Accuracy: the truncated Taylor series stays within ~0.3 % of the exact $\sqrt{T}$ law for $-40$ to $+55$ °C (error grows quadratically with $\theta$, $\approx \theta^2/8T_0^2$ relative — 0.35 % at 55 °C); it is the standard engineering approximation over the outdoor-audio range. Reference values: 331.3 m/s at 0 °C, 346.1 at 25 °C ([Wikipedia table](https://en.wikipedia.org/wiki/Speed_of_sound); [OpenStax](https://phys.libretexts.org/Bookshelves/University_Physics/University_Physics_(OpenStax)/Book%3A_University_Physics_I_-_Mechanics_Sound_Oscillations_and_Waves_(OpenStax)/17%3A_Sound/17.03%3A_Speed_of_Sound)).

### 2.4 Water: ~1480–1520 m/s

For pure water at 20 °C, $K_s \approx 2.19$ GPa, $\rho \approx 998$ kg/m³, giving **~1481 m/s** ([HyperPhysics](https://hyperphysics.gsu.edu/hbase/Sound/souspe2.html) cites 1482 m/s measured). Sound is ~4.3× faster than in air because water is ~15,000× stiffer though only ~800× denser.

For **seawater** the industry standard is the **UNESCO / Chen–Millero equation** (Chen & Millero 1977, JASA 62(5):1129; codified in [Fofonoff & Millard, UNESCO Tech. Papers in Marine Science 44](https://doi.org/10.25607/obp-1450)), a polynomial in temperature $T$ (0–40 °C), salinity $S$ (0–40 PSU), and pressure $P$ (0–1000 bar):

$$c(T,S,P) = C_w(T,P) + A(T,P)\,S + B(T,P)\,S^{3/2} + D(T,P)\,S^2$$

with $C_w$ a 15-term $T$–$P$ polynomial ($C_{00} = 1402.388$, $C_{01} = 5.03830$, ... — full coefficient tables at [gorbatschow/SonarDocs](https://gorbatschow.github.io/SonarDocs/sound_speed_sea_unesco.en/) and [tsuchiya2.org](https://www.tsuchiya2.org/soundspeed/unesco.htm); Wong & Zhu (1995) revised the coefficients for ITS-90 ([JASA 97(3):1732](https://doi.org/10.1121/1.413048))). Typical values: **~1500 m/s at 10 °C, S = 35, surface; ~1516–1543 m/s at 20 °C depending on depth** ([SonarDocs table](https://gorbatschow.github.io/SonarDocs/sound_speed_sea_unesco.en/)). Rules of thumb from the polynomial: $c$ rises ~4.5 m/s per °C near 20 °C (max near 74 °C), ~1.4 m/s per PSU, and ~1.6 m/s per 100 m depth (pressure stiffening). The **SOFAR channel** minimum at ~1 km depth — where the temperature-driven decrease and pressure-driven increase balance — traps and guides sound across ocean basins ([arc.id.au underwater acoustics primer](https://arc.id.au/UWAcoustics.html)).

### 2.5 Solids: longitudinal, shear, and rod waves

An isotropic solid supports two bulk wave speeds ([HyperPhysics table, CRC Handbook data](https://hyperphysics.gsu.edu/hbase/Sound/souspe2.html); [Engineering ToolBox](https://www.engineeringtoolbox.com/sound-speed-solids-d_713.html)):

$$c_L = \sqrt{\frac{K + \tfrac{4}{3}G}{\rho}} \;=\; \sqrt{\frac{E(1-\nu)}{\rho(1+\nu)(1-2\nu)}} \quad(\text{longitudinal/P-wave, bulk}), \qquad c_S = \sqrt{\frac{G}{\rho}} \quad(\text{shear/S-wave}).$$

In a **thin rod** (diameter ≪ λ) the lateral surfaces are stress-free, so only Young's modulus resists the deformation:

$$c_{\text{rod}} = \sqrt{\frac{E}{\rho}}.$$

For mild steel (E ≈ 207 GPa, ν = 0.29, ρ = 7850 kg/m³): $c_L \approx 5960$ m/s, $c_S \approx 3235$ m/s, $c_{\text{rod}} \approx 5200$ m/s — measured: 5960/3235/5200 for mild steel, 5790/3100/5000 for 347 stainless, 6420/3040/5000 for rolled aluminum ([HyperPhysics](https://hyperphysics.gsu.edu/hbase/Sound/souspe2.html), [Engineering ToolBox](https://www.engineeringtoolbox.com/sound-speed-solids-d_713.html), [RF Cafe CRC table](https://www.rfcafe.com/references/general/velocity-sound-media.htm)). Fluids carry no shear stress, so no S-waves — only the fluid is genuinely "one-speed."

Why air < water < steel: the impedance stack. Air: $K_s \sim 1.4\times10^5$ Pa; water: $2.2\times10^9$ Pa; steel: $K + 4G/3 \sim 2.8\times10^{11}$ Pa. Stiffness spans three orders of magnitude; density spans only ~6000×.

### 2.6 Dispersion — when $c$ depends on frequency

The ideal-gas $c$ is frequency-independent (non-dispersive) because $K_s$ is rate-independent — **but only after** molecular relaxation is accounted for. In air, vibrational relaxation of O₂ and N₂ makes the effective compressibility frequency-dependent near the relaxation frequencies (§5.2), producing a small dispersion (fractional velocity change ~ the absorption per wavelength, i.e. tiny — but the attenuation is significant) ([Bass, ISO 9613-1 Annex A background](https://gorbatschow.github.io/SonarDocs/sound_absorption_air_iso.en/); [Kustova et al., Fluids 2023](https://doi.org/10.3390/fluids8020048)). In **bubbly water**, dispersion is violent: the mixture's effective compressibility is dominated by gas bubbles whose response is resonant, so $c(f)$ can fall from ~1500 m/s to below 100 m/s near resonance and the attenuation peaks. The low-frequency (quasi-static) limit is **Wood's equation** ([Wood; derivation in Ando & Prosperetti-type reviews](https://ar5iv.labs.arxiv.org/html/2308.10662)):

$$\frac{1}{c^2} = \frac{(1-\alpha)^2}{c_l^2} + \frac{\rho_l\,\alpha(1-\alpha)}{\gamma p},$$

with void fraction $\alpha$ — a 1 % air fraction drops the sound speed below ~300 m/s. Full dispersion relations: Commander & Prosperetti (JASA 85:732, 1989), reviewed at [PMC2731495](https://pmc.ncbi.nlm.nih.gov/articles/PMC2731495/). Seismic and ultrasonic contexts show the same physics.

## 3. Harmonic Waves, Impedance, Intensity

### 3.1 Plane waves and the dispersion relation

The 1-D wave equation admits $p(x,t) = \hat p \cos(\omega t - kx)$ (and any superposition, e.g. d'Alembert's $f(t \mp x/c)$). Substituting gives $\omega^2 = c^2k^2$, i.e. the **dispersion relation**

$$\boxed{\;\omega = ck, \qquad k = \frac{2\pi}{\lambda} = \frac{\omega}{c},\qquad \lambda f = c\;}$$

For a 1 kHz tone in 20 °C air: $\lambda = 343/1000 \approx 0.343$ m. In water: $\lambda \approx 1.5$ m. In steel: $\lambda \approx 6$ m. **Wavelength, not frequency, controls diffraction and standing-wave behavior** — this single fact explains most of §§6–8.

### 3.2 Pressure–velocity relation and specific acoustic impedance

For the rightward plane wave $p = \hat p\,e^{j(\omega t - kx)}$, Euler's equation (2) integrates to

$$u = \frac{p}{\rho_0 c}$$

— pressure and particle velocity are **in phase**, with constant ratio. This defines the **specific acoustic impedance** of a plane progressive wave:

$$\boxed{\;z = \frac{p}{u} = \rho_0 c \quad [\text{Pa·s/m} \equiv \text{rayl}]\;}$$

([Dahl, ME525 Lecture 18](https://oalib-acoustics.org/website_resources/xxeducation/Dahl_Applied_Acoustics_2022/Lectures%2018-19-20.pdf); [Moore, TU Darmstadt lecture](https://theorie.ikp.physik.tu-darmstadt.de/qcd/moore/ph224/notes/lecture18.pdf)). Values (20 °C, 1 atm): **air: 1.204 × 343 = 413 Pa·s/m** (commonly quoted 413–415) ([Wikipedia table](https://en.wikipedia.org/wiki/Speed_of_sound)); **water: 998 × 1482 ≈ 1.48 × 10⁶ Pa·s/m** ([Omnicalculator table](https://www.omnicalculator.com/physics/acoustic-impedance)); **steel: 7850 × 5960 ≈ 4.7 × 10⁷ Pa·s/m** ([Unseel impedance table](https://unseel.com/physics/acoustic-impedance)). The air/water ratio is **~3500** — the single most consequential number in acoustics (§6.1). The rayl honors Lord Rayleigh; 1 MRayl = 10⁶ rayl is the medical/ultrasound convention ([Omnicalculator](https://www.omnicalculator.com/physics/acoustic-impedance)).

### 3.3 Intensity

Instantaneous power flux (the acoustic Poynting vector) is $\vec\imath = p\,\vec u$ [W/m²] ([LibreTexts Acoustics §1.1](https://phys.libretexts.org/Bookshelves/Waves_and_Acoustics/Acoustics/01%3A_Fundamentals/1.01%3A_Fundamentals_of_Acoustics)). For harmonic fields in complex notation the time average is

$$\boxed{\;\langle\vec I\rangle = \tfrac{1}{2}\,\mathrm{Re}\!\left\{\tilde p\,\tilde{\vec u}^{\,*}\right\}\;}$$

([ScienceDirect: Sound Intensity](https://www.sciencedirect.com/topics/physics-and-astronomy/sound-intensity); [Dahl ME525 Lectures 7–8](https://oalib-acoustics.org/website_resources/xxeducation/Dahl_Applied_Acoustics_2022/Lectures%207-8.pdf)). The ½ appears because $\tilde p, \tilde u$ are peak amplitudes (use rms phasors and the ½ disappears). Substituting $u = p/z$ for a plane wave:

$$I = \frac{p_{\text{rms}}^2}{\rho_0 c}.$$

**Worked example:** 60 dB SPL (conversation) → $p_{\text{rms}} = 20\,\mu\text{Pa}\times10^{60/20} = 0.02$ Pa → $I = 0.02^2/413 \approx 10^{-6}$ W/m². A 194 dB SPL wave at $p_{\text{rms}} = 1$ atm ≈ 10⁵ Pa carries $I \approx 2.4\times10^7$ W/m² — the linear theory's edge ([CDC noise exposure chapter](https://stacks.cdc.gov/view/cdc/193439/cdc_193439_DS1.pdf) notes 194 dB corresponds to 1 atm).

### 3.4 Spherical waves, 1/r spreading, near and far field

Spherically symmetric solutions of the wave equation are $p(r,t) = \frac{A}{r}f(t - r/c)$. Energy conservation *forces* the $1/r$ amplitude: power $\Pi = I(r)\cdot 4\pi r^2$ must be $r$-independent in a lossless medium, so

$$p_{\text{rms}}(r) = \frac{p_{\text{rms}}(r_0)\,r_0}{r}, \qquad I(r) = \frac{\Pi}{4\pi r^2} \propto \frac{1}{r^2}$$

([LibreTexts Acoustics §1.1](https://phys.libretexts.org/Bookshelves/Waves_and_Acoustics/Acoustics/01%3A_Fundamentals/1.01%3A_Fundamentals_of_Acoustics); [StackExchange derivation](https://physics.stackexchange.com/questions/679144/expression-for-pressure-and-intensity-of-spherical-waves)). Inverse-square intensity = **−6 dB per doubling of distance** (§7).

The exact spherical-wave impedance is not real: $z(r) = \rho_0 c\left(1 + \tfrac{1}{jkr}\right)^{-1}$-structured, so near the source pressure and velocity are ~90° out of phase and the field is partly **reactive** (energy sloshing back and forth rather than radiating) ([Dahl ME525 Lectures 7–8](https://oalib-acoustics.org/website_resources/xxeducation/Dahl_Applied_Acoustics_2022/Lectures%207-8.pdf); [StackExchange](https://physics.stackexchange.com/questions/679144/expression-for-pressure-and-intensity-of-spherical-waves)). Convention: a compact source of dimension $a$ has a **near field** $r \lesssim \lambda/2\pi$ (or $ka \gtrsim 1$) where the $1/r$ law fails and the source's geometry matters, and a **far field** $kr \gg 1$ where it radiates as a point source with the plane-wave relation $u \approx p/\rho c$. A 100 Hz tone (λ = 3.43 m) only reaches its far field meters away; a 4 kHz tone (λ = 8.6 cm) is far-field within centimeters. Game-audio aside: distance-attenuation and filter models are far-field constructs — near a source, both the level and the spectrum vary non-monotonically with position.

## 4. The Decibel System

### 4.1 Definitions

Because the ear handles roughly a **10⁶ range of sound pressures** (10¹² in intensity) — threshold of hearing ~20 µPa to threshold of pain ~20–200 Pa ([CDC/NIOSH](https://stacks.cdc.gov/view/cdc/209822)) — acoustics compresses it logarithmically. **Sound pressure level:**

$$\boxed{\;L_p = 20\log_{10}\!\left(\frac{p_{\text{rms}}}{p_0}\right)\ \text{dB}, \qquad p_0 = 20\ \mu\text{Pa}\;}$$

The reference $p_0 = 20$ µPa is the approximate threshold of human hearing at 1–4 kHz — chosen so 0 dB SPL ≈ just-audible; it corresponds to $I_0 = p_0^2/\rho c \approx 10^{-12}$ W/m² ([CDC noise chapter](https://stacks.cdc.gov/view/cdc/193439/cdc_193439_DS1.pdf); [NIOSH criteria document](https://www.cdc.gov/niosh/docs/98-126/pdfs/CriteriaDoc_98-126.pdf?id=10.26616%2FNIOSHPUB98126)). The factor 20 (not 10) is because intensity ∝ $p^2$: doubling pressure = +6.02 dB; ×10 pressure = +20 dB ([CDC](https://stacks.cdc.gov/view/cdc/193439/cdc_193439_DS1.pdf)).

**Sound power level:** $L_W = 10\log_{10}(\Pi/10^{-12}\,\text{W})$. **Sound intensity level:** $L_I = 10\log_{10}(I/10^{-12}\,\text{W/m}^2)$. For a **free plane or spherical progressive wave** with $\rho c \approx 400$ Pa·s/m (air), $I = p^2/\rho c$ makes $L_I = L_p$ numerically — they coincide only for unrestrained progressive waves, not in standing or reverberant fields ([ScienceDirect](https://www.sciencedirect.com/topics/physics-and-astronomy/sound-intensity)).

### 4.2 Reference table

| SPL (dB) | Source |
|---|---|
| 0 | Threshold of hearing (20 µPa) |
| 20 | Leaves rustling |
| 30 | Whisper |
| 40 | Quiet library |
| 50 | Moderate rainfall, dishwasher |
| 60 | Normal conversation |
| 70 | Traffic (in-cabin/city), vacuum |
| 80 | Alarm clock |
| 85 | NIOSH 8-hour exposure limit (A-weighted) |
| 90 | Lawnmower, power tools |
| 100 | Car horn, snowmobile, earbuds at full volume |
| 110 | Chain saw (NIOSH); concerts, car horns |
| 120 | Jet plane at takeoff (pain threshold region) |
| 130 | Jackhammers, ambulances |
| 140 | Jet engine, fireworks, gunshot |
| 165 | 12-gauge shotgun |

Sources: [American Academy of Audiology noise chart](https://www.audiology.org/wp-content/uploads/2023/09/PR23-Poster-NoiseChart-16x20-1.pdf) and [NIOSH "General Estimates of Work-Related Noises"](https://stacks.cdc.gov/view/cdc/209822) (whisper 30, chainsaw 110, jet engine 140, shotgun 165 dB); NIOSH REL 85 dBA/8h with 3 dB exchange rate ([CDC Understand Noise Exposure](https://www.cdc.gov/niosh/noise/prevent/understand.html)). Above ~85 dBA sustained exposure causes permanent hearing loss ([CDC](https://www.cdc.gov/niosh/noise/prevent/understand.html)). Pressure range check: 0→140 dB spans 20 µPa→200 Pa = seven decades / 10⁷ in pressure ([CDC noise chapter](https://stacks.cdc.gov/view/cdc/193439/cdc_193439_DS1.pdf)) — the ear's ~10⁶–10⁷ pressure range maps onto a manageable ~120–140 dB.

### 4.3 dB arithmetic

Energetic (incoherent) addition of $N$ equal sources: $L_{\text{tot}} = L + 10\log_{10}N$. Two identical sources = **+3 dB**; ten = +10 dB. Combining two *different* levels: $L_{\text{tot}} = 10\log_{10}(10^{L_1/10} + 10^{L_2/10})$. Consequence: a source 10 dB below another contributes negligibly (−0.4 dB total); equal-level sums never "double the loudness" perceptually (~+10 dB is heard as twice as loud). Subtraction is the same machinery in reverse — standard for background correction. A-weighting $L_{pA}$ applies the ear's frequency response before summing ([NIOSH criteria](https://www.cdc.gov/niosh/docs/98-126/pdfs/CriteriaDoc_98-126.pdf?id=10.26616%2FNIOSHPUB98126)).

## 5. Absorption of Sound in Air

### 5.1 Classical (Stokes–Kirchhoff) losses

Viscosity and thermal conduction convert ordered wave energy into heat. Starting from the linearized Navier–Stokes + Fourier heat conduction and solving for the complex wavenumber (Kirchhoff 1868; modern treatment in [Laurens et al., arXiv:2107.08886](https://ar5iv.labs.arxiv.org/html/2107.08886)) gives the amplitude attenuation coefficient:

$$\alpha_{\text{classical}} = \frac{\omega^2}{2\rho_0 c^3}\left[\frac{4}{3}\mu + \kappa\left(\frac{1}{c_v}-\frac{1}{c_p}\right)\right] = \frac{\omega^2}{2\rho_0 c^3}\left[\frac{4}{3}\mu + \kappa\frac{\gamma - 1}{c_p}\right]$$

([Springer Attenuation of Sound chapter](https://link.springer.com/chapter/10.1007/978-3-030-44787-8_14); equivalent forms in [Kustova et al.](https://doi.org/10.3390/fluids8020048) and [Lin, Scalo, Hesselink](https://doi.org/10.48550/arxiv.1707.05876)). Here $\mu$ = shear viscosity, $\kappa$ = thermal conductivity. The $4/3$ is the bulk-compression average of shear stress; the thermal term vanishes for $\gamma = 1$ (isothermal gas stores no compression heat). Key physics: **$\alpha \propto \omega^2$** — attenuation per meter grows as frequency squared, and is independent of amplitude (a linear loss). Equivalently, *attenuation per wavelength* $\alpha\lambda \propto f$ is small at audio frequencies, so the wave remains nearly lossless over one cycle but dies over many cycles.

**Worked example (classical only, verified by scaling):** air at 20 °C, $\mu = 1.81\times10^{-5}$ Pa·s, $\kappa = 0.0257$ W/m·K, $\gamma = 1.4$, $c_p = 1005$ J/kg·K: the bracket evaluates to $2.41\times10^{-5} + 1.02\times10^{-5} = 3.4\times10^{-5}$ (units Pa·s), giving $\alpha_{\text{cl}}/f^2 \approx 1.4\times10^{-11}$ Np/m/Hz² — about 13 dB/km at 10 kHz, several times less than measured, which is precisely the classical deficit that molecular relaxation fills in ([Lin et al.](https://doi.org/10.48550/arxiv.1707.05876): classical theory "gives values much lower than those observed experimentally").

### 5.2 Molecular relaxation absorption (O₂, N₂)

Air's N₂ and O₂ molecules store energy in vibrational modes that equilibrate with translation at a **finite rate** — with relaxation times $\tau$ of microseconds to milliseconds depending on humidity. When the acoustic period approaches $\tau$, the vibrational mode lags the pressure, the density–pressure relation acquires a phase offset, and energy is dissipated per cycle. Each species contributes a term of the universal relaxation form:

$$\alpha_{\text{vib}}(f) \;=\; \frac{(\alpha\lambda)_{\max}}{c}\;\frac{f^2/f_r}{1 + (f/f_r)^2}\;\cdot 2 \;=\; (\alpha\lambda)_{\max}\frac{2f^2 f_r}{c(f_r^2 + f^2)}$$

peaking near the **relaxation frequency** $f_r$ and falling off as $f^2$ below it, plateauing above it ([SONAR.m ISO 9613-1 page](https://gorbatschow.github.io/SonarDocs/sound_absorption_air_iso.en/)). Water vapor is the catalyst: it taps energy out of the vibration, so **$f_{r,O}$ and $f_{r,N}$ are strong functions of humidity and temperature**:

$$f_{r,O} = \frac{p_a}{p_r}\left(24 + 4.04\times10^4\,h\,\frac{0.02 + h}{0.391 + h}\right), \qquad f_{r,N} = \frac{p_a}{p_r}\left(\frac{T}{T_0}\right)^{-1/2}\!\left(9 + 280\,h\,e^{-4.170\left[(T/T_0)^{-1/3} - 1\right]}\right)$$

with $h$ the molar water-vapor concentration (%) and $T_0 = 293.15$ K ([ISO 9613-1](https://www.iso.org/standard/17426.html); equations reproduced in [SONAR.m](https://gorbatschow.github.io/SonarDocs/sound_absorption_air_iso.en/) and [Sengpiel](https://sengpielaudio.com/AirdampingFormula.htm)). For 20 °C, 50 % RH: $h \approx 1.16$ %, $f_{r,O} \approx 630$ Hz, $f_{r,N} \approx 285$ Hz. The non-monotonic humidity behavior — very dry air absorbs *more* at mid frequencies because the relaxation peak sits in the audio band — is a classic gotcha: at 1–2 kHz, absorption is worst near 10–20 % RH and *improves* with more humidity.

### 5.3 The engineering total: ISO 9613-1

**ISO 9613-1:1993, "Attenuation of sound during propagation outdoors — Part 1: Calculation of the absorption of sound by the atmosphere"** specifies the pure-tone attenuation coefficient for 50 Hz–10 kHz, −20 to +50 °C, 10–100 % RH, 1 atm ([ISO catalog](https://www.iso.org/standard/17426.html)). The computationally condensed form (ISO eq. (5); units dB/m):

$$\alpha = 8.686\,f^2\left[\frac{1.84\times10^{-11}}{p_a/p_r}\left(\frac{T}{T_0}\right)^{1/2} + \left(\frac{T}{T_0}\right)^{-5/2}\!\left(\frac{0.01275\,e^{-2239.1/T}}{f_{r,O} + f^2/f_{r,O}} + \frac{0.1068\,e^{-3352.0/T}}{f_{r,N} + f^2/f_{r,N}}\right)\right]$$

([ISO 9613-1 preview](https://cdn.standards.iteh.ai/samples/iso/iso-9613-1-1993/505cecd76313448e9e6e456f1dd6d471/iso-9613-1-1993.pdf); implementation in [acoustic-toolbox](https://acoustic-toolbox.readthedocs.io/en/latest/standards/iso_9613_1_1993/) and [Sengpiel](https://sengpielaudio.com/AirdampingFormula.htm)). Level decays as $L_p(d) = L_p(0) - \alpha d$ (dB), amplitude as $e^{-\alpha d/8.686}$. Open implementations: [acoustic_toolbox ISO 9613-1 module](https://acoustic-toolbox.readthedocs.io/en/latest/standards/iso_9613_1_1993/).

**Verified worked example (20 °C, 50 % RH, 1 atm, computed from the ISO equations):**

| Frequency | α (dB/km) | Loss over 100 m |
|---|---|---|
| 1 kHz | ~4.7 dB/km | **0.5 dB** |
| 4 kHz | ~30 dB/km | **3 dB** |
| 10 kHz | ~159 dB/km | **16 dB** |

(Consistent with ISO's own Table 1 trends and with [Sengpiel's calculator](https://sengpielaudio.com/AirdampingFormula.htm); the 10 kHz figure varies strongly with humidity — drier air at 4 kHz can exceed 60 dB/km.) So a 1 kHz tone loses a negligible half dB over a football field, while 10 kHz content is essentially destroyed. **This is why distant thunder rumbles**: the lightning channel's broadband crack loses its >1 kHz components within the first kilometer, leaving the low-frequency rumble; additional low-passing comes from scattering on turbulence and raindrops ([Wikibooks: Thunder acoustics](https://en.wikibooks.org/wiki/Engineering_Acoustics/Thunder_acoustics)). Game-audio aside: a distance-dependent low-pass filter whose cutoff tracks $\alpha(f)\cdot d$ is a physically grounded way to sell distance.

## 6. Reflection, Transmission, Refraction, Diffraction

### 6.1 Normal incidence between two media

At a plane interface, pressure and normal particle velocity must both be continuous. Let a wave in medium 1 ($z_1 = \rho_1 c_1$) hit medium 2 ($z_2$) at normal incidence. Continuity of $p$ and $u_n$ with incident + reflected + transmitted plane waves gives ([Moore lecture 18](https://theorie.ikp.physik.tu-darmstadt.de/qcd/moore/ph224/notes/lecture18.pdf); [NovaSolver theory page](https://novasolver.jp/en/tools/acoustic-impedance.html); [stemcalculators](https://stemcalculators.app/calculators/physics-oscillations-and-waves/sound-waves-and-acoustics/acoustic-impedance-matcher)):

$$\boxed{\;r = \frac{p_r}{p_i} = \frac{z_2 - z_1}{z_2 + z_1}, \qquad \tau = \frac{p_t}{p_i} = \frac{2z_2}{z_2 + z_1}\;}$$

Energy fractions (normal incidence): $R = |r|^2$ and $T = \dfrac{4z_1z_2}{(z_1+z_2)^2}$, with $R + T = 1$ for lossless media. Note $\tau$ can exceed 1 (pressure gain) while transmitting almost no energy — pressure is not energy ([Unseel](https://unseel.com/physics/acoustic-impedance), [TU Delft seafloor chapter](https://ocw.tudelft.nl/wp-content/uploads/Reader_chapter_4_01.pdf)).

**Air → water:** $z_1 = 413$, $z_2 = 1.48\times10^6$ rayl → $r \approx +0.99944$, $T = 4z_1z_2/(z_1+z_2)^2 \approx 4z_1/z_2 \approx 1.1\times10^{-3}$: **~99.9 % of the energy reflects; only ~0.1 % enters the water** — about a 29–30 dB transmission loss, the same in both directions ([Moore](https://theorie.ikp.physik.tu-darmstadt.de/qcd/moore/ph224/notes/lecture18.pdf): "only 4/3500 of the intensity makes it through... almost 30 dB"; [Leighton et al., Acoustics Bulletin / ISVR Pub 12693](https://resource.isvr.soton.ac.uk/staff/pubs/PubPDFs/Pub12693.pdf)). This is why the underwater world is nearly deaf to the airborne one and vice versa — the physical basis of "you barely hear anything above the surface." Refinements: for grazing/spherical incidence and low frequencies, evanescent and anomalous-transparency effects (Godin) let compact near-surface sources couple far better than the plane-wave number suggests ([Godin review](https://www.researchgate.net/publication/232922323_Sound_transmission_through_water-air_interfaces_New_insights_into_an_old_problem)). Ultrasound coupling gel exists precisely to defeat this mismatch ($z_{\text{gel}} \approx 1.5$ MRayl matching tissue) ([Omnicalculator](https://www.omnicalculator.com/physics/acoustic-impedance)).

### 6.2 Oblique incidence and Snell's law

At incidence angle $\theta_1$ (from the normal), phase matching along the interface gives **Snell's law for sound**:

$$\frac{\sin\theta_1}{c_1} = \frac{\sin\theta_2}{c_2}$$

with the reflected ray leaving at $\theta_1$ (specular) ([TU Delft reader](https://ocw.tudelft.nl/wp-content/uploads/Reader_chapter_4_01.pdf); [Dahl lecture 18](https://oalib-acoustics.org/website_resources/xxeducation/Dahl_Applied_Acoustics_2022/Lectures%2018-19-20.pdf) — note Dahl states it in grazing-angle cosine form $\cos\theta_0/c_0 = \cos\theta_1/c_1$). Sound bends **toward the slower medium** on refraction (opposite mnemonic to light, since sound speed plays the role of 1/n). Going from slow to fast media (water → air, c ratio 1482/343 = 4.3), a **critical angle** $\theta_c = \arcsin(c_1/c_2) \approx 13.4°$ from the normal exists beyond which total internal reflection occurs and the transmitted field in the faster medium is evanescent (this is why airborne sound entering water arrives within a cone of ±13° of vertical — swimmers hear the above-water world only when it is nearly overhead) ([Dahl lecture](https://oalib-acoustics.org/website_resources/xxeducation/Dahl_Applied_Acoustics_2022/Lectures%2018-19-20.pdf)). At a fluid–solid interface, mode conversion adds refracted shear waves with their own Snell branch ([TU Delft](https://ocw.tudelft.nl/wp-content/uploads/Reader_chapter_4_01.pdf)).

### 6.3 Refraction by temperature and wind gradients

The atmosphere is never isothermal. Since $c = \sqrt{\gamma R_* T}$, a temperature gradient is a sound-speed gradient, and ray paths curve per Snell toward slower (cooler) air. With wind, the effective propagation speed is $c_{\text{eff}}(z) = c(T(z)) + \vec w(z)\cdot\hat n$; its vertical gradient $dc_{\text{eff}}/dz$ controls the curvature ([Hohenwarter, Acta Acustica 2022](https://acta-acustica.edpsciences.org/articles/aacus/full_html/2022/01/aacus210007/aacus210007.html)):

- **Daytime lapse** (T decreases with height, $dT/dz < 0$ roughly the dry adiabatic $-9.8$ K/km plus superadiabatic near hot ground): $dc_{\text{eff}}/dz < 0$, rays bend **upward**, ground-level receivers sit in a **shadow zone** — levels drop several dB beyond a few hundred meters ([Waddington & Lam, ICSV review](https://doi.org/10.25144/18277); [Hohenwarter](https://acta-acustica.edpsciences.org/articles/aacus/full_html/2022/01/aacus210007/aacus210007.html)).
- **Nighttime inversion** (clear sky, ground cools, T increases with height): $dc_{\text{eff}}/dz > 0$, rays bend **down**, repeatedly reflecting off the ground and staying in a duct — **sound carries far**. This is the "carrying night air." Measured inversion enhancements: **+3–4 dB(A) at 150–250 m** relative to neutral conditions in Hohenwarter's road/rail data; up to **5–8 dB at >400 m** in Phoenix valley monitoring; older studies report up to ~10 dB over 75 m in strong stable stratification ([Hohenwarter](https://acta-acustica.edpsciences.org/articles/aacus/full_html/2022/01/aacus210007/aacus210007.html); [ADOT highway noise study](https://rosap.ntl.bts.gov/view/dot/40318/dot_40318_DS1.pdf); [Acoustics 2008 Budapest paper](https://dael.euracoustics.org/confs/acoustics2008/data/fa2005-budapest/paper/391-0.pdf)).
- **Wind gradient**: wind speed increases with height (log profile), so **downwind** the gradient adds to $dc_{\text{eff}}/dz > 0$ (downward bending, reinforcement) and **upwind** it subtracts ($dc_{\text{eff}}/dz < 0$, upward bending, shadow zone) ([Waddington & Lam](https://doi.org/10.25144/18277); [NASA/Army ray-tracing study](https://ntrs.nasa.gov/api/citations/20060004780/downloads/20060004780.pdf)). Unlike temperature effects, wind effects are **directional** — a 180° azimuthal asymmetry. Turbulence scatters sound into shadow zones, partially filling them ([Waddington & Lam](https://doi.org/10.25144/18277)).

Game-audio aside: a single scalar — the effective sound-speed gradient — plus ray curvature predicts all four of these behaviors; a time-of-day cycle that flips its sign is enough to reproduce "quiet afternoon / carrying night."

### 6.4 Diffraction

Sound bends around obstacles because of the **Huygens–Fresnel principle**: every point on a wavefront acts as a secondary source, so a wavefront that grazes an edge re-radiates into the geometric shadow. The controlling dimensionless group is the **Fresnel number** $N_F = 2\delta/\lambda$, where $\delta$ is the extra path length of the diffracted ray over the direct line ([Maekawa implementation notes, Arup Strutt](https://strutt.arup.com/help/Environmental_Noise/EnviroBarrierAtten.htm)). Rule of thumb: **λ comparable to or larger than the obstacle → strong diffraction; λ ≪ obstacle → geometric shadow.** A 2 m wall blocks 8 kHz sound (λ = 4.3 cm) effectively but barely attenuates 100 Hz (λ = 3.4 m); light (λ ~ 500 nm) casts sharp shadows from the same wall. This wavelength-selectivity is why barriers attenuate high frequencies first and why speech behind a wall loses intelligibility (consonants are high-frequency) while the low murmur gets through. Quantified barrier models (Maekawa, ISO 9613-2 screening) in §7.3.

## 7. Outdoor Sound Propagation: The Engineering Model

### 7.1 The budget

Level at a receiver from a point source of sound power level $L_W$ ([ISO 9613-2:2024](https://cdn.standards.iteh.ai/samples/74047/0893db36c3314c458e3a87d551f5c534/ISO-9613-2-2024.pdf), [Probst, DataKustik white paper](https://www.datakustik.com/fileadmin/user_upload/e-Learning-Center/Papers-and-Publications/2020_WP_Investigation_ISO9613_2.pdf)):

$$L_{fT} = L_W + D_c - A, \qquad A = A_{\text{div}} + A_{\text{atm}} + A_{\text{gr}} + A_{\text{bar}} + A_{\text{misc}}$$

with $D_c$ the directivity correction and the five attenuation terms as follows.

**Geometric divergence** $A_{\text{div}} = 20\log_{10}(d/d_0) + 11$ dB, $d_0 = 1$ m ([ISO 9613-2 eq. 8](https://cdn.standards.iteh.ai/samples/74047/0893db36c3314c458e3a87d551f5c534/ISO-9613-2-2024.pdf)) — pure spherical spreading: **−6 dB per distance doubling**, −20 dB per decade. (The +11 is $10\log_{10}4\pi$.) So 1→10→100 m is −11→−31→−51 dB relative to 1 m free-field.

**Atmospheric absorption** $A_{\text{atm}} = \alpha d$ from ISO 9613-1 (§5.3), octave-band by octave-band ([ISO 9613-2 §7.2](https://cdn.standards.iteh.ai/samples/74047/0893db36c3314c458e3a87d551f5c534/ISO-9613-2-2024.pdf)).

### 7.2 Ground effect

Over an acoustically finite-impedance ground (grass, soil, snow — "soft"), the direct ray and the ground-reflected ray **interfere**. Because the reflection coefficient of a porous ground is frequency-dependent (near −1 at high frequency for grazing rays, small magnitude at low frequency where the ground leaks into its pores), the direct+reflected sum shows a characteristic **dip in the 200–500 Hz region** over grass and a mild low-frequency boost over hard ground. ISO 9613-2 §7.3 models this by splitting the path into source, middle, and receiver regions with a ground factor $G$ (0 = hard: concrete, water; 1 = porous: soil, snow) and semi-empirical curves; the simplified alternative for A-weighted levels uses $A_{\text{gr}} = 4.8 - (2h_m/d)[17 + 300/d]$ dB with $h_m$ the mean source–receiver height ([ISO 9613-2](https://cdn.standards.iteh.ai/samples/74047/0893db36c3314c458e3a87d551f5c534/ISO-9613-2-2024.pdf); [EPA WA appendix](https://www.epa.wa.gov.au/sites/default/files/PER_documentation2/Appendix%2010%20-%20Noise%20Modelling.pdf); [Probst](https://www.datakustik.com/fileadmin/user_upload/e-Learning-Center/Papers-and-Publications/2020_WP_Investigation_ISO9613_2.pdf)). Physically: hard ground reinforces (in-phase reflection, up to +6 dB at grazing), soft ground produces the interference dip. Measured ground-effect dips of 3–10 dB at 250–500 Hz are typical over grass ([Attenborough et al., outdoor propagation literature summarized in ebrary foliage chapter](https://ebrary.net/134991/environment/models_foliage_effects)).

### 7.3 Barriers: Maekawa and ISO screening

**Maekawa's approximation** (Maekawa 1968; the curve-fit form used today) gives barrier insertion loss in the shadow zone from the Fresnel number $N_F = 2\delta/\lambda$ ([Arup Strutt implementation](https://strutt.arup.com/help/Environmental_Noise/EnviroBarrierAtten.htm)):

$$IL = -\left[5 + 20\log_{10}\frac{\sqrt{2\pi N_F}}{\tanh\sqrt{2\pi N_F}}\right] \ \text{dB} \qquad (N_F > 0)$$

— i.e. ~5 dB grazing attenuation growing logarithmically with $N_F$. The **ISO 9613-2 screening** formula ([ISO 9613-2 §7.4](https://cdn.standards.iteh.ai/samples/74047/0893db36c3314c458e3a87d551f5c534/ISO-9613-2-2024.pdf); [Strutt](https://strutt.arup.com/help/Environmental_Noise/EnviroBarrierAtten.htm)):

$$D_z = -10\log_{10}\!\left(3 + \frac{C_2}{\lambda}C_3\,\delta\,K_{\text{met}}\right), \qquad A_{\text{bar}} = D_z - A_{\text{gr}} > 0$$

with path difference $\delta = d_{ss} + d_{sr} + e - d$, $C_2 = 20$ (or 40 when ground reflections are handled separately), $C_3$ a thickness factor for double-edge diffraction, and $K_{\text{met}} = \exp[-\sqrt{d_{ss}d_{sr}d}/(2000\,\delta\cdot 2)]$ a meteorological limiter that kills the barrier's effect at long range (curved rays creep over the top under downward refraction). Caps: 20 dB single-edge, 25 dB double-edge per octave band. Known critique: ISO's combination of $A_{\text{bar}}$ with the general ground method leaves a residual ~5 dB far-field insertion loss even for non-screening geometries — documented and under revision ([Probst](https://www.datakustik.com/fileadmin/user_upload/e-Learning-Center/Papers-and-Publications/2020_WP_Investigation_ISO9613_2.pdf)). Realistic highway barriers deliver 5–10 dB(A) insertion loss; the theoretical ceiling near 20–25 dB is rarely reached because of flanking, leakage, and refraction.

### 7.4 Vegetation

Dense vegetation attenuates mainly **above ~1 kHz**, through scattering by trunks/branches/foliage plus ground-effect modification ([ebrary: Models for Foliage Effects](https://ebrary.net/134991/environment/models_foliage_effects)). Measured rates are modest and cover a range: ISO 9613-2 Annex A/Table A.1 assigns dense-foliage belts attenuation that grows from ~0 dB below 500 Hz to ~0.05 dB/m (i.e. ~5 dB per 100 m) at 4 kHz for propagation through the canopy — but field measurements in conifer stands give **12–50 dB/km at 1–4 kHz (1.2–5 dB per 100 m)**, and dense young pine with branches to the ground measured ~50 dB/km at 4 kHz, while an open oak stand gave ~12 dB/km at 1 kHz ([Trimpop, Inter-Noise 2014](https://acoustics.asn.au/conference_proceedings/INTERNOISE2014/papers/p499.pdf) — K_lin values 22 and 12 dB/km at 1 kHz, 50 and 20 dB/km at 4 kHz for two pine stands; see also [Trimpop 2023 correction](https://doi.org/10.3397/in_2023_0659) showing ISO 9613-2's foliage model *underestimates* long-range attenuation). Empirical pine-forest fit 2–6 kHz: attenuation rate grows with log(f) ([Huisman data via ebrary](https://ebrary.net/134991/environment/models_foliage_effects)). Whole-forest belt effects over 100–500 m: **up to ~10 dB additional A-weighted attenuation** in the densest stands (basal area > 15–40 m²/ha), with a ceiling set by the path diffracting *over* the canopy ([ScienceDirect forest shield model](https://www.sciencedirect.com/science/article/pii/S0301479724000562)). A belt of trees is real but weak shielding; treat marketing claims of "green barriers" with the ISO numbers, not the brochures.

### 7.5 The ISO 9613-2 model overall

**ISO 9613-2** ("engineering method") predicts the equivalent-continuous downwind octave-band SPL at a receiver from point sources: the five-term budget above, summed over source/image contributions and eight octave bands 63 Hz–8 kHz, then A-weighted, with a long-term meteorological correction $C_{\text{met}}$ ([ISO 9613-2:2024](https://cdn.standards.iteh.ai/samples/74047/0893db36c3314c458e3a87d551f5c534/ISO-9613-2-2024.pdf); [Probst](https://www.datakustik.com/fileadmin/user_upload/e-Learning-Center/Papers-and-Publications/2020_WP_Investigation_ISO9613_2.pdf)). Its stated validity window is **downwind propagation or a well-developed moderate ground-based temperature inversion** (source-to-receiver wind within ±45°, 1–5 m/s at 3–11 m height) — i.e. it is deliberately a *worst-case-favorable* model, which is why regulators use it. Validity: ~1 km (to 8 kHz bands) per the standard's accuracy clause; extensions to several km and shooting-noise ranges are active research ([Trimpop 2023](https://doi.org/10.3397/in_2023_0659)). It is the backbone of nearly every national environmental-noise map and wind-farm assessment worldwide ([ScienceDirect review](https://www.sciencedirect.com/science/article/pii/S0301479724000562)).

## 8. Room Acoustics

### 8.1 Reverberation and Sabine's formula — derived

Sound in a room after the source stops decays through repeated reflections. Sabine's statistical treatment (1898) tracks the total acoustic energy $E(t)$, assuming a **diffuse field** — energy density uniform, directions isotropic.

**Mean free path.** In a convex room of volume $V$ and total surface $S$, kinetic-theory/geometric averaging gives the mean distance between wall reflections ([AcousPlan derivation](https://acousplan.com/blog/reverberation-time-formula-derivation); historical notes in [Eyring 1930](https://trueaudio.com/array/downloads/CF%20Eyring-Reverb%20Time%20in%20Dead%20Rooms.pdf)):

$$\ell = \frac{4V}{S}$$

(a result traced to Jaeger via kinetic theory; Kosten's 1960 rigorous proof; Eyring tabulates shape-specific values: cube $2\sqrt[3]{V}$-scaled, sphere $6V/S$ — the diffuse value $4V/S$ is the working assumption). Mean time between reflections: $\Delta t = \ell/c = 4V/(cS)$.

**Energy decay.** Each reflection from a surface of average absorption $\bar\alpha$ multiplies the energy by $(1-\bar\alpha)$. After $n$ reflections, $E = E_0(1-\bar\alpha)^n$; with $n \approx ctS/4V$ continuous in time:

$$E(t) = E_0 \exp\!\left[\frac{ctS}{4V}\ln(1-\bar\alpha)\right].$$

**RT60.** Define $T_{60}$ by $E(T_{60})/E_0 = 10^{-6}$ (a 60 dB decay):

$$\exp\!\left[\frac{cT_{60}S}{4V}\ln(1-\bar\alpha)\right] = 10^{-6} \;\Rightarrow\; T_{60} = \frac{-24\ln 10\,V}{cS\ln(1-\bar\alpha)}.$$

**Sabine's approximation** — the log linearized for live rooms, $\ln(1-\bar\alpha) \approx -\bar\alpha$, with total absorption $A = \sum_i S_i\alpha_i = S\bar\alpha$:

$$\boxed{\;T_{60} = \frac{24\ln 10}{c}\,\frac{V}{A} = \frac{0.161\,V}{A} \ \ \text{s}\;}\qquad (V\ \text{in m}^3,\ A\ \text{in m}^2\ \text{Sabine})$$

**Verified:** $24\ln10/c = 24 \times 2.302585/343 = 55.262/343 = 0.16111$ — the mysterious 0.161 is exactly $24\ln 10/c$ ([AcousPlan](https://acousplan.com/blog/reverberation-time-formula-derivation); [Unseel](https://unseel.com/physics/sabine-reverberation)). In imperial units the same constant is 0.049 with feet ([Eyring 1930](https://trueaudio.com/array/downloads/CF%20Eyring-Reverb%20Time%20in%20Dead%20Rooms.pdf)). With air absorption added (important in large halls at high frequency), $A \to A + 4mV$ where $m$ is the intensity absorption constant of air ([AcousPlan](https://acousplan.com/blog/reverberation-time-formula-derivation)).

**Worked example:** an 8×6×3 m office: $V = 144$ m³, $S = 180$ m², $\ell = 3.2$ m. With $\bar\alpha = 0.2$ (typical furnished office), $A = 36$ m² → $T_{60} = 0.161\times144/36 = 0.64$ s — right in the office comfort zone.

### 8.2 Eyring's correction for dead rooms

Sabine fails as $\bar\alpha \to 1$: it predicts $T_{60} = 0.161V/S \neq 0$ for a perfectly absorbing (anechoic) room, which is unphysical. **Eyring (1930)** keeps the exact log:

$$\boxed{\;T_{60} = \frac{0.161\,V}{-S\ln(1-\bar\alpha)}\;}$$

which correctly → 0 as $\bar\alpha \to 1$ ([Eyring 1930 original](https://trueaudio.com/array/downloads/CF%20Eyring-Reverb%20Time%20in%20Dead%20Rooms.pdf); [AcousPlan](https://acousplan.com/blog/reverberation-time-formula-derivation)). The two agree within a few % for $\bar\alpha \lesssim 0.2$; Sabine overestimates by ~10 % at $\bar\alpha = 0.4$ and ~40 % at $\bar\alpha = 0.6$ ([Unseel table](https://unseel.com/physics/sabine-reverberation)). Millington–Sette applies the log per-surface: $T_{60} = 0.161V/[-\sum_i S_i\ln(1-\alpha_i)]$ for strongly heterogeneous rooms ([AcousPlan](https://acousplan.com/blog/reverberation-time-formula-derivation)). Beranek's caveat: absorption coefficients must be measured in rooms similar to the application; Sabine coefficients can legitimately exceed 1.0 for anechoic-class absorbers ([Beranek, JASA 2006](https://doi.org/10.1121/1.2221392)). Absorption coefficients are strongly frequency-dependent — porous absorbers work at mid/high frequency (velocity maxima, thickness ≥ λ/4), panel/membrane absorbers at low frequency — so $T_{60}(f)$ is always a curve, not a number (see e.g. [Sengpiel absorption data](https://sengpielaudio.com/AirdampingFormula.htm) for context on frequency dependence).

### 8.3 Room modes and the modal region

Below some frequency the diffuse assumption collapses: the room is a small finite set of standing waves. For a rigid-walled rectangular room $L_x \times L_y \times L_z$, separation of variables on the Helmholtz equation $\nabla^2 p + k^2 p = 0$ with $\partial p/\partial n = 0$ at the walls gives the eigenfrequencies ([Wikipedia: Schroeder frequency](https://en.wikipedia.org/wiki/Schroeder_frequency); [COMSOL eigenmodes example](https://doc.comsol.com/6.4/doc/com.comsol.help.models.mph.eigenmodes_of_room/eigenmodes_of_room.html); [MathWorks FEM example](https://www.mathworks.com/help/audio/ug/find-room-modes-with-finite-element-analysis.html); [Irvine vibrationdata tutorial](https://www.vibrationdata.com/tutorials_alt/aco_rec.pdf)):

$$\boxed{\;f_{n_xn_yn_z} = \frac{c}{2}\sqrt{\left(\frac{n_x}{L_x}\right)^2 + \left(\frac{n_y}{L_y}\right)^2 + \left(\frac{n_z}{L_z}\right)^2}\;}, \qquad n_x, n_y, n_z = 0,1,2,\dots$$

with mode shapes $p \propto \cos(n_x\pi x/L_x)\cos(n_y\pi y/L_y)\cos(n_z\pi z/L_z)$. Classification by nonzero index count: **axial** (one nonzero — bounces between one wall pair, strongest, most audible), **tangential** (two nonzero — grazes four walls, ~3 dB weaker), **oblique** (all three nonzero — weakest per mode but the most numerous at high order) ([vibrationdata tutorial](https://www.vibrationdata.com/tutorials_alt/aco_rec.pdf); [COMSOL](https://doc.comsol.com/6.4/doc/com.comsol.help.models.mph.eigenmodes_of_room/eigenmodes_of_room.html)). Example (COMSOL's 4.7×4.1×3.1 m room, c = 343): first axial 34.3 Hz, then 42.9, 54.9, 66.0, 68.6 Hz... — packing ever denser ([COMSOL table](https://doc.comsol.com/6.4/doc/com.comsol.help.models.mph.eigenmodes_of_room/eigenmodes_of_room.html)). All modes have pressure maxima at the corners, so a subwoofer or absorber in a corner couples to all of them.

### 8.4 The Schroeder frequency: modal vs statistical

Modal density grows (asymptotically $\sim 4\pi V f^2/c^3$ modes per Hz), and each mode has a finite bandwidth $\Delta f \approx 2.2/T_{60}$ (the 3-dB-down width of a resonance with decay time $T_{60}$). Modes begin to overlap when roughly three fall within one bandwidth. Setting that condition and solving gives Schroeder's **transition frequency** between the modal region and the statistical/diffuse region ([Linkwitz derivation](https://www.linkwitzlab.com/rooms.htm); [Wikipedia](https://en.wikipedia.org/wiki/Schroeder_frequency); [Kuttruff eq. 3.44 as cited](https://github.com/jmrplens/phonometry/blob/main/docs/buildings/rooms/room-image-sources.md)):

$$\boxed{\;f_s \approx 2000\sqrt{\frac{T_{60}}{V}}\ \ \text{Hz}\;}\qquad (T_{60}\ \text{in s},\ V\ \text{in m}^3)$$

**Worked examples:** a domestic 100 m³ living room with $T_{60} = 0.5$ s: $f_s \approx 141$ Hz — below ~140 Hz individual modes color the bass, above it statistics apply ([Linkwitz](https://www.linkwitzlab.com/rooms.htm) gets 157 Hz for a similar case). A 20,000 m³ concert hall at 2.0 s: $f_s \approx 20$ Hz — effectively the whole audible band is statistical, which is why Sabine works for halls. The transition is fuzzy (±an octave); between $f_s$ and ~$8f_s$ lies a transition region where neither picture is exact ([Wikipedia](https://en.wikipedia.org/wiki/Schroeder_frequency)).

### 8.5 Critical distance, room constant, and the steady-state field

A source in a reverberant room produces a **direct field** ($\propto Q/4\pi r^2$) plus a **reverberant field** (approximately uniform). With directivity $Q$ (1 = omnidirectional; 2 on a hard floor; 4 at an edge; 8 in a corner) and room constant $R = S\bar\alpha/(1-\bar\alpha)$, the steady-state level is (Bies, *Engineering Noise Control* eq. 6.43, as documented in [phonometry](https://github.com/jmrplens/phonometry/blob/main/docs/buildings/rooms/room-image-sources.md)):

$$L_p = L_W + 10\log_{10}\left(\frac{Q}{4\pi r^2} + \frac{4}{R}\right)$$

The **critical distance** $r_c$ — where direct equals reverberant — follows from setting the two terms equal:

$$\boxed{\;r_c = \sqrt{\frac{Q\,R}{16\pi}}\;}\qquad\text{(Bies form; Kuttruff's } r_h = \sqrt{A/16\pi} \text{ for } Q=1 \text{ and small } \bar\alpha)$$

([phonometry, citing Bies 6.4 and Kuttruff 5.6](https://github.com/jmrplens/phonometry/blob/main/docs/buildings/rooms/room-image-sources.md)). Typical listening rooms put $r_c$ at under a meter — you sit in the reverberant field more often than not ([Linkwitz](https://www.linkwitzlab.com/rooms.htm) computes 0.72 m for a monopole in a domestic room). Inside $r_c$, intelligibility and localization behave free-field-like; beyond it the level stops falling with distance — the classic open-plan office problem, and why directional sources (high $Q$) buy intelligibility.

### 8.6 The impulse response

The room's transfer function in time — its **impulse response** (IR) — decomposes into:

1. **Direct sound**: first arrival, sets distance/level percept.
2. **Early reflections** (< ~50 ms after the direct): strong discrete echoes off walls/ceiling. Psychoacoustically they are **fused** with the direct sound (the Haas precedence effect) and contribute to perceived clarity, warmth, and apparent source width; beyond ~50 ms they degrade into audible echo for speech ([AcousPlan RT60 reference](https://acousplan.com/blog/reverberation-time-complete-reference)).
3. **Late reverberant tail**: a statistically dense decay, perceptually "room size" and "liveness," with $T_{60}$ as its time constant.

Modern measurement integrates the IR backward in time (Schroeder 1965) to get robust decay curves; $T_{20}$/$T_{30}$ extrapolate the −20/−30 dB slopes to 60 dB because background noise usually hides the last 20 dB ([AcousPlan](https://acousplan.com/blog/reverberation-time-complete-reference); [Unseel](https://unseel.com/physics/sabine-reverberation)). Game-audio aside: this three-part structure is exactly what artificial reverb implements (direct + early reflection taps + statistical late tail), and the Sabine/Eyring machinery supplies physically-correct parameter ranges for each room size and material set.

### 8.7 RT60 targets by room use

| Room type | Target $T_{60}$ (mid-freq) |
|---|---|
| Recording studio control room | 0.2–0.3 s |
| Broadcast studio | 0.3–0.5 s |
| Home cinema | 0.3–0.5 s |
| Classroom | 0.4–0.6 s (ANSI S12.60, BB93) |
| Private/open-plan office | 0.4–0.6 s (BS 8233, WELL ≤ 0.5–0.6) |
| Restaurant | 0.6–1.0 s |
| Lecture hall | 0.7–1.2 s (volume-scaled, DIN 18041) |
| Opera house | 1.2–1.6 s |
| Chamber music hall | 1.4–1.8 s |
| **Concert hall (orchestral)** | **1.8–2.2 s** |
| Church/worship | 1.5–3.0+ s |
| Cathedral | 3.0–8+ s |

Sources: [AcousPlan complete reference](https://acousplan.com/blog/reverberation-time-complete-reference) (with per-standard citations: EBU Tech 3276, BS 8233, DIN 18041, ANSI S12.60, ISO 3382); [AcousPlan RT60 by room type](https://acousplan.com/blog/rt60-target-wrong-room-type-matters) (ISO 3382-1 Table A.1 ranges: opera 1.2–1.5, chamber 1.4–1.7, symphony 1.7–2.1, romantic 2.0–2.2, organ 2.0–3.5 s); [Commercial Acoustics guide](https://commercial-acoustics.com/guides/reverberation-time-for-different-rooms/). Measured halls: Boston Symphony Hall 1.85–1.9 s occupied, Vienna Musikverein 2.0 s, Concertgebouw 2.0 s, Royal Albert Hall 2.4 s (the problematic one) ([AcousPlan venue table](https://acousplan.com/blog/reverberation-time-complete-reference)). Speech wants < 1 s; each music genre has its preferred band, and low-frequency $T_{60}$ typically runs 1.1–1.3× the mid value ("bass warmth," Beranek) ([AcousPlan](https://acousplan.com/blog/reverberation-time-complete-reference)).

## 9. The Doppler Effect

### 9.1 Derivation

Two distinct mechanisms, and the asymmetry between them is the whole point.

**Moving source** (speed $v_s$, stationary observer). Each compression is emitted from where the source *is* at that moment. In one period $T_s = 1/f_s$ the source advances $v_s T_s$ while the previous wavefront advances $cT_s$. Ahead of the source the wavelength is **physically compressed**:

$$\lambda_{\text{ahead}} = (c - v_s)T_s = \frac{c - v_s}{f_s}, \qquad \lambda_{\text{behind}} = (c + v_s)T_s.$$

The wave still travels at $c$ in the still air (wave speed is a property of the medium, not the source), so the observed frequency is

$$f_{\text{ahead}} = \frac{c}{\lambda_{\text{ahead}}} = f_s\,\frac{c}{c - v_s} \;>\; f_s.$$

**Moving observer** (speed $v_o$, stationary source). The wavelength in the air is untouched ($\lambda = c/f_s$), but the observer **sweeps through crests faster**, meeting them at relative speed $c + v_o$:

$$f_{\text{obs}} = \frac{c + v_o}{\lambda} = f_s\,\frac{c + v_o}{c}.$$

Combining, with the "approaching" sign convention upper ([OpenStax College Physics §17.4](https://openstax.org/books/college-physics-2e/pages/17-4-doppler-effect-and-sonic-booms); [Harvard Schwartz lecture 21](https://scholar.harvard.edu/files/schwartz/files/lecture21-doppler.pdf)):

$$\boxed{\;f' = f\,\frac{c \pm v_o}{c \mp v_s}\;}$$

The **asymmetry**: the source term rescales the *denominator* (it deforms the wavelength in the medium, an effect that survives to all observers downstream) while the observer term rescales the *numerator* (a pure interception-rate effect that exists only for that observer). Swap source and observer at equal relative speed and the shifts differ: for $v_s = v_o = 0.1c$, moving-source gives $f' = 1.111f$ but moving-observer gives $f' = 1.1f$ ([OpenStax worked example: 17 Hz vs 14 Hz asymmetry](https://openstax.org/books/college-physics-2e/pages/17-4-doppler-effect-and-sonic-booms)). The effect depends only on the line-of-sight velocity component; at passing angle $\theta$, replace $v_s \to v_s\cos\theta$ ([Schwartz](https://scholar.harvard.edu/files/schwartz/files/lecture21-doppler.pdf)). Both formulas are exact for steady line-of-sight motion; the general 3-D constant-velocity result needs emission- vs reception-time angles ([Gratuze, Eur. J. Phys. 2024](https://google.iopscience.iop.org/article/10.1088/1361-6404/ad230c)). Game-audio aside: pitch-shift the *source's* emission by $c/(c - v_s\cos\theta)$ evaluated per-frame from actual geometry — the correct asymmetry falls out for free, and a source circling a stationary listener at constant radius gets no shift, as it should.

**Worked example:** an 800 Hz siren approaching at 30 m/s (c = 343): $f' = 800\times343/313 = 877$ Hz on approach, $800\times343/373 = 737$ Hz receding — a 140 Hz drop as it passes.

### 9.2 The Mach cone and sonic booms

As $v_s \to c$, $\lambda_{\text{ahead}} \to 0$ and $f_{\text{ahead}} \to \infty$: every wavefront the source emits is emitted exactly where the previous one is. Beyond it ($v_s > c$), the source outruns its own wavefronts, which now overlap on a **cone** trailing the source. From the geometry — after time $t$ the source has gone $v_s t$, the first wavefront a sphere of radius $ct$ — the cone half-angle satisfies ([OpenStax](https://openstax.org/books/college-physics-2e/pages/17-4-doppler-effect-and-sonic-booms); [Maricopa shock waves](https://open.maricopa.edu/mccphy121jg5/chapter/shock-waves/)):

$$\boxed{\;\sin\theta = \frac{ct}{v_s t} = \frac{c}{v_s} = \frac{1}{M}\;}$$

with $M = v_s/c$ the Mach number. The superposed crests form the **sonic boom** — an N-shaped pressure pulse (the "N-wave") sweeping the ground *behind* the aircraft, which has already passed ([OpenStax](https://openstax.org/books/college-physics-2e/pages/17-4-doppler-effect-and-sonic-booms); [Jiménez nonlinear acoustics simulations](https://nojigon.webs.upv.es/simulations_nonlinear1D.php) — note the whip crack is the same physics at ~2 m scale). Two booms per aircraft (nose and tail) were distinctly audible in Space Shuttle landings ([OpenStax](https://openstax.org/books/college-physics-2e/pages/17-4-doppler-effect-and-sonic-booms)). M = 2.85 (SR-71 record) gives θ = 20.5°.

## 10. Nonlinear Acoustics (Brief but Real)

### 10.1 Why linearity breaks

The adiabatic law $P \propto \rho^\gamma$ is a *curve*; §1.3 replaced it by its tangent. Retaining the curvature, the local sound speed acquires an amplitude-dependent correction. Two physical effects act in the same direction ([Shepherd, Gee & Hanford, JASA 2011](https://doi.org/10.1121/1.3595743)):

1. **Convection**: the compression phase of the wave moves the fluid *forward*, so a crest rides a medium already moving in its direction — crest speed $\approx c + \beta u$, where $\beta = 1 + B/2A \approx 1.2$ for air is the coefficient of nonlinearity built from the equation-of-state parameter $B/A$ ([Prasad, JASA](https://math.iisc.ac.in/~prasad/prasad/Articles-parus/Articles/JASA_Fresnel.pdf); [Jiménez](https://nojigon.webs.upv.es/simulations_nonlinear1D.php)).
2. **Local heating**: the adiabatically compressed crest is warmer, and $c = \sqrt{\gamma R_* T}$ rises with $T$.

Both make **crests travel faster than troughs**. A sinusoid progressively steepens into a sawtooth: the front face leans over until it becomes a near-discontinuity — a **shock**. The propagation distance at which the shock forms (for an initially sinusoidal plane wave of peak particle velocity $v_0$) is ([Jiménez](https://nojigon.webs.upv.es/simulations_nonlinear1D.php); the same form appears in [Prasad](https://math.iisc.ac.in/~prasad/prasad/Articles-parus/Articles/JASA_Fresnel.pdf)):

$$\boxed{\;z_s = \frac{c_0^2}{\beta\,\omega\,v_0} = \frac{\rho_0 c_0^3}{\beta\,\omega\,p_0}\;}$$

**Worked example:** 1 kHz at 140 dB SPL ($p_0 = 200$ Pa): $z_s = (1.2\times343\times343^2)/(2\pi\times1000\times200) \approx 66$ m — a jet at takeoff steepens into a shock within a stadium's length. At 160 dB ($p_0 = 2000$ Pa) it is under 7 m. Speech at 60 dB would need ~600 km. The threshold where nonlinearity matters in *propagation* is roughly 140–150 dB; below that the linear theory of §§1–8 is essentially exact. The classic analytic solutions: Fubini (pre-shock harmonic growth, energy cascading into harmonics), then Fay/Blackstock (sawtooth regime) ([Shepherd et al.](https://doi.org/10.1121/1.3595743); [Khokhlova et al., JASA 2001](https://doi.org/10.1121/1.1369097)).

### 10.2 Shock absorption and why thunder cracks then rumbles

Shocks dissipate energy at a rate *independent of viscosity* (the Rankine–Hugoniot jump conditions set the entropy production; viscosity only sets the shock's thickness) — a weak shock loses roughly a fixed fraction of its amplitude per wavelength, which is an enormous effective attenuation of the high-frequency sharp front ([Yuldashev et al., JASA 2010](https://acoustique.ec-lyon.fr/publi/yuldashev_jasa10.pdf) — measured and simulated N-wave rise-time lengthening and pulse lengthening in air; [Wikibooks: Thunder acoustics](https://en.wikibooks.org/wiki/Engineering_Acoustics/Thunder_acoustics)).

**Thunder** assembles all of this ([Wikibooks: Thunder acoustics](https://en.wikibooks.org/wiki/Engineering_Acoustics/Thunder_acoustics)): the lightning channel heats to ~24,000 K in microseconds, launching a strong cylindrical/spherical shock. That shock relaxes to an ordinary acoustic N-wave within the "relaxation radius" (of order tens of meters for typical stroke energies — $R_s = (3E_t/4\pi p_0)^{1/3}$ for spherical geometry). What you hear depends on distance and geometry:

- **Close**: the still-near-shock, high-frequency-rich crack/clap — especially within ~30° of the perpendicular to a macro-tortuous channel segment, where segments' radiation adds coherently ([Wikibooks](https://en.wikibooks.org/wiki/Engineering_Acoustics/Thunder_acoustics), citing A. A. Few's mechanism).
- **Far (≥ 1 km)**: the crack's high frequencies have been destroyed — by nonlinear shock dissipation (the "eroding" effect that rounds the pressure jump), by the f² classical + relaxation air absorption of §5, and by scattering on turbulence, raindrops, and cloud aerosol — leaving the **low-frequency rumble**, stretched further by the different arrival times from a kilometers-long tortuous channel and by multi-path ground reflections.
- **Infrasound** (< 20 Hz) from intra-cloud charge redistribution can carry for tens of km.

The same physics chains through §5.3's numbers: at 1 km, ISO 9613-1 air absorption alone removes ~5 dB at 1 kHz, ~30 dB at 4 kHz, ~160 dB at 10 kHz (20 °C, 50 % RH) — and the shock-dissipation mechanism adds amplitude-dependent extra high-frequency loss on top for the close-in boom. **Crack near, rumble far, rumble forever for the big ones** — all of it linear filtering plus a nonlinear birth.

---

*End of section draft. All URLs inline above were live on 2026-09-06. Unverified-in-source items: none material; the 4.7/30/159 dB/km attenuation row and the 66 m shock-formation distance are my own computations from cited standard equations rather than quoted table values, and are marked as such.*

---

# Part 2 — The Mathematics of Sound as a Signal: Fourier Analysis, Sampling, Noise, and Synthesis


> **Provenance.** Drafted 2026-09-06 by a delegated research agent using Exa web search (`mcp__exa__web_search_exa` / `mcp__exa__web_fetch_exa`); 29 search queries across 12 batches against primary sources (AES e-library, ITU, CCRMA/Stanford online books — verified live on 2026-09-06 — original paper PDFs hosted at Stanford, UCSD, Rochester, and elsewhere). Every formula, constant, paper attribution, and date below was checked against at least one live web source; corrections to the commissioning brief are flagged inline (notably: Voss & Clarke 1975 appeared in *Nature*, not PNAS; the "16-bit ≈ 96 dB" figure is exactly 98.08 dB under the full-scale-sine convention). This section is written to feed the engine's procedural-audio work: every result here is either a formula a C++ audio module will implement verbatim, a constant it will embed, or a constraint that bounds what it can perceptually get away with.

**Summary.** Sound, once transduced, is a real-valued function of time. Because the acoustic wave equation and (approximately) the ear are linear and time-invariant at ordinary levels, the entire machinery of linear systems applies: sinusoids are the eigenfunctions, so Fourier analysis is not a convention but *the* natural coordinate system for sound. This section develops that machinery rigorously — Fourier series and transform with the convolution theorem and the time-frequency uncertainty bound, the sampling theorem with exact reconstruction, quantization noise with the full $6.02N + 1.76$ dB derivation, the statistics and generation algorithms of colored noise (Voss–McCartney, Kellet), filters and resonators culminating in the RBJ biquad cookbook, envelope and modulation mathematics including the complete Jacobi–Anger/Bessel treatment of Chowning FM, additive/subtractive synthesis with the exact Fourier series of the classic waveforms and the source–filter model of vowels, physical modeling synthesis (Karplus–Strong, digital waveguides, modal, banded waveguides, FDTD), the perceptual frequency and loudness scales an audio engine needs in code (mel, Bark, ERB, dBFS/LUFS/LU per ITU-R BS.1770), and the reverberation mathematics of Schroeder comb/allpass networks, feedback delay networks, and partitioned convolution.

---

## 2.1 Sound as a Signal — the Mathematical Model

### 2.1.1 Acoustic pressure as a function

Sound in air is a longitudinal pressure disturbance. The canonical mathematical model takes the instantaneous acoustic pressure deviation from ambient,

$$p(t) = p_{\text{atm}} + s(t),$$

and works with $s(t)$ — a real scalar function of time once we fix a listening point (the full field $s(\mathbf{x},t)$ is a function of space and time; the single-point signal is what a microphone, and one ear, sees). Human hearing spans roughly $20\ \mu\text{Pa}$ (threshold of hearing) to $20\ \text{Pa}$ (threshold of pain), a factor of $10^6$ in amplitude, which motivates the logarithmic sound-pressure level

$$L_p = 20\log_{10}\!\left(\frac{p_{\text{rms}}}{20\ \mu\text{Pa}}\right)\ \text{dB SPL},$$

a range of about 120 dB — a number that will recur when we size our quantization word length in §2.3.

### 2.1.2 Linearity and superposition

The small-amplitude acoustic wave equation is linear in $s$; at ordinary sound levels (roughly below $\sim$130–140 dB SPL, where local pressure variations are still a tiny fraction of atmospheric pressure) the propagation medium itself does not distort, so two sources playing simultaneously produce, to excellent approximation, the arithmetic sum of their individual pressure fields. The ear is *approximately* linear over its central operating range, but not exactly: the middle-ear reflex kicks in above roughly 80–90 dB SPL, and the cochlea's outer-hair-cell active process compresses dynamic range (a roughly compressive, power-law input–output curve rather than a linear one). Linearity breaks in three places the engine must respect: very high SPLs (nonlinear propagation, shock formation), the cochlear compression just mentioned, and phase — the ear is largely phase-deaf for steady tones but exquisitely sensitive to temporal fine structure below ~1.5 kHz. For synthesis purposes, the linear model is the correct first-order theory, and its failure modes are perceptual effects to be modeled separately (e.g., loudness compression) rather than corrections to the signal algebra.

### 2.1.3 Why sinusoids: eigenfunctions of LTI systems

A linear time-invariant (LTI) system is characterized by its impulse response $h(t)$; its action is convolution $y = x * h$. The single fact that makes Fourier analysis *the* tool of audio is:

**Claim.** If $x(t) = e^{i\omega t}$, then $y(t) = H(i\omega)\, e^{i\omega t}$, where $H(i\omega) = \int_{-\infty}^{\infty} h(\tau) e^{-i\omega \tau}\, d\tau$.

*Derivation (one line of substance).* $y(t) = \int h(\tau)\, x(t - \tau)\, d\tau = \int h(\tau) e^{i\omega(t-\tau)} d\tau = e^{i\omega t} \int h(\tau) e^{-i\omega \tau} d\tau$. ∎

Complex exponentials pass through an LTI system *unchanged in form* — only scaled and phase-shifted. No other family of signals has this property, so sinusoids are not a habit but the eigenbasis of every linear audio process: filters, rooms (in the linear regime), and (approximately) the ear's basilar membrane mechanics. This is why decomposing sound into sinusoids, doing arithmetic on the coefficients, and recombining, is not an approximation strategy but an exact change of coordinates.

**Phasor representation.** A real sinusoid $A\cos(\omega t + \varphi)$ is written as the real part of $A e^{i\varphi} e^{i\omega t}$; the complex number $A e^{i\varphi}$ is the *phasor*. Amplitude and phase become a single complex gain, and cascading LTI stages becomes multiplying phasors.

### 2.1.4 Amplitude, frequency, phase, RMS vs peak

For $x(t) = A\sin(\omega t)$:

- **Peak amplitude** $A$; **peak-to-peak** $2A$.
- **RMS**: derive it — $x^2(t) = A^2 \sin^2(\omega t) = \tfrac{A^2}{2}\left(1 - \cos(2\omega t)\right)$. The mean over one period kills the cosine term (its average is zero), so

$$x_{\text{rms}}^2 = \frac{A^2}{2} \quad\Longrightarrow\quad x_{\text{rms}} = \frac{A}{\sqrt{2}} \approx 0.707\,A.$$

That is the entire content of the famous $1/\sqrt{2}$: a sinusoid spends its time near its peaks, so its power ($\propto$ mean square) is half the square of its peak. Worked example: a sine at 0 dBFS (peak $=1$ in normalized digital units) has RMS $1/\sqrt2$, i.e. $-3.01$ dBFS — the same $-3.01$ that will reappear in the ITU loudness calibration (§2.9.4).

### 2.1.5 Beats and roughness

Sum two sines of nearby frequency: using $\sin a + \sin b = 2 \sin\!\big(\tfrac{a+b}{2}\big)\cos\!\big(\tfrac{a-b}{2}\big)$,

$$\sin(2\pi f_1 t) + \sin(2\pi f_2 t) = 2\cos\!\big(\pi (f_1 - f_2) t\big)\, \sin\!\big(\pi (f_1 + f_2) t\big).$$

The result is a tone at the average frequency $\tfrac{f_1+f_2}{2}$ whose amplitude envelope is $2|\cos(\pi \Delta f\, t)|$ with $\Delta f = |f_1 - f_2|$. The envelope's maxima recur every $1/\Delta f$ seconds, so the **beat frequency is $f_b = |f_1 - f_2|$** — e.g. 440 Hz and 443 Hz produce 3 beats per second. (Zeros of the envelope occur twice as often, at rate $2\Delta f$; the perceptual "throb" is at $\Delta f$.)

When $\Delta f$ exceeds roughly 10–20 Hz, the ear can no longer follow individual beats and the percept becomes *roughness*; as $\Delta f$ exceeds the critical bandwidth of the pair's center frequency (§2.9), the two tones resolve as separate pitches. This progression — beats → roughness → two clean tones — is the raw material of sensory dissonance curves (Plomp & Levelt 1965) and is why detuned unisons sound "fat" (deliberate beating) and minor seconds close together sound "sour" (roughness inside one critical band).

---

## 2.2 Fourier Analysis — the Full Treatment

### 2.2.1 Fourier series of periodic signals

A signal periodic with period $T$ ($f(t+T) = f(t)$, fundamental angular frequency $\omega_0 = 2\pi/T$) that satisfies the Dirichlet conditions (absolutely integrable over one period, finitely many maxima/minima and discontinuities per period) admits

$$f(t) = \sum_{n=-\infty}^{\infty} c_n\, e^{in\omega_0 t}, \qquad c_n = \frac{1}{T}\int_{0}^{T} f(t)\, e^{-in\omega_0 t}\, dt.$$

The coefficients are projections onto the orthogonal basis $\{e^{in\omega_0 t}\}$; the orthogonality integral $\int_0^T e^{i(n-m)\omega_0 t} dt = T\,\delta_{nm}$ is what makes the analysis formula fall out of the synthesis formula. For real $f$, $c_{-n} = \overline{c_n}$, so the spectrum is conjugate-symmetric and the signal is a sum of real sinusoids at $n f_0$ (the *harmonics*) with amplitude $2|c_n|$ and phase $\arg c_n$.

**Convergence caveats.** At a jump discontinuity, the series converges to the *midpoint* of the jump (Dirichlet's theorem), and — the famous part — the partial sums *overshoot* the jump by a fixed fraction that does **not** vanish as terms are added.

**Gibbs phenomenon.** For a jump of height $h$, the $N$-th partial sum overshoots by

$$h \cdot \left(\frac{1}{\pi}\int_0^{\pi}\frac{\sin t}{t}\,dt - \frac{1}{2}\right) = h \cdot 0.089489872236\ldots$$

— about **8.95%** of the jump on each side (so the partial sum's apparent jump is ≈17.9% too tall). The overshoot's *width* shrinks as $1/N$ (its energy vanishes — convergence in mean square is fine even where pointwise convergence fails), but its *height* is constant. The constant $\int_0^\pi \sin(t)/t\,dt = 1.85194\ldots$ is the Wilbraham–Gibbs constant ([Wikipedia: Gibbs phenomenon](https://en.wikipedia.org/wiki/Gibbs_phenomenon); [MIT 18.03 notes](https://ocw.mit.edu/courses/18-03sc-differential-equations-fall-2011/05cce833730ffd3c39f420a41ad82fd6_MIT18_03SCF11_s22_7text.pdf) derive the 1.089 figure for the unit square wave step by step).

**Why synthesized square waves "ring."** A naive square-wave oscillator is a *truncated* Fourier series — a brick-wall band-limit of an ideal square wave. Convolving the ideal square wave with a sinc kernel (the ideal lowpass) is exactly what Gibbs quantifies: pre-/post-oscillations near every corner at ±9% of the edge, decaying as $1/t$. Every band-limited square wave *must* do this; it is mathematics, not a bug. Practical synthesis removes the audible artifact either by synthesizing transitions as integrated band-limited steps (the BLEP family of algorithms — the overshoot is distributed into the step itself) or by simply not using naive rectangles.

### 2.2.2 The Fourier transform pair

For aperiodic, finite-energy signals the series becomes an integral:

$$\boxed{\;X(f) = \int_{-\infty}^{\infty} x(t)\, e^{-2\pi i f t}\, dt, \qquad x(t) = \int_{-\infty}^{\infty} X(f)\, e^{\,2\pi i f t}\, df\;}$$

(The $e^{-2\pi i f t}$ convention puts the $2\pi$ in the exponent and keeps both formulas unitary and symmetric; Julius O. Smith's CCRMA texts use it throughout — e.g. [*Mathematics of the DFT*](https://ccrma.stanford.edu/~jos/).)

**Properties used daily in audio:**

- **Linearity** — obvious and load-bearing: spectra of a mix are the mix of spectra.
- **Time shift ↔ phase factor**: $x(t - t_0) \leftrightarrow X(f)\, e^{-2\pi i f t_0}$. Delaying a signal rotates every spectral component's phase linearly in $f$; a pure delay does not change magnitude. (This is why inter-channel delay is inaudible as coloration but audible as localization — ITD.)
- **Convolution theorem** — *the* theorem of filtering:

$$\boxed{\;x * h \;\longleftrightarrow\; X \cdot H\;}$$

*Derivation.* Write the transform of the convolution and substitute variables:
$$\mathcal{F}\{x*h\}(f) = \int\!\!\int x(\tau)\, h(t-\tau)\, e^{-2\pi i f t} d\tau\, dt = \int x(\tau) \left[\int h(t-\tau) e^{-2\pi i f t} dt\right] d\tau.$$
Inner integral: substitute $u = t - \tau$: $\int h(u) e^{-2\pi i f (u + \tau)} du = e^{-2\pi i f \tau} H(f)$. Then
$$\int x(\tau)\, e^{-2\pi i f \tau} d\tau \cdot H(f) = X(f)\, H(f). \;∎$$
(Interchange of integrals is justified by absolute integrability of both signals.) Filtering — the single most common audio operation — is multiplication in the frequency domain; a cascade of filters multiplies transfer functions; a room's effect on a signal is (linear regime) the convolution of the source with the room's impulse response. Everything in §§2.5–2.10 rides on this theorem.
- **Parseval / Plancherel** — energy conservation between domains:

$$\int_{-\infty}^{\infty} |x(t)|^2\, dt = \int_{-\infty}^{\infty} |X(f)|^2\, df.$$

*Skeleton of proof:* expand $|x(t)|^2 = x(t)\overline{x(t)}$, substitute the inverse transform for one factor, interchange integrals, and recognize the inner integral as a delta function $\delta(f' - f)$ (the orthogonality of complex exponentials in the distributional limit); the outer integral is then $\int |X(f)|^2 df$. Practically: total energy is domain-independent, so one may compute loudness, spectral tilt, or noise power in whichever domain is convenient, and windowed spectra must be normalized so this still (approximately) holds.

**The uncertainty principle.** With durations defined as normalized second moments, $\Delta t$ of the signal's energy in time and $\Delta f$ of its energy in frequency obey

$$\Delta t \cdot \Delta f \;\ge\; \frac{1}{4\pi} \approx 0.0796,$$

with equality **only** for the Gaussian pulse (Gabor 1946, [*Theory of Communication*](https://jmft.dev/uncertainty-principle-and-spectrograms.html) restates the standard result). This is a property of the Fourier pair itself, not of any instrument. Practical audio version, in mainlobe terms: a window of duration $T$ has frequency resolution on the order of $1/T$ (rectangular mainlobe: $2/T$; Hann: $4/T$).

**Worked example (get this right).** *Can a 20 ms analysis window resolve two tones 25 Hz apart?* The bin spacing alone is $\Delta f = 1/T = 1/0.020 = 50\ \text{Hz} > 25\ \text{Hz}$ — already fatal at the coarsest level of counting bins. The mainlobe makes it worse: a rectangular window's mainlobe is $2/T = 100$ Hz wide, a Hann window's is $4/T = 200$ Hz. To *resolve* 25 Hz separation you need at minimum $T \ge 1/25 = 40$ ms (bin-spacing criterion, rectangular), and realistically $T \approx 4/25 = 160$ ms with a Hann window so the two mainlobes are cleanly separated (a practical design rule: choose $L \ge k_w f_s / \Delta f_{\min}$ with $k_w = 4$ for Hann; [practical STFT guide](https://yuhi-sa.github.io/en/posts/20260703_spectrogram_practice/1/)). Short window = good time localization, poor frequency resolution; long window = the reverse; no window wins both. This trade governs every spectrogram, every pitch detector, and every filterbank choice the audio engine will make.

### 2.2.3 DFT/FFT, windows, spectrograms

The **discrete Fourier transform** of a length-$N$ frame:

$$X[k] = \sum_{n=0}^{N-1} x[n]\, e^{-2\pi i k n / N}, \qquad k = 0, \ldots, N-1,$$

computable in $O(N\log N)$ by the FFT. Bin spacing is $\Delta f = f_s / N$ (e.g. $f_s = 48\,\text{kHz}$, $N = 4096$ → 11.72 Hz per bin). The DFT evaluates the DTFT of the windowed frame at $N$ equally spaced frequencies; a real sinusoid not exactly bin-centered smears energy across all bins through the window's transform — *spectral leakage* — which is why one multiplies the frame by a **window function** before the FFT. Verified window properties (cross-checked against [Smith's *Spectral Audio Signal Processing*, §Windows](https://dsprelated.com/freebooks/sasp/Spectrum_Analysis_Windows.html), the [VRU window table](https://vru.vibrationresearch.com/lesson/table-of-window-function-details/), and [Harris's classic tables as reproduced by Stable32](http://www.stable32.com/Properties%20of%20FFT%20Windows%20Used%20in%20Stable32.pdf)):

| Window | Highest sidelobe | Sidelobe roll-off | Mainlobe width (bins) | ENBW (bins) |
|---|---|---|---|---|
| Rectangular | **−13.3 dB** | −6 dB/oct | 2 | 1.00 |
| Hann | **−31.5 dB** | −18 dB/oct | 4 | 1.50 |
| Hamming | **−42.7 dB** (≈−43) | −6 dB/oct | 4 | 1.36 |
| Blackman | **−58 dB** (−58.1) | −18 dB/oct | 6 | 1.73 |

(The precise values −13.3239, −31.5565/−31.5, −43.7547/−42.7 depending on the exact Hamming parameter, −58.2336 differ by decimals across sources; the one-digit values above are what appear in every authoritative table.) The trade is one-for-one: every dB of sidelobe suppression is bought with mainlobe width, i.e. frequency resolution — the same uncertainty principle again, dressed as filter design.

The **spectrogram** is $|STFT|^2$: slide a window of length $L$ along the signal with hop $H$, FFT each frame, display magnitude-squared, typically in dB and with log-frequency warping. Short windows (e.g. 256 samples @ 48 kHz ≈ 5 ms) resolve onsets but smear pitch; long windows (4096+ samples) resolve harmonics but smear onsets. Standard practice: 50–75% overlap ($H = L/2$ to $L/4$) for display; COLA/NOLA conditions if the STFT must be invertible.

---

## 2.3 Sampling and Digitization

### 2.3.1 The Nyquist–Shannon sampling theorem

**Statement (precise).** Let $x(t)$ be a continuous-time signal whose Fourier transform $X(f)$ vanishes for all $|f| \ge B$. Then $x(t)$ is uniquely determined by its samples $x[n] = x(nT_s)$ taken at any rate $f_s = 1/T_s > 2B$, and is recovered exactly by

$$\boxed{\;x(t) = \sum_{n=-\infty}^{\infty} x[n]\, \operatorname{sinc}\!\left(\frac{t}{T_s} - n\right), \qquad \operatorname{sinc}(u) = \frac{\sin(\pi u)}{\pi u}\;}$$

— the **Whittaker–Shannon interpolation formula** (Whittaker 1915; Kotelnikov 1933; Shannon's 1949 proof in ["Communication in the Presence of Noise"](https://en.wikipedia.org/wiki/44,100_Hz), *Proc. IRE* 37(1):10–21; the cleanest modern treatment is [Smith, *Introduction to Digital Filters* / *Digital Audio Resampling*](https://ccrma.stanford.edu/~jos/resample/resample.pdf)). Sampling a bandlimited signal multiplies it by an impulse train; in frequency this *convolves with* an impulse train, replicating $X(f)$ as images centered at every multiple of $f_s$. If $X$ is supported on $|f| < f_s/2$, the images do not overlap and the baseband copy can be re-isolated by an ideal lowpass — whose impulse response is the sinc, giving the reconstruction formula term by term.

**Aliasing as spectral folding.** If content exists above $f_s/2$ (the *Nyquist frequency*), the image centered at $f_s$ folds down into the baseband: a component at $f_s/2 + \delta$ reappears at $f_s/2 - \delta$ — a phantom tone that was never in the original, non-removable after the fact. Classic game-audio example: a footstep containing energy to 30 kHz sampled at 44.1 kHz aliases 22.05 kHz of it down to the mid-teens — a metallic zip riding the thud. The defense is the **anti-aliasing filter**: an analog lowpass before the ADC (or a decimation filter before any downsampling) that attenuates everything above the new Nyquist to below audibility. Real filters have transition bands, which is why $f_s$ must exceed $2B$ with margin: 44.1 kHz gives a 20 kHz passband and a 2.05 kHz transition band; oversampling converters (e.g. $\Delta\Sigma$ at MHz rates) push the analog filter so far out that a gentle one suffices, then decimate digitally.

**Why 44.1 kHz and 48 kHz (one paragraph).** In the late 1970s the only affordable high-bandwidth storage for digital audio was the video cassette recorder: PCM adapters (Sony PCM-1600, 1979, and successors) encoded audio samples as pseudo-video on U-matic tape, so the sample rate had to lock to the video line/field arithmetic — an integer number of samples per usable scan line. The magical coincidence: NTSC video has 245 usable lines per field × 60 fields/s × 3 samples per line = **44,100**, and PAL has 294 × 50 × 3 = **44,100** — one rate compatible with both world TV systems ([Wikipedia: 44,100 Hz](https://en.wikipedia.org/wiki/44,100_Hz); the account originates in Watkinson, *The Art of Digital Audio*). Color NTSC's 59.94 Hz field rate yields the 44,056 Hz variant. The CD Red Book (1980) inherited 44.1 kHz from the mastering machines; note $44100 = 2^2 \times 3^2 \times 5^2 \times 7^2$ ([verified factorization](https://metanumbers.com/44100)) — a 5-smooth-style highly composite number with 81 divisors, which is why it has so many exact rational relationships, but that is a *consequence* of the video-line arithmetic, not its cause (the "245 × 180" form sometimes quoted is wrong; it is 245 × 60 × 3). 48 kHz descends from the video/broadcast lineage instead (AES5 professional rate; $48 = 32 \times 3/2$, tied to the 32 kHz DAT speech rate), coexisting with 44.1 kHz ever since.

### 2.3.2 Quantization: the $6.02N + 1.76$ dB law, derived

Sampling discretizes time; quantization discretizes amplitude. An $N$-bit uniform quantizer divides the full-scale range $\text{FSR}$ into $2^N$ steps of

$$\Delta = \frac{\text{FSR}}{2^N}.$$

**Noise power.** If the input is "busy" relative to $\Delta$ (crosses many levels between samples), the rounding error $e$ is well modeled as uniform on $(-\Delta/2, \Delta/2)$:

$$P_e = \int_{-\Delta/2}^{\Delta/2} \frac{1}{\Delta} e^2\, de = \frac{1}{\Delta}\left[\frac{e^3}{3}\right]_{-\Delta/2}^{\Delta/2} = \frac{1}{\Delta}\cdot\frac{2\,(\Delta/2)^3}{3} = \frac{\Delta^2}{12}.$$

**Signal power.** A full-scale sine has peak amplitude $\text{FSR}/2 = 2^{N}\Delta/2$, so its RMS is $\frac{2^N \Delta/2}{\sqrt{2}}$ (the §2.1.4 result).

**SNR.**

$$\text{SNR} = 20\log_{10}\!\left(\frac{2^N\Delta / 2\sqrt{2}}{\Delta/\sqrt{12}}\right) = 20\log_{10}\!\left(2^N \cdot \sqrt{\tfrac{12}{8}}\right) = 20\log_{10}\!\left(2^N \sqrt{\tfrac{3}{2}}\right)$$

$$= 20 N\log_{10} 2 + 10\log_{10}(3/2) = 6.02\,N + 1.76\ \text{dB}.$$

$$\boxed{\;\text{SNR}_{\text{quant}} = 6.02\,N + 1.76\ \text{dB} \quad \text{(full-scale sine, noise over } 0 \ldots f_s/2\text{)}\;}$$

This is the Analog Devices MT-001 derivation verbatim ([Kester, "Taking the Mystery out of the Infamous Formula SNR = 6.02N + 1.76 dB"](https://www.analog.com/media/en/training-seminars/tutorials/MT-001.pdf)); each added bit halves $\Delta$, doubling SNR (+6.02 dB).

**Numerically, exactly:** $6.02 \times 16 + 1.76 = 96.32 + 1.76 = \mathbf{98.08\ \text{dB}}$ for 16-bit, and $6.02 \times 24 + 1.76 = 146.24 \approx \mathbf{146\ \text{dB}}$ for 24-bit. The folkloric "16-bit ≈ 96 dB" is the $6.02N$ term alone (or the $6N + 6$ variant used for full-scale-square/headroom conventions); state 98.08 dB for the full-scale-sine definition and say which convention you are using. Against the ear's ~120 dB range: 16-bit's 98 dB is short of full auditory range but adequate given that real program material rarely uses the top 20 dB; 24-bit's 146 dB covers the ear entirely, which is why 24-bit is "more than enough" and why real converters run out of bits to thermal noise around 20–21 ENOB anyway.

**Dither.** The uniform-error model fails for *small or slowly varying* signals: a decaying tone slides down the quantization staircase and the error becomes correlated with the signal — audible as grainy, harmonic-spiced distortion ("quantization distortion") rather than benign noise. **Dither** — adding a small random signal before quantization — decorrelates the error from the signal at the price of a slight noise-floor increase. The rigorous treatment is the Waterloo school: Vanderkooy & Lipshitz, ["Resolution below the Least Significant Bit in Digital Systems with Dither,"](https://secure.aes.org/forum/pubs/journal/?elib=7047) *JAES* 32(3):106–113 (1984), and the full survey Lipshitz, Wannamaker & Vanderkooy, ["Quantization and Dither: A Theoretical Survey,"](https://secure.aes.org/forum/pubs/journal/?elib=7047) *JAES* 40(5):355–375 (1992). Key results: a **rectangular-pdf** dither of 1 LSB peak-to-peak renders the error's *first-order* statistics (mean) independent of the input; a **triangular-pdf** (TPDF) dither of 2 LSB peak-to-peak (sum of two independent rectangulars) additionally renders the error *power* independent of the input and makes the error spectrum white — the standard audio choice. Undithered, a fade-to-zero in 16-bit ends in ghastly stepped distortion over the last few LSBs; TPDF-dithered, it fades smoothly into an innocent hiss.

### 2.3.3 Sample-rate conversion (one paragraph)

Converting between rates is a resampling problem governed by the same theorem: conceptually reconstruct the bandlimited continuous signal (sinc interpolation) and sample it again at the new rate. In practice, upsample by $L$, lowpass-filter (anti-imaging + anti-aliasing, cutoff $\min(f_s, f_s')/2$), downsample by $M$, for a rational ratio $L/M$, implemented with **polyphase** FIR structures so the filter runs only at the output rate ([Crochiere & Rabiner's multirate framework; see Smith, *Digital Audio Resampling*](https://ccrma.stanford.edu/~jos/resample/resample.pdf), which describes the interpolated-lookup table variant used for arbitrary/continuously-varying rates — e.g. Doppler-shifted game audio "scrubbing"). Irrational or time-varying ratios use windowed-sinc or polynomial (Farrow) interpolators; the audio-quality bar is flat passband and >100 dB image rejection (e.g. the [de Soras resampler design](https://ldesoras.fr/doc/articles/resampler-en.pdf)).

---

## 2.4 Noise Theory — Colors and Statistics

### 2.4.1 White noise

A discrete-time noise process is *white* if its power spectral density is flat: $S(f) = \sigma^2$ for all $f$, equivalently (Wiener–Khinchin) its **autocorrelation is a delta**: $R[\tau] = \sigma^2 \delta[\tau]$ — every sample uncorrelated with every other. **Gaussian white noise** (each sample drawn i.i.d. from $\mathcal{N}(0,\sigma^2)$) is the universal model because of the central limit theorem: any sum of many small independent contributions converges to it. It is also the *maximum-entropy* distribution for a given variance — the least-structured noise there is — which is why pure white noise sounds like "static": no correlation, no structure, nothing to grab perceptually.

### 2.4.2 Pink noise (1/f)

**Definition and the slope, verified.** Pink noise has equal *energy per octave* (equivalently per decade, per any log-width band). Derive the slope from that requirement: energy in $[f, 2f]$ is $\int_f^{2f} S(u)\,du$; for this to be constant in $f$ we need $S(f) \propto 1/f$, since

$$\int_f^{2f} \frac{C}{u}\,du = C \ln 2 \quad \text{(independent of } f\text{)}.$$

In dB per octave (power): $10\log_{10} 2 = \mathbf{3.0103 \approx 3\ \text{dB/oct}}$, i.e. **−3 dB/octave = −10 dB/decade** ([Whittle's canonical pink-noise DSP page](https://www.firstpr.com.au/dsp/pink-noise/) states precisely this: −10 dB/decade = 3.0102999 dB/octave, since power $\propto$ amplitude²). Note the subtlety: −3 dB/oct is a *power* slope; the amplitude spectral density falls at −1.5 dB/oct.

**Where it appears.** Voss & Clark**e** measured that loudness fluctuations in music and speech, and pitch (melody) fluctuations in music, exhibit 1/f spectra down to $5\times10^{-4}$ Hz — correlations over minutes. **Attribution correction:** the classic 1975 paper is Voss, R. F. & Clarke, J., ["'1/f noise' in music and speech,"](https://doi.org/10.1038/258317a0) ***Nature*** **258**:317–318 (27 Nov 1975) — *Nature*, not PNAS as often mis-cited (confirmed via the [eScholarship LBL manuscript](https://escholarship.org/uc/item/04t64495) and OSTI records); the follow-up with the generation algorithm and the "music from 1/f noise" listening experiments is Voss & Clarke, [JASA 63:258–263 (1978)](https://doi.org/10.1121/1.381721). Their stochastic compositions: white-noise melodies sounded too random, 1/f² too correlated, 1/f "pleasing" — a strong hint that 1/f is the statistics of *interesting* temporal structure.

**Generation algorithm 1: Voss–McCartney.** The original Voss scheme (popularized by Martin Gardner's 1978 *Scientific American* column) sums $N$ "dice" (white sources) updated at octave-spaced rates: source 0 every sample, source 1 every 2nd sample, source 2 every 4th, and so on. Each row is a sample-and-hold of white noise — a zero-order hold with $|\mathrm{sinc}|^2$ power response — and the rows' power spectra stack into a staircase approximating $1/f$ within about ±1 dB ripple that does *not* shrink with more rows ([Herriman's theoretical analysis](https://www.firstpr.com.au/dsp/pink-noise/allan-2/spectrum2.html); [Downey's walkthrough](https://www.dsprelated.com/showarticle/908.php)). James McCartney's 1999 refinement (music-dsp list, [archived by Whittle](https://www.firstpr.com.au/dsp/pink-noise/)): stagger the updates so *exactly one* row changes per sample — select the row by **counting trailing zeroes** of an incrementing counter (a single `CTZ` instruction), update the running total by `total += (new − old)` in O(1), and add one extra pure-white "row −1" every sample to fill the high-frequency sinc nulls (response is otherwise ~5 dB down at $f_s/4$, with a deep null at Nyquist). Cost: one PRNG draw, one add/subtract pair, one counter increment per sample — unbeatable.

**Generation algorithm 2: filtered white noise (Paul Kellet).** A weighted sum of first-order one-pole lowpass sections approximates the −3 dB/oct slope; Kellet's hand-tuned "instrumentation-grade" filter (17 Oct 1999, music-dsp / [Whittle archive](https://www.firstpr.com.au/dsp/pink-noise/); [musicdsp.org pink.txt](https://www.musicdsp.org/en/latest/_downloads/84bf8a1271c6bb0b3c88253c0546ae0f/pink.txt)), accurate to **±0.05 dB above 9.2 Hz at 44.1 kHz**:

```
b0 = 0.99886*b0 + white*0.0555179;
b1 = 0.99332*b1 + white*0.0750759;
b2 = 0.96900*b2 + white*0.1538520;
b3 = 0.86650*b3 + white*0.3104856;
b4 = 0.55000*b4 + white*0.5329522;
b5 = -0.7616*b5 - white*0.0168980;
pink = b0+b1+b2+b3+b4+b5+b6 + white*0.5362;
b6 = white*0.115926;
```

and the **"economy" version** (±0.5 dB) that everything embeds:

```
b0 = 0.99765*b0 + white*0.0990460;
b1 = 0.96300*b1 + white*0.2965164;
b2 = 0.57000*b2 + white*1.0526913;
pink = b0 + b1 + b2 + white*0.1848;
```

Kellet's design insight (from his own note): a single lowpass knee rolls off at −6 dB/oct, too steep; but the *transition* from 0 to −6 dB/oct at a knee is softer; stacking enough knees in a staircase buys −3 dB/oct, with a little delayed/high-passed signal mixed in to cancel the rise near Nyquist.

### 2.4.3 Brown noise (1/f²)

Integrate white noise: $x[n] = x[n-1] + w[n]$ is a random walk; differencing is the discrete-time integration operator whose magnitude response is $\propto 1/\sin(\omega/2) \approx 1/\omega$ at low frequencies, so the **PSD $\propto 1/f^2$** — −6 dB/octave (power). It is called *Brown* (for Brownian motion, not the color) or *red* noise. Caution for implementations: a raw random walk is not stationary — its variance grows without bound — so practical brown generators leak the integrator ($x[n] = a\,x[n-1] + w[n]$, $a \lesssim 1$), which flattens the spectrum below the leak frequency.

### 2.4.4 Blue and violet

The spectral mirror images: **blue** noise has PSD $\propto f$ (+3 dB/oct, differentiated white); **violet** $\propto f^2$ (+6 dB/oct, twice-differentiated white). Blue noise is the dither of choice for images (least-visible noise on a display); in audio it appears mainly as a conceptual complement and in noise-shaping contexts.

### 2.4.5 Why natural sounds cluster toward 1/f

The dominant theoretical account is **self-organized criticality**: Bak, Tang & Wiesenfeld, ["Self-organized criticality: an explanation of the 1/f noise,"](https://link.aps.org/doi/10.1103/PhysRevLett.59.381) *Phys. Rev. Lett.* **59**:381–384 (1987). Their sandpile model evolves to a critical state with avalanches of *all* sizes — no characteristic scale — and the superposition of uncorrelated events with a scale-free lifetime distribution yields $S(f) \propto 1/f$. One-liner for the engine docs: systems driven slowly, with threshold dynamics and many coupled degrees of freedom, sit at criticality and leak 1/f fluctuations; natural soundscapes (wind, water, crowd murmur, music itself per Voss–Clarke) are such systems, which is why 1/f-modulated noise *sounds natural* and white-modulated noise sounds synthetic.

### 2.4.6 Noise in games

Noise is the raw material of procedural audio texture: wind = pink/brown noise through slowly-modulated bandpass filters (cutoff and amplitude driven by 1/f modulators — noise modulated by noise); water = filtered noise with envelope bursts on the "splash" scale and 1/f texture on the "wash" scale; footsteps on gravel = short bursts of bandpass-filtered noise with randomized spectral tilt per step, layered over an impact transient; fire = brown-ish noise with sparse crackle transients (Poisson-distributed, amplitude-distributed ~1/f). The recipe in every case: colored noise × slow 1/f modulation × event-triggered transients.

---

## 2.5 Filters and Resonators

### 2.5.1 LTI systems and transfer functions

A causal LTI system with impulse response $h(t)$ acts by convolution $y(t) = \int_0^\infty h(\tau) x(t-\tau) d\tau$ (discrete: $y[n] = \sum_k h[k] x[n-k]$). By the convolution theorem, $Y = XH$; the **transfer function** is the transform of $h$: $H(s) = \int_0^\infty h(t) e^{-st} dt$ (Laplace, continuous) or $H(z) = \sum_n h[n] z^{-n}$ (z-transform, discrete), and the frequency response is the evaluation on the stability boundary: $s = i\omega$ or $z = e^{i\omega T_s}$.

### 2.5.2 First-order low-pass (the RC filter)

$$H(s) = \frac{1}{1 + sRC} \quad\Longrightarrow\quad |H(i\omega)| = \frac{1}{\sqrt{1 + (\omega RC)^2}}.$$

At $\omega_c = 1/(RC)$: $|H| = 1/\sqrt2$ — the **−3 dB cutoff $f_c = 1/(2\pi RC)$**. Above cutoff the magnitude falls as $1/\omega$: **−20 dB/decade (−6 dB/oct)** — a single pole per decade of slope. Worked example: $R = 1\ \text{k}\Omega$, $C = 15.9\ \mu\text{F}$ → $f_c = 10$ Hz. The digital one-pole equivalent is in §2.5.4.

### 2.5.3 Second-order resonators, Q, and the RBJ biquad

The canonical resonator (lowpass form):

$$\boxed{\;H(s) = \frac{\omega_0^2}{s^2 + \dfrac{\omega_0}{Q}\,s + \omega_0^2}\;}$$

$\omega_0$ is the natural (resonant) frequency; $Q$ the **quality factor**, which has three equivalent definitions worth keeping straight:

1. $Q = \omega_0 / (2\zeta)$ — pole distance from the $j\omega$-axis over twice the real-part... precisely $Q = \omega_0/(2|\text{Re}\,p|)$ for the pole pair $p = -\omega_0/(2Q) \pm j\omega_0\sqrt{1 - 1/(4Q^2)}$;
2. **Bandwidth**: $Q = f_0 / \text{BW}_{-3\,\text{dB}}$, i.e. $\text{BW} = f_0/Q$ — the definition that matters for EQ;
3. Energy: $Q = 2\pi \times (\text{energy stored}) / (\text{energy dissipated per cycle})$ — the physical definition, tying $Q$ to decay time ($t_{60} \approx 13.8\, Q / f_0$... more usefully: amplitude decays as $e^{-\omega_0 t/(2Q)}$).

The resonant peak: for the bandpass form $H(s) = \frac{(\omega_0/Q)s}{s^2+(\omega_0/Q)s+\omega_0^2}$, $|H(i\omega_0)| = 1$ and the −3 dB width is exactly $\omega_0/Q$. High $Q$ = narrow, ringing, "tonal" (a vowel formant at $Q \sim 10$); low $Q$ = broad, gentle (a shelf-ish tilt). Musical instruments' resonances live at $Q$ from ~5 (body modes) to ~1000+ (sustained string modes).

**The biquad, and the cookbook.** Every practical second-order digital audio filter is the **biquad**

$$H(z) = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{a_0 + a_1 z^{-1} + a_2 z^{-2}}, \qquad y[n] = \tfrac{b_0}{a_0}x[n] + \tfrac{b_1}{a_0}x[n{-}1] + \tfrac{b_2}{a_0}x[n{-}2] - \tfrac{a_1}{a_0}y[n{-}1] - \tfrac{a_2}{a_0}y[n{-}2],$$

and the canonical reference for its coefficients is Robert Bristow-Johnson's **Audio EQ Cookbook** (["Cookbook formulae for audio EQ biquad filter coefficients"](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html), maintained as a W3C note; plain-text original on [GitHub](https://github.com/WebAudio/Audio-EQ-Cookbook/blob/main/Audio-EQ-Cookbook.txt) and [musicdsp.org](https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html)). It gives closed-form coefficients for LPF, HPF, BPF (two gain conventions), notch, APF, peaking EQ, and low/high shelves, all derived by bilinear-transform digitization of the analog prototypes with frequency prewarping. The universal parameterization:

$$\omega_0 = 2\pi f_0/f_s, \qquad \alpha = \frac{\sin\omega_0}{2Q}, \qquad A = 10^{\text{dBgain}/40}.$$

For example, the lowpass:

$$b_0 = \tfrac{1-\cos\omega_0}{2},\; b_1 = 1-\cos\omega_0,\; b_2 = \tfrac{1-\cos\omega_0}{2};\qquad a_0 = 1+\alpha,\; a_1 = -2\cos\omega_0,\; a_2 = 1-\alpha.$$

**Bandpass Q and musical behavior.** In musical terms, $Q$ maps to how much the filter "rings" and how narrow its grab on the spectrum is. A $Q = 1$ bandpass at 2 kHz on pink noise sounds like a soft vowel-colored wash; $Q = 20$ sounds almost like a tone. The "constant-skirt vs constant-0dB-peak" BPF variants in the cookbook correspond to peak gain $= Q$ vs peak gain $= 0$ dB — a classic integration trap when porting the formulas (the engine should wrap both and name them).

### 2.5.4 The one-pole smoother (every game audio parameter ever)

The single most-used filter in game audio is the one-pole lowpass used to smooth a parameter:

$$y[n] = y[n-1] + \alpha\,(x[n] - y[n-1]) = \alpha x[n] + (1-\alpha) y[n-1].$$

Two time-constant descriptions circulate, and they must be **reconciled**, not confused:

- **Exponential-decay view:** the step response is $y[n] = x\,(1 - (1-\alpha)^n)$, i.e. error decays geometrically; in continuous time the error obeys $e^{-t/\tau}$ with time constant $\tau$. The **60 dB time** — the audio-relevant "how long until it's gone" — is $t_{60} = \tau \ln 1000 = \mathbf{6.9078\,\tau} \approx 6.91\,\tau$ (since $e^{-t_{60}/\tau} = 10^{-3}$).
- **Cutoff view:** the one-pole's −3 dB frequency is $f_c = 1/(2\pi\tau)$ — **the same $\tau$**, viewed in the frequency domain. One time constant, two readings: $\tau = 1/(2\pi f_c)$ and $t_{60} = 6.91\,\tau$. A 10 ms time constant is a 15.9 Hz cutoff *and* a 69 ms 60-dB settle; specify whichever the design is actually about.

The exact mapping to the discrete coefficient (matching the pole of $H(z) = \alpha / (1 - (1-\alpha) z^{-1})$ to $e^{-T_s/\tau}$):

$$\alpha = 1 - e^{-T_s/\tau} = 1 - e^{-2\pi f_c / f_s} \;\approx\; 2\pi f_c / f_s \quad (f_c \ll f_s).$$

Use the exponential form (not the approximation) whenever $f_c$ exceeds a few percent of $f_s$ — e.g. for a 5 kHz smoothing at 48 kHz, $\alpha_{\text{exact}} = 0.479$ vs $\alpha_{\text{approx}} = 0.654$, a clearly audible difference. The lerp form $y \mathrel{+}= (x - y)\cdot k$ in game code is exactly this filter with $\alpha = k$.

---

## 2.6 Envelopes and Modulation

### 2.6.1 ADSR and exponential envelope math

The ADSR envelope (attack–decay–sustain–release, Moog-era standard) is piecewise; the mathematically important choice is the *curve* of each segment. **Linear** decay $y = 1 - t/T$ crosses any fixed amplitude exactly once with constant slope; **exponential** decay $y = y_0\, e^{-t/\tau}$ has constant *percentage* rate: loudness falls by a fixed ratio per unit time. Perceptually, loudness is roughly logarithmic in amplitude, so a *linear* amplitude decay has a *quadratic* loudness trajectory — it lingers near the top then rushes to silence — and sounds "robotic"; an exponential amplitude decay is linear-in-dB and sounds like a natural dying-away. The canonical constant is the **60 dB time**: $e^{-t_{60}/\tau} = 10^{-3}$ gives

$$t_{60} = \tau \ln(1000) = 6.9078\,\tau,$$

so "a 2-second reverb tail" and "$\tau \approx 0.29$ s" are the same statement. (Same constant as §2.5.4 — because a decaying resonator *is* an exponential envelope generator.) Exponential segments are also the only segments that splice seamlessly at arbitrary junction times without slope discontinuities, which is why every mature synth envelope implementation uses them (or linear-in-dB, which *is* exponential).

### 2.6.2 Amplitude modulation

$$y(t) = \big(1 + m\cos(\omega_m t)\big)\cos(\omega_c t).$$

Expand with the product-to-sum identity $\cos a \cos b = \tfrac12[\cos(a-b) + \cos(a+b)]$:

$$y(t) = \cos(\omega_c t) + \frac{m}{2}\cos\big((\omega_c - \omega_m) t\big) + \frac{m}{2}\cos\big((\omega_c + \omega_m) t\big).$$

**Sidebands at $\omega_c \pm \omega_m$, each at $m/2$ (i.e. −6 dB relative to the carrier at $m=1$).** The spectrum is *three* lines: carrier plus a symmetric pair. **Tremolo** is AM at $f_m \lesssim 20$ Hz — below the rate at which the ear resolves the sidebands from the carrier, so it is heard as amplitude wobble (and note: at exactly $m=1$, 100% modulation, the perceived wobble depth is not what the naive amplitude plot suggests because the sidebands add *coherently*). **Ring modulation** is $y = \cos(\omega_m t)\cos(\omega_c t)$ — plain AM with the carrier suppressed (the $1+$ gone) — so only the two sidebands remain; with an inharmonic $f_m:f_c$ ratio the result is the classic metallic "ring mod" clang, exactly because the partial set $\{f_c \pm f_m\}$ has no common fundamental.

### 2.6.3 FM synthesis (Chowning 1973)

The crown jewel of compact synthesis mathematics. John Chowning, ["The Synthesis of Complex Audio Spectra by Means of Frequency Modulation,"](https://aes.org/e-lib/browse.cfm?elib=1954) *J. Audio Eng. Soc.* **21**(7):526–534 (Sept. 1973) ([author's PDF at CCRMA](https://ccrma.stanford.edu/sites/default/files/user/jc/fm_synthesis_paper.pdf)):

$$\boxed{\;y(t) = A\,\sin\!\big(\omega_c t + I \sin(\omega_m t)\big)\;}$$

where $I = \Delta f / f_m$ is the **modulation index** (peak phase deviation in radians). Note this is technically *phase* modulation — equivalent to FM for sinusoidal modulators, and the form the analysis actually solves (as [Lazzarini et al. note](https://export.arxiv.org/pdf/2305.07909v1.pdf)).

**The spectrum, via Jacobi–Anger.** The Jacobi–Anger expansion ([Wikipedia: Jacobi–Anger expansion](https://en.wikipedia.org/wiki/Jacobi-Anger_expansion)) states

$$e^{\,i z \sin\theta} = \sum_{n=-\infty}^{\infty} J_n(z)\, e^{\,i n\theta},$$

where $J_n$ is the Bessel function of the first kind, order $n$. Apply with $z = I$, $\theta = \omega_m t$:

$$y(t) = A\,\mathrm{Im}\!\left[e^{i\omega_c t} \sum_{n=-\infty}^{\infty} J_n(I)\, e^{i n \omega_m t}\right] = A \sum_{n=-\infty}^{\infty} J_n(I)\, \sin\big((\omega_c + n\omega_m) t\big),$$

using $J_{-n}(I) = (-1)^n J_n(I)$ (which is why, in Chowning's Eq. 2, the odd-order *lower* sidebands carry negative signs — phase inversion, inaudible in magnitude but essential when sidebands overlap). Collecting terms:

- **Carrier** at $f_c$ with amplitude $J_0(I)$;
- **Sideband pairs** at $f_c \pm n f_m$ with amplitudes $J_n(I)$, $n = 1, 2, \ldots$

Sidebands with $n > f_c/f_m$ land at negative frequency and *reflect* around 0 Hz (with sign flips) — Chowning's "reflected side frequencies," the source of the technique's richest inharmonic spectra. Energy is conserved: $\sum_{n=-\infty}^{\infty} J_n^2(I) = 1$ (Parseval for Bessel series), so raising $I$ does not make the sound louder, it *redistributes* energy outward into more sidebands — brightness as a single scalar knob. The effective bandwidth follows Carson's rule: $B \approx 2(\Delta f + f_m) = 2 f_m (I + 1)$.

**The famous ratio rule.** If $f_m : f_c$ is a ratio of small integers, all sidebands $f_c \pm n f_m$ lie on a common harmonic grid — the sound is *tonal*. If the ratio is irrational-ish (e.g. 1 : 1.4, 1 : √2), the partials have no common fundamental — the sound is *inharmonic, metallic, bell-like*. Chowning's own bell recipe from the paper: $f_c = 200$ Hz, $f_m = 280$ Hz (ratio 1 : 1.4), index swept from $I \approx 10$ down to 0 *proportional to the amplitude envelope* — dense clangorous spectrum at the strike, pure sine at the decay. This one-envelope-drives-both-loudness-and-brightness coupling is why FM reproduces the spectral evolution of struck and plucked things with two oscillators, and why the 1983 Yamaha DX7 (6 operators of exactly this) defined a decade of sound.

### 2.6.4 Vibrato and Doppler-style glides

**Vibrato** is FM with tiny index: $y = \sin(\omega_c t + I\sin(\omega_m t))$ with $I \lesssim 0.5$ and $f_m \sim 5\!-\!7$ Hz — the sidebands ($J_1(I)$-weighted, close to the carrier) are not resolved perceptually; the result is heard as pitch modulation of depth $\approx I \cdot f_m$ Hz. Six-Hz vibrato is near the transition between pitch-wobble and roughness — the same beat/roughness physics as §2.1.5. **A passing source** produces a Doppler glide: $f_{\text{obs}} = f_{\text{src}}\, \dfrac{c}{c \pm v_{\text{radial}}}$ (approaching: denominator $c - v$; receding: $c + v$). A source passing at closest distance $d$ with speed $v$ has radial velocity $v_r(t) = -v^2 t / \sqrt{v^2 t^2 + d^2}$, giving a smooth sigmoid pitch glide centered at closest approach — the "neeee-yooooow" — whose steepness scales with $v/d$. For a game engine: pitch-shifting a looping source by this closed-form $c/(c - v_r(t))$ is the cheap exact model; convolving with a varying delay $\big(\Delta t = v_r t / c$, the physical mechanism$\big)$ additionally produces the correct comb-flutter for free.

---

## 2.7 Additive and Subtractive Synthesis

### 2.7.1 Fourier synthesis and the harmonic series

Additive synthesis is the constructive direction of §2.2.1: build $f(t) = \sum_k A_k \sin(k \omega_0 t + \varphi_k)$. **Timbre = spectrum + envelope**: which harmonics, at what amplitudes, evolving how in time. The three classic geometric waveforms have exactly known series (verified against [MathWorld](https://mathworld.wolfram.com/SawtoothWave.html), [ProofWiki](https://proofwiki.org/wiki/Fourier_Series_for_Sawtooth_Wave), and the [UAM simple Fourier series page](http://matematicas.uam.es/~fernando.chamizo/dark/d_sim_fou.html)):

**Sawtooth** (peak amplitude $A$, all harmonics, $1/n$):

$$x_{\text{saw}}(t) = \frac{2A}{\pi} \sum_{n=1}^{\infty} \frac{(-1)^{n+1}}{n} \sin(2\pi n f t).$$

*Derivation sketch:* the sawtooth on $(-\tfrac{T}{2}, \tfrac{T}{2})$ is the identity ramp $2A\,t/T$; by odd symmetry only sine terms survive; $b_n = \frac{2}{T}\int_{-T/2}^{T/2} \frac{2At}{T} \sin(n\omega_0 t) dt$ integrates by parts to $\frac{2A}{\pi} \frac{(-1)^{n+1}}{n}$ — the $\pm 1$ alternation comes from $\cos(n\pi)$ evaluated at the endpoints.

**Square** (odd harmonics only, $1/n$):

$$x_{\text{sq}}(t) = \frac{4A}{\pi} \sum_{\substack{n=1,3,5,\ldots}}^{\infty} \frac{\sin(2\pi n f t)}{n}.$$

*Sketch:* odd square wave is the sign of a sine; only odd harmonics survive by the half-wave symmetry $f(t + T/2) = -f(t)$ (which kills every even coefficient of *any* waveform possessing it), and the same integration by parts gives $b_n = 4A/(\pi n)$ for odd $n$.

**Triangle** (odd harmonics, $1/n^2$, alternating sign):

$$x_{\text{tri}}(t) = \frac{8A}{\pi^2} \sum_{\substack{n=1,3,5,\ldots}}^{\infty} \frac{(-1)^{(n-1)/2}}{n^2} \sin(2\pi n f t).$$

*Sketch:* the triangle is the integral of the square wave; integrating multiplies each harmonic's coefficient by $1/(n\omega_0)$, turning $1/n$ into $1/n^2$ (and introducing the sign alternation through the integration constant choice). This is why triangle waves are *smooth* — coefficients falling as $1/n^2$ mean the spectrum rolls off at −12 dB/oct in amplitude, versus the saw/square's −6 dB/oct — and why they sound mellow where saws sound buzzy.

Worked amplitude check: sawtooth fundamental = $2A/\pi \approx 0.637 A$; square fundamental = $4A/\pi \approx 1.27 A$ (of the half-peak $A$); triangle fundamental = $8A/\pi^2 \approx 0.81 A$. The $1/n$ vs $1/n^2$ fall-off is also the single most useful fact in **aliasing-aware oscillator design**: a naive sampled sawtooth aliases badly because its harmonic amplitudes fall too slowly; band-limited synthesis (additive up to Nyquist, or BLIT/BLEP) must respect exactly this spectral envelope.

### 2.7.2 Subtractive synthesis and formants

Subtractive synthesis is the subtractive direction: start with a harmonically rich source (sawtooth, pulse train, noise) and *filter* it. This was the analog model — a sawtooth oscillator into a voltage-controlled filter — because generating a rich spectrum and sculpting it was vastly cheaper in analog parts than generating 30 sine oscillators. The source-filter decomposition is also, literally, the physics of the voice.

**Formants: the voice as filter.** Source–filter theory (Fant, *Acoustic Theory of Speech Production*, 1960): the glottal folds produce a harmonic-rich buzz at $f_0$ (adult male mean ≈ 130 Hz, female ≈ 220 Hz); the vocal tract above is a tube (~17.5 cm for an adult male) whose shape determines its resonance frequencies. For the neutral tract — a uniform tube closed at the glottis, open at the lips — the resonances are the quarter-wave modes:

$$F_k = \frac{(2k-1)\,c}{4L} \;\overset{L = 17.5\,\text{cm},\, c = 343\,\text{m/s}}{\longrightarrow}\; F_1 = 500\ \text{Hz},\; F_2 = 1500\ \text{Hz},\; F_3 = 2500\ \text{Hz}$$

([USC source-filter course notes](https://sail.usc.edu/~lgoldste/General_Phonetics/Source_Filter/SFc.html), [Manitoba phonetics notes](https://home.cc.umanitoba.ca/~krussll/phonetics/acoustic/formants.html)). Moving the tongue and jaw reshapes the tube and shifts the resonances — the **formants** $F_1, F_2, \ldots$. The vowel identity is carried by the formant *positions*, not by $f_0$: the same vowel can be sung on any pitch, and different vowels on the same pitch. Representative $F_1/F_2$ for an adult male (from the [Manitoba tables](https://home.cc.umanitoba.ca/~krussll/phonetics/acoustic/formants.html); canonical measurements Peterson & Barney 1952, [Hillenbrand et al. 1995]):

| Vowel | F1 (Hz) | F2 (Hz) |
|---|---|---|
| /i/ ("ee") | ~280 | ~2230 |
| /æ/ ("a" in "had") | ~860 | ~1550 |
| /ɑ/ ("ah") | ~830 | ~1170 |
| /u/ ("oo") | ~330 | ~1260 |

**"Ahh" vs "eee" on the same buzz = two different filters.** $F_1$ tracks vowel *height* (inverse: high tongue → low $F_1$); $F_2$ tracks *frontness*. This is the entire acoustic basis of vowel synthesis and of creature-voice design in games: a glottal buzz (sawtooth or pulse train at $f_0$) through 2–3 formant bandpass biquads (§2.5.3, $Q \sim 8\!-\!12$) at the $F_1$/$F_2$ of your choice, with a noise source mixed in for consonants, is a vowel — and morphing the biquad centers between vowel targets is intelligible speech-like vocalization without a single recorded sample.

---

## 2.8 Physical Modeling Synthesis

### 2.8.1 Karplus–Strong (1983)

Kevin Karplus & Alex Strong, ["Digital Synthesis of Plucked-String and Drum Timbres,"](https://doi.org/10.2307/3680062) *Computer Music Journal* **7**(2):43–55 (Summer 1983) ([PDF at Karplus's site](https://users.soe.ucsc.edu/~karplus/papers/digitar.pdf)); extensions in David A. Jaffe & Julius O. Smith, ["Extensions of the Karplus-Strong Plucked-String Algorithm,"](http://musicweb.ucsd.edu/~trsmyth/papers/KSExtensions.pdf) *CMJ* **7**(2):56–69 — the **same issue**. The algorithm is one line:

$$\boxed{\;y[n] = \tfrac{1}{2}\big(y[n-N] + y[n-N-1]\big) \qquad \text{(initial } y[0..N-1] = \text{noise burst)}\;}$$

A delay line of length $N$ is filled with a short burst of random numbers (the "pluck") and then read back while each circulating sample is replaced by the **average of itself and its predecessor** — a two-point lowpass, $H(z) = \tfrac12(1 + z^{-1})$, in the feedback loop of the delay. (Karplus–Strong's own presentation: $y[n] = x[n] + \tfrac12(y[n-N] + y[n-N-1])$, with $x$ the excitation; setting $x = 0$ after the burst gives the free vibration above.)

**Why it is a plucked string:**

- **Pitch:** the loop length is $N$ (or $N + \tfrac12$ — the loop filter's half-sample average), so the fundamental is $f \approx f_s / N$ (e.g. 44.1 kHz, $N = 100$ → 441 Hz, ≈ concert A).
- **Decay:** each pass around the loop multiplies a partial at $\omega$ by $|H(e^{i\omega})| = \big|\cos(\omega/2)\big|$. Low partials pass nearly unattenuated ($|H| \approx 1 - \omega^2/8$); partials near Nyquist are crushed ($|H| \to 0$). So **high harmonics die first — exactly a plucked string's brightness decay** — and the naturalness comes from *frequency-dependent damping*, which no static wavetable has. Per-second decay of harmonic $k$: $|H|^{f/N \cdot \ldots}$ — concretely, amplitude after 1 s $= |H(e^{i\omega_k})|^{f_s/N}$ where $\omega_k = 2\pi k f/f_s$.
- **Excitation:** a noise burst is a maximally-dumb initial condition; the loop filter does all the timbral work — which is why *any* initial table sounds string-like after a few loop passes.

**Jaffe–Smith extensions** (tuning, brightness, pick position): an allpass filter in the loop to fine-tune the phase (fractional delay) for correct tuning; a one-pole lowpass whose cutoff is raised/lowered for brightness control; a **comb filter applied to the excitation** to model pick *position* (plucking at $1/P$ of the string length kills harmonics of $P$ — $x_{\text{exc}}[n] - x_{\text{exc}}[n - N/P]$); "decay stretching" (blend $y[n-N]$ and $y[n-N-1]$ with weight $S$) so short strings don't die instantly. Historical note from [Smith's own account](https://ccrma.stanford.edu/~jos/smith-nam/Karplus_Strong_Algorithms.html): Strong invented the two-point-average trick to make wavetable synthesis less boring on an 8-bit micro; the string-likeness was a happy accident; Smith recognized the filtered delay loop as the transfer function of an idealized string and derived the whole family as a special case of digital waveguides.

### 2.8.2 Digital waveguide synthesis (Julius O. Smith III)

The wave equation for the ideal string, $\partial^2 y/\partial x^2 = (1/c^2)\,\partial^2 y/\partial t^2$ with $c = \sqrt{T/\mu}$ (tension over linear density), has as its general solution the **d'Alembert decomposition** (published 1747):

$$y(x, t) = y^{+}(x - ct) + y^{-}(x + ct)$$

— the string's state is exactly two arbitrary traveling waves, one moving right, one moving left, at speed $c$ ([Smith, *Physical Audio Signal Processing*](https://ccrma.stanford.edu/~jos/pasp/), verified live 2026-09-06; the [derivation from eigenfunctions/Fourier](https://ccrma.stanford.edu/~jos/smithbook/D_Alembert_Derived.html) is in the appendices). **Sample** the traveling waves at $x_m = mX$, $t_n = nT$ with the *magic choice* $X = cT$:

$$y(t_n, x_m) = y^{+}\big((n-m)T\big) + y^{-}\big((n+m)T\big) = y^{+}[n-m] + y^{-}[n+m].$$

A spatial shift is exactly a time shift — so **each traveling wave is a digital delay line**, and the pair of delay lines *is an exact solution of the 1-D wave equation* (exact, not approximated, for bandlimited content below Nyquist). The string displacement at any point is the sum of the two rails at that position; driving and observing are likewise point operations. Losses and dispersion in a real string are also LTI, so by commutativity they can be **lumped** into a single loop filter at one point — the waveguide string is two delay lines and one small filter, plus reflection/scattering junctions at the boundaries (wave impedance $R = \sqrt{T\mu}$ governs junction arithmetic). This is why waveguide synthesis is orders of magnitude cheaper than grid methods: the propagation itself costs *nothing but memory*. The full treatment — strings, tubes, scattering junctions, wave digital filters, the equivalence to Karplus–Strong — is Smith's free online book [*Physical Audio Signal Processing*](https://ccrma.stanford.edu/~jos/pasp/) (with [*Introduction to Digital Filters*](https://ccrma.stanford.edu/~jos/filters/) and [*Spectral Audio Signal Processing*](https://dsprelated.com/freebooks/sasp/) alongside; all live at [ccrma.stanford.edu/~jos/](https://ccrma.stanford.edu/~jos/)).

### 2.8.3 Modal synthesis

The complement of waveguides: expand the object's vibration in its *eigenmodes* (each a damped second-order resonator — §2.5.3 with $\omega_0$ = mode frequency, $Q$ = mode damping) and drive the bank with an excitation signal. For a stiff object (bars, plates, shells) the modes are measured (tap-and-FFT) or derived from the material's elasticity equations — which ties directly to the engine's materials section: given Young's modulus, density, and geometry, the modal frequencies and $Q$s of e.g. a rectangular bar follow from the Euler–Bernoulli beam equation, and the modal bank *is* the audible signature of the material. Modal synthesis is the natural choice when the object is struck (free vibration, no sustained nonlinearity needed) and when the mode data is available or measurable; each mode is one biquad, so a 20-mode object costs 20 biquads — trivially cheap, embarrassingly parallel, trivially tuneable per-material.

### 2.8.4 Banded waveguides, FDTD, and the engineering moral

**Banded waveguides** (Essl & Cook, ICMC 1999; [theory paper: Essl, Serafin, Cook & Smith, *CMJ* 28(1):37–50, 2004](https://doi.org/10.1162/014892604322970634)): split the spectrum into bands, each containing primarily one mode, and give *each band its own waveguide loop* whose delay matches the mode's round-trip time. This hybrid of modal and waveguide synthesis preserves *spatial sampling* (the ability to excite/observe/listen at a physical point, needed for nonlinear bowing interactions) while keeping cost bounded — designed for bar percussion (marimba, bowed bars, Tibetan bowls). **Finite-difference time-domain (FDTD)** methods (Hiller & Ruiz 1971, *JAES* 19(6):462-470 — ["Synthesizing Musical Sounds by Solving the Wave Equation for Vibrating Objects"](https://soundlab.cs.princeton.edu/publications/1999_icmc_bar.pdf) is the modern entry point) discretize the PDE directly on a grid: general, physical, handles nonlinearities and arbitrary geometry — and costs $O(\text{cells} \times \text{timesteps})$, which for a 3-D object at audio rates is far beyond real time for game use. **The practical insight for a game engine:** procedural audio wants the *cheapest model that passes perceptual muster*. The hierarchy is: wavetable (cheapest, static) → FM (2 oscillators, dynamic spectra) → Karplus–Strong (1 delay line + 1 add/shift, physically-motivated decay) → digital waveguide (2 delay lines + small filter, exact linear physics) → modal bank (N biquads, measured physics) → banded waveguide (spatial + cheap) → FDTD (exact, unaffordable). Choose per sound: a sword *ting* is 5 modes; a plucked lute string is a waveguide; wind through grass is filtered noise; and nothing in a shipping game needs FDTD.

---

## 2.9 Perceptual Scales for Audio Code

### 2.9.1 The mel scale

The mel scale (Stevens & Volkmann, ["The Relation of Pitch to Frequency: A Revised Scale,"](https://en.wikipedia.org/wiki/Mel_scale) *Am. J. Psychol.* 53(3):329–353, 1940) is a perceptual pitch scale with the anchor **1000 Hz = 1000 mel** (1 kHz tone, 40 dB SL). The standard modern formula (attributed to O'Shaughnessy 1987, with the 700 Hz breakpoint introduced by Makhoul & Cosell 1976; [origin traced on the AUDITORY list](http://www.auditory.org/mhonarc/2008/msg00191.html)):

$$m = 2595\,\log_{10}\!\left(1 + \frac{f}{700}\right) = 1127\,\ln\!\left(1 + \frac{f}{700}\right).$$

**The two forms are the same, shown:** $2595/\ln(10) = 2595/2.302585 = 1126.96 \approx 1127$ (the sometimes-seen 1127.01048 is the value that makes the 1 kHz anchor exact to more decimals than the data deserves). **Anchor check:** $m(1000\,\text{Hz}) = 2595\,\log_{10}(1 + 10/7) = 2595 \times \log_{10}(2.42857) = 2595 \times 0.38539 = \mathbf{1000.1}$ ✓. The scale is roughly linear below ~700 Hz and logarithmic above — one bend, one parameter. Use in code: mel-spaced filterbanks for audio analysis/ML features and for any UI that needs "perceptually uniform" frequency sliders.

### 2.9.2 Bark and ERB (for completeness)

The **Bark** scale (Zwicker 1961; 24 critical bands over the audible range) — two standard conversions ([Traunmüller 1990, *JASA* 88:97-100](https://resources.ling.su.se/hartmut/bark.htm)):

$$z = 13\arctan(0.00076\,f) + 3.5\arctan\!\big((f/7500)^2\big) \quad \text{(Zwicker–Terhardt 1980)}, \qquad z = \frac{26.81\,f}{1960 + f} - 0.53 \quad \text{(Traunmüller)}.$$

The **ERB** (equivalent rectangular bandwidth) scale — the modern auditory-filter measurement ([Glasberg & Moore 1990, *Hearing Research* 47:103–138](https://en.wikipedia.org/wiki/Equivalent_rectangular_bandwidth)):

$$\mathrm{ERB}(f) = 24.7\,(4.37\,f_{\text{kHz}} + 1)\ \text{Hz}, \qquad \mathrm{ERB}\text{-rate}(f) = 21.4\,\log_{10}(4.37\,f_{\text{kHz}} + 1)\ \text{ERBs}.$$

ERB is narrower than the classical critical band at all frequencies (they agree above ~500 Hz); Bark measures tonotopic position, ERB measures frequency resolution. For a game engine these matter wherever "can the ear separate these two components?" is the question (roughness, masking, spectral-spreading decisions).

### 2.9.3 Loudness units in code — dBFS, LUFS, LU

**dBFS** (dB full scale): the digital-peak/rms scale of a sample stream — $20\log_{10}(|x|/x_{\text{max}})$; 0 dBFS is the largest representable amplitude, everything else negative. It is a *level* measure with no perceptual weighting: a 0 dBFS 1 kHz sine and a 0 dBFS 20 Hz sine read identically and sound wildly different.

**LUFS / LKFS** (loudness units, full scale): the ITU broadcast loudness measure. [ITU-R BS.1770](https://www.itu.int/rec/R-REC-BS.1770) (current revision [BS.1770-5, 11/2023](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf)) defines the algorithm: (1) **K-weighting** — a shelving pre-filter modeling head acoustics cascaded with a RLB high-pass; (2) mean-square per channel; (3) channel-weighted power summation (surround channels +1.5 dB, LFE excluded); (4) gating over 400 ms blocks (75% overlap) with an absolute −70 LUFS threshold and a relative −10 LU threshold. The result:

$$L_K = -0.691 + 10\log_{10}\sum_i G_i\, z_i \quad \text{LUFS},$$

calibrated so a 0 dBFS 997 Hz sine reads **−3.01 LUFS** (the $1/\sqrt2$ RMS of §2.1.4, minus the 0.691 correction). LUFS and LKFS are the same unit under different naming conventions ([EBU R128](https://tech.ebu.ch/files/live/sites/tech/files/shared/r/r128v5_0.pdf) uses LUFS; broadcast target −23 LUFS ±1 LU; streaming platforms cluster around −14 to −16 LUFS integrated).

**LU** (loudness unit): the *relative* unit — 1 LU ≡ 1 dB of difference on the LUFS scale. Meters display "0 LU" at the target loudness; a program 3 LU below target reads −3 LU. In engine terms: use dBFS for clipping/headroom safety (never normalize in dBFS), use LUFS for "how loud does the game actually sound," and use LU for expressing the difference.

---

## 2.10 Reverberation Mathematics

### 2.10.1 Schroeder's 1962 architecture

Manfred R. Schroeder, ["Natural Sounding Artificial Reverberation,"](http://ece.rochester.edu/~zduan/teaching/ece472/reading/Schroeder_1962.pdf) *J. Audio Eng. Soc.* **10**(3):219–223 (July 1962) (with the companion ["Colorless Artificial Reverberation,"](http://languagelog.ldc.upenn.edu/myl/Logan1961.pdf) IRE Trans. Audio, 1961, for the allpass derivation). Schroeder's diagnosis: delay-based reverberators of the era failed two ways — colored frequency response (comb resonances) and too-low echo density (audible flutter). His architecture, still the skeleton inside most algorithmic reverbs:

**Comb filter** — a delay with feedback:

$$\boxed{\;H_{\text{comb}}(z) = \frac{z^{-D}}{1 - g\, z^{-D}}\;} \qquad y[n] = x[n-D] + g\, y[n-D].$$

Impulse response: exponentially decaying pulse train spaced $D$ samples; frequency response: $|H| = 1/|1 - g e^{-i\omega D}|$ — peaks of $1/(1-g)$ at multiples of $f_s/D$, nulls of $1/(1+g)$ between. The **decay time** to −60 dB: each round trip multiplies by $g$, so $g^{\,t f_s / D} = 10^{-3}$, giving

$$T_{60} = \frac{D}{f_s}\cdot\frac{\ln 1000}{-\ln g} \quad\Longleftrightarrow\quad g = 10^{-3 D / (f_s\, T_{60})}.$$

Worked: $D/f_s = 40$ ms, $g = 0.85$ → $T_{60} = 0.040 \times 6.9078 / 0.1625 \approx 1.7$ s.

**Allpass filter** — the comb plus a judiciously proportioned direct path:

$$\boxed{\;H_{\text{ap}}(z) = \frac{z^{-D} - g}{1 - g\, z^{-D}}\;} \qquad y[n] = -g\,x[n] + x[n-D] + g\, y[n-D].$$

$|H(e^{i\omega})| = 1$ for **all** $\omega$ (numerator and denominator are conjugate reciprocals on the unit circle — Schroeder's Eq. 13–14: $H(\omega) = e^{-i\omega T}\frac{1 - g e^{i\omega}}{1 - g e^{-i\omega}}$, each factor unit magnitude) — "colorless": same resonances as the comb in *decay*, invisible in *magnitude*. Cascades of allpasses multiply echo density without coloring.

**The complete Schroeder reverb:** 4 parallel combs (incommensurate delays ~30–45 ms, gains set per the $T_{60}$ formula) into 2 series allpasses (~5 ms and ~1.7 ms, $g \approx 0.7$). Schroeder's requirements: **echo density ≥ ~1000 echoes/second** for flutter-free tails (a single 40 ms comb gives 25/s; four in parallel give 100/s; each allpass roughly triples), and **mode spacing fine enough that multiple modes fall within a critical band** (§2.9.2) so the magnitude response is statistically smooth — the perceptual criterion behind "incommensurate delays."

### 2.10.2 Feedback delay networks (FDN)

The generalization: $N$ delay lines in parallel, whose outputs are mixed back to their inputs by an $N \times N$ **feedback matrix** $\mathbf{A}$:

$$\mathbf{s}[n] = \mathbf{A}\, \mathbf{s}[n - \mathbf{m}] + \mathbf{B}\mathbf{x}[n], \qquad \mathbf{y}[n] = \mathbf{C}\mathbf{s}[n] + \mathbf{D}\mathbf{x}[n].$$

History, verified: **Gerzon** proposed the "orthogonal matrix feedback reverberation unit" ([*Electronics Letters*, 1976](https://doi.org/10.1049/el:19760215), "Unitary (energy-preserving) multichannel networks with feedback") — noting that cross-coupled combs beat independent ones; **Stautner & Puckette** (1981 ICMC; [*CMJ* 6(1), 1982, "Designing multi-channel reverberators"](https://www.ee.columbia.edu/~dpwe/e4896/papers/StautP82-reverb.pdf)) gave the first concrete 4-channel FDN with stability conditions and the feedback matrix

$$\mathbf{A} = \frac{g}{\sqrt2}\begin{bmatrix} 0 & 1 & 1 & 0 \\ -1 & 0 & 0 & -1 \\ 1 & 0 & 0 & -1 \\ 0 & 1 & -1 & 0 \end{bmatrix}$$

(a signed permutation of a Hadamard matrix — [Smith's FDN history](https://ccrma.stanford.edu/~jos/pasp/History_FDNs_Artificial_Reverberation.html)); **Jot & Chaigne** (["Digital Delay Networks for Designing Artificial Reverberators,"](https://aes.org/e-lib/browse.cfm?elib=5663) AES Convention 90, Paris, paper 3030, Feb. 1991) turned it into a design methodology: a **unitary** (lossless) feedback matrix makes the FDN's modes decay *equally* — no ringing resonances — and then per-frequency reverberation control is achieved by inserting a small attenuation filter $\boldsymbol{\Lambda}(z)$ (e.g. a one-pole lowpass per line, or a common "damping" filter) inside the loop, with a companion **tonal correction filter** on the output equalizing the coloration the damping introduces. The design decouples: delays → mode density; feedback matrix → losslessness/quality; damping filters → frequency-dependent $T_{60}$; correction filter → flat response. This is the architecture of essentially every modern algorithmic reverb.

### 2.10.3 Convolution reverb

The exact linear model of a room is the convolution theorem itself: the room *is* an LTI system with impulse response $h[n]$ (measured — an exponential sine sweep or balloon pop — or synthesized), and the reverberated signal is $y = x * h$. Direct convolution of a signal with an $M$-sample IR costs $O(N M)$ per $N$ output samples — a 3-second IR at 48 kHz ($M = 144{,}000$) costs 144k multiply-accumulates *per sample*, ~7 billion/sec: unshippable. The FFT makes it $O(N \log N)$ via **partitioned (block) convolution**: split the IR into blocks, use overlap-save/overlap-add block FFT convolution per block, and sum the delayed partial results. The catch is latency: a block of length $B$ must accumulate before its FFT can run, so naive uniform partitioning adds $B$ samples of input-output delay. **Gardner's classic solution** (William G. Gardner, ["Efficient Convolution without Input/Output Delay,"](https://aes.org/e-lib/browse.cfm?elib=7957) *JAES* 43(3):127–136, 1995): a **non-uniform partition** — direct (time-domain) convolution for the head of the IR to produce output *immediately*, while the first FFT block's worth of input accumulates, then geometrically growing FFT blocks (each at least twice as long as its start offset into the filter) so that every block's computation is hidden under the accumulation of its own input, with a three-priority scheduler evening the CPU load. Later work (García, ["Optimal Filter Partition for Efficient Convolution with Short Input/Output Delay,"](https://secure.aes.org/forum/pubs/conventions/?elib=11275) AES 113th Conv., 2002) formalized the optimal partition choice; GPU implementations extend the same structure. For the engine: convolution is the reference-grade path (measured spaces, exact coloration); FDN/Schroeder-style algorithmic reverb is the interactive-grade path (parameter morphing — room size, wetness — with zero IR-switching artifacts); both are required, for different sounds.

---

## 2.11 Consolidated numeric sanity checks

| Check | Value | Status |
|---|---|---|
| RMS of sine | $A/\sqrt2 = 0.7071A$ | derived §2.1.4 |
| Gibbs overshoot | $0.08949 \times$ jump ≈ 8.9% | [Wikipedia/MIT 18.03](https://en.wikipedia.org/wiki/Gibbs_phenomenon) ✓ |
| Window sidelobes | rect −13.3, Hann −31.5, Hamming −42.7/−43, Blackman −58 dB | [VRU table](https://vru.vibrationresearch.com/lesson/table-of-window-function-details/) ✓ |
| 20 ms window vs 25 Hz tones | $1/T = 50$ Hz > 25 Hz → unresolved; need ≥ 40 ms (rect) / ~160 ms (Hann mainlobe) | derived §2.2.2 |
| 44,100 | $245 \times 60 \times 3 = 294 \times 50 \times 3 = 2^2 3^2 5^2 7^2$ | [Wikipedia](https://en.wikipedia.org/wiki/44,100_Hz), [metanumbers](https://metanumbers.com/44100) ✓ |
| Quantization SNR, 16-bit | $6.02 \times 16 + 1.76 = \mathbf{98.08}$ dB ("≈96 dB" = the $6.02N$ term alone) | derived §2.3.2, [AD MT-001](https://www.analog.com/media/en/training-seminars/tutorials/MT-001.pdf) ✓ |
| Quantization SNR, 24-bit | $6.02 \times 24 + 1.76 = 146.24$ dB | same |
| Pink slope | −3.0103 dB/oct = −10 dB/decade | derived §2.4.2 ✓ |
| Mel constants | $2595/\ln 10 = 1127.0$; $m(1000\,\text{Hz}) = 1000.1$ | §2.9.1 ✓ |
| $t_{60}$ | $\ln 1000 = 6.9078$; $t_{60} = 6.9078\,\tau$ | §2.5.4, §2.6.1 |
| KS pitch | $f_s/N$ (e.g. 44100/100 = 441 Hz) | §2.8.1 |
| Schroeder comb gain | $g = 10^{-3D/(f_s T_{60})}$ | §2.10.1 |
| BS.1770 calibration | 0 dBFS 997 Hz sine → −3.01 LUFS | [BS.1770-5](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf) ✓ |

**Primary sources verified live 2026-09-06:** Julius O. Smith's CCRMA books ([index](https://ccrma.stanford.edu/~jos/), [PASP](https://ccrma.stanford.edu/~jos/pasp/), [filters](https://ccrma.stanford.edu/~jos/filters/), [resample](https://ccrma.stanford.edu/~jos/resample/resample.pdf)); [RBJ Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html); [Chowning 1973 PDF](https://ccrma.stanford.edu/sites/default/files/user/jc/fm_synthesis_paper.pdf); [Karplus–Strong 1983 PDF](https://users.soe.ucsc.edu/~karplus/papers/digitar.pdf) + [Jaffe–Smith PDF](http://musicweb.ucsd.edu/~trsmyth/papers/KSExtensions.pdf); [Schroeder 1962 PDF](http://ece.rochester.edu/~zduan/teaching/ece472/reading/Schroeder_1962.pdf); [Jot & Chaigne 1991](https://aes.org/e-lib/browse.cfm?elib=5663); [Voss & Clarke 1975](https://doi.org/10.1038/258317a0) (Nature 258:317–318 — **not PNAS**); [BTW 1987](https://link.aps.org/doi/10.1103/PhysRevLett.59.381); [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770); [Gardner 1995](https://aes.org/e-lib/browse.cfm?elib=7957); [Stautner–Puckette 1982](https://www.ee.columbia.edu/~dpwe/e4896/papers/StautP82-reverb.pdf); [Gerzon 1976](https://doi.org/10.1049/el:19760215); [Whittle's pink-noise compendium](https://www.firstpr.com.au/dsp/pink-noise/) (Kellet coefficients); [Lipshitz–Wannamaker–Vanderkooy 1992](https://secure.aes.org/forum/pubs/journal/?elib=7047); [Essl et al. banded waveguides](https://doi.org/10.1162/014892604322970634); [Traunmüller 1990](https://resources.ling.su.se/hartmut/bark.htm).

---

# Part 3 — The Physics of Natural Sound Generation: Mechanisms, Math, and Spectra


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

---

# Part 4 — How the Human Ear Works and How Humans Perceive Sound


## 0. Provenance

- **Date:** 2026-09-06. **Tools:** Exa web search/fetch (12 search calls, ~40 fetched sources; every numeric claim below traced to a fetched URL). **Verification:** all boxed equations and key constants were re-derived or cross-checked against primary sources (Greenwood 1990 JASA; ISO 226:2003 Table B.1; Glasberg & Moore 1990; Zwicker & Terhardt 1980; Hartmann & Aaronson 2013 JASA on Woodworth; NIOSH 98-126; OSHA 1910.95; Shaw 1974/1980 measured ear gain; Plomp & Levelt 1965; Eguíluz et al. 2000 PRL; Dallos 2008; Liberman et al. 2002).
- **Summary:** A signal enters a resonant funnel (pinna + canal, peak gain ~15–20 dB near 2.6–3 kHz), crosses an impedance-matching transformer (middle ear, ~25–34 dB pressure gain) into a hydraulic traveling-wave spectrum analyzer (cochlea) whose active, Hopf-like outer hair cells sharpen and amplify the wave 40–60 dB, and is transduced by ~3,500 inner hair cells into ~30,000 phase-locking nerve fibers whose tonotopy persists to auditory cortex. Perception then layers on: a compressive loudness scale (~$I^{0.3}$), ~24 critical-band filters, masking (the basis of both MP3 and game mixing), and binaural/spatial decoding (ITD < 1.5 kHz, ILD above, pinna spectra, precedence). This document derives the mechanisms with the actual numbers, and flags every known disagreement between sources.

---

## 1. The Peripheral Auditory System, Mechanically

### 1.1 Pinna and ear canal: a funnel plus a quarter-wave resonator

The pinna (auricle) is a cartilaginous horn ~6–7 cm across. Below ~1 kHz its dimensions are acoustically invisible (wavelength ≫ pinna), so it contributes nothing; above ~2 kHz it acts as a reflector/dish concentrating energy toward the canal, and above ~5 kHz its folds, concha cavity, and helix introduce the direction-dependent spectral peaks and notches that later resolve elevation and front/back (§7). The concha alone acts as a Helmholtz-like resonator peaking near 4–6 kHz ([Hiipakka thesis, Helsinki UT](http://lib.tkk.fi/Dipl/2008/urn012834.pdf); [Sinyor & Shaw, McGill](https://escholarship.mcgill.ca/concern/theses/sq87bv946)).

The ear canal (external auditory meatus) is a tube ~2.3–2.6 cm long and ~7 mm in diameter, open at the concha and terminated by the tympanic membrane — i.e., a **quarter-wave resonator**: pressure node at the open end, pressure antinode at the closed eardrum. For a tube of effective length $L$ closed at one end, resonances occur when the tube holds an odd number of quarter wavelengths:

$$\lambda_n = \frac{4L}{2n-1}, \qquad f_n = \frac{(2n-1)\,c}{4L},\quad n=1,2,3,\dots$$

**Worked example (first resonance).** With $L = 2.5$ cm and $c = 343$ m/s:
$$f_1 = \frac{343}{4\times0.025} = 3430\ \text{Hz}.$$
For the anatomically more typical $L = 2.3$ cm: $f_1 = 3728$ Hz. The measured first resonance, however, sits at **2.6–3.3 kHz**, lower than the naive $c/4L$, because the compliant eardrum and the finite radiation impedance at the open end effectively lengthen the resonator (open-end correction plus drum compliance), and because the canal is curved and non-uniform — Hiipakka's simulator puts the first two quarter-wave peaks at 3 and 9 kHz for a 25-mm canal ([Hiipakka 2004, JAES](https://tikander.net/miikka/Science/Publications_files/hiipakka04_JAES.pdf); [Hiipakka thesis](http://lib.tkk.fi/Dipl/2008/urn012834.pdf)). Shaw's classic synthesis of ~100 subjects across 12 studies found the **primary external-ear resonance at ≈2.6 kHz, with free-field-to-eardrum pressure gain of ≈17 dB at 0° azimuth and ≈21 dB at 60°**, and a diffuse-field gain of ~18 dB at 2.7 kHz ([Shaw 1980, JCAA](https://jcaa.caa-aca.ca/index.php/jcaa/article/view/441); [Shaw 1976, JASA 60, 1025](https://doi.org/10.1121/1.2003044); [Shaw 1974, JASA](https://doi.org/10.1121/1.1903522)). So the commonly quoted "~10–15 dB gain in the 2–5 kHz band" is conservative; the true peak gain including pinna + head diffraction is ~15–20 dB, tapering through a concha-sustained shoulder for nearly an octave above the peak. This resonance is precisely why human hearing is most sensitive at 2–5 kHz (§4) and why the 3–6 kHz region is where noise-induced hearing loss strikes first.

### 1.2 Middle ear: a pressure transformer bridging air and water

Sound in air has characteristic impedance $Z_{\text{air}} = \rho c \approx 415\ \text{Pa·s/m}$; cochlear fluid is essentially water, $Z_{\text{water}} \approx 1.5\times10^6\ \text{Pa·s/m}$ — a mismatch of ~3,600:1 (experimental human cochlear input specific impedance: 56 kPa·s/m, [Mason 2016, J. Anat. 228, 331](https://pmc.ncbi.nlm.nih.gov/articles/PMC4718164/)). At an interface with impedance ratio $r = Z_2/Z_1$, the reflected energy fraction is $((Z_2-Z_1)/(Z_2+Z_1))^2$; with $Z_2/Z_1 \sim 10^3$–$10^4$, **>99.9% of incident energy would reflect — a loss >30 dB, and Killion & Dallos computed the mismatch loss to exceed 50 dB at 100 Hz** ([Ugarteburu et al. 2022, Front. Bioeng.](https://doi.org/10.3389/fbioe.2022.983510)). Without a transformer, we would be functionally deaf.

The ossicular chain (malleus → incus → stapes) is that transformer, with three gain mechanisms ([Contemporary Mechanics of Conductive Hearing Loss](https://pmc.ncbi.nlm.nih.gov/articles/PMC11052546/)):

1. **Area (hydraulic) ratio:** effective tympanic-membrane area ~55 mm² vs stapes footplate ~3.2 mm² → ratio ≈ 17–20:1 (commonly quoted 20.8:1), worth ~26 dB.
2. **Ossicular lever:** the malleus manubrium is ~1.3× longer than the incus long process → ~1.3:1, worth ~2 dB.
3. **Catenary (membrane buckling) lever** of the curved drum: ~2:1 displacement reduction, worth ~6 dB.

**Disagreement, reported:** textbooks and clinical sources quote total middle-ear pressure gain variously as **~22× (~27 dB)**, **25× (~28 dB)**, or **~50× (~34 dB)**. The ENT-surgery literature states "the acoustic transformer theory predicts a middle ear gain of approximately 27 to 34 dB" ([Ento Key, Ossiculoplasty](https://entokey.com/ossiculoplasty-i/)); the conductive-loss review above says "almost 50-fold (~34 dB)"; measured umbo-to-stapes velocity ratios and cochlear pressures suggest the *measured* gain is frequency-dependent and lower — ~20 dB at 250–500 Hz peaking ~25 dB at 1 kHz, falling ~6 dB/octave above ([Ento Key](https://entokey.com/ossiculoplasty-i/)), and Puria et al. (1997) found anatomical predictions overestimate measured gain by ~6 dB on average ([Mason 2016](https://pmc.ncbi.nlm.nih.gov/articles/PMC4718164/)). Compounding the honest picture: Killion & Dallos (1979) argued the middle ear *alone* cannot fully compensate, and that it is the **combined outer + middle ear** that yields near-efficient transmission in the 1–4 kHz band where reflectance is lowest ([Ugarteburu et al. 2022](https://doi.org/10.3389/fbioe.2022.983510)). For engine purposes: model the ear's forward transfer as a band-pass peaking ~2.6–3 kHz with ~30±5 dB total gain.

### 1.3 The stapedius (acoustic) reflex — the ear's built-in limiter

The stapedius muscle, anchored to the stapes neck, contracts reflexively (bilateral, brainstem-mediated arc) in response to loud sound. Key numbers ([Acoustic reflex, Wikipedia summary of primary sources](https://en.wikipedia.org/wiki/Acoustic_reflex); [Trevino, Zang & Lobariñas 2023, JASA 153, 436](https://pmc.ncbi.nlm.nih.gov/articles/PMC9867568/)):

- **Threshold:** ~70–100 dB SPL for pure tones in normal hearers (broadband noise ~15–20 dB lower, ~70–75 dB SPL); the task spec's "~80 dB SPL" is a fair central value. Tonal reflexes are typically present until hearing loss exceeds ~60 dB HL.
- **Attenuation:** when driven ≥20 dB above threshold, contraction reduces transmitted low-frequency energy by **~10–15 dB** (some sources up to 20–30 dB at 250–500 Hz).
- **Latency:** onset ~25–35 ms at high SPLs, ~150 ms near threshold, and full tension may take 100+ ms — which is why the reflex **cannot protect against impulsive sounds** (gunshots, explosions) but does explain why a concert or factory "sounds less loud a few seconds in," and why repeated impulses spaced >2–3 s re-trigger it with some protective effect.
- **Frequency dependence:** the reflex attenuates predominantly below ~1–2 kHz (stiffening the annular ligament), leaving high frequencies relatively unattenuated.

### 1.4 The cochlea: a 2½-turn hydraulic spectrum analyzer

The cochlea is a ~35 mm-long coiled duct (2½ turns) divided into three fluid chambers: **scala vestibuli** and **scala tympani** (both perilymph, joined at the apical helicotrema) sandwiching the **scala media** (endolymph), which is bounded below by the basilar membrane (BM) and above by Reissner's membrane ([Purves, Neuroscience, "Two Kinds of Hair Cells"](https://ncbi.nlm.nih.gov/books/NBK11122/); [StatPearls, Cochlear Function](https://www.ncbi.nlm.nih.gov/sites/books/NBK531483/)). Stapes piston motion at the oval window sets up pressure differences across the cochlear partition, relieved at the round window; the result is a slow dispersive wave on the BM (§2).

The scala media's chemistry is unique among extracellular fluids: **endolymph holds ~150 mM K⁺, ~5 mM Na⁺, ~20 µM Ca²⁺ and sustains a +80 mV endocochlear potential (EP)** relative to perilymph — an "extracellular fluid behaving like intracellular fluid," generated by the stria vascularis from two series K⁺ diffusion potentials across electrically isolated intrastrial compartments ([Nin et al. 2008, PNAS](https://pmc.ncbi.nlm.nih.gov/articles/PMC2234216/); [Nin et al. 2016, Pflügers Arch.](https://link.springer.com/article/10.1007/s00424-016-1871-0)). Because a hair cell's interior rests around −45 to −60 mV, the total electrochemical driving force across its transducer channels is ~140 mV — the "battery of the ear." Anoxia or ouabain collapses the EP to −30/−40 mV and abolishes hearing ([Nin et al. 2016](https://link.springer.com/article/10.1007/s00424-016-1871-0)).

The organ of Corti rides on the BM and contains two functionally distinct receptor populations ([OpenStax Biology 36.4](https://openstax.org/books/biology/pages/36-4-hearing-and-vestibular-sensation); [Purves NBK11122](https://ncbi.nlm.nih.gov/books/NBK11122/)):

- **Inner hair cells (IHCs):** one row, **~3,500 cells**, the actual sensors. ~90–95% of auditory-nerve afferent fibers synapse on them (each IHC drives ~10 spiral-ganglion fibers via ribbon synapses).
- **Outer hair cells (OHCs):** three (sometimes four) rows, **~12,000 cells** — **disagreement reported:** OpenStax and Purves give ~12,000; other summaries (and the upper range in the task brief) give 12,000–15,000; the ~15,000 total hair-cell figure in [Liu et al. 2015, He et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC4412841/) ("approximately 15,000 sensory hair cells" total including IHCs) brackets the range. OHCs receive mostly *efferent* innervation and act as the biological amplifiers (§2.3), not primary sensors.

Overlying both, the gelatinous **tectorial membrane** shears the tallest OHC stereocilia directly (they are embedded in it), while IHC stereocilia are only viscously coupled via sub-tectorial fluid ([OpenStax](https://openstax.org/books/biology/pages/36-4-hearing-and-vestibular-sensation)) — a geometric detail that lets OHCs feed mechanical power back into the partition.

---

## 2. Cochlear Mechanics: The Traveling Wave and Its Amplifier

### 2.1 The passive traveling wave

Von Békésy (Nobel 1961), observing cadaver cochleae under a microscope, established the canonical picture: stapes pressure launches a **displacement wave that travels base→apex along the BM at speeds orders of magnitude below sound speed in water (which crosses the whole cochlea in microseconds)**; the wave grows, peaks, and collapses at a position set by frequency — high frequencies peak basally, low frequencies apically ([Robles & Ruggero 2001, Physiol. Rev. 81, 905](https://pmc.ncbi.nlm.nih.gov/articles/PMC3590856/)).

The mechanism is a graded mass–stiffness resonance. The BM is narrow and stiff at the base, wide and floppy at the apex. In humans, BM width runs **~126–144 µm at the base to ~418 µm at the apex**, while thickness *decreases* from ~1.5 µm to ~0.2–0.75 µm; overall stiffness falls by roughly a factor of **100 from base to apex** ([Liu et al. 2015, "Macromolecular organization … of the human basilar membrane"](https://pmc.ncbi.nlm.nih.gov/articles/PMC4412841/); [Bhatt et al. 2001 cited therein]). A local BM segment behaves like a damped oscillator whose resonant frequency $\omega(x) \approx \sqrt{k(x)/m}$ falls as stiffness $k$ falls, so a wave entering at the base propagates until it reaches the place whose local resonance matches its frequency — where phase velocity plummets, energy piles up, and the wave dies. At behavioral threshold the BM moves ~**0.1 nm**, the diameter of a hydrogen atom ([Liu et al. 2015](https://pmc.ncbi.nlm.nih.gov/articles/PMC4412841/)).

**Passive tuning is broad.** Békésy's dead cochleae showed $Q_{10}$ (CF ÷ 10-dB bandwidth) far below neural tuning, motivating decades of "second filter" theories that turned out to be wrong: live, laser-interferometric measurements show the *mechanical* tuning is itself sharp — but active, level-dependent, and powered by OHCs ([Robles & Ruggero 2001](https://pmc.ncbi.nlm.nih.gov/articles/PMC3590856/); [Ashmore 2008, Physiol. Rev. 88, 173](https://www.uclahealth.org/sites/default/files/documents/OHC_Motility-Ashmore_Review08.pdf)).

### 2.2 The place–frequency map: the Greenwood function

Greenwood integrated empirical critical-bandwidth data into an exponential place-frequency map, fitted it to human cadaver cochleae in 1961, and re-validated it against a much larger multi-species dataset in 1990 ([Greenwood 1990, JASA 87, 2592](https://doi.org/10.1121/1.399052)). With $x$ the fractional distance from the apex (0 at apex, 1 at base) and human constants $A = 165.4$ Hz, $a = 2.1$, $k = 0.88$:

$$\boxed{\; f(x) = A\left(10^{a x} - k\right) = 165.4\left(10^{2.1 x} - 0.88\right)\ \text{Hz} \;}$$

([Greenwood 1990](https://doi.org/10.1121/1.399052); constants confirmed in [Wikipedia: Greenwood function](https://en.wikipedia.org/wiki/Greenwood_function) and applied in [Frontiers in Neuroscience 2025 cochlear-implant study](https://www.frontiersin.org/journals/neuroscience/articles/10.3389/fnins.2025.1624499/full). Note the same $a = 2.1$ holds across mammals from gerbil to elephant when $x$ is normalized to cochlear length ([Robles & Ruggero 2001](https://pmc.ncbi.nlm.nih.gov/articles/PMC3590856/)); if $x$ is in **mm**, $a \approx 0.06$.)

**Sanity checks (verified):** $x = 0$ (apex) gives $f = 165.4(1 - 0.88) = 19.8$ Hz ≈ 20 Hz; $x = 1$ (base) gives $165.4(10^{2.1} - 0.88) = 165.4 \times 124.4 \approx 20{,}600$ Hz ≈ 20 kHz. An octave subtends a constant ~14% of BM length over the basal 75% of the cochlea; the apical quarter is compressed. Example from [Wikipedia (Greenwood)](https://en.wikipedia.org/wiki/Greenwood_function): a cochlear implant inserted 25 mm into a 35 mm cochlea leaves $x = 10/35$, i.e. its lowest stimulated place maps to $f = 165.4(10^{2.1 \cdot 10/35} - 0.88) \approx 513$ Hz — the reason deeply inserted electrodes sacrifice low-frequency percepts.

### 2.3 The cochlear amplifier: outer hair cells and prestin

The 40–60 dB sensitivity gap between passive and live BM response is closed by OHC **somatic electromotility**: OHCs change length with membrane voltage — depolarization shortens, hyperpolarization elongates, up to **4% of cell length**, with a total length change of up to ~30–50 nm per cell ([Ashmore 2008](https://www.uclahealth.org/sites/default/files/documents/OHC_Motility-Ashmore_Review08.pdf)). The motor is **prestin (SLC26A5)**, an 81 kDa, 744-amino-acid membrane protein packed at extreme density in the OHC lateral wall; identified by Zheng et al. in 2000, it operates as an incomplete anion transporter whose chloride-bound conformational flip changes its in-plane area ([Zheng et al. 2000, Nature 405, 149](https://doi.org/10.1038/35012009); [Ashmore et al. 2009 review, "Outer Hair Cells and Electromotility"](https://pmc.ncbi.nlm.nih.gov/articles/PMC6601450/)). Electromotility is fast: the microchamber/vibrometer measurements of Frank et al. (1999) put its 3-dB limit at **~79 kHz in basal OHCs** — comfortably above any mammalian CF ([Ashmore 2008](https://www.uclahealth.org/sites/default/files/documents/OHC_Motility-Ashmore_Review08.pdf); OpenStax's behavioral-neuroscience chapter quotes prestin-driven shape changes "at frequencies up to ~50 kHz" — **disagreement reported**, both values from credible sources). Knocking out prestin costs **40–60 dB** of sensitivity and abolishes sharp frequency selectivity ([Liberman et al. 2002, Nature 419, 300](https://doi.org/10.1038/nature01059); [Dallos 2008, Curr. Biol. 18, R200](https://doi.org/10.1016/j.cub.2008.01.006)).

The amplifier's physiological signature is a triad of nonlinearities:

- **Compression:** near CF, BM response grows at roughly **0.2–0.5 dB per dB of input** above ~20–30 dB SPL — Eguíluz et al. describe ~80 dB of input range squeezed into ~20 dB of BM response near the peak ([Eguíluz et al. 2000, PRL 84, 5232](https://doi.org/10.1103/physrevlett.84.5232)). This is why loudness grows so slowly with intensity (§4).
- **Two-tone suppression:** the response to one tone at CF is *reduced* by a second tone at neighboring frequencies — a hallmark of a saturating nonlinearity, measurable psychophysically and mechanically ([Robles & Ruggero 2001](https://pmc.ncbi.nlm.nih.gov/articles/PMC3590856/)).
- **Otoacoustic emissions (OAEs):** the amplifier leaks acoustic energy back out of the ear (Kemp 1978). Stimulating with two primaries $f_1 < f_2$ (ratio ~1.2) produces a measurable distortion product at $2f_1 - f_2$ in the ear canal — the **DPOAE**, now the standard newborn hearing screen and a frequency-specific probe of OHC health ([Robinette & Glattke review](https://pmc.ncbi.nlm.nih.gov/articles/PMC3614374/); [Dorn et al. 2000, JASA](https://doi.org/10.1121/1.428494)). The ear generates its own intermodulation distortion and re-emits it: a passive filter cannot do this.

The measured sharpness of the active filter: neural $Q_{10}$ rises monotonically with CF — from **~1–1.5 below 1 kHz to ~4–6 near 10 kHz in cat/chinchilla**, with macaque values averaging ~9.7 at 10 kHz, and human CAP-derived estimates ~1.6× sharper than cat/chinchilla, i.e. **Q₁₀ ≈ 1–10 across the audible range as the task brief states** ([Temchin et al., chinchilla ANF tuning](https://pmc.ncbi.nlm.nih.gov/articles/PMC2585409/); [Verschooten et al. 2015, AIP Conf. Proc.](https://doi.org/10.1063/1.4939375); [Verschooten et al. 2018, PLOS Biology](https://journals.plos.org/plosbiology/article?id=10.1371%2Fjournal.pbio.2005164)).

### 2.4 The active filter as a Hopf bifurcation near criticality

The cleanest mathematical account of why *one* mechanism delivers both sharp tuning and compression is that each cochlear place operates near a **Hopf bifurcation** — the parameter point where a damped oscillator becomes self-oscillatory. Eguíluz, Ospeck, Choe, Hudspeth & Magnasco showed that a system at the bifurcation, forced at its characteristic frequency $\omega_0$ with amplitude $F$, responds as $R \propto F^{1/3}$: infinite small-signal gain, cubic compression, and combination-tone generation, all "essentially nonlinear" (no input is soft enough to escape them) ([Eguíluz et al. 2000, PRL 84, 5232](https://doi.org/10.1103/physrevlett.84.5232)). Camalet, Duke, Jülicher & Prost generalized this to **self-tuned criticality**: Ca²⁺-dependent feedback automatically poises each hair bundle just below its bifurcation, explaining the ear's sensitivity, dynamic range, and even spontaneous otoacoustic emissions within one framework ([Camalet et al. 2000, PNAS 97, 3183](https://pmc.ncbi.nlm.nih.gov/articles/PMC16213/)). The biological implementation differs by clade — stereociliar motility in non-mammals, prestin-driven somatic motility in mammals ([Dallos 2008](https://doi.org/10.1016/j.conb.2008.08.016)) — but the dynamical-systems logic is the same: run the amplifier as close to instability as possible without tipping over, exactly like a sound engineer riding a PA gain just below feedback (Gold's 1948 intuition; see [Ashmore 2008](https://www.uclahealth.org/sites/default/files/documents/OHC_Motility-Ashmore_Review08.pdf) for the history).

---

## 3. Transduction to Electricity and the Ascending Pathway

### 3.1 Mechanoelectrical transduction

Each hair cell apically bears a bundle of ~50–150 stereocilia arranged in graded rows. Oblique **tip links** (~150–170 nm long, 8–11 nm diameter) connect each short stereocilium's tip to the side of the next taller neighbor ([Zheng & Holt 2021, Annu. Rev. Biophys. 50](https://www.annualreviews.org/content/journals/10.1146/annurev-biophys-062420-081842)). Each link is an antiparallel tetramer: a **cadherin-23 homodimer above, protocadherin-15 homodimer below**, joined N-terminus to N-terminus in a Ca²⁺-dependent handshake ([Kazmierczak et al. 2007, cited in](https://www.sciencedirect.com/science/article/abs/pii/S0378595515001136); [Pan et al., PCDH15–TMC interaction](https://pmc.ncbi.nlm.nih.gov/articles/PMC4156717/)). Deflection toward the tallest row tensions the links and pulls open mechanically gated channels at the lower insertion points — the **TMC1/TMC2** proteins are the leading candidates for the pore ([Pan et al. 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC4156717/); [Zheng & Holt 2021](https://www.annualreviews.org/content/journals/10.1146/annurev-biophys-062420-081842)). The channel is a nonselective cation pore (~1.2 nm minimum diameter); because endolymph is K⁺-rich and +80 mV, the inward current is carried mostly by K⁺ (with a Ca²⁺ component that tunes adaptation) — the hair cell is *depolarized by an extracellular cation*, which is why the endocochlear potential exists at all ([Zheng & Holt 2021](https://www.annualreviews.org/content/journals/10.1146/annurev-biophys-062420-081842); [Nin et al. 2016](https://link.springer.com/article/10.1007/s00424-016-1871-0)). Operating range is astonishingly tight: bullfrog saccular data suggest channels move from 10% to 90% open probability over ~10–15 pN of tip-link force ([Żerdziono et al., Nat. Commun. 2021](https://preview-www.nature.com/articles/s41467-021-21033-6.pdf)). A single IHC depolarization releases glutamate at ribbon synapses onto 10–20 auditory-nerve fibers within microseconds ([Moser et al. review, "Encoding sound in the cochlea"](https://pmc.ncbi.nlm.nih.gov/articles/PMC8127127/)).

### 3.2 Firing rates, phase locking, and the volley principle

Auditory-nerve fibers (ANFs; ~30,000 per side) fire up to a few hundred spikes/s each — saturating near ~200–300 spikes/s in most mammals (some fibers transiently exceed 1 kHz), so the commonly cited "200–1000 spikes/s per fiber" spans the sustained-to-transient range ([Moser et al. 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8127127/); [Heinz et al. discussion of saturation](https://pmc.ncbi.nlm.nih.gov/articles/PMC6524635/)). Spikes phase-lock to the stimulus waveform — firing at a preferred phase of each cycle — but only up to a few kHz:

- In **cat** (the classic dataset, Johnson 1980), vector strength declines above ~1 kHz corner and becomes undetectable above ~**4–5 kHz** ([Verschooten & Joris 2015, J. Neurosci. 35, 2255](https://www.jneurosci.org/content/35/5/2255); [Allen 1983](https://jontalle.web.engr.illinois.edu/Public/Allen83a.pdf)).
- In **humans**, the limit is debated and *task-dependent*: binaural fine-structure use collapses hard between 1300–1500 Hz ([Brughera et al. 2013, cited in](https://pmc.ncbi.nlm.nih.gov/articles/PMC6524635/)), while frequency-discrimination data imply monaural temporal information is usable up to ~8–10 kHz; expert estimates in the 2019 *Hearing Research* viewpoint collection span **1500 to 10000 Hz** ([Verschooten, Bharadwaj, Shinn-Cunningham et al. 2019, Hearing Research 377](https://www.sciencedirect.com/science/article/pii/S0378595518305604)). Mechanistically the ceiling comes from the IHC membrane time constant (~0.2–1 ms, corner 160–800 Hz) plus synaptic jitter ([Moser et al. 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8127127/)).

The **volley principle** (Wever & Bray 1930; Wever 1949) resolves the apparent paradox that we hear pitch above any single fiber's lock rate: groups of fibers take turns — each fires on some cycles, the ensemble's pooled activity preserves the period well beyond individual rates ([Purves NBK11105, "Tuning and Timing in the Auditory Nerve"](https://www.ncbi.nlm.nih.gov/books/NBK11105/); [Joris & Smith 2008, "The volley theory and the spherical cell puzzle"](https://pmc.ncbi.nlm.nih.gov/articles/PMC2486254/)). Purves states the one-to-one following limit in humans as ~3 kHz, beyond which labeled-line (place) coding takes over. Both codes run in parallel, and pitch models use both (§5.3).

### 3.3 Tonotopy all the way up

The place map set by the BM is preserved through the entire pathway: ANF → **cochlear nucleus** (tonotopically laminated) → **inferior colliculus** → **medial geniculate nucleus of the thalamus** → **primary auditory cortex** (A1), whose tonotopic axis runs roughly posterolateral (low CF) to anteromedial (high CF) ([OpenStax Biology 36.4](https://openstax.org/books/biology/pages/36-4-hearing-and-vestibular-sensation); [OpenStax Behavioral Neuroscience 7.2](https://openstax.org/books/introduction-behavioral-neuroscience/pages/7-2-how-does-acoustic-information-enter-the-brain)). Binaural comparison begins at the **superior olivary complex**, whose bushy-cell endbulbs of Held preserve microsecond timing for ITD computation ([Heinz et al. 2019](https://pmc.ncbi.nlm.nih.gov/articles/PMC6524635/)) — the only place in the CNS where timing this precise survives a synapse.

---

## 4. Loudness

### 4.1 Equal-loudness contours (ISO 226:2003)

A **phon** is the loudness level of a tone judged as loud as a 1 kHz tone at that SPL: 40 phon = the loudness of 1 kHz at 40 dB SPL, by definition. ISO 226:2003 specifies normal equal-loudness-level contours for otologically normal 18–25-year-olds, binaural, frontal free field ([ISO 226:2003 catalog](https://www.iso.org/standard/34222.html); the informative-preview PDF exposes the parameter table and Annex B values — [iTeh preview](https://cdn.standards.iteh.ai/samples/34222/d93363dbdafa470aab734f04d091065b/ISO-226-2003.pdf)). Representative **verified** values from ISO 226:2003 Table B.1 (SPL in dB required for the given loudness level):

| Frequency | 40 phon | 60 phon | 70 phon |
|---|---|---|---|
| 63 Hz | 53.4* | 66.2 | 73.1 |
| 100 Hz | 50.4* | 61.2 | 68.5 |
| 1 kHz | 40.0 | 60.0 | 70.0 |
| 4 kHz | 43.2* | 57.6 | 68.0 |

\*Interpolated between tabulated 50 Hz/80 Hz and 3.15/5 kHz rows; the directly tabulated neighbors: 40 phon at 80 Hz = 47.6, at 3.15 kHz = 39.2, at 5 kHz = 40.0; 60 phon at 80 Hz = 59.9, at 3.15 kHz = 57.6 ([ISO 226:2003 Annex B, tables reproduced in the iTeh preview](https://cdn.standards.iteh.ai/samples/34222/d93363dbdafa470aab734f04d091065b/ISO-226-2003.pdf)). Cross-check: an independent calculator implementing Table 1 returns 78.65 dB SPL for 60 phon at 100 Hz — matching Table B.1's 61.2/66.2 bracket at 80/63 Hz to within interpolation error ([Found-tools ISO 226 calculator](https://found-tools.com/en/audio-equal-loudness-contour-spl-calculator/)). The task brief's sanity target — "40-phon at 63 Hz ~30-ish dB" — is **not** confirmed; the standard's value is ~53 dB (the 30-ish figure belongs to ~500 Hz, and roughly matches the *threshold* value $T_f = 37.5$ dB at 63 Hz plus the ~10–15 dB gap between threshold and 40 phon at low frequency). The 20 Hz threshold is 78.5 dB.

**The shape and why it flattens:** at 40 phon the ear needs ~50 dB at 100 Hz versus 40 dB at 1 kHz — i.e. ~10 dB *less sensitive* at 100 Hz (and >30 dB less at 20 Hz); at 70 phon the gap shrinks to ~8.5 dB at 100 Hz and the 3–4 kHz region actually becomes *more* sensitive than 1 kHz (the ear-canal resonance of §1.1 doing its work). The contours converge at high level because the cochlear amplifier's compression (§2.3) boosts low-level responses far more than high-level ones, equalizing growth across frequency. The standard's own analytic form, for reference ([iTeh preview, Eq. 1](https://cdn.standards.iteh.ai/samples/34222/d93363dbdafa470aab734f04d091065b/ISO-226-2003.pdf)):

$$L_p = \alpha_f\,(L_N - L_U) - \alpha_f\,T_f + L_U \quad\text{(valid 20–90 phon, 20 Hz–12.5 kHz)}$$

with per-frequency $\alpha_f$, $L_U$, $T_f$ tabulated (e.g. at 1 kHz: $\alpha = 0.250$, $L_U = 0$ dB, $T_f = 2.4$ dB).

### 4.2 The sone scale and Stevens' power law

The phon scale is logarithmic in loudness-*matching*; the **sone** scale is *ratio*-scaled by direct magnitude estimation. Definition: 1 sone = 40 phon; a sound judged twice as loud is 2 sones. Stevens' 1955 synthesis of halving/doubling data (median 10.3 dB across experiments) fixed the ratio so that **a 10-phon increase doubles loudness**, which is exactly a power law of exponent 0.3 in intensity ([Stevens 1955, JASA 27, 815](http://languagelog.ldc.upenn.edu/myl/StevensJASA1955.pdf)):

$$\boxed{\; N = 2^{\dfrac{L_N - 40}{10}}\ \text{sones} \quad\Longleftrightarrow\quad L_N = 40 + 10\log_2 N\ \text{phon} \;}$$

([Sengpiel sone/phon conversion, ISO/R 131-1959](https://sengpielaudio.com/calculatorSonephon.htm); [Wikipedia: Sone](https://en.wikipedia.org/wiki/Sone)). Equivalently Stevens' law: $\psi \propto I^{0.3} \propto p^{0.6}$ — **10× intensity ≈ 2× loudness**. Caveats worth keeping: the ANSI/Moore loudness model uses exponent 0.31 over 40–100 dB, below 40 phon the relation breaks (the conversion is only valid above ~1 sone), and Warren's later bias-controlled work argues half-loudness ≈ half-*pressure* (−6 dB) — the 10 dB doubling is a definitional convention with real experimental scatter, not a physical constant ([Sengpiel](https://sengpielaudio.com/calculatorSonephon.htm); [Heeren et al. discussion](https://pmc.ncbi.nlm.nih.gov/articles/PMC5736394/)). Warren/Neuhoff's ~6 dB finding is the standing rebuttal to dogmatic 10 dB.

### 4.3 Absolute dynamic range

Threshold of hearing at 1–5 kHz is ~0 dB SPL (20 µPa) — indeed the ear operates *near the thermal-noise limit* around 5 kHz, Shaw calculated ([Shaw 1980](https://jcaa.caa-aca.ca/index.php/jcaa/article/view/441)). Threshold of pain is ~120–140 dB SPL. That is a **10¹²:1 intensity range (10⁶:1 in pressure)** mapped through cochlear compression into a ~40 dB neural response span — compression is not a bug but the enabler of the range (§2.4).

### 4.4 Temporal integration and forward masking

Detection threshold improves with signal duration up to ~**200 ms**, at roughly **3 dB per doubling of duration** — classic energy-detector behavior with an integration window τ ≈ 200 ms (Plomp & Bouman 1959; Zwicker & Feldtkeller 1967; confirmed in Dau's modeling: "thresholds were found to decrease with an increasing signal duration up to about 200 ms," [Dau et al. 1996, JASA 100, 3310](https://doi.org/10.1121/1.414960)). Modern refinement (Oxenham, Moore & Vickers) splits this into a ~10 ms short-term integrator plus multiple-looks/statistical accumulation, with the level dependence of short-duration slopes explained by BM compression ([Oxenham et al. 1997, JASA 101, 1631](https://doi.org/10.1123/1.418328) — note the DOI as fetched). Forward (post-)masking decays over **~50–200 ms**: a signal reaches absolute threshold only ~200 ms after masker offset, decaying steeply at first and more slowly later; the decay rate grows with masker level and shortens with shorter maskers, and is shallower at low frequencies because the cochlear filter's ringing lasts longer there ([Dau et al. 1996](https://doi.org/10.1121/1.414960); [Nagaraj et al. 2010, POMA 9, 050007](https://doi.org/10.1121/1.3481455)).

### 4.5 Hearing damage: the regulators disagree

- **NIOSH (1998, Criteria Doc 98-126):** REL = **85 dBA as an 8-h TWA**, **3-dB (equal-energy) exchange rate** — 88 dBA for 4 h, 91 dBA for 2 h, …130–140 dBA for <1 s; 40-year excess risk of occupational hearing loss at the REL ≈ 8% (vs 25% at OSHA's 90 dBA PEL) ([NIOSH 98-126](https://www.cdc.gov/niosh/docs/98-126/pdfs/CriteriaDoc_98-126.pdf?id=10.26616%2FNIOSHPUB98126); [CDC/NIOSH bulletin](https://www.cdc.gov/niosh/bulletin/2016/noise.html)).
- **OSHA (29 CFR 1910.95):** PEL = **90 dBA 8-h TWA**, **5-dB exchange rate** — 95 dBA for 4 h, 100 dBA for 2 h, 115 dBA for ¼ h; hearing-conservation program trigger (action level) at 85 dBA TWA ([OSHA 1910.95, Table G-16](https://www.osha.gov/laws-regs/regulations/standardnumber/1910/1910.95)).
- **The disagreement is explicit and acknowledged:** noise *energy* doubles every 3 dB; OSHA's historical rationale for 5 dB was to credit quiet time within a shift, and NIOSH explicitly "now recommends a 3-dB exchange rate, which is more firmly supported by scientific evidence" while OSHA still enforces 5 dB ([OSHA Technical Manual III-5](https://osha.prod.pace.dol.gov/otm/section-3-health-hazards/chapter-5); [NIOSH 98-126 preface](https://www.cdc.gov/niosh/docs/98-126/pdfs/CriteriaDoc_98-126.pdf?id=10.26616%2FNIOSHPUB98126)). Use NIOSH numbers for actual safety margins; OSHA numbers are the legal floor.
- **Presbycusis** adds a progressive, roughly linear-with-log-frequency high-frequency threshold shift, ~1 dB/year above 60 kHz-place… i.e., losses accumulate earliest and fastest at 4–8 kHz (the basal cochlea, poorest blood supply, longest OHC load), producing the familiar steep high-frequency audiogram slope in older listeners; DPOAE fine-structure studies show measurable cochlear decline *above 8 kHz* even in young-normal groups ([Mills et al., DPOAE normative study](https://pmc.ncbi.nlm.nih.gov/articles/PMC3986236/)).

**Game-audio aside:** a mixer that respects §4 will mix dialogue near ~60 phon, keep transient SFX short (<200 ms buys integration gain), and never stack >2 simultaneous loud elements in the same Bark band — the ear's own compressor will otherwise crush everything into mud.

---

## 5. Frequency Selectivity and Pitch

### 5.1 Critical bands: the Bark scale

Fletcher's 1940 masking experiments implied the ear analyzes sound through a bank of ~24 overlapping band-pass filters. Zwicker's tabulated critical bands (24 spanning 0–15.5 kHz, e.g. edges 0, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500 Hz) define the **Bark scale** ([Zwicker band table via dsprelated](https://www.dsprelated.com/freebooks/sasp/Bark_Frequency_Scale.html); [Wikipedia: Bark scale](https://en.wikipedia.org/wiki/Bark_scale)). Zwicker & Terhardt's (1980) analytic fit, with $f$ in Hz:

$$\boxed{\; z(f) = 13\arctan(0.00076\,f) + 3.5\arctan\!\left[\left(f/7500\right)^2\right]\ \text{Bark} \;}$$

([Traunmüller 1990, JASA 88, 97, quoting Zwicker & Terhardt 1980](https://resources.ling.su.se/hartmut/JASA_88_1990a.pdf); [CCRMA lecture notes](https://ccrma.stanford.edu/courses/120-fall-2003/lecture-5.html); the DAGA 2015 paper confirms it fits the tabulated points to ±0.2 Bark and notes it is *not* closed-form invertible and underestimates above ~16 kHz — [Völk 2015](https://pub.dega-akustik.de/DAGA_2015/data/articles/000054.pdf)). Verified: $z(20\,\text{Hz}) \approx 0.2$; $z(1\,\text{kHz}) \approx 8.5$; $z(15.5\,\text{kHz}) = 24$. Alternative simple fits exist (Schroeder's $\sinh^{-1}$ forms; Traunmüller's logistic $z = 26.81f/(1960+f) - 0.53$, accurate to ±0.05 Bark over 0.2–6.7 kHz — [Traunmüller 1990](https://resources.ling.su.se/hartmut/JASA_88_1990a.pdf)).

### 5.2 The ERB scale

Modern notched-noise masking experiments (Patterson's method) give the auditory filter's **equivalent rectangular bandwidth** as a simple function of center frequency $f$ (in kHz; ERB in Hz), valid 100 Hz–10 kHz at moderate levels ([Glasberg & Moore 1990, Hearing Research 47, 103](https://doi.org/10.1016/0378-5955(90)90170-t); [Wikipedia: ERB](https://en.wikipedia.org/wiki/Equivalent_rectangular_bandwidth)):

$$\boxed{\; \mathrm{ERB}(f) = 24.7\left(4.37\,f_{\text{kHz}} + 1\right)\ \text{Hz} \;}$$

**Verified values:** ERB(100 Hz) = 24.7(4.37×0.1+1) = **35.5 Hz**; ERB(1 kHz) = 24.7(4.37+1) = **132.6 Hz**; ERB(4 kHz) = 24.7(17.48+1) = **456 Hz**. (Direct low-frequency measurements agree: Moore, Peters & Glasberg 1990 measured average ERBs of 36, 47, 87, 147 Hz at 100, 200, 400, 800 Hz — [JASA 88, 132](https://doi.org/10.1121/1.399960).) The ERB *rate* scale converts frequency to filter number: $ERB_{\text{rate}}(f) = 21.4\log_{10}(4.37 f_{\text{kHz}} + 1)$; integrating from 20 Hz to 20 kHz yields **~42 ERBs versus ~24–25 Barks** — the ERB filter bank is finer because it reflects the sharper low-level filters of the living cochlea, while Bark absorbed the broader effective bandwidths of loud masking experiments. Below ~500 Hz the Bark scale is near-linear with 100 Hz bands (an artifact of Zwicker's table rounding, per [Traunmüller 1990](https://resources.ling.su.se/hartmut/JASA_88_1990a.pdf)); above ~500 Hz both approach logarithmic spacing of roughly 1/6–1/3 octave per band.

### 5.3 Frequency JND and pitch theories

Frequency discrimination at moderate levels is best around 500 Hz–2 kHz. At 1 kHz, the JND is **Δf ≈ 2–3 Hz (~0.2–0.3%)** for long-duration tones, worsening both at very low frequencies (where temporal precision fails) and above ~4–5 kHz (where phase locking has collapsed and only place coding remains, with its coarser spatial representation) ([Moore 1973 data discussed in](https://pmc.ncbi.nlm.nih.gov/articles/PMC6524635/); classic values summarized in [Shower & Biddulph 1931, cited in](https://www.frontiersin.org/journals/neuroscience/articles/10.3389/fnins.2025.1624499/full): a normal listener discriminates ~1,400–1,600 frequencies). The deterioration above 4–8 kHz is the strongest single piece of evidence for temporal coding's role in frequency discrimination.

**Place vs. temporal theory.** Place theory (Helmholtz, and Ohm's law of sound analysis) says pitch = cochlear position; temporal theory (Seebeck → Schouten → Licklider) says pitch = waveform periodicity. The **missing fundamental** is the decisive phenomenon: a complex of 400, 500, 600… Hz with no 100 Hz component still yields a ~100 Hz pitch. Schouten (1938–40), using his optical siren, showed the ear does not regenerate the fundamental by nonlinear distortion (his residue theory: unresolved upper harmonics are perceived collectively with the pitch of their common periodicity) ([Schouten 1940, KNAW Proc.](https://dwc.knaw.nl/DL/publications/PU00017418.pdf); [Plomp 1991, "J.C.R. Licklider and the case of the missing fundamental"](https://doi.org/10.1121/1.2029388)). Licklider's duplex theory (1951) and the modern **autocorrelation models** (Meddis & Hewitt 1991; Meddis & O'Mard 1997) reconcile both: each cochlear channel's rectified output is auto-correlated across lags, then summed across channels — the summary ACF peaks at the period, so *resolved* low harmonics (place) and *unresolved* high harmonics (temporal) both contribute to one periodicity estimate ([Plack et al. pitch chapter, IRCAM](http://recherche.ircam.fr/equipes/perception/pdf/2004_pitch_SHAR.pdf); [Meddis & O'Mard 1997, JASA 102, 1818](https://doi.org/10.1121/1.420088)). This is why we hear pitch below the phase-locking limit for *pure* tones: the *periodicity* of harmonic complexes is encoded by envelope/AM timing that survives to much higher carrier frequencies — up to ~20 kHz in principle via AM of combined harmonics ([Moser et al. 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8127127/)).

### 5.4 Musical intervals and consonance

Equal temperament: $f_n = f_0 \cdot 2^{n/12}$; the octave is exactly 2:1; the tempered fifth is $2^{7/12} = 1.4983$, off the just 3:2 by ~2 cents, and stacking twelve tempered fifths overshoots seven octaves by the **Pythagorean comma** $ (3/2)^{12}/2^7 \approx 1.0136$ (~23.5 cents).

The classical view — consonance = low-order integer ratios, from coincidence of partials — was substantially **revised by Plomp & Levelt (1965)**. They had listeners judge simple-tone pairs across frequency separations and found: intervals are judged consonant once their separation **exceeds the critical bandwidth**, and are maximally dissonant at a separation of **about a quarter of the critical bandwidth** — *regardless of whether the ratio is a small integer* ([Plomp & Levelt 1965, JASA 38, 548](https://www.mpi.nl/world/materials/publications/levelt/Plomp_Levelt_Tonal_1965.pdf)). Roughness comes from beats between partials too close to resolve but too far to fuse (Helmholtz's insight, retained); what the integer ratios *actually* buy is that for complex tones, low-order ratios place neighboring upper partials outside each other's critical bands. Their chord-census analysis of a Bach trio sonata and a Dvořák quartet showed composers' simultaneous-partial densities track the critical-bandwidth curve across frequency — "critical bandwidth plays an important role in music." Implication for synthesis: dissonance is a *spectral-distance* phenomenon measurable in Barks, not a mystical property of ratios; a sawtooth and a square wave at the same root have different dissonance profiles purely through partial spacing.

---

## 6. Masking

### 6.1 Simultaneous masking and the upward spread

A tone is masked by noise or another tone lying within roughly one critical band of it — energy inside the same cochlear filter raises that filter's output and the signal's increment becomes undetectable (Fletcher's original 1940 logic). Two features dominate the masking pattern's shape ([Zwicker & Fastl, Psychoacoustics, via the CCRMA/DAGA summaries](https://ccrma.stanford.edu/courses/120-fall-2003/lecture-5.html)):

- **Upward spread of masking:** a masker masks tones *above* its own frequency far more effectively than below — a low-frequency masker at high level raises thresholds across a wide higher-frequency span, sometimes 1+ octave up. The mechanism is cochlear mechanics: the masker's traveling wave passes through (and is amplified at) all places basal to its own peak, so a louder low-frequency tone contaminates the high-frequency channels' response. The spread grows with masker level (a direct consequence of the saturating amplifier nonlinearity of §2.3). Game-mixing translation: a bass-heavy rumble bed can easily swallow footstep transients 2 octaves up; the reverse almost never happens.
- **Asymmetry of tone-on-tone masking:** a tonal masker masks higher-side probe tones up to ~60+ dB above threshold while the lower skirt falls off within a fraction of a critical band. The MPEG spreading functions encode exactly this asymmetry (below).

### 6.2 Temporal masking

- **Premasking (backward):** a masker can mask a signal ending up to **~5–20 ms *before* masker onset** — the auditory system's temporal integration window smears the order ([Fastl & Zwicker 2007, via the MPEG tutorial's treatment](https://icg.isy.liu.se/en/courses/TSBK38/material/mpegaud.pdf)).
- **Postmasking (forward):** after masker offset, threshold elevation decays over **~50–200 ms**, steeply at first, then slowly, reaching absolute threshold only ~200 ms out; decay depends on masker level, duration, and probe frequency (§4.4 sources: [Dau et al. 1996](https://doi.org/10.1121/1.414960), [Nagaraj et al. 2010](https://doi.org/10.1121/1.3481455), [Jesteadt et al. 1982 via](https://pmc.ncbi.nlm.nih.gov/articles/PMC7363451/)).

### 6.3 Psychoacoustic models in codecs: MPEG-1 Model 1 as the canonical example

MPEG-1/2 audio (Layers I–III; MP3 is Layer III, AAC the successor) allocates quantization noise *under* the masking threshold rather than uniformly. The encoder's psychoacoustic model runs in parallel with the polyphase filter bank and, per analysis window, computes a per-subband signal-to-mask ratio that drives bit allocation ([Davis, "The Reference Model" tutorial](https://icg.isy.liu.se/en/courses/TSBK38/material/mpegaud.pdf); [IEEE Multimedia MPEG tutorial](https://docencia.ac.upc.edu/FIB/PIAM/TutorialMPEG.pdf)). **Model 1's nine-step procedure (ISO/IEC 11172-3 Annex D)**, as documented in the tutorials and an open-source implementation's spec-faithful module ([oxideav-mp1 model1.rs](https://docs.rs/oxideav-mp1/latest/src/oxideav_mp1/model1.rs.html)):

1. Hann-windowed **FFT — 512 samples for Layer I, 1024 for Layers II/III** (at 48 kHz that is a ~21 ms window — long enough to resolve critical bands, short enough to track temporal masking) → power density spectrum normalized to 96 dB SPL.
2. Per-subband sound-pressure level from spectral max + scalefactor.
3. Threshold in quiet $T_q$ from the absolute-threshold tables (with a −12 dB offset for high bitrates).
4. **Tonal vs. non-tonal component identification** (local maxima vs. broadband energy).
5. **Decimation:** drop components below $T_q$ and tonal components within 0.5 Bark of a stronger tonal one (the psychoacoustic justification for quantization: the ear cannot hear the weaker of two partials in the same critical band).
6. Individual masking thresholds via the **masking index and masking function** — an empirical spreading function, asymmetric in Bark distance, whose upward tail is longer (§6.1).
7. **Global masking threshold** per critical-band sample: power-sum of individual thresholds plus the threshold in quiet.
8. Per-subband minimum threshold $LT_{\min}(n)$.
9. $SMR(n) = L_{sb}(n) - LT_{\min}(n)$, handed to the bit allocator, which iteratively assigns bits to the subband with the worst mask-to-noise ratio ($MNR = SNR - SMR$) until the budget is spent.

Model 2 refines this with unpredictable-measure tonality estimation and two offset 1024-point windows per Layer II/III frame, taking the higher SMR. The whole edifice is §5's critical-band filter bank + §6's masking shapes, expressed in code — the single most successful commercial application of psychoacoustics.

### 6.4 Auditory scene analysis (Bregman)

The ear receives one pressure sum; perception recovers sources. Bregman's **auditory scene analysis** (1990) describes the heuristic grouping that achieves this ([Bregman encyclopedia article](https://themusiclab.github.io/bregman-archive/pdf/2004_%20Encyclopedia-Soc-Behav-Sci.pdf); [Bregman archive findings](https://themusiclab.github.io/bregman-archive/asafind.htm)):

- **Sequential (streaming):** sounds group across time by proximity in frequency, pitch, timbre, amplitude, location — and by rate: alternating high/low tones at ≤3 Hz fuse into one up-down stream; at ≥8–12 Hz they split into two interleaved streams (the canonical streaming illusion). A "perceptual distance" $d$ combining frequency and time separation governs grouping; streams resist crossing in frequency.
- **Simultaneous grouping:** components fuse into one sound when they share **onset synchrony (±15–30 ms)**, harmonic relation to a common fundamental, parallel amplitude modulation, and proximity. Harmonicity is a powerful cue — two harmonic series with different fundamentals segregate from a mixture, which is why we can pick one voice from a crowd.
- **The old-plus-new heuristic:** when the spectrum suddenly gains complexity, the system attempts to subtract the continuing ("old") spectrum and hear the residue as a new source — making onsets the critical moments for source identification.
- Modern computational accounts recast this as **temporal coherence**: populations of auditory-cortex neurons whose responses correlate over time group into one stream; temporally incoherent responses split ([Elhilali & Shamma via](https://pmc.ncbi.nlm.nih.gov/articles/PMC3973443/)).

This is why a game can layer 20 simultaneous sounds and listeners still segregate them: give each source a distinct onsetting envelope, a distinct spectral region (≥1 ERB separation), coherent pitch, and ideally distinct spatial position, and Bregman's heuristics do the de-mixing for free. Conversely, two incoherent sounds sharing a critical band with synchronized onsets *fuse* — useful when you want layers to blend (walla + ambience) and fatal when you don't (dialogue + music fighting at 1–3 kHz).

---

## 7. Spatial Hearing and Localization

### 7.1 Interaural time difference (ITD)

Sound from azimuth $\theta$ reaches the near ear first; the interaural path difference around a rigid spherical head of radius $a$ is modeled by **Woodworth's formula** (Woodworth 1938; plane-wave, antipodal-ears ray-tracing approximation), with $\theta$ in radians, $|\theta| \le 90°$ ([Hartmann & Aaronson 2013, JASA 134, 1272](https://doi.org/10.1123/1.4830822); [Manchester demonstration](https://personalpages.manchester.ac.uk/staff/richard.baker/WebDemonstrations/BinauralCues/ITDwoodworth.html)):

$$\boxed{\; \mathrm{ITD}(\theta) = \frac{a}{c}\left(\sin\theta + \theta\right), \quad |\theta|\le\tfrac{\pi}{2}; \qquad \mathrm{ITD} = \frac{a}{c}\left(\sin\theta + \pi - \theta\right),\ |\theta|>\tfrac{\pi}{2} \;}$$

**Maximum, verified:** for $a = 8.75$ cm, $c = 343$ m/s, $\theta = 90°$: $\mathrm{ITD} = (0.0875/343)(1 + \pi/2) = 6.55\times10^{-4}$ s ≈ **0.65 ms** — the standard "0.6–0.7 ms" figure ([Hartmann & Aaronson 2013](https://doi.org/10.1123/1.4830822), whose Fig. 5 shows the 654 µs peak; [Manchester demo](https://personalpages.manchester.ac.uk/staff/richard.baker/WebDemonstrations/BinauralCues/ITDwoodworth.html) quotes 0.65–0.70 ms). **Worked example, 60° azimuth:** $\sin 60° = 0.866$, $\theta = 1.047$ rad → $\mathrm{ITD} = (2.551\times10^{-4})(1.913) = 4.88\times10^{-4}$ s ≈ **0.49 ms**. Sensitivity to ITD is extraordinary: just-noticeable ITDs at low frequency are on the order of **10 µs** ([Heinz et al. 2019](https://pmc.ncbi.nlm.nih.gov/articles/PMC6524635/)).

Two caveats from the exact theory: Woodworth is a *high-frequency* ray approximation (valid when wavelength ≪ head radius; for a 87.5-mm head, ≳4 kHz), and the low-frequency diffraction limit is instead $\mathrm{ITD} \approx (3a/c)\sin\theta$; Woodworth underestimates the 500 Hz ITD by up to ~180 µs near 60° but beats the low-frequency formula above ~1.5 kHz ([Hartmann & Aaronson 2013](https://doi.org/10.1123/1.4830822)). The cusp at 90° in the textbook formula is an artifact of ignoring the longer creeping-wave path, which the extended model repairs.

### 7.2 Interaural level difference (ILD) and the duplex theory

Above ~1.5 kHz the head casts a real acoustic shadow: at 4–8 kHz, ILDs for lateral sources reach **~20 dB** (a 1.5 kHz tone at 55° azimuth shows a >8 dB ILD; the bright-spot diffraction anomaly makes ILD non-monotonic in azimuth near 55° — [Hartmann, Macaulay & Rakerd 2013, "The acoustical bright spot"](https://pmc.ncbi.nlm.nih.gov/articles/PMC2856510/)). **Lord Rayleigh's duplex theory (1907)** — ITD for low frequencies, ILD for high — holds with a known soft spot: pure-tone localization is *worst* between ~1.5 and 4 kHz (ITD fading, ILD not yet strong, plus the phase-ambiguity problem: at $f > 1/(2\,\mathrm{ITD}_{max}) \approx 770$ Hz a given interaural phase difference matches multiple cycles, which is why binaural phase use collapses by ~1.5 kHz) ([ISVR localization notes](https://resource.isvr.soton.ac.uk/audiology/Localisation/Information.htm); [auditory localization review 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11267622/)).

### 7.3 Cone of confusion and pinna spectral cues

For a spherical head, any position on a **cone about the interaural axis** shares the same ITD *and* ILD — azimuth α and 180°−α are indistinguishable, as are symmetric elevations ([auditory localization review 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11267622/); [cones-of-confusion study](https://pmc.ncbi.nlm.nih.gov/articles/PMC10624503/), noting front-back error rates can exceed 50% for low-passed noise). The disambiguating information is **monaural spectral**: the pinna's direction-dependent filtering imposes notches and peaks (e.g., the notch whose center frequency tracks elevation; an ~8 kHz peak biases perception "up", an ~7 kHz notch "down"; energy in 2–10 kHz correlates strongly with perceived elevation) ([Rajendran & Gamper 2019, JASA 145, EL298](https://doi.org/10.1121/1.5093641); [Hebrank & Wright 1974, via](https://doi.org/10.1121/1.5093641)). These cues are weak below ~4 kHz because the pinna is acoustically small there — hence more front-back confusions for low-passed sounds, resolving as high-frequency content is added ([Stevenson-Hoare et al. 2022](https://doi.org/10.1121/10.0014599)).

### 7.4 HRTFs

The **head-related transfer function** $H(f, \theta, \phi, d)$ is the full direction- and distance-dependent filter from free field to eardrum — the sum of everything in §1.1 plus §7.2–7.3, individually as unique as a fingerprint (they encode ITD via phase, ILD via magnitude, and pinna/torso spectral cues; [HRTF individualization review, arXiv 2003.06183](https://arxiv.org/pdf/2003.06183)). Virtual 3D over headphones = convolving an anechoic signal with the correct left/right pair of head-related impulse responses; loudspeaker playback additionally requires crosstalk cancellation. **Generic HRTFs often fail:** a VR study with 39 subjects found **0% front-back confusions with individualized HRTFs versus 27% with the MIT-KEMAR dummy-head set** (χ² = 22.0, p < .001), plus degraded externalization, timbre, and realism; "maximally deviant" non-individual HRTFs from a human database hit 24% confusions ([Masaniai et al. 2020, JMIR](https://pmc.ncbi.nlm.nih.gov/articles/PMC7509635/); classic: [Wenzel et al. 1993](https://doi.org/10.1121/1.407089)). Individualization routes: acoustic measurement, numerical simulation from 3D scans, anthropometry-based selection/scaling, and perceptual-feedback selection ([arXiv 2003.06183 review](https://arxiv.org/pdf/2003.06183)); public databases include MIT-KEMAR, CIPIC, ARI, and HUTUBS. Practical mitigations for generic HRTFs: head tracking (dynamic cues break the cone of confusion) and spectral enhancement filters (an 8 kHz peak / 7 kHz notch pair measurably biases elevation; [Rajendran & Gamper 2019](https://doi.org/10.1121/1.5093641)).

### 7.5 The precedence (Haas) effect

Within a short window after the first arrival, later-arriving correlated energy is not heard as a separate echo and barely affects the perceived location — the auditory system weights the **first wavefront**. Numbers: **summing localization below ~1 ms** (image between speakers); **localization dominance ~2–5 ms**; the effect robust through **~1–35 ms** where a lagging copy can be up to **~10 dB louder** than the lead without being heard as a separate event; echo threshold (perceiving two sounds) at **~50 ms for speech** and up to ~100 ms+ for music, dropping to a few ms for clicks ([Haas 1951, translated](https://www.effectrode.com/wp-content/uploads/2025/07/The_Influence_of_a_Single_Echo_on_the_Audibility_of_Speech_Helmut_Haas_1949.pdf); [Wallach 1949 / Litovsky et al. review](https://pmc.ncbi.nlm.nih.gov/articles/PMC4310855/); [Precedence effect, Wikipedia summary](https://en.wikipedia.org/wiki/Precedence_effect)). Fusion thresholds vary 2–100 ms across stimuli and subjects. Engineering consequences: PA delay alignment (delaying a fill speaker 10–25 ms keeps the audience localizing the stage source); game engines should render first reflections as filtered, delayed copies that the precedence effect automatically attributes to the direct source, which both saves the mix and produces correct externalization.

### 7.6 Distance perception

Humans are poor at *absolute* distance. The cues, in rough order of strength ([Zahorik 2002 ICAD review](http://www.icad.org/Proceedings/2002/Zahonk2002.pdf); [Zahorik 2002, JASA 111, 1832](https://doi.org/10.1121/1.1458027)):

1. **Intensity** (−6 dB per distance doubling in free field) — but confounded with source loudness; useless across unfamiliar sources.
2. **Direct-to-reverberant (D/R) energy ratio** — the most robust cue in rooms; the *only* cue that supports near-absolute judgments from a single presentation; yet discrimination thresholds are ~**5–6 dB**, corresponding to >2× distance changes ([Zahorik 2002, JASA 112, 2116](https://doi.org/10.1121/1.1506692)).
3. **Spectral** — HF air absorption (a few dB per 100 m; negligible at game scales) and distance-dependent HRTF changes.
4. **Near-field binaural** — ILD/ITD become distance-dependent within ~1 m (acoustic parallax), maximal on the interaural axis.

The universal bias is **underestimation** of far sources and overestimation of near ones (<~1 m), compressing perceived distance roughly as a power function with an "auditory horizon" — Zahorik's virtual-acoustics experiments showed consistent exponential underestimation with cue weights shifting between intensity and D/R depending on source type and azimuth ([Zahorik 2002](https://doi.org/10.1121/1.1458027)). For unfamiliar sounds, absolute distance is essentially unavailable (Coleman 1962: failure to localize distance of unfamiliar sounds; [Demirkaplan 2020](https://acta-acustica.edpsciences.org/articles/aacus/full_html/2020/06/aacus200041/aacus200041.html) shows even interpersonal voice familiarity shifts distance judgments non-linearly). Game consequence: teach distance through reverberation and level *ratios* the player can compare against a moving reference, not absolute level.

### 7.7 Minimum audible angle (MAA)

The smallest detectable change in sound direction: **~1° (Mills 1958; modern replications 1.1–1.2°) for broadband sources dead ahead**, growing steeply toward the interaural axis (~3.1–3.3° at 90°) and worse behind for oblique angles; ~1.7° directly behind ([Zhang et al. 2021, MAA with virtual synthesis](https://pmc.ncbi.nlm.nih.gov/articles/PMC8206507/); [Stevenson-Hoare et al. 2022](https://doi.org/10.1121/10.0014599); [localization review 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11267622/) quoting Aggius-Vella: 3° front / 5° rear). Absolute localization accuracy (pointing) is ~1–2° horizontal, 3–4° vertical in front, degrading to ~20° at ±90° azimuth and ~30° at high elevations ([review 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11267622/)). MAA is strongly frequency-dependent: Mills found it matched by ITD JNDs below ~1 kHz, by ILD JNDs above ~1.5 kHz in front, and became **"indeterminately large" for 1.5–2 kHz tones at azimuths > 45°** because of the bright-spot non-monotonic ILD ([Hartmann et al. 2013](https://pmc.ncbi.nlm.nih.gov/articles/PMC2856510/)). At low frequencies, large wavelengths make ITD/ILD tiny and MAA coarsens — a 100 Hz tone has essentially no instantaneous localization cue beyond gross ITD. For a 3D audio engine this sets the meaningful angular resolution of panning: finer than ~1° azimuth in front is wasted; lateral/back placement tolerances are several times looser.

---

## 8. Synthesis: What This Means for a Game Engine

A short index of the actionable consequences, each traceable to a section above:

- **Spectral budget:** the audible spectrum is ~24 Barks / ~42 ERBs wide (§5); a mixer that keeps simultaneous loud sources in distinct ERB neighborhoods (§6.1) exploits Bregman fusion (§6.4) instead of fighting masking.
- **The 2–5 kHz band is sacred:** ear-canal resonance makes it the sensitivity peak (§1.1, §4.1), the first casualty of hearing damage (§4.5), and the dialogue-intelligibility band; protect it in the mix, and beware the 1.5–4 kHz localization dead zone for pure tones (§7.2).
- **Loudness is compressive:** a 10× intensity change is only ~2× loudness (§4.2); fader moves below ~3 dB are nearly inaudible on single sources, while stacking three equal sources is +4.8 dB (≈ +43% loudness).
- **Temporal windows:** integration to ~200 ms, premasking to ~20 ms, postmasking to ~200 ms, precedence fusion to ~35 ms (§4.4, §6.2, §7.5) — the ear's own time constants define every audio fade, ducking release, and reverb pre-delay that can be made to "disappear."
- **3D audio:** ITD (≤0.65 ms) for < 1.5 kHz, ILD above, HRTF convolution for the rest, individualized if possible (27% vs 0% front-back confusions, §7.4), head tracking to break the cone of confusion (§7.3), and first-reflection rendering that the Haas window will attribute to the source (§7.5).
- **Distance is a ratio game:** render D/R ratio as the primary cue, accept systematic underestimation, and never expect absolute distance from level alone (§7.6).

The ear, in one sentence: a resonant horn feeding a hydraulic FFT whose active elements run at the edge of instability, sampled by 3,500 sensors whose timing precision is measured in microseconds — and every number in that sentence is sourced above.

---

# Part 5 — Real-Time Game Audio: Spatialization, Propagation, and Procedural Sound


**Provenance.** Drafted 2026-09-06 for the voxel-engine research repository (C++20, custom renderer, no audio system yet). Research performed with live web tools (`mcp__exa__web_search_exa` / `web_fetch`), 17 multi-query search calls covering ~60 queries and 60+ distinct sources; every project URL, license, formula, and GDC talk cited below was verified against a live page this session. Where public sources disagree (platform loudness targets) or where a requested claim could not be verified (e.g. a Zelda: BotW audio talk, a Battlefield procedural-footsteps talk, "Fleck"), the item was dropped rather than guessed. **Summary:** this survey covers the full interactive-audio pipeline — source → spatialization (pan laws, VBAP, Ambisonics, HRTF binaural) → distance/occlusion/diffraction modeling → reverb (parametric I3DL2/EAX-style, image-source early reflections, convolution and ray tracing) → Doppler → voice management, bus mixing, compression, and LUFS loudness — then surveys procedural-audio practice in shipped games, and closes with an engineering recommendation for a minimum-viable custom audio system plus an upgrade path, built on verified open-source components (miniaudio, OpenAL Soft, Steam Audio, PortAudio, SDL3).

---

## 1. The Game Audio Stack

### 1.1 The layered pipeline

Every real-time interactive audio system, from a hand-rolled mixer to AAA middleware, decomposes into the same layered signal path. A **sound source** — either a streamed or memory-resident sample asset, or a real-time synthesis graph — produces a mono or stereo signal. The signal is **spatialized**: panned onto the output layout (stereo pair, 5.1/7.1, binaural headphones) according to the source's direction relative to the listener, which requires the pan-law mathematics of §2. In parallel, the signal is **attenuated and filtered by distance and environment**: a distance-gain curve (§3.1), an optional frequency-dependent air-absorption low-pass (§3.2), and occlusion/transmission filtering when geometry blocks the direct path (§3.3). A **send** taps a copy of the signal into a **reverb bus** whose parameters describe the acoustic space (§4). The per-source results are summed into a hierarchy of **mixer buses** (SFX, music, dialogue, ambience, master), where bus-level effects, ducking, and limiters live. Finally the master bus feeds the platform's **output endpoint** (WASAPI/CoreAudio/ALSA), converted and clocked by the device at 48 kHz or 44.1 kHz in blocks of typically 128–1024 frames on a dedicated real-time thread.

The key architectural fact is that this chain runs on an **audio render thread** driven by the hardware clock, not the game frame loop. FMOD documents this explicitly: its Studio API "is built on a multithreaded processing model, in which API calls on a game thread try to be fast by only reading and writing shadow data or enqueuing commands to a buffer, while a separate Studio update thread triggered by the mixer asynchronously processes the API commands" ([FMOD Studio API Guide, §13.1](https://fmod.com/docs/2.03/api/studio-guide.html)). Game code posts commands; the mixer consumes them at block boundaries.

### 1.2 Middleware: FMOD and Wwise

**FMOD** splits into the Core API (channels, DSP, 3D attributes) and the Studio API (authoring + runtime data). Its authoring model is the **event**: "an event contains and is primarily composed of tracks, instruments, action sheets, and parameter sheets… the tracks route into other tracks, or into the event's master track," and "the output of the event's master track routes into the project mixer" ([FMOD Studio Concepts](https://www.fmod.com/docs/2.03/studio/fmod-studio-concepts.html)). **Parameters** are the game-facing controls: continuous game variables (distance, speed, RPM) that drive automation curves on volume, pitch, filters, and trigger conditions ([FMOD Authoring Events](https://fmod.com/docs/2.03/studio/authoring-events.html)). The **mixer** is a bus graph — group buses, return buses (sends), the master bus, plus **VCAs** (remote volume groups decoupled from routing, the standard way to build a settings-screen volume slider) and **snapshots** ("sets of values for bus properties that can be applied to your game's mix on the fly") ([FMOD Mixing](https://www.fmod.com/docs/2.03/studio/mixing.html)). Content ships in **banks** — the units of asset packaging and loading.

**Wwise (Audiokinetic)** mirrors this with different vocabulary: **Events** (game triggers with actions), the **Actor-Mixer hierarchy** (sound structure) and **Interactive Music hierarchy** (segments, playlists, switch containers), **SoundBanks** (packaging), and **RTPCs** — Real-Time Parameter Controls, the equivalent of FMOD parameters, drivable from any game variable and mappable to essentially any property of any object ("RTPCs can be created for all objects, busses, Effect and Attenuation instances, Switch Groups, and blend tracks", with the caveat that they "consume a significant amount of the platform's memory and CPU" if overused — [Wwise RTPC tips](https://www.audiokinetic.com/en/public-library/2023.1.14_8770/?id=rtpc_tips_best_practices)). The **Master-Mixer hierarchy** is the bus graph ([Working with busses](https://www.audiokinetic.com/en/public-library/2024.1.5_8803/?id=adding_busses_in_master_mixer_hierarchy)). Wwise also ships a full geometric-acoustics layer — Rooms, Portals, geometry, diffraction — covered in §3.3.

What each layer does mathematically is the subject of the rest of this document: the event layer selects and triggers content (discrete logic), the parameter layer is piecewise-linear curve evaluation, spatialization is a per-sample matrix multiply or convolution, distance is a scalar gain function, air absorption and occlusion are time-varying IIR filters, reverb is either a parametric feedback-delay network or a full convolution, and the bus layer is summing plus per-bus dynamics.

### 1.3 Engine-native systems

**Unreal Engine** has rebuilt its audio stack twice: the legacy Sound Cue graph, and since UE5, **MetaSounds** — "the modern audio rendering system… a DSP graph where each node represents a signal processor: oscillators, filters, envelopes, random streams, and trigger events," explicitly analogous to the Material Editor for shaders ([Unreal Audio overview](https://unrealdocs.win/pages/audio.html), [MetaSounds Reference Guide](https://dev.epicgames.com/documentation/unreal-engine/metasounds-reference-guide-in-unreal-engine?lang=en-US)). 3D behavior is factored out into **Sound Attenuation assets**: "Inner Radius — full volume within this distance. Attenuation Distance — falloff shape from Inner Radius to max distance. Falloff Function — Linear, Logarithmic, Inverse, Natural Sound. Spatialization — HRTF binaural panning for headphones… Occlusion — low-pass filter applied when geometry blocks line-of-sight" ([MetaSounds environmental/spatial tutorial](https://dev.epicgames.com/community/learning/tutorials/1bZK/unreal-engine-metasounds-module-environmental-spatial-sounds)). Reverb regions are **Audio Volume** actors ("apply reverb effects, set volumes, define the zones affected, emulate occlusion… and define the fade time between reverb settings" — [UE4.27 Audio Volume](https://docs.unrealengine.com/4.27/en-US/WorkingWithAudio/AudioVolume/)), and per-listener/submix reverb uses the submix effect chain with an I3DL2-style parameter set whose defaults are directly inspectable (e.g. `decay_time` 1.49 s, `density`/`diffusion` 0.85, `air_absorption_gain_hf` 0.994 — [SubmixEffectReverbSettings](http://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SubmixEffectReverbSettings?application_version=5.3)). UE also supports runtime **convolution reverb** from impulse-response assets ([Convolution Reverb docs](https://docs.unrealengine.com/4.27/en-US/WorkingWithAudio/ConvolutionReverb/)).

**Unity** exposes the same concepts per-component: `AudioSource.spatialBlend` morphs 2D↔3D ("0.0 makes the sound full 2D, 1.0 makes it full 3D" — [AudioSource.spatialBlend](https://docs.unity3d.com/ScriptReference/AudioSource-spatialBlend.html)); rolloff is Logarithmic / Linear / Custom-curve ([Audio Source reference](https://docs.unity3d.com/Manual/AudioSource-reference.html)); and notably Unity makes *every* 3D property a distance curve — Volume, Spatial Blend, Spread, Low-Pass cutoff, and Reverb Zone Mix are all editable as functions of distance in the curve editor, which is a very clean data model for attenuation.

**Valve's Source/Source 2** is the classic engine-native stack worth studying because it is script-driven rather than tool-driven: **soundscapes** are KeyValues scripts of `playlooping` and `playrandom` operators with per-sound volume/pitch/soundlevel, positioned at one of eight target locations, and can force a **soundmixer** ("Soundmixers manage the priority and volume of groups of sounds") and a DSP preset ([Valve Developer Community: Soundscape](https://developer.valvesoftware.com/wiki/Soundscape)). Soundscapes activate via `env_soundscape` entities when the player is within radius **and has line-of-sight** — an early, widely-copied LOS-gated ambience/zone system ([env_soundscape](https://developer.valvesoftware.com/wiki/Env_soundscape)). The engine's per-frame audio update threads listener state "down the sound event tree to the spatializers," and notably Valve's own tooling warns that "everything time-based in there (occlusion smoothing, doppler) has to run off the real elapsed time rather than per-update steps, or it changes character with the frame rate" ([Source 2 audio internals, ValveResourceFormat docs](https://s2v.app/ValveResourceFormat/api/ValveResourceFormat.Renderer.Audio.html)) — a lesson for any custom engine: run smoothing on wall-clock audio time, not frame counts.

---

## 2. Spatialization Mathematics

### 2.1 Stereo pan laws

For a mono source panned between two loudspeakers with a single parameter, the two families in use are **linear** panning ($L = 1-a,\ R = a$) and **constant-power** panning. Constant power parameterizes the pan as an angle $\theta \in [0°, 90°]$:

$$\boxed{\;L = \cos\theta, \qquad R = \sin\theta \;\Longrightarrow\; L^2 + R^2 = \cos^2\theta + \sin^2\theta = 1\;}$$

The derivation of *why* this is "constant power" is the identity itself: total radiated acoustic power (energy, not amplitude) is proportional to $L^2 + R^2$ under incoherent summation at the listener, so the loudness of the phantom image is invariant as the source sweeps across the pan. Linear panning instead holds $L + R = 1$ (amplitude), which makes the center image $L = R = 0.5$, i.e. $-6.02$ dB per side and a total power of $0.5$ — a **−3 dB center dip** relative to a hard-panned edge. Constant-power's center is $L = R = 0.707$ ($-3.01$ dB per side, power $=1$), eliminating the dip. Real consoles and engines pick points in between (a common compromise scales the center to $\approx -4.5$ dB/side), because a slight center attenuation compensates for the fact that two loudspeakers summing coherently near the listener can produce a center image *louder* than either edge in some rooms. The classic stereo localization law governing where the image actually appears is the **tangent law**, $\tan\theta_{\text{image}} = \dfrac{g_R - g_L}{g_L + g_R}\tan\theta_0$, with $\theta_0$ the half loudspeaker angle; Pulkki derives and discusses it alongside the amplitude-panning formulation in the VBAP paper ([Pulkki 1997](https://ccrma.stanford.edu/courses/tu/space2008/topics/amplitude_panning/materials/vbap.pdf)), and loudspeaker-playback perception is treated in depth in Zotter & Frank ch. 2 ([open access](https://link.springer.com/chapter/10.1007/978-3-030-17207-7_2)).

### 2.2 Vector Base Amplitude Panning (VBAP)

VBAP (Pulkki, JAES 45(6):456–466, 1997; [AES e-library](https://aes2.org/publications/elibrary-page/?id=7853), [free PDF via Aalto/HUT](http://lib.tkk.fi/Diss/2001/isbn9512255324/article1.pdf)) generalizes constant-power panning to *any* number of loudspeakers at *any* positions, in 2D (pairs) or 3D (triplets). Each loudspeaker $l_i$ is a unit vector from the listener; for the pair/triplet whose convex hull contains the source direction $\mathbf{p}$ (unit vector), form the matrix $\mathbf{L} = [\mathbf{l}_1\ \mathbf{l}_2\ (\mathbf{l}_3)]$. The un-normalized gains solve $\mathbf{p} = \mathbf{L}\mathbf{g}$:

$$\boxed{\;\mathbf{g}^{\,T} = \mathbf{p}^{\,T}\,\mathbf{L}^{-1}, \qquad \mathbf{g}_{\text{scaled}} = \frac{\mathbf{g}}{\sqrt{g_1^2 + g_2^2 + g_3^2}}\;}$$

The scaling (quoted verbatim from the paper) is exactly the constant-power normalization of §2.1 lifted to $n$ speakers: $\|\mathbf{g}_{\text{scaled}}\|_2 = 1$. Pulkki's stated properties: the virtual source is "as sharp as is possible with current loudspeaker configuration and amplitude panning methods"; gains are nonnegative within the active region and go smoothly to zero at its edges (no clicks when the source hands off between triangles); and for an *orthogonal* loudspeaker layout, first-order VBAP gains coincide with the absolute values of first-order Ambisonics gains — the two systems are the same object in different coordinates.

**Worked example — source at 15° on a ±30° stereo pair.** $\mathbf{p} = (\cos 15°, \sin 15°) = (0.9659,\ 0.2588)$. Loudspeaker vectors: $\mathbf{l}_1 = (0.866,\ -0.5)$, $\mathbf{l}_2 = (0.866,\ 0.5)$, so

$$\mathbf{L} = \begin{bmatrix} 0.866 & 0.866 \\ -0.5 & 0.5 \end{bmatrix},\qquad \mathbf{L}^{-1} = \frac{1}{0.866}\begin{bmatrix} 0.5 & -0.866 \\ 0.5 & 0.866 \end{bmatrix}$$

$$\mathbf{g}^T = \mathbf{p}^T \mathbf{L}^{-1} = (0.9659 \cdot 0.5774 - 0.2588,\;\; 0.9659 \cdot 0.5774 + 0.2588) = (0.2989,\; 0.8165)$$

Normalization: $\|\mathbf{g}\| = \sqrt{0.0893 + 0.6667} = 0.8695$, giving final gains $g_L = 0.344,\ g_R = 0.939$. Sanity checks: (a) the vector sum $\sum g_i\mathbf{l}_i$ points at exactly 15° azimuth — VBAP preserves direction; (b) $g_L^2 + g_R^2 = 1$ — constant power; (c) the tangent law holds exactly: $\frac{0.939-0.344}{0.939+0.344}\tan 30° = 0.268 = \tan 15°$. And at a speaker's own direction ($\theta = 30°$) the algebra collapses to $\mathbf{g} = (0,1)$ — hard pan, as it must.

For 3D layouts, loudspeaker triples are precomputed into a triangulation (typically a spherical Delaunay/convex-hull triangulation); at runtime the active triangle is found by a point-location query and the 3×3 $\mathbf{L}^{-1}$ (precomputed per triangle) is applied — a 3-dot-product cost per source per block. The one audible weakness of plain VBAP is that a source's *apparent width* grows when it sits near a loudspeaker (only one speaker active ⇒ maximal sharpness) versus inside a triangle edge — which motivates Ambisonics' energy-controlled formulations and the "spread" parameters found in Wwise/Unity.

### 2.3 Ambisonics

Ambisonics represents the *sound field* rather than speaker feeds: the signal is decomposed into spherical harmonics up to order $N$, requiring $(N+1)^2$ channels (4 for first order, 9 for second, 16 for third — the interchange standard literally enumerates "channels 0 to 15 correspond respectively to W YZX VTRSU QOMKLNP" for third order, [Chapman et al., IEM Ambisonics Symposium 2009](https://ambisonics.iem.at/symposium2009/proceedings/ambisym09-chapman-etal-ambiinterchangeformat.pdf/@@download/file/AmbiSym09_Chapman_etal_AmbiInterchangeFormat.pdf)). First-order **B-format** is four channels: $W$ (omnidirectional pressure) and $X, Y, Z$ (figure-of-eight dipoles along the Cartesian axes) — "a signal W corresponding to an omnidirectional pickup pattern, and three signals (X, Y, and Z) corresponding to figure-of-eight pickup patterns aligned with the Cartesian coordinate axes" ([Zotter & Frank, *Ambisonics: A Practical 3D Audio Theory…*, ch. 1, open access](https://link.springer.com/chapter/10.1007/978-3-030-17207-7_1); the whole book is [free online](https://link.springer.com/book/10.1007/978-3-030-17207-7)).

For a plane wave arriving from azimuth $\theta$ (measured counter-clockwise from front) and elevation $\varphi$, with direction unit vector $\mathbf{d} = (\cos\varphi\cos\theta,\ \cos\varphi\sin\theta,\ \sin\varphi)$, the first-order encoding in the modern **AmbiX** convention (ACN channel order, SN3D normalization — "the current standard, which most plug-in suites use", [IEM Plug-in Suite compatibility guide](https://plugins.iem.at/docs/compatibility/)) is:

$$\boxed{\;\begin{aligned}
W &= 1 \\
Y &= \cos\varphi \sin\theta \\
Z &= \sin\varphi \\
X &= \cos\varphi \cos\theta
\end{aligned}
\qquad\text{(SN3D/ACN: } \mathbf{ch} = [\,W,\ Y,\ Z,\ X\,]\text{)}\;}$$

The normalization conventions differ only in per-order scale factors and are the classic interoperability trap. **SN3D** ("semi-normalization") leaves the first-order dipoles at unit scale, with the documented property that "when you encode a source, the levels of all channels won't exceed the one in the first (omni, W) channel" ([IEM compatibility guide](https://plugins.iem.at/docs/compatibility/)). **N3D** multiplies each order-$n$ block by $\sqrt{2n+1}$ (first order by $\sqrt 3$); the interchange standard chose N3D because "the math only works in N3D… beamforming or decoding will happen in N3D" ([IEM guide](https://plugins.iem.at/docs/compatibility/), [Chapman et al. 2009](https://ambisonics.iem.at/symposium2009/proceedings/ambisym09-chapman-etal-ambiinterchangeformat.pdf/@@download/file/AmbiSym09_Chapman_etal_AmbiInterchangeFormat.pdf)). **FuMa** (the historical Furse–Malham "B-format") scales $W$ by $1/\sqrt2 \approx 0.707$ — Pulkki's encoding equations literally list $g_w = 0.707$ ([VBAP paper](https://ccrma.stanford.edu/courses/tu/space2008/topics/amplitude_panning/materials/vbap.pdf)) — a choice the standards literature documents as "a historical artifact… added to improve the utilization of the dynamic range of recording media, based on the observation that the typical signal levels in the W channel are several dB higher than in X, Y or Z" (Benjamin et al., quoted in [Chapman et al. 2009](https://ambisonics.iem.at/symposium2009/proceedings/ambisym09-chapman-etal-ambiinterchangeformat.pdf/@@download/file/AmbiSym09_Chapman_etal_AmbiInterchangeFormat.pdf)). Concretely: SN3D→N3D multiplies $X,Y,Z$ by $\sqrt3$; FuMa→SN3D multiplies $W$ by $\sqrt2$.

**Decoding.** A basic "sampling" decoder for first order evaluates each channel's directivity pattern at each loudspeaker's direction: $S_l = \tfrac{1}{2}\,[\,1\ \ \boldsymbol{\theta}_l^{\,T}\,]\,[\,W\ X\ Y\ Z\,]^T$ ([Zotter & Frank ch. 1](https://link.springer.com/chapter/10.1007/978-3-030-17207-7_1)) — i.e. each loudspeaker is fed a (scaled) *cardioid*, $(1+\cos\theta_d)$, aimed at itself. Better decoders weight the orders. Because this is widely mis-stated, it is worth deriving rather than quoting: for a first-order decoder with per-loudspeaker pattern $D(\theta_d) = a_0 + a_1\cos\theta_d$ (native/SN3D encoding), the **energy vector** magnitude of the decoded image over a uniform loudsphere works out to

$$|r_E| = \frac{2t}{3+t^2}, \qquad t = \frac{a_1}{a_0},$$

(using $\sum\mathbf{l}=0$, $\sum(\mathbf{l}\!\cdot\!\mathbf{d})\mathbf{l} = \tfrac{L}{3}\mathbf{d}$, and vanishing third moments). Maximizing over $t$ gives $t=\sqrt3$, $|r_E| = 1/\sqrt3 \approx 0.577$ — the first-order **max-rE** solution, whose pattern $\propto 1+\sqrt3\cos\theta_d$ is a hypercardioid with a small rear lobe. The cardioid pattern $\propto(1+\cos\theta_d)$ the literature associates with "max-rE"-flavored weighting is actually the *basic / in-phase-optimal* first-order case ($t=1$, $|r_E| = 0.5$, and $a_1 \le a_0$ is precisely the condition that keeps every loudspeaker gain non-negative for every source direction — the "in-phase" property). In N3D normalization these become equal weights $(1,1,1,1)$ for max-rE versus $(1,\tfrac{1}{\sqrt3},\tfrac{1}{\sqrt3},\tfrac{1}{\sqrt3})$ for basic. General-order max-rE/in-phase weightings are standard decoder options — the IEM tooling exposes exactly `Weights: none, inPhase or maxrE` ([IEM configuration-file docs](https://plugins.iem.at/docs/configurationfiles/)) — and the practical effect is directional sharpness (max-rE) versus artifact-free robustness off-center (in-phase). Higher order sharpens localization because the usable spherical-harmonic bandwidth grows: order $N$ resolves direction to roughly $180°/(N+1)$, which is why Steam Audio encodes its reverb in higher-order Ambisonics and why the IEM suite ships "Ambisonic plug-ins up to 7th order" (64 channels) ([plugins.iem.at](https://plugins.iem.at/)).

The strategic property for an engine: Ambisonics is a **renderer-independent intermediate format** — encode once (sources, reverb, ambiences), then decode to whatever the player has (stereo, 5.1, binaural) with one matrix or one per-channel convolution. A rotation of the listener is a sparse linear transform of the channels, cheap and click-free.

### 2.4 HRTF-based binaural rendering

For headphones, the gold standard is convolution with **head-related transfer functions**: $H(f, \theta, \varphi, \text{ear})$, the frequency-domain pair of transfer functions from a free-field source at direction $(\theta,\varphi)$ to each eardrum, in practice stored as measured **HRIRs** (head-related impulse responses). Binaural synthesis is simply

$$y_L[n] = (s * h_L^{\theta,\varphi})[n], \qquad y_R[n] = (s * h_R^{\theta,\varphi})[n]$$

with $h$ typically 128–512 taps at 48 kHz — a per-source cost of two FIR convolutions (partitioned-FFT or direct) that modern CPUs absorb for dozens of voices. The CIPIC database measured HRIRs for 45 subjects at "25 different azimuths and 50 different elevations (1250 directions) at approximately 5° angular increments," with 200-sample (4.5 ms) HRIRs at 44.1 kHz, plus 27 anthropometric measurements per subject for individualization research ([Algazi, Duda, Thompson, Avendaño, *The CIPIC HRTF Database*, IEEE WASPAA 2001](https://www.ece.ucdavis.edu/cipic/wp-content/uploads/sites/12/2015/04/cipic_WASSAP_2001_143.pdf), [DOI](https://doi.org/10.1109/aspaa.2001.969552); data at [interface.cipic.ucdavis.edu](http://interface.cipic.ucdavis.edu)). The IRCAM **LISTEN** database provides 187 directions per subject at 44.1 kHz/24-bit, 8192-point HRIRs, in raw and diffuse-field-compensated variants ([LISTEN download page](http://recherche.ircam.fr/equipes/salles/listen/download.html)). The MIT-KEMAR mannequin set (Gardner & Martin 1994) rounds out the classics; NYU's MARL repository repackages LISTEN, CIPIC, FIU and MIT-KEMAR in one format ([MARL HRIR repository](https://steinhardt.nyu.edu/marl/research/resources/head-related-impulse-responses-repository)).

Why generic HRTFs fail some listeners is documented directly in the CIPIC paper: "HRTFs vary significantly from person to person, and… serious perceptual distortions (particularly front/back confusion and elevation errors) can occur when one listens to sounds spatialized with a non-individualized HRTF." Front/back disambiguation and elevation perception live in the high-frequency pinna notches (5–10 kHz+) that are the most individual part of the response. Practical engines therefore (a) ship a small set of HRTFs and let the user pick, (b) interpolate across the measurement grid as the source moves — linear/bilinear interpolation of adjacent HRIRs (or magnitude-interpolation with common phase) on a ~5° grid keeps artifacts small, which is exactly why CIPIC's dense grid exists — and (c) cross-fade rather than switch. Steam Audio applies "HRTF-based binaural rendering to the Audio Source, using default settings" per source with spatialization quality controls ([Steam Audio guide](https://valvesoftware.github.io/steam-audio/doc/unity/guide.html)), and can even render its *reverb* binaurally ("Apply HRTF… improvement in spatialization quality when using convolution or hybrid reverb, at the cost of slightly increased CPU usage" — [Steam Audio Reverb](https://valvesoftware.github.io/steam-audio/doc/unreal/reverb.html)).

### 2.5 Object-based, channel-based, scene-based

**Channel-based** audio stores speaker feeds (stereo, 5.1, 7.1.4): the mix is finalized at authoring time to one layout and downmixed for others. **Object-based** audio stores a mono (or stereo) signal *plus positional metadata* — "each object is an audio signal plus" position/size metadata — and defers rendering to playback: Dolby Atmos masters carry up to 128 total channels of which up to 118 may be objects ("a maximum of 118 objects" alongside static beds, per the [Dolby Atmos master ADM profile](https://developer.dolby.com/globalassets/documentation/technology/dolby_atmos_master_adm_profile_v1.0.pdf); the Renderer "supports up to 118 mono objects, or a combination of mono and stereo objects totaling up to 118 object channel paths" — [Atmos Renderer guide](https://professional.dolby.com/siteassets/content-creation/dolby-atmos/dolby_atmos_renderer_guide.pdf)). **Scene-based** audio is Ambisonics: the field itself is the format, layout-agnostic by construction. **MPEG-H 3D Audio** (ISO/IEC 23008-3) is the unifying standard — its bitstream carries channel-based, object-based (with a dedicated object-metadata syntax) and HOA scene payloads, with a renderer that adapts to "home theatre setups with 3D loudspeaker configurations, 22.2 loudspeaker systems, automotive entertainment systems and playback over headphones" ([MPEG-H 3D Audio page](https://www.mpeg.org/standards/MPEG-H/3/), [ISO/IEC 23008-3](https://www.iso.org/standard/90199.html)). A game engine's runtime scene is naturally object-based (emitter + transform), with Ambisonics as the natural intermediate scene representation — exactly the MPEG-H triad.

---

## 3. Distance and Environmental Modeling

### 3.1 Distance attenuation models

The OpenAL 1.1 specification is the canonical, freely-available statement of the standard models ([spec PDF](https://www.openal.org/documentation/openal-1.1-specification.pdf), §3.4). The default **inverse-clamped** model (explicitly "the IASIG I3DL2 model") is:

$$\text{gain} = \frac{R_{\text{ref}}}{R_{\text{ref}} + f_{\text{rolloff}}\,(d - R_{\text{ref}})}\qquad \text{with } d \leftarrow \max(d, R_{\text{ref}}),\ d \leftarrow \min(d, d_{\max})$$

With $f_{\text{rolloff}} = 1$ this is $\approx 1/d$ beyond the reference distance — the physically-motivated far-field behavior of a point source, −6.02 dB per distance doubling: a source at $d = 10\,\mathrm{m}$ with $R_{\text{ref}} = 1$ has gain $0.1$ (−20 dB); at 20 m, $0.053$ (−25.5 dB) — the −6 dB/dd asymptote. The spec also defines **linear** ("a linear drop-off in gain… the linear models are not physically realistic, but do allow full attenuation of a source beyond a specified distance") and **exponent** variants, and notes OpenAL deliberately performs **no distance culling**, leaving it to the application precisely because "rule based culling inevitably introduces acoustic artifacts… there will be popping artifacts in the absence of hysteresis" — a directly applicable design warning. FMOD's Core API mirrors the set — `FMOD_3D_INVERSEROLLOFF` (default), `FMOD_3D_LINEARROLLOFF`, `FMOD_3D_LINEARSQUAREROLLOFF`, `FMOD_3D_INVERSETAPEREDROLLOFF` (inverse near, linear-square far, "approximates realistic behavior while still guaranteeing the sound attenuates to silence at maxdistance"), plus **custom rolloff** as a piecewise-linear curve of $(distance, volume)$ points or a full callback ([FMOD Spatializing Sounds, §5.0.3](https://fmod.com/docs/2.03/api/spatializing-sounds-in-the-core-api.html), [`set3DCustomRolloff`](https://www.fmod.com/docs/2.03/api/core-api-channelcontrol.html)). Unity offers Logarithmic / Linear / Custom curves ([AudioSource reference](https://docs.unity3d.com/Manual/AudioSource-reference.html)); Unreal's attenuation falloff functions are Linear / Logarithmic / Inverse / Natural ([UE tutorial](https://dev.epicgames.com/community/learning/tutorials/1bZK/unreal-engine-metasounds-module-environmental-spatial-sounds)). The min/reference distance doubles as the authoring control for *apparent source size*: "a sound with a small 3D minimum distance… will appear small, and the sound will attenuate quickly. A sound with a large minimum distance will appear larger" ([FMOD docs](https://fmod.com/docs/2.03/api/spatializing-sounds-in-the-core-api.html)).

### 3.2 Air absorption

Real air attenuates high frequencies by viscous/relaxation losses — on the order of "<0.2 dB/m for 8 kHz in normal conditions," so that "the sound must travel distances >15 m for a listener to detect changes in the sound spectrum" ([Genard et al., Frontiers in Psychology 2017](https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2017.00969/full), citing Ingard and Blauert). At 100 m that is ~20 dB of HF loss — a strong distance cue long before the level cue saturates. The standard runtime approximation is a **distance-driven low-pass**: one biquad per source whose cutoff falls with distance (Unity literally exposes "Low-Pass — cutoff frequency (22000.0 to 10.0) over distance" as a curve, [AudioSource reference](https://docs.unity3d.com/Manual/AudioSource-reference.html); Unreal has an "Air Absorption" toggle in attenuation settings, "to simulate high-frequency roll-off over distance" ([UE tutorial](https://dev.epicgames.com/community/learning/tutorials/1bZK/unreal-engine-metasounds-module-environmental-spatial-sounds))). The I3DL2/EFX parameterization of this is a **HF reference frequency plus a gain/ratio**: OpenAL's Effects Extension exposes `AL_AIR_ABSORPTION_FACTOR` per source (0–10) scaling the per-distance HF attenuation, with reverb-side `HFReference` defaulting to 5 kHz in OpenAL Soft's implementation ([Effects Extension Guide](http://zhang.su/seal/EffectsExtensionGuide.pdf); defaults visible in [openal-soft `al/effects/reverb.cpp`](https://github.com/kcat/openal-soft/blob/993b8c82/al/effects/reverb.cpp)); XAudio2's I3DL2 structure carries the same `HFReference` field ([Microsoft docs](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2fx/ns-xaudio2fx-xaudio2fx_reverb_i3dl2_parameters)).

### 3.3 Occlusion, obstruction, transmission, and diffraction

**Occlusion** (geometry blocks the path) versus **obstruction** (path is clear but a nearby edge/obstacle filters it) is the classic EAX-era distinction. The modern baseline is a raycast from listener to source driving a dB loss plus a low-pass — Unreal's attenuation literally describes occlusion as "low-pass filter applied when geometry blocks line-of-sight" ([UE docs](https://unrealdocs.win/pages/audio.html)). Steam Audio generalizes it three ways, all documented: **raycast occlusion** ("trace a single ray from the listener to the source. If the ray is occluded, the source is considered occluded") versus **volumetric occlusion** ("trace multiple rays… based on the Occlusion Radius setting. The proportion of rays that are occluded determine how much of the direct sound is considered occluded" — a partial-occlusion model for extended sources that transitions smoothly); **transmission** through the blocker ("ray tracing will be used to determine how much of the sound is transmitted through occluding scene geometry", with per-material transmission); and **pathing/diffraction** — "shortest paths taken by sound as it propagates from the source to the listener will be simulated," with baked probe-to-probe paths, real-time re-validation against dynamic geometry, and "Find Alternate Paths: if a baked path… is found to be occluded by dynamic geometry, alternate paths are searched for in real-time" ([Steam Audio Source, Unreal integration](https://valvesoftware.github.io/steam-audio/doc/unreal/source.html)). This is the shipped, practical form of diffraction: the sound keeps a virtual position at the diffracting edge, attenuated and filtered by the path's bend.

The physics underneath is **Keller's Geometrical Theory of Diffraction** (1962): "an extension of geometrical optics which accounts for diffraction. It introduces diffracted rays in addition to the usual rays… produced by incident rays which hit edges, corners, or vertices of boundary surfaces," with diffraction coefficients determined from canonical problems ([Keller, JOSA 52(2):116–130](https://doi.org/10.1364/josa.52.000116), [abstract](https://opg.optica.org/abstract.cfm?uri=josa-52-2-116)). Engines do not solve GTD; they adopt its ray picture — sound bends around edges along detour paths with edge-dependent loss — and approximate the coefficients empirically.

**Per-material transmission loss** comes from architectural acoustics, where wall isolation is rated by **STC** (Sound Transmission Class; per ASTM E90/E413, the rating equals the fitted contour's transmission loss at 500 Hz — [NCMA TEK](https://www.lampus.com/files/Resources/NCMA-TEK-13-01C.pdf)). Verified ranges: a single ½″ drywall stud wall is ~20–25 dB STL and standard single-stud drywall assemblies (one layer each side) measure STC 32–42, rising to 38–46 with double drywall; staggered-stud walls 42–49; double-stud walls 51–59 and up to 64–69 ([NRMCA compilation, Tables 2/6](https://www.nrmca.org/wp-content/uploads/2022/07/Compilation_of_Acoustic_Information_for_Concrete_Construction.pdf); [Commercial Acoustics STL table](https://commercial-acoustics.com/guides/sound-transmission-loss-stl-rating-101/)). Concrete: 6″/8″ cast concrete walls measured STC 57/58 bare, up to 63 with furring/insulation/drywall finishes (PCA tests), lightweight 8″ CMU around 44, grouted higher ([Portland PCA report](https://www.portlandoregon.gov/bds/appeals/index.cfm?action=getfile&appeal_id=24673&file_id=31063); [NRMCA](https://www.nrmca.org/wp-content/uploads/2022/07/Compilation_of_Acoustic_Information_for_Concrete_Construction.pdf)). Building codes require STC ≥ 45–50 between dwellings (IBC). The voxel-engine translation: a per-voxel-material transmission coefficient in dB, applied per unit thickness along the occlusion ray, plus a low-pass — drywall ≈ −30 to −40 dB and strong HF loss, stone/concrete ≈ −50 to −60 dB, wood ≈ −20 dB; a "game-realistic" tuning usually compresses these ranges so one wall is audible but muffled.

### 3.4 Portal-based and precomputed propagation

The cheap, robust answer to "sound in the next room" is **topological**: sound propagates through a graph of rooms connected by portals, not through Euclidean distance. This lineage runs from the sector-connectivity sound propagation of the DOOM/Quake family through Source's LOS-gated `env_soundscape` zones (§1.3) to today's middleware. **Wwise's Rooms and Portals** is the fully documented modern statement: "Rooms… have settings… the most important Room setting is `AkRoomParams::ReverbAuxBus`, which tells Wwise Acoustics to which auxiliary bus emitters should send when they are in that Room"; "Portals represent openings between two Rooms… Portal size is given by `AkPortalParams::Extent`. Width and height are used by Wwise Acoustics to compute diffraction and spread, while depth defines a region in which Wwise performs a smooth transition between the two connected Rooms by carefully manipulating the auxiliary send levels, Room object placement, and Spread" ([Rooms and Portals API Configuration](https://www.audiokinetic.com/en/public-library/2025.1.9_9197/?id=spatial_audio_roomsportals_apiconfig.html)). Emitters in another room are re-virtualized at the portal position with diffraction loss around the portal edge — exactly Keller's edge ray, discretized.

Precomputed **propagation fields** push the whole computation offline. Microsoft's **Project Triton** ("Gears of War 4", with The Coalition) "robustly models complex wave phenomena such as diffraction, scattered reflections and reverberation on static 3D level geometry… smooth occlusion around obstacles or longer reverberation in large halls, as well as how these effects change when source and listener move" — a precomputed wave-acoustics field queried at runtime, integrated in UE4 and Wwise ([GDC Vault: *'Gears of War 4', Project Triton: Pre-Computed Environmental Wave Acoustics*](https://www.gdcvault.com/play/1024008/-Gears-of-War-4)). Related verified talks: [*Sound Propagation in Hitman*](https://www.gdcvault.com/play/1022774/Sound-Propagation-in) ("close to the listener but located in a different space or behind obstacles. Regular distance based attenuation cannot handle these cases… supports both open and closed spaces as well as dynamic geometry"), [*Real-time Sound Propagation in Video Games*](https://www.gdcvault.com/play/1016001/Real-time-Sound-Propagation-in) (Ubisoft's lightweight obstruction/occlusion/diffraction with virtual source repositioning), [*Geometric Modeling of Sound Propagation*](https://www.gdcvault.com/play/1022671/Geometric-Modeling-of-Sound), [*Audio Propagation Through the Ears of VERA*](https://www.gdcvault.com/play/1025325/Audio-Propagation-Through-the-Ears) (Microsoft's voxel acoustics engine — "occlusion, obstruction, early reflections, portals, and more, all for virtually unlimited objects and in real-time"; directly relevant precedent for a *voxel* engine), and [*GPU Raytracing for Audio in Snowdrop*](https://www.gdcvault.com/play/1035485/GPU-Raytracing-for-Audio-in) (Ubisoft, shipped in *Avatar: Frontiers of Pandora* and *Star Wars: Outlaws*).

---

## 4. Reverb in Real-Time Engines

### 4.1 Parametric reverb: the I3DL2/EAX/EFX parameter set

The lingua franca of game reverb is the **I3DL2** (Interactive Audio Special Interest Group, "Interactive 3D Audio Rendering Guidelines Level 2") listener-effect parameter set, which descended into EAX, then into OpenAL's EFX, and is still recognizable in Unreal's submix reverb and XAudio2 today ([Microsoft: XAudio2 I3DL2 parameters](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2fx/ns-xaudio2fx-xaudio2fx_reverb_i3dl2_parameters); [DirectSound environmental reverberation](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417547(v=vs.85))). The canonical fields, with the XAudio2 documented ranges:

| Parameter | Meaning | Range (XAudio2/I3DL2) |
|---|---|---|
| `Room` | room effect attenuation | −10000..0 (hundredths of dB) |
| `RoomHF` | room HF attenuation | −10000..0 |
| `RoomRolloffFactor` | rolloff of reflected sound | 0.0..10.0 |
| `DecayTime` | RT60 at low frequencies | 0.1..20.0 s |
| `DecayHFRatio` | HF/LF decay ratio | 0.1..2.0 |
| `Reflections` | early-reflection gain rel. Room | −10000..1000 |
| `ReflectionsDelay` | first-reflection delay | 0.0..0.3 s |
| `Reverb` | late-reverb gain rel. Room | −10000..2000 |
| `ReverbDelay` | late-vs-early delay | 0.0..0.1 s |
| `Diffusion` | echo density of the tail | 0..100 % |
| `Density` | modal density | 0..100 % |
| `HFReference` | reference frequency for HF ratios | 20..20000 Hz |

([source table](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2fx/ns-xaudio2fx-xaudio2fx_reverb_i3dl2_parameters)). OpenAL EFX's `AL_EFFECT_EAXREVERB` extends this with `GainLF`/`DecayLFRatio`, per-band `ReflectionsPan`/`LateReverbPan`, echo and modulation blocks, `AirAbsorptionGainHF`, and `DecayHFLimit` ([Effects Extension Guide, Appendix 1](http://zhang.su/seal/EffectsExtensionGuide.pdf); the full parameter list as implemented in [openal-soft's reverb example](https://github.com/kcat/openal-soft/blob/993b8c82/examples/alreverb.c)). Typical implementations are a **feedback delay network** (FDN) or comb/allpass bank whose delays are set from room size, whose damping filters realize `DecayHFRatio`, and whose input taps realize `ReflectionsDelay`/`ReverbDelay` — Schröeder's architecture with two-band damping. The value of the standard is *interchange*: presets (Generic, Alley, Underwater, …) transfer across every EFX-compatible engine.

### 4.2 Reverb zones and the direct/reverberant ratio

Engines place reverb in the world as **zones**. Unreal: Audio Volume actors with a Reverb Effect asset, Volume, and "Fade Time — the time (in seconds) it takes to interpolate from the current reverb settings to the new ones as you enter and leave audio volumes" ([UE4.27 Audio Volume](https://docs.unrealengine.com/4.27/en-US/WorkingWithAudio/AudioVolume/)), plus the newer component-driven Audio Gameplay Volumes ([quick start](https://dev.epicgames.com/documentation/en-unreal-engine/audio-gameplay-volumes-quick-start)). Wwise: reverb-per-Room via an auxiliary bus (§3.4). Unity: Reverb Zones with a per-source distance curve ("Reverb Zone Mix… the volume property and distance and directional attenuation are applied to the signal first," [AudioSource reference](https://docs.unity3d.com/Manual/AudioSource-reference.html)). Steam Audio: reverb *simulated* rather than authored — ray-traced against scene geometry with per-material absorption (§4.3).

The psychoacoustic reason a distance-driven send matters: past a few meters, level alone stops disambiguating distance, and the **direct-to-reverberant energy ratio (DRR)** becomes the dominant absolute distance cue — "in a typical listening room, the direct sound field energy decays proportionally to (logarithmic) distance, while the reverberant sound field has approximately equal energy irrespective of distance. Thus, D/R can, in principle, be used to estimate the distance of a sound source" ([Larsen, Iyer, Lansing, Feng, JASA 2008](https://doi.org/10.1121/1.2936368); see also [Zahorik, *Direct-to-reverberant energy ratio sensitivity*, JASA 112(5) 2002](https://doi.org/10.1121/1.1506692) and [Bronkhorst & Zahorik's overview](https://doi.org/10.1121/1.4809156)). Quantitatively, DRR falls ~6 dB per distance doubling beyond the room's critical distance, and measured DRR JNDs are ~2–6 dB depending on DRR region ([Zahorik 2002](https://doi.org/10.1121/1.1506692), [Larsen et al. 2008](https://doi.org/10.1121/1.2936368)) — which is also why distance perception compresses for far sources (the "auditory horizon"). An engine that keeps reverb send constant while attenuating direct sound gets distance for free; Meta's Unreal plugin even exposes this as a control ("Reverb Reach… A value of 0 causes reverb to attenuate with the direct sound (constant direct-to-reverberant ratio). A value of 1 increases reverb level linearly with distance," [Meta XR Audio spatialization docs](https://developers.meta.com/horizon/documentation/unreal/meta-xr-audio-sdk-unreal-spatialize/)).

### 4.3 Convolution reverb and ray tracing at runtime

**Convolution reverb** replaces the parametric tail with a measured or simulated **impulse response**: $y = s * h$, per output channel. Steam Audio implements the full pipeline: IRs are *generated from scene geometry and materials* by ray tracing — "Reflections reaching the listener are encoded in an Impulse Response (IR), which is a filter that records each reflection as it arrives. This algorithm renders reflections with the most detail, but may result in significant CPU usage" — with a **hybrid** mode ("the initial portion of the IR is rendered using convolution reverb, but the later part is used to estimate a parametric reverb," with an explicit transition time and cross-fade overlap), a **parametric** mode, and **TrueAudio Next** GPU convolution ([Steam Audio settings](https://valvesoftware.github.io/steam-audio/doc/unreal/settings.html)). Because simulation is expensive, it is **baked** at probes: "For scenes with mostly static geometry, you can pre-compute (bake) these effects in the editor… at run-time, the source and listener positions relative to the probes are used to quickly estimate the reflections or reverb" ([Steam Audio guide](https://valvesoftware.github.io/steam-audio/doc/unity/guide.html)), with bake controls for rays, bounces, duration, and visibility sampling between probes ([Steam Audio baking API](https://valvesoftware.github.io/steam-audio/doc/capi/baking.html)). Physics-based reverb is also *directional*: it "can model the direction from which a distant echo can be heard, and keep it consistent as the player looks around," unlike a parametric stereo wash ([guide](https://valvesoftware.github.io/steam-audio/doc/unity/guide.html)) — the IR is encoded in Ambisonics and decoded with everything else. GPU audio ray tracing has a pedigree: NVIDIA's VRWorks Audio was an OptiX-based path tracer for acoustics ("generates invisible rays representing sound wave propagation… from its origin to the various surfaces it will interact with, and eventually to its arrival at the listener," [RoadToVR's 2017 launch coverage](https://roadtovr.com/nvidia-shows-how-physically-based-audio-can-greatly-enhance-vr-immersion-vrworks-audio-release/); [NVIDIA developer blog, VRWorks Audio 2.0 with RTX](https://developer.nvidia.com/blog/vrworks-audio-dials-up-the-immersion-with-rtx-acceleration/)), but it is effectively dormant now — worth knowing, not worth depending on. The live frontier is general GPU ray tracing (Snowdrop talk above, Steam Audio's Radeon Rays/Embree backends).

### 4.4 Early reflections: the image-source method, and why the late field isn't

Early reflections (< ~80 ms) are perceived discretely and are what makes a space "read" — and they admit an exact geometric solution for box-shaped rooms: the **image-source method** of Allen & Berkley (JASA 65(4):943–950, 1979; [DOI](https://doi.org/10.1121/1.382599), [free PDF](https://www.umiacs.umd.edu/~ramani/cmsc828d_audio/AllenBerkley79.pdf)). Mirror the source across each wall into an infinite lattice of image sources; each image contributes "a pure impulse of known strength and delay" at the receiver, with amplitude $\beta^{|q|}$-style product wall-reflection factors and $1/r$ spreading. The cost is the killer: "the computation time (and number of images) goes up approximately as the cube of response length" (for 3D rooms) — their own table shows 37,500 images for a 256 ms response in a small room, in 1979 hardware. Exact imageSource is therefore a *baking* tool (Steam Audio's baked IRs are computed this way in spirit — ray/image hybrids), while runtime early reflections are approximated by a handful of traced rays or by parametric reflection taps. The **late field** (the statistically diffuse tail) is where parametric FDN/comb reverb wins: past the mixing time the individual reflection identities are irrelevant and a stochastic feedback network with the right decay, damping, and diffusion is perceptually equivalent at ~1% of the cost. The modern consensus is exactly Steam Audio's hybrid split: geometric/convolution early field + parametric (or GPU-convolved) late field, with the crossover around 0.5–1.5 s ([Steam Audio settings](https://valvesoftware.github.io/steam-audio/doc/unreal/settings.html)).

---

## 5. Doppler and Motion

The OpenAL 1.1 spec states the runtime formula exactly ([spec §3.5.2](https://www.openal.org/documentation/openal-1.1-specification.pdf)) — note the sign convention with the source-to-listener vector $\mathbf{S_L}$, listener velocity projected as $v_{ls} = \mathrm{dot}(\mathbf{S_L}, \mathbf{LV})/\mathrm{mag}(\mathbf{S_L})$ and source velocity likewise, both clamped to $SS/DF$:

$$\boxed{\;f' = f \cdot \frac{SS - DF \cdot v_{ls}}{SS - DF \cdot v_{ss}}, \qquad SS = 343.3\ \tfrac{\mathrm{m}}{\mathrm{s}} \text{ (default)},\quad DF = 1.0 \text{ (default)}\;}$$

This is the classical Doppler relation with the medium at rest and both velocities measured along the line of sight (positive away from the listener in the SL convention). The `AL_DOPPLER_FACTOR` is an artistic exaggerator: "a simple scaling of source and listener velocities to exaggerate or deemphasize the Doppler (pitch) shift."

**Worked example — source passing at 10 m/s.** A 440 Hz source flies straight past a stationary listener at 10 m/s. Approaching head-on: $f' = 440 \cdot 343.3/(343.3 - 10) = 453.2$ Hz (+53 cents). Receding: $f' = 440 \cdot 343.3/(343.3+10) = 427.6$ Hz (−96 cents). Total swing ≈ 149 cents (1.5 semitones); at the instant of closest passage the pitch sweeps through the inflection — and if implemented naively, audibly so.

Implementation is a **variable delay line** (the same structure as a flanger): the read pointer advances at rate $f_{\text{ratio}} = f'/f$ relative to the write pointer, i.e. resampling with a fractional delay; per-sample fractional-delay interpolation (linear at minimum, Catmull-Rom/windowed-sinc for quality) provides continuous pitch. The classic artifact — **"zipper noise"** — appears when the ratio is recomputed per game frame and stepped: the delay jumps discontinuously, modulating the signal with frame-rate clicks. Remedies, all standard practice: (a) update the *target* ratio per frame but **smooth the ratio itself** with a one-pole on audio time (Valve's own guidance that time-based audio behavior "has to run off the real elapsed time rather than per-update steps" — §1.3); (b) clamp the per-frame ratio delta; (c) interpolate the delay line across the block. Wwise's documented best practice is to bypass physics entirely and drive pitch by RTPC: "have the game engine keep track of the position delta between the listener and the sound source, which basically equates to a speed value. This 'speed' Game Parameter can then be mapped to the pitch property of a sound using an RTPC… by far the least CPU intensive procedure for creating Doppler effects" ([Wwise RTPC tips](https://www.audiokinetic.com/en/public-library/2023.1.14_8770/?id=rtpc_tips_best_practices)) — with the honest caveat that with multiple listeners (split-screen) a single voice can only have one pitch.

---

## 6. Voice Management, Mixing, and Loudness

### 6.1 Voice limits, priority, virtualization

Real engines cap simultaneous real (DSP-processed) voices (tens to a few hundreds) and evict by priority. Wwise is the best-documented: per-object **Playback Priority** (0–100 scale in authoring; a typical table puts UI/dialogue/music near 100, weapon impacts ~90, footsteps ~50, "nice to have" ambience ~30 — [Wwise course, Priority Level](https://www.audiokinetic.com/en/courses/wwise251/?id=Lesson3_Priority_Level)), **playback limits** per object hierarchy ("If Limit Reached: Kill / Use virtual voice settings" — [Wwise community Q&A](https://www.audiokinetic.com/qa/14299/random-ambience-with-voice-limit-isnt-rotating-back)), and **virtual voices** — the mechanism this section exists for: "Virtualizing inaudible or lower priority sounds allows Wwise to track a sound's state without processing the sound's voices through the mix engine in order to save CPU, memory, and in some cases hardware voices. There are two reasons a sound goes into a virtual voice mode… the voice falls below the volume threshold, or the voice has been pushed out due to playback limits and priority settings." A virtualized voice either **continues to play** (state advances, e.g. a looping ambience resumes seamlessly when it becomes audible again) or is **killed**; "of the different behaviors, 'Kill if finite else virtual' is recommended as the default" (Nic Taylor, *Understanding Wwise Virtual Voices*, in *Game Audio Programming*; [DOI](https://doi.org/10.1201/b22247-9); see also [Wwise: Understanding playback limit and priority](https://www.audiokinetic.com/en/library/edge/?id=concept_advanced_settings.html)). This is the audio analogue of render LOD/occlusion culling and belongs in any custom engine from day one.

### 6.2 Bus architecture, ducking, sidechain compression

The bus graph (hierarchy of submixes + send-style returns + one master) is universal: FMOD's mixer with group buses, sends/returns, VCAs and snapshots (§1.2); Wwise's Master-Mixer hierarchy; Unreal's Sound Classes → Submixes (a Sound Class is a "hierarchical group for volume mixing (Master, Music, SFX, Voice)" and Sound Mixes apply EQ/volume modifications — e.g. the classic "muffled underwater" mix — [Unreal Audio overview](https://unrealdocs.win/pages/audio.html)). **Ducking** (dialogue automatically dips the SFX/ambience beds) is implemented either as mix automation — FMOD snapshots ("sets of values for bus properties applied… on the fly," with blend/intensity controls, [FMOD Mixing §9.9](https://www.fmod.com/docs/2.03/studio/mixing.html)) — or as a true **sidechain compressor** keyed on another bus's envelope. The compressor math is the standard feed-forward design of Giannoulis, Massberg & Reiss, *Digital Dynamic Range Compressor Design — A Tutorial and Analysis*, JAES 60(6):399–408, 2012 ([AES](https://aes2.org/publications/elibrary-page/?id=16354), [free PDF](http://redmine.jamoma.org/attachments/download/175/JAES_V60_6_PG399.pdf)) — the paper's recommendation is feed-forward "because they are stable and predictable," with the detector in the log domain after the gain computer. The three stages:

**1. Level detector** (peak or RMS): $x_{db}[n] = 20\log_{10}|x[n]|$ (or over a short RMS window).

**2. Gain computer** (static curve, threshold $T$, ratio $R$, soft knee width $W$):

$$y_{db} = \begin{cases} x_{db} & x_{db} \le T - \tfrac{W}{2} \quad\text{(below)}\\[2pt] x_{db} + \dfrac{(1/R - 1)\,(x_{db} - T + W/2)^2}{2W} & |x_{db} - T| < \tfrac{W}{2} \quad\text{(soft knee)}\\[6pt] T + \dfrac{x_{db} - T}{R} & x_{db} \ge T + \tfrac{W}{2}\quad\text{(above)} \end{cases}$$

**3. Gain smoothing** (attack $\tau_A$, release $\tau_R$, as one-pole branch filters — the *envelope follower on the control signal*):

$$g_{db}[n] = \alpha\, g_{db}[n-1] + (1-\alpha)\, y_{db}[n],\qquad \alpha = \begin{cases} \alpha_A = e^{-1/(\tau_A f_s)} & y_{db}[n] < g_{db}[n-1] \ (\text{attack})\\ \alpha_R = e^{-1/(\tau_R f_s)} & \text{otherwise} \ (\text{release}) \end{cases}$$

and the output applies linear gain $g[n] = 10^{(g_{db}[n] - x_{db}[n])/20}$ (plus make-up). Game ducking is typically gentle: threshold a few dB under the dialogue bus level, ratio 2:1–6:1, attack 10–50 ms, release 200–500 ms. A limiter (ratio ∞:1, fast attack) on the master is the last line of defense against clipping.

### 6.3 Loudness: LUFS, K-weighting, gating, and platform targets

Perceived programme loudness is standardized in **ITU-R BS.1770** (current revision [BS.1770-5, 11/2023](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf)); LKFS and LUFS are the same unit under ITU and EBU names. The algorithm: per channel, apply **K-weighting** — a two-stage pre-filter (stage 1: a high shelf "account[ing] for the acoustic effects of the head, where the head is modelled as a rigid sphere"; stage 2: the "RLB" revised low-frequency B-curve high-pass) — then mean square, channel weights (1.0 for L/R/C, **1.41 for surrounds**, LFE excluded), and the −0.691 calibration constant. The filter is fully specified by 48 kHz biquad coefficients in the Recommendation (Tables 1–2):

$$\boxed{\;
\text{Stage 1 (shelf): } \frac{1.53512485958697 - 2.69169618940638 z^{-1} + 1.19839281085285 z^{-2}}{1 - 1.69065929318241 z^{-1} + 0.73248077421585 z^{-2}}
\qquad
\text{Stage 2 (RLB HP): } \frac{1 - 2z^{-1} + z^{-2}}{1 - 1.99004745483398 z^{-1} + 0.99007225036621 z^{-2}}
\;}$$

$$L_K = -0.691 + 10\log_{10} \sum_i G_i\, z_i \quad \text{[LKFS]}$$

(At other sample rates the coefficients must be re-derived to match the 48 kHz response; the analog prototypes are a ≈+4 dB shelf at ~1682 Hz and a ~38 Hz Butterworth high-pass — [BS.1770-5 Annex 1](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf).) **Gating** makes the measure robust: the programme is cut into 400 ms blocks overlapping by 75%; blocks below the absolute gate of −70 LKFS are dropped; the relative gate is the surviving mean minus 10 LU; the integrated loudness is the mean over blocks above both gates. The gate is what keeps long quiet atmospheres from dragging the number down. EBU R128 builds normalization practice on this (−23.0 LUFS / −1 dBTP in Europe; US ATSC A/85 and Japan use −24 LKFS) — [Sony's loudness slide deck](https://www.sambuz.com/doc/loudness-and-how-to-measure-it-ppt-presentation-886969) and [BS.1770-5](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf).

**Game platform targets** — where sources genuinely disagree by year, so the range: Sony's Audio Standards Working Group recommendation **ASWG-R001** (first published Nov 2012, updated from v1.1 of Aug 2013) specifies **−23 LUFS for PS3/PS4 and −18 LUFS for PS Vita, ±2 LU tolerance, max true peak −1 dBTP, recommended max loudness range 20 LU**, measured "for as long as is practical and for a minimum of 30 minutes" over representative gameplay ([Sony ASWG *Loudness* repository on GitHub](https://github.com/Sony-ASWG/Loudness); [Garry Taylor interview, Designing Sound 2012](https://designingsound.org/2012/07/30/video-games-and-loudness-standards-interview-with-sonys-garry-taylor/); [Taylor's follow-up post](http://gameaudionoise.blogspot.com/2013/01/on-subject-of-loudness.html)). By the PS4 mastering era, SCE's enforced standard was quoted as **−24 LKFS** — "hit SCE's average loudness standard for PlayStation 4 of −24 LKFS, easily," via the PS4 Audio Mastering Suite (EQ/dynamics/limiter/loudness metering running on a system core outside the game) ([Audio Media International, 2016](https://audiomediainternational.com/sonys-garry-taylor-on-audio-mastering-for-playstation-4/)). So the Sony-family range across years is **−24 to −18 LUFS depending on platform and era** (consoles −23/−24, handheld −18). Microsoft's public Xbox Requirements (XRs) do not publish a numeric loudness requirement on their public pages ([Xbox Requirements](https://learn.microsoft.com/en-us/gaming/gdk/docs/store/policies/console/certification-requirements?view=gdk-2604)); what is verifiable is that the industry group **IESD** (Interactive Entertainment Sound Developers — Microsoft, EA, Activision and others) "has adopted our standard and it has been agreed to in principle by the members" (Taylor, [blog](http://gameaudionoise.blogspot.com/2013/01/on-subject-of-loudness.html)) — i.e. the −23/−24 LUFS regime industry-wide. Context: this is the late landing of the **loudness war** in games — the ASWG was formed precisely because measured titles varied wildly ("we measured over 120 games, films, TV shows and trailers," Taylor, [Designing Sound](https://designingsound.org/2012/07/30/video-games-and-loudness-standards-interview-with-sonys-garry-taylor/)); a custom engine should simply build a BS.1770 meter into the debug overlay and target roughly −23 LUFS integrated with −1 dBTP true peak, leaving headroom rather than fighting for loudness.

---

## 7. Procedural Audio for Games

### 7.1 The case: samples vs synthesis

The trade is memory/CPU/variability. Sample libraries scale linearly with content (a big open world's footstep matrix is thousands of assets — *Sunset Overdrive*'s traversal/vanity system alone "totaled over 9,000k assets," [GDC Vault](https://www.gdcvault.com/play/1021783/All-Style-All-Substance-The)); synthesis inverts it — near-zero asset memory, nonzero per-voice CPU, unbounded variation. The canonical textbook is Andy Farnell's *Designing Sound* (MIT Press, 2008; [book page](https://mitpress.mit.edu/9780262014410/designing-sound/)): "Sound is considered as a process, rather than as data — an approach sometimes known as 'procedural audio'… procedural sound is a living sound effect that can run as computer code and be changed in real time according to unpredictable events," with 650 pages and 30+ Pure Data exercises (alarms, engines, wind, rain, doors) whose code is [freely downloadable](https://aspress.co.uk/sd/). The best verified GDC treatments: Nicolas Fournel (Sony), [*Procedural Audio for Video Games: Are we there yet?*](https://www.gdcvault.com/play/1012704/Procedural-Audio-for-Video-Games) ("Explosion of content, non-repetitive sound effects, need for interactive music… procedural audio is a compelling solution… but are we ready for it?" — an honest production-cycle and QA-centric assessment); and Leonard J. Paul's long-running series, including [*Granular Synthesis in Next-Generation Games*](https://www.gdcvault.com/play/1013403/Granular-Synthesis-in-Next-Generation) ("granular synthesis… allows sampled sound to be warped in real-time to allow pitch to change independently of tempo… gives the video game audio artist a host of new tools to fight the repetitive nature of sampled sounds") and [*Advanced Granular Synthesis for Next Generation Games*](https://www.gdcvault.com/play/620/Advanced-Granular-Synthesis-for-Next) (GDC 2007), with materials at [videogameaudio.com](https://videogameaudio.com/main.htm).

### 7.2 Techniques in shipped games

**Granular synthesis for ambiences.** A grain is a short (10–200 ms) windowed excerpt of a source buffer; the granulator emits grains at hop interval $H$ (density $1/H$), each with its own read offset $o_k$, rate $r_k$, and gain $g_k$:

$$y[n] = \sum_{k} g_k\, w\!\left(\frac{n - t_k}{D}\right) s\!\left[\,o_k + r_k\,(n - t_k)\,\right], \qquad t_k = k\,H,\ \ D = \text{grain length}$$

with $w$ a window (Hann/triangular) so grains sum smoothly — overlapping windows at $H < D$ yield continuous texture, random $o_k$ yields infinite non-repeating variation from seconds of source. Shipped-scale proof: Square Enix's [*Data-Driven Granular Synthesis for Video Games*](https://www.gdcvault.com/play/1023987/Data-Driven-Granular-Synthesis-for) automatically extracts features from recordings of rain/water/debris and resynthesizes similar sounds of arbitrary length — "a few seconds of recorded audio data provide enough information to create similar sounds of high quality and arbitrary length… they can reduce the necessary memory by up to 90% without compromising on quality." The IEM Plug-in Suite even ships an Ambisonic granular encoder as a plugin ("the first Ambisonic synthesizer plug-in," [plugins.iem.at](https://plugins.iem.at/)).

**Physical modeling: Karplus–Strong.** For plucks and impacts, the Karplus–Strong algorithm is a one-line physical model — a delay line of $p$ samples fed back through a low-pass — with the classic recurrence $y[n] = \tfrac{1}{2}(y[n-p] + y[n-p-1])$: "the entire plucked-string algorithm requires only as much computation as one or two sine wave oscillators" yet rivaled 30-oscillator additive resynthesis in realism, and the drum variant (probabilistic averaging with blend factor $b$) morphs string→snare as $b \to \tfrac12$ ([Karplus & Strong, *Digital Synthesis of Plucked-String and Drum Timbres*, Computer Music Journal 7(2):43–55, 1983](https://doi.org/10.2307/3680062), [free PDF](https://users.soe.ucsc.edu/~karplus/papers/digitar.pdf)). For an engine, KS-class models (and their extended/waveguide descendants) are the cheapest credible impact/pluck synthesis available.

**Sample layering with round-robins and pitch/velocity mapping.** The standard foley approach — $N$ recorded variants per (surface, action) cell, cycled or randomized without immediate repeats, pitch-jittered ±a few percent, layers selected by velocity/speed — exists because of perceptual **habituation**: repeated identical samples are recognized as repetition within a handful of exposures, and randomized pitch/variant selection pushes recognition past the attention horizon. Every sampler in middleware exposes this (Wwise Random/Sequence Steps, FMOD's multi-instrument events with parameter-driven selection). *Sunset Overdrive* is the documented extreme: a footstep system "that not only supported 9 behaviors across many material types but also supported six different shoe types" ([GDC Vault](https://www.gdcvault.com/play/1021783/All-Style-All-Substance-The)) — the cross-product explosion that motivates the procedural alternatives above.

**Wind systems.** The shipped pattern (verified for *Ghost of Tsushima*) is: a global wind vector field (magnitude modulated by Perlin-style noise: "the main wind vector has a constant direction, but we varied the magnitude a bit from place to place using time-varying Perlin noise," plus "vorticles" — "invisible wind-generating particles that can be sampled by other particles" — for local gusts; [*Blowing from the West: Simulating Wind in 'Ghost of Tsushima'*, GDC 2021](https://www.gdcvault.com/play/1027350/Blowing-from-the-West-Simulating), [Game Developer's writeup](https://www.gamedeveloper.com/design/using-vorticles-to-simulate-wind-in-i-ghost-of-tsushima-i-)). The audio side of the same game is documented separately: a tiny team built an "adaptive audio system reacting to time, weather, region and other game parameters" for a world "14x larger than anything the studio had worked on previously" ([*Big World, Small Team: Designing a Scalable Ambience System for 'Ghost of Tsushima'*](https://www.gdcvault.com/play/1027222/Big-World-Small-Team-Designing)). The audio translation — a filterbank + noise bed whose band gains and gust envelopes are driven by the *same* wind field the vegetation shaders sample — is the canonical wind-audio architecture, and the reason it works is that the visual and auditory systems share one believable input. (A frequently-cited Zelda: BotW wind-audio talk could not be verified and is omitted.)

**Adaptive/interactive music.** Two orthogonal axes, per Audiokinetic's own design classification: **vertical layering** ("vertical remixing… always mixing-related": stem tracks brought in/out or filtered by game state) and **horizontal re-sequencing** ("essentially about how to switch the Music Segments and how to transition" — playlists, switch containers, transition matrices, stingers) ([Audiokinetic: About Dynamic Music Design](https://www.audiokinetic.com/blog/about-dynamic-music-design-part-1-design-classification); [Wwise interactive music docs](https://www.audiokinetic.com/en/library/edge/?id=creating_interactive_music)). FMOD's equivalent is the event timeline + parameters ("use timelines and parameters to change the behavior of events as your game runs," [Authoring Events](https://fmod.com/docs/2.03/studio/authoring-events.html)). The engineering requirement both impose: sample-accurate scheduling of musical time (entry/exit cues, tempo maps) on the audio thread, not the game thread.

### 7.3 Footstep systems in detail

The full architecture, as documented across the talks above: (1) **surface material map** — the ground query (voxel material, physical material, or decal) returns a material ID that selects a layer set; the layer set is a cross-product of material × behavior (walk/run/land/strafe) × footwear; each cell holds round-robin variants. (2) **Timing** — either animation events (footstep notifies on the walk cycle; exact but requires animation cooperation) or foot-phase detection (trigger when the foot bone passes a phase threshold or when vertical velocity reverses at contact; works with procedural animation). (3) **The heel-toe split** — the foley-standard double impulse: a real footfall is two events (heel strike, then toe/roll-off ~60–120 ms later), and layering them as separate samples with separate material selection is a large share of why recorded foley reads as "real" versus a single thud per step. (4) **Variation** — pitch jitter, gain jitter, and no-immediate-repeat randomization to defeat habituation (§7.2). (5) **Scaling** — the *Sunset Overdrive* numbers (9 behaviors × materials × 6 shoes ⇒ 9,000+ assets, [GDC](https://www.gdcvault.com/play/1021783/All-Style-All-Substance-The)) are the cautionary tale that pushes teams toward parametric/procedural cells; Guy Somberg's [*How to Write an Audio Engine, Part Two*](https://gdcvault.com/play/1024677/How-to-Write-an-Audio) walks through footstep and obstruction systems as engine features with "many subtle details."

---

## 8. What a Custom Engine Should Actually Implement

*This section is engineering recommendation, grounded in the cited material above, not surveyed fact.*

### 8.1 Minimum viable 3D audio

For this engine (C++20, voxel world, existing raycast machinery from the SVO/collision work), the floor that produces a *believable* 3D soundscape is small and every piece has a verified reference implementation:

1. **Device + mixer core.** One real-time audio thread, 48 kHz, 256-frame blocks, float32 internal, a lock-free command queue from the game thread (the FMOD/Valve model of §1.1: game thread enqueues, mixer consumes; smooth everything on audio-clock time). Use **miniaudio** (public-domain/MIT-0, single .c file, built-in decoders and a node graph — [github.com/mackron/miniaudio](https://github.com/mackron/miniaudio), [miniaud.io](https://miniaud.io/)) or **SDL3 audio** (zlib license, the `SDL_AudioStream` model — [libsdl.org](https://www.libsdl.org/), [SDL3 audio wiki](https://wiki.libsdl.org/SDL3/CategoryAudio)) as the device layer; **PortAudio** (MIT, [portaudio.com](https://portaudio.com/), [GitHub](https://github.com/PortAudio/portaudio)) is the classic alternative if only I/O is wanted.
2. **Spatialization: constant-power stereo day one, HRTF binaural day two.** Constant-power panning costs two multiplies (§2.1). HRTF binaural for headphones is a pair of ~128-tap FIRs per source with interpolated HRIRs from CIPIC or IRCAM LISTEN (§2.4) — the single biggest immersion win per line of code, and the CIPIC paper's warning about generic-HRTF front/back confusion argues for shipping 2–3 selectable sets. Keep the source representation renderer-agnostic (direction + spread) so an Ambisonic intermediate can slot in later.
3. **Distance: inverse-clamped rolloff** (§3.1) with per-source reference distance, rolloff factor, and max distance — the OpenAL formula verbatim, plus hysteresis on any culling (the spec's own warning).
4. **Air absorption + occlusion: one biquad each.** A distance-driven LPF (Unity's curve model: cutoff vs distance, §3.2), and a raycast listener→source driving per-material dB loss + cutoff from a small STC-style material table (§3.3: drywall ≈ −30/−40 dB, stone/concrete ≈ −50/−60, wood ≈ −20; HF loss scaled with thickness). The engine already has world raycasts; this is the cheapest "sounds like a world" feature available.
5. **One parametric reverb** — an FDN with the EFX/I3DL2 parameter set (§4.1) so presets are interchangeable with a decade of published tables; a global reverb + distance-driven send buys the DRR distance cue (§4.2). Zones can wait; a single smoothly-interpolated global preset ("outside"/"cave"/"interior") is enough for v1.
6. **Doppler** via per-voice variable delay lines with audio-clock smoothing of the ratio (§5) — and the restraint to leave it off for slow gameplay if it zippers.
7. **Voice management + loudness metering from the start.** Priority + virtual voices (continue-to-play vs kill, §6.1) and a BS.1770 block (K-weighting biquads + gated measurement, §6.3) in the debug overlay — both are cheap early and expensive to retrofit.

### 8.2 The upgrade path

- **Ambisonic intermediate representation** (§2.3): encode sources and reverb at 1st–3rd order (SN3D/ACN, the ambiX convention), decode to stereo/binaural/5.1 with one matrix; a listener rotation becomes a sparse channel transform. This decouples the scene simulation from the output format permanently.
- **Steam-Audio-style baked IRs** (§4.3): probe points through the world, offline ray-traced convolution IRs (image-source or ray hybrid) with per-material absorption, trilinear interpolation between probes at runtime; hybrid early-convolution/late-parametric to bound CPU. The voxel world makes probe placement (e.g. per-region grid) and ray tracing straightforward.
- **Portal/room propagation** (§3.4): a room graph over the already-computed voxel connectivity, with sources re-virtualized at portals — the Wwise Rooms/Portals model is the blueprint, and the VERA GDC talk is the voxel-native precedent.
- **Procedural surfaces & wind**: Karplus–Strong impacts and filterbank wind driven by the same wind/noise fields as the terrain and vegetation — the *Tsushima* lesson that shared simulation input is what makes audio feel diegetic (§7.2).
- **Diffraction**: detour-path virtual sources at edges (Steam Audio's pathing model, §3.3), only after occlusion is solid.

### 8.3 Verified open-source building blocks

| Project | License (verified this session) | URL | Role |
|---|---|---|---|
| **OpenAL Soft** | **LGPL-2.0 (not MIT — the user's brief was wrong here)** | [github.com/kcat/openal-soft](https://github.com/kcat/openal-soft), [openal-soft.org](https://openal-soft.org/) | Full OpenAL 1.1 + EFX implementation: distance models, Doppler, HRTF, reverb — an LGPL dynamic library is fine for a closed engine, but it is *not* MIT |
| **miniaudio** | Public domain or MIT-0 (choice) | [github.com/mackron/miniaudio](https://github.com/mackron/miniaudio) | Device I/O, decoding, mixing, 3D spatialization, effects — the fastest path to v1 |
| **Steam Audio SDK** | Apache-2.0, free for all developers (4.8.1 current) | [github.com/ValveSoftware/steam-audio](https://github.com/ValveSoftware/steam-audio), [docs](https://valvesoftware.github.io/steam-audio/) | Occlusion, transmission, diffraction/pathing, baked + real-time reverb, HRTF, Ambisonics — adopt wholesale instead of building §8.2 yourself |
| **PortAudio** | MIT | [portaudio.com](https://portaudio.com/), [GitHub](https://github.com/PortAudio/portaudio) | Callback-style cross-platform audio I/O |
| **SDL3 (audio)** | zlib | [libsdl.org](https://www.libsdl.org/), [SDL3/CategoryAudio](https://wiki.libsdl.org/SDL3/CategoryAudio) | If SDL is already the windowing layer, `SDL_AudioStream` may be free |
| **JUCE** | AGPLv3 / commercial (dual) | [juce.com](https://www.juce.com/get-juce/), [GitHub](https://github.com/juce-framework/JUCE) | Authoring *tools* only — AGPL makes it unusable inside a closed-source engine without a paid licence |
| **IEM Plug-in Suite** | Free/open-source (GPL family) | [plugins.iem.at](https://plugins.iem.at/) | Reference Ambisonics implementation up to 7th order (tools/reference, not runtime) |
| **CIPIC / IRCAM LISTEN / MARL HRTF sets** | Public research databases | [CIPIC](http://interface.cipic.ucdavis.edu), [LISTEN](http://recherche.ircam.fr/equipes/salles/listen/), [MARL repo](https://steinhardt.nyu.edu/marl/research/resources/head-related-impulse-responses-repository) | HRIR data for binaural rendering |
| **Pure Data (+ Farnell's examples)** | Free/open-source | [aspress.co.uk/sd](https://aspress.co.uk/sd/) | Prototyping procedural sound models before porting them to C++ |

*Not adopted:* NVIDIA VRWorks Audio (effectively dormant — §4.3); "Fleck" and other unverified externals were dropped after failing live-verification.

**Bottom line.** The minimum-viable stack (miniaudio + constant-power/HRTF panning + inverse distance + raycast occlusion + one EFX-style reverb + smoothed Doppler + virtualized voices + a BS.1770 meter) is on the order of a few thousand lines on top of the device library, every formula in it is specified by a free standard or paper cited above, and every subsystem has a shipping-quality open-source reference to diff against.

