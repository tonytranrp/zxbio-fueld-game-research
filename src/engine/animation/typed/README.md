# engine/animation/typed

Status: design sketch only -- no code in this folder implements the contract
described below yet.

Typed animation labels and policies live here. These are compile-time contracts
for animation names, default durations, and easing choices.

## Current contents

```text
engine/animation/typed/
`-- README.md
```

This folder currently holds only this README. The typed track tags and easing
policies described below are the intended contract for when concrete track
headers are added here.

## How to use it

Define a track tag with `Name` and `Duration`, then specialize
`EasingPolicy<TTag>` if the default linear easing is not right.

```cpp
namespace track {
struct FarmPanelOpen {
    static constexpr std::string_view Name = "farm.panel_open";
    static constexpr f32 Duration = 0.24f;
};
}

template<>
struct EasingPolicy<track::FarmPanelOpen> {
    static constexpr auto Function = Easing::easeOutCubic;
};
```

Register the tag in the local registry so compile-time checks can catch
duplicates and missing names.

## Coding standards

- Tags are data-only; no runtime state belongs here.
- Names use lowercase dotted labels, grouped by feature.
- Durations are in seconds as `f32`.
- Keep the registry sorted by feature area when it grows.
