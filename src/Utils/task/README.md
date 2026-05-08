# Utils/task — Parallel Task Execution

Wraps `Taskflow` for async task scheduling.

## Architecture

```
Utils/task/
├── TaskUtils.hpp   ← TaskSystem: async, waitForAll
└── TaskUtils.cpp   ← Static executor initialization
```

## Coding Standards

### 1. Use async() for Fire-and-Forget

```cpp
TaskSystem::async([]() {
    // Heavy computation off the main thread
    doExpensiveCalculation();
});
```

### 2. Always Wait Before Shutdown

```cpp
TaskSystem::waitForAll();  // Blocks until all tasks complete
```

Call this in `App::shutdown()` before destroying resources that tasks might reference.

### 3. Don't Share Mutable State

Tasks run on worker threads. Use:
- Value captures (copy)
- `std::shared_ptr` for shared immutable data
- Mutex-protected state if mutation is required

```cpp
// ✅ Safe — copies the data
auto data = std::make_shared<GameData>(loadData());
TaskSystem::async([data]() { process(*data); });

// ❌ Dangerous — captures a reference to stack data
GameData data;
TaskSystem::async([&data]() { process(data); });  // data may be destroyed
```

### 4. Types

- Task functions: `std::function<void()>` or lambdas
- Executor: `tf::Executor` (wrapped, don't use directly)

## Templates

`TaskSystem::async()` is a function template that accepts any callable:

```cpp
template<typename Func>
static auto async(Func&& func);
```

It forwards to `tf::Executor::async()` and returns a `std::future`. The template is necessary to support arbitrary lambdas — this is the correct use of templates at the utility boundary.
