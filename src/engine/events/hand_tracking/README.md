# engine/events/hand_tracking

Hand-tracking service event payloads live here.

## Current contents

```text
engine/events/hand_tracking/
|-- HandTrackingEvents.hpp
`-- HandTrackingEventModule.hpp
```

## How to use it

The vision service publishes camera consent, worker lifecycle, frame, hand-loss,
and gesture-change events. Screens may subscribe for UI feedback, but mapping
math should consume `HandTrackingFrame` snapshots through the service API.

```cpp
auto sink = Events::sink<typed::hand_tracking::FrameReceived>();
sink.connect<&MyTool::onFrame>(*this);
```

## Coding standards

- Events mirror service state changes; they should not own worker control flow.
- Payloads should use typed vision structs from `engine/vision/hand_tracking/`.
- Avoid publishing raw camera buffers through events.
- Register new event tags in `HandTrackingEventModule.hpp`.
