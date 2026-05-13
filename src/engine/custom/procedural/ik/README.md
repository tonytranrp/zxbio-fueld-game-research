# engine/custom/procedural/ik

Inverse-kinematics primitives for procedural rigs live here.

## Current contents

```text
engine/custom/procedural/ik/
|-- FabrikSolver.hpp
|-- IkTypes.hpp
`-- JointLimits.hpp
```

## How to use it

The hand renderer and retargeting path use IK types to shape fingers and
mechanical joints. New procedural rigs should describe joints with local typed
limits and pass explicit chains into the solver.

```cpp
std::array<Vector3, FingerChainSpec<FingerId::Index>::jointCount> chain = makeIndexFingerChain();
auto result = FabrikSolver<FingerChainSpec<FingerId::Index>>::solve(chain, target, settings);
```

## Coding standards

- Solvers should be deterministic and allocation-light.
- Do not read input, camera, screen, or service state here.
- Keep chain tags typed and compile-time checked.
- Joint limits must be explicit and documented in degrees or radians.
- Keep general IK utilities reusable by future rigs, not only robot hands.
