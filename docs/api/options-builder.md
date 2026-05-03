# FreyrOptionsBuilder

`fr::FreyrOptionsBuilder` configures Freyr's runtime parameters. It is used inside the `WithOptions` callback of `FreyrExtension`:

```cpp
freyr.WithOptions([](fr::FreyrOptionsBuilder& opts) {
    opts.WithMaxEntities(1'000'000)
        .WithArchetypeChunkCapacity(512)
        .WithThreadCount(8);
});
```

All methods return `*this` for chaining. Unset options fall back to the defaults shown below.

---

## Methods

### `WithMaxEntities(n)`

Sets the maximum number of live entities allowed simultaneously.

| Parameter | Type | Default |
|-----------|------|---------|
| `n` | `size_t` | `1 048 576` (1 M) |

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

### `WithAllPhysicalCores()`

Configures the engine to utilize all available physical CPU cores, excluding logical processors created by SMT (Hyper-Threading).

```cpp
opts.WithAllPhysicalCores();
```

---

## Default values summary

| Option | Default |
|--------|---------|
| `MaxEntities` | 1 048 576 |
| `ArchetypeChunkCapacity` | 512 |
| `ThreadCount` | 4 |
