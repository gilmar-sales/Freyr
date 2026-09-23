# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@AGENTS.md

@PATTERNS.md

## Additional notes

### Toolchain

- Requires a C++26 compiler **with reflection support** (GCC 16+ or Clang 22+) and CMake 3.29+. On Windows the project is built with MinGW GCC.
- `ccache` is picked up automatically in standalone builds.
- Standalone builds also build `examples/` and `benchmarks/` (`FREYR_BUILD_EXAMPLES`, `FREYR_BUILD_BENCHMARKS`). Turn them off to speed up iteration on the library + tests.
- All tests compile into a single `Tests_run` executable (every `test/src/**.cpp` is globbed in), registered with CTest via `gtest_discover_tests`. Library sources are also globbed (`src/**.cpp`, `include/**.hpp`), so re-run the CMake configure step after adding new files.

### Benchmarks

Google Benchmark targets live in `benchmarks/<Name>/`, e.g.:

```bash
cmake --build [build_dir] --target HierarchyTransformBench
./[build_dir]/benchmarks/HierarchyTransform/HierarchyTransformBench --benchmark_filter=Propagate --benchmark_repetitions=5
```

`benchmarks/_compare_baselines.py` compares benchmark runs against baselines.

### Frame update order (`Registry::Update`, `src/Core/Registry.cpp`)

1. Flush `EventManager` (merge pending listeners), advance component change-detection tick, start worker threads.
2. `SystemManager::Accumulate(dt)` decides which pipelines are ready based on their `WithRate` (Hz; `0` = every frame).
3. For each of `PreUpdate`, `Update`, `PostUpdate`: run the phase on ready pipelines → wait for all thread-pool tasks → flush hierarchy component sync → apply pending mutations (`ComponentManager::ExecutePendingMutations`) → process deferred destroys (hierarchy expands the destroy set to descendants first).
4. Stop workers, flush observers.

Consequences: structural changes (add/remove component, destroy) made during a phase become visible only at the end of that phase; `EachAsync` work is not complete until the phase's `WaitForAllTasks` (outside `Update`, call `registry->ExecuteTasks()`).

### Query vs. Mutation

- `registry->CreateQuery()` — read-only access (`Count`, `EntitiesWith`, iteration).
- `registry->CreateMutation()` — write access; `Each` runs synchronously, `EachAsync` dispatches one task per archetype chunk to the `ThreadPool`.
- Filters (`Core/Filter.hpp`) and change detection (`Core/ComponentTicks.hpp`) narrow iteration; `Disabled` and `Prefab` tag components (`Base/Tags.hpp`) are excluded by default (opt in with `IncludingDisabled()` / `IncludingPrefabs()`; register them via `WithDisabled()` / `WithPrefabs()`).

### Dependency injection / app setup

Freyr runs as a Skirnir extension: `skr::ApplicationBuilder().AddExtension<fr::FreyrExtension>(...)`. Systems are DI singletons whose constructor dependencies (e.g. `skr::Arc<fr::Registry>`, `skr::Arc<fr::EventManager>`, resources) are injected by Skirnir. `fr::World::Create(configure)` (`Core/World.hpp`) wraps this for standalone/multiple independent registries. Tests build an app around `test/src/EmptyApp.hpp`.

### Other subsystems

- **Hierarchy** (`include/Freyr/Hierarchy/`): non-fragmenting parent/child via `ChildOf` + a side-table in `HierarchyManager`; destroying a parent cascades to children. Transform propagation is policy-based (`Hierarchy/Policies/`, e.g. `Mat4TransformPolicy`) and enabled via `WithHierarchyPropagation<Policy>()`. See `docs/concepts/hierarchy.md`.
- **Resources** (`Core/ResourceManager.hpp`), **Prefabs**, **Worlds**, **Serialization** (`Serialization/Snapshot`), **Change detection** — each has a concept page under `docs/concepts/`.
- **Profiling**: `FREYR_TRACE_BEGIN/END` macros (`Core/Profiling.hpp`) compile to no-ops unless `FREYR_PROFILING=ON`.

### Tests

- Shared test components and systems live in `test/src/Components/` and `test/src/Systems/`; reuse them before adding new ones.
- Name regression tests after the broken behavior (e.g. `MatchShouldSucceedWhenOtherHasHigherPageBits`) and place them next to the existing specs for that module.
