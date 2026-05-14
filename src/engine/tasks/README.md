# engine/tasks

Reusable task/job utilities live here.

`TaskManager` is the narrow engine wrapper over Taskflow. Game and screen code should schedule work through this wrapper rather than including or exposing Taskflow types directly.

Thread boundary rules:

- Background tasks may do CPU work, filesystem checks, data parsing, and pure value preparation.
- Background tasks must not call Raylib `Load*`, `Unload*`, `Draw*`, audio-device, window, or GPU APIs.
- Background tasks should return plain data and let the main/render thread commit results into runtime services.
