# engine/tasks

Reusable task/job utilities live here.

`TaskManager` is the narrow engine wrapper over Taskflow. Game and screen code should schedule work through this wrapper rather than including or exposing Taskflow types directly.

Thread boundary rules:

- Background tasks may do CPU work, filesystem checks, data parsing, and pure value preparation.
- Background tasks must not call Raylib `Load*`, `Unload*`, `Draw*`, audio-device, window, or GPU APIs.
- Background tasks should return plain data and let the main/render thread commit results into runtime services.

Cancellation rules:

- Each scheduled task receives its own `std::stop_token`; `cancel(TaskId)` requests stop for one task, and `cancelAll()` requests stop for the currently active task set.
- `cancelAll()` must not poison future schedules. New work scheduled after a cancellation gets a fresh stop source.
- Terminal state is worker-owned for normal cancellation: cooperative tasks become `Cancelled` when they exit after observing stop, and non-cooperative tasks must not be marked terminal while still running.
- `shutdown()` is the only broad cleanup path that may force active records to `Cancelled` after waiting for the executor.
