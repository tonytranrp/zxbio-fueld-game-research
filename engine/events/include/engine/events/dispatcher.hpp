#pragma once

#include <entt/signal/dispatcher.hpp>

namespace engine::events {

// The ONE shared dispatch mechanism (modular-architecture reference §1) — a thin alias, not a
// wrapper: entt::dispatcher already has exactly the right API surface (trigger<T> for immediate
// synchronous dispatch, enqueue<T> + update() for deferred), and wrapping it would only obscure
// EnTT's own documentation. EnTT is already a pinned dependency; this adds nothing new.
//
// Event-vs-polling convention (ENGINE_HARDENING_BRIEF.md Group L task 34):
//
//   Reach for an EVENT when state *changes* somewhere and one-or-more *other* systems need to
//   react to the change itself — chunk loaded/unloaded/mesh-ready (chunk_events.hpp), a future
//   save system reacting to chunk-modified, a future audio system reacting to biome transitions.
//   The emitter must not know or care who listens; listeners must not need the emitter's
//   internals. Deliverable is the *transition*, not the current value.
//
//   Keep POLLING when a consumer needs a *current value* every frame regardless of whether it
//   changed — fps, frame time, GPU byte counters, camera position. Turning a per-frame read into
//   an event stream just moves the same coupling behind extra machinery.
//
//   Threading: this dispatcher is NOT thread-safe. trigger()/enqueue()/update() from the main
//   thread only — worker threads hand results to the main thread through the existing completion
//   queues first (app/src/chunk_streaming.hpp's threading model), and events fire during the
//   main-thread drain. That rule is what makes the alias sufficient; revisit only if an emitter
//   genuinely can't route through a main-thread drain.
using Dispatcher = entt::dispatcher;

} // namespace engine::events
