# FreyrOptionsBuilder

`fr::FreyrOptionsBuilder` configures Freyr's runtime parameters. Used inside the `WithOptions` callback of
`FreyrExtension`:

```cpp
freyr.WithOptions([](fr::FreyrOptionsBuilder& opts) {
    opts.WithMaxEntities(1'000'000)
        .WithArchetypeChunkCapacity(512)
        .WithThreadCount(8);
});
```

All methods return `*this` for chaining. Unset options fall back to the defaults.

---

## Methods

### `WithMaxEntities(n)`

Sets the maximum number of live entities allowed simultaneously. Pre-allocates the entity index map at startup.

**Signature:** `FreyrOptionsBuilder& WithMaxEntities(const size_t maxEntities)`

**Complexity:** $O(1)$ — stores the value; pre-allocation happens during Registry construction.

**Default:** `1 048 576` (1 Mi)

```cpp
opts.WithMaxEntities(500'000);
```

!!! warning "Hard limit"
    Attempting to create more entities than `MaxEntities` triggers a runtime assertion.
    The entity index map is sized to this value at startup, so don't set it too high for embedded/console targets.

---

### `WithArchetypeChunkCapacity(n)`

Sets the number of entities per archetype chunk. This is the primary knob for task granularity.

**Signature:** `FreyrOptionsBuilder& WithArchetypeChunkCapacity(const size_t archetypeChunkCapacity)`

**Complexity:** $O(1)$ — stored as configuration value.

**Default:** `512`

```cpp
opts.WithArchetypeChunkCapacity(1024);
```

!!! tip "Tuning chunk size"
    | Value | Effect |
    |-------|--------|
    | 128   | Fine-grained parallelism, high scheduling overhead |
    | 256   | Good load balancing for light callbacks |
    | 512   | **Recommended starting point** |
    | 1024  | Better for heavy callbacks (physics, AI) |
    | 4096  | Minimal overhead, poor for small entity counts |

    Start with **512**, then benchmark with 128, 256, 1024, 4096 for your workload.

See the [Performance Tuning guide](../guides/performance-tuning.md) for detailed guidance.

---

### `WithThreadCount(n)`

Sets the number of worker threads in the task pool.

**Signature:** `FreyrOptionsBuilder& WithThreadCount(const size_t threadCount)`

**Complexity:** $O(1)$ — thread creation happens at pool construction.

**Default:** `4`

```cpp
opts.WithThreadCount(std::thread::hardware_concurrency());
```

!!! tip "Thread count guidelines"
    - Value `0` is reserved for future auto-detection (currently defaults to 4)
    - Prefer physical core count: use `WithAllPhysicalCores()`
    - Reserve 1 core for the main thread if it does significant work

---

### `WithAllPhysicalCores()`

Configures the engine to utilize all available physical CPU cores, excluding logical processors created
by SMT (Hyper-Threading).

**Signature:** `FreyrOptionsBuilder& WithAllPhysicalCores()`

**Complexity:** $O(N)$ where N is the number of CPU packages — queries OS topology.

**Thread safety:** Not thread-safe — call during application setup.

```cpp
opts.WithAllPhysicalCores();
```

On a system with 8 physical cores (16 with HT), this sets thread count to **8** (not 16).

!!! note "Platform support"
    - **Windows:** Uses `GetLogicalProcessorInformation`
    - **macOS:** Uses `sysctlbyname("hw.physicalcpu")`
    - **Linux:** Parses `/proc/cpuinfo` for physical/core IDs

---

## Default values summary

| Option                  | Default    | Description                    |
|-------------------------|------------|--------------------------------|
| `MaxEntities`           | 1,048,576  | Maximum live entity count      |
| `ArchetypeChunkCapacity`| 512        | Entities per chunk             |
| `ThreadCount`           | 4          | Worker threads                 |

---

## Underlying options struct

The builder produces a `FreyrOptions` struct used internally:

```cpp
struct FreyrOptions {
    std::uint64_t MaxEntities            = 1024 * 1024;
    std::uint64_t ArchetypeChunkCapacity = 512;
    std::uint64_t MaxSystems             = 1024;
    std::uint64_t ThreadCount            = 4;
};
```

`MaxSystems` is currently fixed and not configurable via the builder. It controls the maximum number of
distinct system types that can be registered.
