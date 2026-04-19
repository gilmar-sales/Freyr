# FreyrOptionsBuilder

`fr::FreyrOptionsBuilder` configures Freyr's runtime parameters. It is used inside the `WithOptions` callback of `FreyrExtension`:

```cpp
freyr.WithOptions([](fr::FreyrOptionsBuilder& opts) {
    opts.WithMaxEntities(1'000'000)
        .WithArchetypeChunkCapacity(512)
        .WithThreadCount(8)
        .WithFixedDeltaTime(1.0f / 60.0f)
        .WithExecutionStrategy(fr::FreyrExecutionStategy::ChunkAffinity);
});
```

All methods return `*this` for chaining. Unset options fall back to the defaults shown below.

---

## Methods

### `WithMaxEntities(n)`

Sets the maximum number of live entities allowed simultaneously.

| Parameter | Type | Default |
|-----------|------|---------|
| `n` | `size_t` | `16 777 216` (16 M) |

```cpp
opts.WithMaxEntities(500'000);
```

---

### `WithArchetypeChunkCapacity(n)`

Sets the number of entities per archetype chunk. This is the primary knob for task granularity.

| Parameter | Type | Default |
|-----------|------|---------|
| `n` | `size_t` | `512` |

```cpp
opts.WithArchetypeChunkCapacity(1024);
```

!!! tip "Tuning"
    Smaller values → more chunks → finer parallelism, higher scheduling overhead.
    Larger values → fewer chunks → better sequential throughput, coarser parallelism.
    Start with 512, then benchmark with 128, 256, 1024, 4096.

---

### `WithThreadCount(n)`

Sets the number of worker threads in the task pool.

| Parameter | Type | Default |
|-----------|------|---------|
| `n` | `size_t` | `4` |

```cpp
opts.WithThreadCount(std::thread::hardware_concurrency());
```

---

### `WithExecutionStrategy(strategy)`

Selects how chunk tasks are distributed across worker threads.

| Parameter | Type | Default |
|-----------|------|---------|
| `strategy` | `FreyrExecutionStategy` | `ChunkAffinity` |

```cpp
opts.WithExecutionStrategy(fr::FreyrExecutionStategy::DispatchOrder);
```

| Value | Description |
|-------|-------------|
| `ChunkAffinity` | Chunks are preferentially processed by the same worker thread frame-over-frame, maximising cache reuse |
| `DispatchOrder` | Chunks are dispatched in creation order; simpler, more predictable scheduling |

---

## Default values summary

| Option | Default |
|--------|---------|
| `MaxEntities` | 16 777 216 |
| `ArchetypeChunkCapacity` | 512 |
| `ThreadCount` | 4 |
| `ExecutionStrategy` | `ChunkAffinity` |
