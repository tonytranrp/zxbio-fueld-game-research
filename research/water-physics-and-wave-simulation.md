# Water Dynamics & Wave Physics: A Simulation-Grade Reference

**Scope:** how water behaves at rest, how gravity waves propagate on it, what happens to a wave vertically below the surface, and what happens when the seafloor — specifically a coral reef — gets involved. Every formula below is either a standard derivation (shown) or pulled from a specific paper (cited). Numbers that come from field measurements are labeled as such and given as ranges, not single "true" values, because that's what the literature actually supports.

**How to read this:** Section 2 is the core linear theory everything else builds on. Section 5 is the reef-specific section you asked for. Sections 7–9 are the "how do I actually turn this into code" sections — read those last, after the physics makes sense, or the design choices won't mean anything.

---

## 1. Foundations

### 1.1 The governing equations

Water at the scales you care about (waves, currents, flow around a reef) is well described as an incompressible Newtonian fluid. The full statement is the incompressible Navier–Stokes system:

$$
\frac{\partial \mathbf{u}}{\partial t} + (\mathbf{u}\cdot\nabla)\mathbf{u} = -\frac{1}{\rho}\nabla p + \nu \nabla^2 \mathbf{u} + \mathbf{g}
$$
$$
\nabla \cdot \mathbf{u} = 0
$$

where **u** is velocity, $p$ pressure, $\rho$ density (~1000 kg/m³ fresh, ~1025 kg/m³ seawater), $\nu$ kinematic viscosity (~1.0×10⁻⁶ m²/s fresh, ~1.05×10⁻⁶ m²/s seawater at 20 °C), and **g** gravity.

Everything else in this document — wave theory, shallow-water equations, spectral ocean synthesis — is a *simplification* of this system obtained by dropping or approximating specific terms. Knowing which term you dropped tells you exactly what physical effect your simulation is blind to. That's the organizing idea of this whole document.

### 1.2 Reynolds number: why you never solve the full equations directly

$$Re = \frac{UL}{\nu}$$

For ocean-scale flow (U ~ 1 m/s, L ~ 10–1000 m), Re is $10^6$–$10^9$. This is deeply turbulent. Direct numerical simulation (DNS) of the full Navier–Stokes equations at this Re is computationally impossible for anything larger than a small tank, even on a research cluster. This single fact is *why* the entire field of wave modeling exists as a stack of approximations (potential flow, shallow water, statistical spectra) rather than "just solve Navier–Stokes" — and it's why your simulation should pick the cheapest approximation that still captures the effect you actually want to show.

### 1.3 Still water: hydrostatic pressure

With no flow, the momentum equation collapses to:

$$p(z) = p_{atm} + \rho g (h - z)$$

for depth $h-z$ below a still surface. This is the baseline every wave calculation perturbs.

### 1.4 Surface tension

Young–Laplace: $\Delta p = \sigma(1/R_1 + 1/R_2)$, with $\sigma_{water} \approx 0.072$ N/m (fresh, 20 °C) or $\approx 0.074$ N/m (seawater). This only matters for very short waves. The crossover wavelength between capillary-dominated and gravity-dominated waves is

$$\lambda_c = 2\pi\sqrt{\sigma/(\rho g)} \approx 1.7\ \text{cm}$$

For anything you'd call an "ocean wave" or even a "pool ripple" from a dropped object beyond a few cm, ignore surface tension. Include it only if you're rendering fine foam/spray detail, where the full capillary-gravity dispersion relation is:

$$\omega^2 = \left(gk + \frac{\sigma k^3}{\rho}\right)\tanh(kh)$$

---

## 2. Linear (Airy) wave theory — the core model

This is small-amplitude, irrotational, inviscid wave theory. It's the workhorse: almost every practical wave formula (dispersion, orbital motion, energy, shoaling) traces back to this.

### 2.1 Setup

Assume irrotational flow, so a velocity potential $\phi$ exists with $\mathbf{u} = \nabla\phi$. Incompressibility then gives Laplace's equation:

$$\nabla^2\phi = 0$$

with boundary conditions: no flow through the flat bottom ($\partial\phi/\partial z = 0$ at $z=-h$), and linearized kinematic + dynamic conditions at the free surface ($z=0$). Solving this system for a sinusoidal surface $\eta(x,t) = a\cos(kx-\omega t)$ gives:

$$\phi(x,z,t) = \frac{ag}{\omega}\frac{\cosh[k(z+h)]}{\cosh(kh)}\sin(kx-\omega t)$$

Every other Airy-theory result below is derived from this potential.

### 2.2 The dispersion relation

$$\boxed{\omega^2 = gk\tanh(kh)}$$

This single equation is the backbone of the whole subject: it links period, wavelength, and depth. Two limits matter enormously for both physics and code:

**Deep water** ($h/\lambda > 1/2$, equivalently $kh > \pi$): $\tanh(kh)\to 1$, so
$$\omega^2 = gk,\quad \lambda_0 = \frac{gT^2}{2\pi},\quad c_0=\frac{gT}{2\pi}$$
Waves don't feel the bottom at all here. Depth is irrelevant to their shape or speed.

