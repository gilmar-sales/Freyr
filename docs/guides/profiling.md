# Profiling

Freyr integrates with [Perfetto](https://perfetto.dev) to produce detailed execution traces. Traces can be opened in the Perfetto UI to visualise system timings, chunk iteration durations, and thread utilisation.

---

## Enable at compile time

Define `FREYR_PROFILING` before building:

=== "CMake"

    ```cmake
    target_compile_definitions(my_app PRIVATE FREYR_PROFILING)
    ```

=== "Compiler flag"

    ```bash
    -DFREYR_PROFILING
    ```

When the flag is absent, all profiling calls are compiled out as no-ops with zero overhead.

---

## Basic usage

Wrap the section you want to trace with `BeginProfiling` / `EndProfiling`:

```cpp
void Run() override {
    mScene->BeginProfiling();

    for (int i = 0; i < 200; i++)
        mScene->Update(1.0f / 60.0f);

    mScene->EndProfiling(); // flushes the trace file to disk
}
```

`EndProfiling` writes a `.perfetto-trace` file to the working directory.

---

## Viewing the trace

1. Open [ui.perfetto.dev](https://ui.perfetto.dev) in Chrome or Edge
2. Click **Open trace file** and select the `.perfetto-trace` file
3. Navigate the timeline to inspect each system and chunk

You will see:

- One track per worker thread showing which chunks they processed
- Named spans for each labelled `ForEach` / `ForEachParallel` call
- System `PreUpdate` / `Update` / `PostUpdate` boundaries

---

## Custom spans

Add your own named spans around any code section:

```cpp
void Update(float dt) override {
    mScene->BeginTrace("CollisionBroadphase");
    runBroadphase();
    mScene->EndTrace();

    mScene->BeginTrace("CollisionNarrowphase");
    runNarrowphase();
    mScene->EndTrace();
}
```

Spans are nested under the calling thread's track in the Perfetto UI.

---

## Labelling iterations

All iteration methods accept an optional label that appears in the trace:

```cpp
mScene->ForEach<Position, Velocity>("Physics::Integrate", fn);
mScene->ForEachParallel<Position>("Render::CullFrustum", fn);
```

Without a label, the lambda's type name is used (often unreadable). Prefer explicit labels in any code you want to profile.

---

## Profiling example

The `examples/Profiling` directory contains a ready-to-run profiling scenario:

```cpp
mScene->BeginProfiling();

// 2M entities with Position only
mScene->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithEntities(2'000'000)
    .Build();

// 2M entities with Position + Velocity
mScene->CreateArchetypeBuilder()
    .WithComponent(Position {})
    .WithComponent(Velocity {})
    .WithEntities(2'000'000)
    .Build();

for (auto i = 0; i < 100; i++)
    mScene->Update(1.0f);

mScene->EndProfiling();
```

Build and run with:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFREYR_PROFILING=ON
cmake --build build --target freyr_profiling_example
./build/examples/Profiling/freyr_profiling_example
```

Then open the resulting trace in [ui.perfetto.dev](https://ui.perfetto.dev).

---

## Tips

- Profile **Release** builds — Debug builds have much higher per-entity overhead that distorts results
- Run multiple warm-up frames before the profiled section to avoid cold-cache skew
- Compare `ChunkAffinity` vs `DispatchOrder` for your specific workload
- Use the **Slice details** panel in Perfetto to see exact durations and thread assignments per chunk
