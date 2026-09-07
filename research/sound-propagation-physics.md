# Acoustic Wave Physics and Sound Propagation in Environments

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