**Shallow water** ($h/\lambda < 1/20$): $\tanh(kh)\approx kh$, so
$$\omega^2 = gk^2h \;\Rightarrow\; c = \sqrt{gh}$$
Non-dispersive: every frequency travels at the same speed, set entirely by local depth. This is the tsunami/shallow-flooding regime and it's also why the Shallow Water Equations (Section 7) work — the wave speed only needs the depth, not the wavelength.

**Intermediate depth**: you need the full transcendental $\omega^2=gk\tanh(kh)$, which has no closed-form solution for $k$ given $\omega$ and $h$. For a simulation that needs to evaluate this per-grid-cell over a bathymetry map every frame, iterating Newton–Raphson per cell is wasteful. Two explicit approximations solve this cleanly:

- **Eckart (1951):** $kh \approx \dfrac{k_0 h}{\sqrt{\tanh(k_0 h)}}$, where $k_0=\omega^2/g$ is the deep-water wavenumber. Error up to ~5%.
- **Fenton & McKee (1990)** — the one to actually hardcode:
$$L = \frac{gT^2}{2\pi}\left\{\tanh\left[\left(\frac{2\pi}{T}\sqrt{\frac{h}{g}}\right)^{3/2}\right]\right\}^{2/3}$$
accurate to within 1.7% across *all* depths, and it's a closed-form expression — no iteration. I checked both limits by hand: as $h\to\infty$ it reduces exactly to $L_0=gT^2/2\pi$; as $h\to 0$ it reduces exactly to $L=T\sqrt{gh}$ (the correct shallow-water non-dispersive speed). Use this as your production formula; only add 1–2 Newton–Raphson refinement steps on top if you need better than 1.7% (rare for graphics, sometimes needed for engineering-accuracy reef models).

Phase velocity $c=\omega/k$; group velocity (the speed *energy* actually travels at):

$$c_g = nc,\qquad n = \frac{1}{2}\left(1+\frac{2kh}{\sinh(2kh)}\right)$$

$n\to1/2$ in deep water, $n\to1$ in shallow water. This distinction matters: wave *energy* (and therefore the leading edge of a wave train) moves at $c_g$, not $c$ — get this backwards and your shoaling/energy-transport code will run at the wrong speed.

### 2.3 What happens below the surface (orbital motion, and how deep a wave "reaches")

This is the direct answer to "how does the water behave at levels below itself." Linear theory gives closed orbits for water particles, whose size decays with depth:

**Horizontal velocity:** $u(x,z,t) = a\omega\dfrac{\cosh[k(z+h)]}{\sinh(kh)}\cos(kx-\omega t)$

**Vertical velocity:** $w(x,z,t) = a\omega\dfrac{\sinh[k(z+h)]}{\sinh(kh)}\sin(kx-\omega t)$

Integrating gives particle displacement about a mean position $(x_0,z_0)$ — ellipses in finite depth, circles in deep water:

$$\xi = -a\,\frac{\cosh[k(z_0+h)]}{\sinh(kh)}\sin(kx_0-\omega t), \qquad \zeta = a\,\frac{\sinh[k(z_0+h)]}{\sinh(kh)}\cos(kx_0-\omega t)$$

**Deep-water simplification:** both semi-axes collapse to $a\,e^{kz}$ (z negative downward from the surface). Orbital motion decays *exponentially* with depth. The standard rule of thumb: at $z=-\lambda/2$, orbital amplitude is down to about 4% of the surface value.

**This is the load-bearing fact for your reef question:** whether a wave "feels" a reef at all is entirely a depth-vs-wavelength question. If the reef crest sits deeper than roughly half the dominant wavelength, the wave's orbital motion has already decayed to near-zero by the time it reaches the reef — the reef is dynamically invisible, full deep-water theory applies, and none of Sections 4–5 matter. Once depth drops below that threshold, the wave starts interacting with the bottom (feeling friction, refracting, shoaling, eventually breaking). For simulation purposes, this gives you a clean, physically correct trigger condition: compute $kh$ per grid cell; below $kh\approx\pi$ (roughly $h<\lambda/2$), switch that cell into "shallow-water-aware" mode (Section 9).

**Pressure below the surface** follows the same attenuation shape (derived directly from the potential in 2.1 via linearized Bernoulli, $p=-\rho\,\partial\phi/\partial t - \rho gz$):

$$p_{dynamic}(x,z,t) = \rho g a\,\frac{\cosh[k(z+h)]}{\cosh(kh)}\cos(kx-\omega t)$$

At the seabed ($z=-h$) this reduces to amplitude $\rho g a/\cosh(kh)$ — i.e. a **pressure response factor** $K_p = 1/\cosh(kh)$. This is exactly why bottom-mounted pressure sensors underestimate short, high-frequency waves in deep water, and it's the correction factor real reef researchers apply when inferring wave statistics from a bottom pressure gauge over rough bathymetry (see Marques et al. 2025 in References). If your simulation ever needs to compute the force a wave exerts on a submerged reef structure at some depth, this is the attenuation curve to apply.

### 2.4 Wave energy

$$E = \frac{1}{8}\rho g H^2 = \frac{1}{2}\rho g a^2 \qquad (H=2a)$$

Energy *flux* (power per unit crest length) is $P = E\,c_g$. In the absence of dissipation, $P$ is conserved along a wave ray — this single conservation law is what produces shoaling (Section 4.1).

---

## 3. Beyond linear theory: steep and shallow waves

