# engine

Reusable runtime code lives here: core types, typed registries, services,
events, graphics, media, animation, input, window helpers, and the screen stack.
Optional runtime integrations, such as camera-based hand tracking, live under
feature-gated subsystem folders and must compile to a no-op when disabled.

Engine code should not depend on concrete game screens except through typed
registries and explicit template parameters owned by the UI stack.