Linear theory assumes $ka\ll1$ (gentle slope) and doesn't predict breaking. Real waves — especially ones approaching a reef — get steep and nonlinear before they break.

### 3.1 Which theory applies where: the Ursell number

$$Ur = \frac{H\lambda^2}{h^3}$$

$Ur\ll1$: linear (Airy) theory is fine. As $Ur$ grows past roughly the low tens (exact cutoffs vary by textbook — this is a genuinely fuzzy boundary, not a hard number), Stokes theory breaks down and **cnoidal wave theory** (shallow, nonlinear, periodic) becomes the right model; in the $Ur\to\infty$ limit you're describing **solitary waves**. Practically: use $Ur$ computed from local $(H,\lambda,h)$ per grid cell to decide which correction (if any) to layer on top of your base linear/spectral wave field near the reef.

### 3.2 Stokes waves and mass transport

Perturbing in wave steepness $\epsilon=ka$ gives sharper crests and flatter troughs (visually, this is why real ocean waves don't look like pure sine curves — see Section 8.2 for how to fake this cheaply). It also produces a genuinely new effect linear theory misses entirely: **Stokes drift**, a small net forward mass transport:

$$u_{drift}(z) = \omega k a^2\,\frac{\cosh[2k(z+h)]}{2\sinh^2(kh)}$$

At the deep-water surface this reduces cleanly to $u_{drift}=\omega k a^2$ (I verified the limit by hand). If your simulation has floating debris, boats, or particles that should visibly drift in the direction of wave travel rather than just bobbing in place, you need this term explicitly added on top of the orbital oscillation — the orbital motion alone has *zero* mean Eulerian velocity and won't produce drift by itself.

### 3.3 Breaking criteria — two independent limits, take whichever binds

**Steepness-limited (Miche, 1944)**, generalizing the deep-water Stokes limiting steepness ($H/\lambda\to1/7\approx0.142$ in deep water) to any depth:

$$\left(\frac{H}{\lambda}\right)_{max} = 0.142\tanh(kh)$$

**Depth-limited (shallow water):**

$$H_{max} = \gamma\, h$$

with breaker index $\gamma$ commonly taken as **0.78** (from the McCowan solitary-wave limit), though field measurements on real beaches and reefs range roughly 0.4–1.0+ depending on slope and spectral shape — this is a genuinely variable empirical parameter, not a universal constant.

**Practical rule for a simulation:** at every point, compute both limits and break the wave (trigger foam/dissipation) at whichever is smaller: $H_{break}=\min\big(0.142\,\lambda\tanh(kh),\ \gamma h\big)$. On a reef, the depth-limited criterion almost always wins right at the crest, because depth drops abruptly there.

### 3.4 The Iribarren number: *how* a wave breaks, not just *whether*

$$\xi = \frac{\tan\beta}{\sqrt{H/L_0}}, \qquad L_0=\frac{gT^2}{2\pi}$$

($\beta$ = bottom slope at the break point). This determines breaker type:

| $\xi$ | Breaker type | Visual character |
|---|---|---|
| $\xi < 0.4$ | Spilling | Gradual, foamy, gentle |
| $0.4 < \xi < 2$ | Plunging | Crest curls over and collapses — classic "barrel" |
| $\xi > 2$ | Surging/collapsing | Wave doesn't really break, surges up the slope |

Coral reefs typically have steep fore-reef slopes (roughly 1:5 to 1:20 for fringing reefs on volcanic islands, gentler for barrier reefs) combined with a sharp crest — this pushes $\xi$ up, so **expect plunging breakers concentrated in a narrow zone right at the reef crest**, not a long, gradual spilling surf zone like a sandy beach. That's a specific, physically-grounded visual target for your renderer: a violent, localized break, not a diffuse one.

---

## 4. Wave transformation over changing bathymetry

This section is the general answer to "what does something under the water do to the wave." Section 5 specializes it to coral.

### 4.1 Shoaling

With no refraction or dissipation, energy flux $E c_g$ is conserved along a ray:

$$K_s = \frac{H}{H_0} = \sqrt{\frac{c_{g,0}}{c_g}}$$

Since $c_g$ decreases as water shallows, wave height *increases* as a wave shoals onto a reef — right up until the breaking criterion (3.3) is hit. This is why waves visibly grow taller just before the reef crest and then collapse. The shallow-water limiting form of this is **Green's Law**: $H\propto h^{-1/4}$.

### 4.2 Refraction

Waves bend toward regions of lower phase speed (shallower water), governed by an analog of Snell's law along a ray: $\sin\theta/c = \text{const}$. This is why wave crests visibly wrap around a reef patch or headland — the part of the crest over shallow water slows down while the part over deep water keeps moving, rotating the crest line toward alignment with the depth contours. For a simulation, this is naturally captured if you solve the *local* wavenumber vector $\mathbf{k}(x,y)$ from the dispersion relation using the local depth at every point and treat $\mathbf{k}=\nabla\theta$ (phase gradient) — this is exactly what phase-averaged spectral wave models (e.g. SWAN) do under the hood.

### 4.3 Diffraction

Refraction alone doesn't explain energy spreading into the geometric "shadow" behind an isolated reef, island, or reef pass — that's diffraction. The standard equation combining shoaling + refraction + diffraction in one PDE is the **mild-slope equation** (Berkhoff, 1972):

$$\nabla\cdot(cc_g\nabla\phi) + k^2 c c_g \phi = 0$$

This is what phase-resolving coastal models (REF/DIF, MIKE 21) actually solve. You almost certainly don't need to implement this directly for a real-time simulation — but if waves need to visibly bend into a lagoon through a reef channel rather than just refract, this is the physics that produces it, and it's worth knowing you're approximating it away if you skip it.

### 4.4 Reflection and transmission

At a reef edge or steep structure: $K_r = H_r/H_i$ (reflection), $K_t=H_t/H_i$ (transmission), with $K_r^2+K_t^2+(\text{fraction dissipated})=1$. Natural reef slopes typically reflect a modest fraction of incident energy (rough order $K_r\sim0.1$–$0.4$, higher for steep/hard structures); on a real coral reef, **most of the incident energy is neither reflected nor transmitted — it's dissipated**, which is the subject of the next section.

### 4.5 Bottom friction: the quadratic drag law

Near-bed shear stress under an oscillatory wave:

$$\tau_b = \tfrac{1}{2}\rho f_w |u_b|\,u_b$$

where $u_b$ is the near-bed orbital velocity from linear theory (Section 2.3, evaluated at $z=-h$) and $f_w$ is an empirical **wave friction factor** — the single most important free parameter for reef wave dissipation, covered with real numbers in Section 5.

Time-averaged energy dissipation rate per unit area: instantaneous dissipation is $\tau_b\cdot u_b=\tfrac12\rho f_w|u_b|^3$; averaging a sinusoidal $u_b=U_b\cos(\omega t)$ uses the identity $\langle|\cos\theta|^3\rangle = 4/(3\pi)$ (I verified this by direct integration: $\int_0^{2\pi}|\cos\theta|^3\,d\theta = 8/3$, divided by $2\pi$ gives $4/(3\pi)\approx0.424$), giving the standard coastal-engineering result:

$$\boxed{\langle\varepsilon\rangle = \frac{2}{3\pi}\rho f_w U_b^3}$$

**Note the convention carefully:** $U_b$ here is the peak (amplitude) near-bed orbital velocity, not the RMS value — mixing the two silently introduces a $2\sqrt2\approx2.83\times$ error, which is exactly the kind of unit-convention bug that's invisible until your dissipation looks wrong by "some factor" you can't immediately place. If you're pulling $f_w$ or $U_{rms}$ values from a paper (Section 5.2), check which convention that specific paper uses before plugging into this formula.

---

## 5. Coral reefs: the specific case of "something under the water"

### 5.1 Reef cross-shore anatomy

A real reef profile is not a flat bed — it's a distinct sequence, and each zone does something different to the wave:

1. **Fore reef** — steep slope (often 1:5–1:20), deep-to-shallow transition. This is where shoaling begins and refraction bends incoming rays.
2. **Reef crest** — the shallowest point, frequently < 1 m deep. Depth-limited breaking (Section 3.3) concentrates here; this is where most wave *height* is lost.
3. **Reef flat** — shallow (~0.5–2 m), extremely rough due to coral structure. This is where most of the remaining wave *energy* is lost, continuously, via bottom friction rather than a single sharp break.
4. **Back-reef lagoon** — deeper, calmer, receives the residual current driven by the wave setup generated at the crest (Section 5.3).

For a simulation, this means your depth function across a reef should be a nonuniform profile with a distinct shallow shelf, not a smooth monotonic ramp — the "shelf" shape is what produces the characteristic crest-break-then-calm-lagoon visual signature.

### 5.2 Two frameworks for reef roughness — and which one to actually use

**(a) Bottom-roughness / friction-factor framework (simplest, and what you should use for a large-area simulation).** Treat the reef as a rough flat bed characterized by a single hydraulic roughness length $k_w$ (or equivalent friction factor $f_w$), plugged straight into the $\langle\varepsilon\rangle$ formula above. This is the dominant approach in the actual reef-hydrodynamics literature because it's simple and it works. Real measured values, pulled directly from field studies (not textbook defaults):

| Site / study | Quantity | Value |
|---|---|---|
| Sandy seafloor (typical) | $f_w$ | ~0.01–0.05 |
| Kaneohe Bay reef flat (Lowe et al. 2005) | $k_w$ | 0.16 ± 0.03 m |
| Kaneohe Bay + Moorea fore-reef (Lowe et al. 2005; Monismith et al. 2013) | $f_w$ | ~0.2–0.3 |
| Broad field survey across reef sites (Sous et al. 2023 review) | $f_w$ | 0.05–0.4 typical range |
| "Remarkably rough" reef, Monismith et al. 2015 | $f_w$ | 1.80 ± 0.07 (an order of magnitude above typical) |
| Barrier reef lagoon, radar-derived (Navarro et al. 2021) | $k_w$ | 0.20–0.30 m |

For scale: sandy-bottom $f_w$ is ~0.01–0.05. Reef-flat $f_w$ is routinely 5–10× that, and outlier reefs go to nearly 100× a sandy bed. **This roughness is *why* coral reefs are such effective natural breakwaters** — a widely cited real-world figure is that a healthy reef can dissipate up to **~97% of incident wave energy** before it reaches the shoreline (Ferrario et al., 2014, cited in Monismith et al. 2015).

**(b) Canopy-drag framework (only if you're resolving individual coral colonies).** Treat the coral canopy as an array of drag/inertia elements using a Morison-type force per unit volume:

$$F = \tfrac12\rho C_d\, a_f\,|u|u + \rho C_m V_f \frac{du}{dt}$$

where $a_f$ is frontal area per unit canopy volume (depends on coral morphology and colony density), $C_d\sim1$–2 (bluff-body drag), $C_m\sim1$–2 (added mass).

**My call, stated plainly:** for an ocean-scale simulation where the reef is a feature of the seafloor, not the subject of a close-up shot of individual coral heads, use framework (a) — a per-cell roughness/friction map painted onto your bathymetry, feeding the quadratic dissipation formula. Framework (b) only earns its much higher cost if you're doing near-field CFD/SPH resolving actual coral-head geometry (a fundamentally different, much smaller-scale simulation than "waves crossing a reef").

### 5.3 Wave setup and reef-driven circulation

As waves break across the crest, the momentum they were carrying (their **radiation stress**, $S_{xx}$) drops sharply. That momentum has to go somewhere — it pushes water level up over the reef flat (**wave setup**) and drives a mean current that flushes out through reef channels. The governing balance (Longuet-Higgins & Stewart, 1962/1964):

$$\frac{d\bar\eta}{dx} = -\frac{1}{\rho g h}\frac{dS_{xx}}{dx}, \qquad S_{xx} = E\left[\frac{2kh}{\sinh(2kh)}+\frac12\right]$$

($S_{xx}\to E/2$ in deep water, $\to\tfrac32 E$ in shallow water.) Physically: sharp drop in $E$ at the breaking crest → sharp drop in $S_{xx}$ → landward pressure gradient → water piles up on the reef flat → drives outflow through any gap in the reef.

**This is the single most important architectural decision to make before you start coding**, so I'll say it as bluntly as the physics allows: a pure surface-displacement method (Gerstner waves, Tessendorf FFT — Section 8) has **no mechanism to produce this**. Those methods displace existing water in place; they carry no net mass transport and therefore cannot generate a setup-driven current. If you want water to visibly pile up against a reef and drain through a channel, you need an actual shallow-water-equations solve (Section 7) forced by the $dS_{xx}/dx$ gradient computed from your wave field. If you only need the *visual* signature of breaking and a calmer lagoon (no real current), the cheaper heightfield approach in Section 8 is sufficient and you should stop there. Decide which one you're building before you write any code — retrofitting currents onto a pure heightfield renderer later means starting over.

### 5.4 The biology-physics coupling, briefly

Roughness isn't a fixed geological constant — it's a property of the living reef structure. Field studies on reef degradation (bleaching, physical damage, sea-level-rise interaction) show measurably *reduced* hydraulic roughness and therefore reduced wave dissipation once coral structure is lost (see the Buccoo Reef, Tobago study and the coral-rugosity/coastal-protection study in References). If you want a "living reef" system where reef health parametrically controls wave behavior — a legitimately interesting simulation feature — the physically grounded way to do it is to make $f_w$ (or $k_w$) a function of a reef-health scalar per cell, decaying $f_w$ toward the sandy-bottom value as health drops toward zero.

---

## 6. Turbulence and boundary layers — how much you actually need

Full turbulence closure (RANS, LES, DNS) exists on a cost ladder: DNS resolves every eddy (infeasible above lab scale, per Section 1.2), LES resolves large eddies and models small ones, RANS models the whole turbulence spectrum statistically. **For a large-area wave/reef simulation, you don't need any of these directly** — the entire effect of bottom-generated turbulence is already folded into the empirical friction factor $f_w$ from Section 5.2. You only need explicit RANS/LES if you're doing near-field CFD around individual coral heads or resolving the aerated, turbulent interior of an actual breaking wave crest (tools like OpenFOAM + waves2Foam target exactly this, and are not real-time). Know that this is the corner you're cutting when you use a friction-factor approach — it's the right corner to cut for anything larger than a single breaking wave close-up.

---

## 7. From physics to solvable equations: picking your model

| Method | Solves | Bathymetry-aware? | Breaking? | Cost | Best use |
|---|---|---|---|---|---|
| Full 3D Navier–Stokes (RANS/LES + VOF) | Ground truth | Yes | Yes | Very high | Validation, small near-reef domains, offline hero shots |
| **Shallow Water Equations** (Saint-Venant) | Depth-averaged continuity + momentum | Yes, natively | Approximated (bore-capturing / eddy viscosity) | Moderate | **Recommended core solver for anything touching the reef bathymetry** — real-time large-area sim, setup, currents |
| Potential-flow spectral (Airy sum / Gerstner / Tessendorf FFT) | Deep-water surface only | No (needs bolt-on) | No (needs a hack) | Very low | Open-ocean far field, deep water only |
| SPH / particle Navier–Stokes | Full 3D free surface incl. splash | Yes (boundary particles) | Yes, naturally | High | Close-up spray, foam, breaking crest detail |
| Boussinesq-type (FUNWAVE, XBeach) | Weakly-dispersive, weakly-nonlinear depth-integrated | Yes | Yes (built-in) | Moderate–high | Research-grade nearshore/reef fidelity; what actual reef scientists run; not typically real-time |

**Single-winner call:** there is no single correct answer across the whole domain, because the deep ocean and the reef genuinely need different physics — but there is a correct *pipeline*, given in Section 9. Don't try to make one method (especially a pure spectral/Gerstner method) cover both regimes; it structurally can't, per Section 5.3.

### 7.1 The Shallow Water Equations, for reference

Depth-integrating the Navier–Stokes continuity and momentum equations over the water column (assuming vertical velocity and pressure variation are hydrostatic — valid once wavelength ≫ depth) gives the Saint-Venant equations, in conservative form for depth $H=h+\eta$ and depth-averaged velocity $(u,v)$:

$$\frac{\partial H}{\partial t} + \frac{\partial(Hu)}{\partial x} + \frac{\partial(Hv)}{\partial y} = 0$$

$$\frac{\partial(Hu)}{\partial t} + \frac{\partial}{\partial x}\!\left(Hu^2+\tfrac12 gH^2\right) + \frac{\partial(Huv)}{\partial y} = -gH\frac{\partial h_b}{\partial x} - \frac{\tau_{b,x}}{\rho} + F_{wave,x}$$

(with the symmetric $y$-momentum equation). The $-gH\,\partial h_b/\partial x$ term is exactly how a bathymetry map — your reef — enters the equations; $\tau_b$ is exactly the friction term from Section 4.5/5.2; $F_{wave}$ is the radiation-stress forcing from Section 5.3 if you're driving currents from breaking waves. This is why SWE, not a pure heightfield method, is the right base solver once bathymetry effects matter.

---

## 8. Building the actual wave field (procedural / graphics methods)

### 8.1 The naive option: single Airy sinusoid

$$\eta(x,t) = a\cos(kx-\omega t)$$

Physically correct for small amplitude, but visually wrong for a "real" ocean — it's symmetric, and real wind waves have sharper crests and flatter troughs.

### 8.2 Gerstner (trochoidal) waves — an *exact* nonlinear solution

Unlike Stokes waves (a perturbative approximation), the Gerstner/trochoidal wave is an **exact solution of the Euler equations** for deep water (Gerstner, 1802; rediscovered by Rankine, 1863) — it's just rotational (has vorticity) rather than potential flow, which is a real physical trade-off, not a numerical shortcut. In the form standard in real-time graphics (Unity/Unreal/Godot ocean shaders all use this), for a single wave with direction $\hat{D}$, wavenumber $k=2\pi/L$, amplitude $A$, and steepness parameter $Q\in[0,1]$:

$$\theta = k(\hat{D}\cdot P) - \omega t$$
$$P'_x = P_x + Q A \hat D_x \cos\theta,\qquad P'_z = P_z + Q A \hat D_z \cos\theta,\qquad P'_y = A\sin\theta$$

with $\omega=\sqrt{gk}$ (deep water) or the full dispersion relation if depth matters. $Q$ controls crest sharpness: $Q=0$ is a plain sinusoid, $Q\to1$ approaches the sharp, pointed trochoidal crest that real steep waves actually show.

**A real, documented failure mode, not a hypothetical one:** summing multiple large-amplitude Gerstner waves can make the surface fold over itself (the parametric mapping stops being one-to-one) once $\sum Q_i A_i k_i$ exceeds about 1 across overlapping components — this is precisely the problem that makes "find the height at a given world-space (x,z)" require solving a transcendental equation with no closed form (confirmed independently in production graphics forum threads — see References). Practical mitigation: keep the steepness budget $\sum Q_i \le 1$, or switch to the spectral method below once you need many wave components, which sidesteps the problem entirely.

### 8.3 Statistical wave spectra — for a realistic irregular sea

These describe the *statistical* distribution of wave energy across frequency for a real sea state, and are what you sample from to generate a convincing ocean rather than a few clean sinusoids.

**Phillips spectrum** (the one Tessendorf's original method used):
$$P_h(\mathbf{k}) = A\,\frac{e^{-1/(kL)^2}}{k^4}\,|\hat{\mathbf{k}}\cdot\hat{\mathbf{w}}|^2\, e^{-k^2\ell^2}$$
where $L=V^2/g$ (largest wave sustainable by wind speed $V$), $\hat{\mathbf{w}}$ is wind direction (the dot-product term suppresses waves misaligned with the wind), and $\ell$ is a small cutoff length suppressing unwanted sub-grid ripples.

**Pierson–Moskowitz spectrum** (fully-developed sea, one free parameter — wind speed only):
$$S_{PM}(\omega) = \frac{\alpha g^2}{\omega^5}\exp\!\left[-\beta\left(\frac{\omega_0}{\omega}\right)^4\right],\quad \alpha=8.1\times10^{-3},\ \beta=0.74,\ \omega_0=g/U_{19.5}$$

**JONSWAP spectrum** (fetch-limited developing sea — usually the better visual default, since most real seas aren't fully developed):
$$S_{JS}(\omega) = \frac{\alpha g^2}{\omega^5}\exp\!\left[-\frac{5}{4}\left(\frac{\omega_p}{\omega}\right)^4\right]\gamma^{\exp\left[-\frac{(\omega-\omega_p)^2}{2\sigma^2\omega_p^2}\right]}$$
with $\sigma=0.07$ ($\omega\le\omega_p$) or $0.09$ ($\omega>\omega_p$), and peak-enhancement factor $\gamma=3.3$ as the standard default (range 1–7 in practice; **setting $\gamma=1$ makes JONSWAP mathematically identical to Pierson–Moskowitz** — useful to know if you want one code path for both).

**Practical recommendation:** expose significant wave height $H_s$ and peak period $T_p$ as your user-facing sliders (they're intuitive), not raw $\alpha$. A standard engineering fit (Houmb & Overvik, via DNV/WAFO) backs out $\alpha$ from those:
$$\alpha \approx 5.061\,\frac{H_s^2}{T_p^4}(1-0.287\ln\gamma)$$

### 8.4 Tessendorf's FFT ocean method — the actual industry standard

This is the method behind *Waterworld*, *Titanic*, and essentially every AAA-game ocean since (Tessendorf, 2001/2004). It beats direct Gerstner summation because it's $O(N^2\log N)$ for an $N\times N$ grid via FFT, versus $O(N^2 M)$ for $M$ summed component waves — and it sidesteps the self-intersection problem from Section 8.2 entirely. Full recipe:

1. Choose a grid resolution $N\times N$ and physical patch size.
2. For each discrete wavevector $\mathbf{k}$, draw a fixed base spectral amplitude:
$$\tilde h_0(\mathbf{k}) = \frac{1}{\sqrt2}(\xi_r+i\xi_i)\sqrt{P_h(\mathbf{k})}$$
   with $\xi_r,\xi_i\sim\mathcal N(0,1)$ — this is your one-time random seed field.
3. Evolve in time using the dispersion relation $\omega(k)=\sqrt{gk}$ (or the full $\sqrt{gk\tanh(kh)}$ if depth matters):
$$\tilde h(\mathbf{k},t) = \tilde h_0(\mathbf{k})e^{i\omega(k)t} + \tilde h_0^*(-\mathbf{k})e^{-i\omega(k)t}$$
4. Inverse-FFT to get the height field: $h(\mathbf{x},t)=\sum_{\mathbf k}\tilde h(\mathbf k,t)e^{i\mathbf k\cdot\mathbf x}$.
5. **Choppy/Gerstner-like crests (Tessendorf's key trick):** also inverse-FFT a horizontal displacement field, obtained by multiplying the spectrum by $-i\hat{\mathbf k}$ before transforming. Displace each surface point by $(\lambda D_x,\, h,\, \lambda D_z)$ with a user "choppiness" scalar $\lambda$. This turns the smooth Gaussian heightfield into the sharp-crested look of a real sea, at FFT cost, across the *whole* domain — this is the entire reason the method won over brute-force Gerstner summation for film-quality water.
6. Get normals for free from the same frequency-domain data (multiply by $i\mathbf k$ before the inverse transform) — avoids finite-difference normal artifacts.
7. Layer multiple resolutions (cascades) at different patch sizes to get both long swells and fine capillary-scale ripples without one absurdly large FFT.

### 8.5 Directional spreading

The 1D spectra above (PM, JONSWAP) describe frequency content only. Multiply by a directional spreading function $D(\theta,\omega)$ (commonly $\cos^{2s}[(\theta-\theta_{mean})/2]$) to get a realistic short-crested sea instead of perfectly parallel swell lines. The $|\hat{\mathbf k}\cdot\hat{\mathbf w}|^2$ term in the Phillips spectrum is a crude built-in version of this.

---

## 9. Coupling it all together: waves meeting a reef, as an actual pipeline

This is the concrete, opinionated architecture, given the physics above.

1. **Represent bathymetry** as a heightfield $h(x,y)$ (depth, negative underwater) plus a per-cell roughness class (sand / rubble / reef-flat / reef-crest) mapped to $f_w$ or $k_w$ using the table in Section 5.2.

2. **Deep water, everywhere the deep-water criterion holds** ($kh>\pi$ for the dominant wavelength, i.e., depth exceeds roughly half a wavelength — Section 2.3): run the Tessendorf FFT method (Section 8.4) unmodified. This region is provably blind to the bottom, so full deep-water visual realism is free here.

3. **At the boundary where local depth first violates that criterion**, per grid cell:
   - Solve local $k(x,y)$ from $\omega^2=gk\tanh(kh)$ using the Fenton–McKee closed form (Section 2.2) — cheap enough to evaluate per cell per frame, or precompute as a depth-indexed lookup table since $\omega$ is fixed per spectral component.
   - Apply shoaling: local amplitude scales by $\sqrt{c_{g,deep}/c_g(x,y)}$ (Section 4.1).
   - Bend the local wave-direction field using the gradient of $k$ (a cheap real-time stand-in for full ray tracing) to reproduce refraction around the reef (Section 4.2).
   - Check both breaking criteria every cell (Section 3.3) and trigger a foam/dissipation state when either binds — **and actually remove the corresponding energy from the wave field into a decaying foam/dissipation buffer, rather than silently clamping height.** Silently deleting energy at the reef edge (the wave just flattens with no visible break) is the single most common and most visually obvious bug in an amateur wave-reef sim.
   - Apply continuous frictional dissipation over reef-flat cells using $\langle\varepsilon\rangle=\frac{2}{3\pi}\rho f_w U_b^3$ (Section 4.5) — this is what keeps a *residual* chop alive across a wide reef flat instead of the water going instantly flat right after the crest break.
   - **Decide explicitly whether you need real reef-driven currents** (visible setup/lagoon flushing, Section 5.3) or only the visual break/chop transition. If you need currents, you need an actual 2D Shallow Water Equations solve (Section 7.1) forced by the cross-shore gradient of radiation stress from your wave field — a heightfield-only method cannot produce this, structurally, no matter how it's tuned. If you only need the visual signature, the heightfield recipe above is sufficient and considerably cheaper. Make this call before writing code; it determines your entire architecture.

4. **Very close to the actual breaking crest or wherever you want hero-shot detail** (splash, spray, foam advection), hand off to SPH or a local VOF Navier–Stokes solve in a small domain, using the SWE solution's surface elevation and velocity as the inflow boundary condition. This is the same "nested fidelity" trick used both in production VFX and in real coastal engineering (a regional SWAN model feeds a local Boussinesq/XBeach nearshore model, which in turn feeds a CFD model of a single structure) — there's no shortcut around needing different physics at different scales, so lean into it rather than fighting it with one solver stretched too far.

---

## 10. Parameter reference table

| Quantity | Value |
|---|---|
| $\rho$, seawater | ≈ 1025 kg/m³ |
| $\rho$, freshwater (20 °C) | ≈ 998 kg/m³ |
| $\nu$, seawater (20 °C) | ≈ 1.05×10⁻⁶ m²/s |
| $\sigma$, seawater surface tension | ≈ 0.074 N/m |
| $g$ | 9.81 m/s² |
| Capillary–gravity crossover $\lambda_c$ | ≈ 1.7 cm |
| Deep-water breaking steepness limit | $H/\lambda \approx 1/7 \approx 0.142$ |
| Breaker index $\gamma$ (depth-limited) | ≈ 0.78 typical open beach; varies ~0.4–1.0+ on reefs |
| JONSWAP defaults | $\gamma=3.3$, $\sigma_a=0.07$, $\sigma_b=0.09$ |
| Pierson–Moskowitz constants | $\alpha=8.1\times10^{-3}$, $\beta=0.74$ |
| $f_w$, sandy seafloor | ~0.01–0.05 |
| $f_w$, typical coral reef flat | ~0.2–0.3 |
| $f_w$, broad reef field range | 0.05–0.4 |
| $f_w$, extreme rough-reef outlier | up to ~1.8 |
| $k_w$, Kaneohe Bay reef flat | 0.16 ± 0.03 m |
| $k_w$, barrier-reef lagoon (radar-derived) | 0.20–0.30 m |
| Max wave-energy dissipation by a healthy reef | up to ~97% (Ferrario et al. 2014) |
| Fore-reef slope, typical fringing reef | 1:5 – 1:20 |

---

## 11. References

Formulas and figures above are drawn from and cross-checked against:

- Hasselmann, K. et al. (1973). JONSWAP spectrum — original Joint North Sea Wave Project formulation.
- Pierson, W.J. & Moskowitz, L. (1964). *J. Geophys. Res.* — Pierson–Moskowitz spectrum.
- Fenton, J.D. & McKee, W.D. (1990). "On calculating the lengths of water waves." *Coastal Engineering*, 14, 499–513.
- Eckart, C. (1952). "The propagation of gravity waves from deep to shallow water." NBS Circular 521.
- Longuet-Higgins, M.S. & Stewart, R.W. (1962, 1964). Radiation stress papers — *J. Fluid Mech.* 13; *Deep-Sea Research* 11.
- Battjes, J.A. (1974). "Surf similarity" — Iribarren number, ICCE.
- Lowe, R.J. et al. (2005). "Spectral wave dissipation over a barrier reef." *J. Geophys. Res.* — Kaneohe Bay $k_w$, $f_w$ data.
- Monismith, S.G. et al. (2013, 2015). Moorea fore-reef and "remarkably rough reef" (Ofu, American Samoa) friction-factor studies.
- Sous, D. et al. (2023). "Spectral wave dissipation over a roughness-varying barrier reef." *Geophysical Research Letters* — $f_w$ field-range review.
- Navarro, O. et al. (2021). "Wave energy dissipation in a shallow coral reef lagoon using marine X-band radar data." *J. Geophys. Res.: Oceans*.
- Ferrario, F. et al. (2014). Reef wave-energy-dissipation figure (~97%), as cited in Monismith et al. (2015).
- Frontiers in Marine Science (2025/2026). Buccoo Reef, Tobago — reef degradation, sea-level rise, and roughness/protection modeling.
- Tessendorf, J. (2001/2004). "Simulating Ocean Water," SIGGRAPH Course Notes — FFT/Phillips-spectrum ocean synthesis.
- Gerstner, F.J. (1802); Rankine, W.J.M. (1863) — trochoidal wave, as reviewed on Wikipedia ("Trochoidal wave") and in Soloviev & Lukas-style derivations.
- Marques, O. et al. (2025). "An effective water depth correction for pressure-based wave statistics on rough bathymetry." *J. Atmos. Ocean. Technol.*
- Wikipedia: "Airy wave theory," "Iribarren number," "Radiation stress," "Pierson–Moskowitz spectrum," "Trochoidal wave" — used for cross-checking standard forms.

All formulas involving physical constants or field-measured coefficients above were independently re-derived or limit-checked where marked in text (dispersion-relation approximations, Stokes drift, and the bottom-friction dissipation coefficient) rather than taken on faith from a single source.
