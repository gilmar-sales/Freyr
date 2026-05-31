# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Build Dir

You should figure out where's the build dir first, every user uses an different setup (find the `CMakeCache.txt` file)
If there's no build dir or multiple build dirs, asks user the preferred dir to use

## Build Commands

```bash
# Configure (from repo root)
cmake -B [build_dir]

# Build
cmake --build [build_dir] --config Debug

# Run tests
ctest --build-config Debug --rerun-failed --output-on-failure
./[build_dir]/test/Tests_run --gtest_filter="SceneSpec.*"
```

## Key Conventions

- Components inherit from `fr::Component` (data only, no logic, no virtual functions)
- `fr::Component` has a **protected virtual destructor** — do NOT make it public; only `Scene` accesses it
- Systems inherit from `fr::System`, override lifecycle hooks (`PreUpdate`, `Update`, `PostUpdate`, `FixedUpdate`, etc.)
- Register components via `FreyrExtension::WithComponent<T>()` before use
- Entity IDs are `uint64_t` — **never cast to signed** for comparison
- **`scene->DestroyEntity(e)` is deferred** — processed at end of `Update`
- **Never call `scene->Update` from within a `ForEach` callback**
- **`ForEach` callbacks must not throw** — unpredictable behavior in parallel execution
- Components must not hold owning raw pointers — use `Ref<T>` (Skirnir)
- Use `[[no_unique_address]]` for optional sub-object storage in components

## Build Options

- `FREYR_ASSERTIONS=ON` — enable runtime assertions (`FREYR_ASSERT` macro)
- `FREYR_PROFILING=ON` — enable Perfetto profiling
- `FREYR_BUILD_TESTS=ON` — build tests (set automatically in standalone builds)

## Code Style

- Format: `.clang-format` (Microsoft-based, column limit 120)
- C++ standard: C++23
- **No comments unless requested**
- Components: `PascalCase` structs (e.g., `struct Position`)
- Systems: `PascalCase` ending in `System` (e.g., `class MovementSystem`)
- Member variables: `mCamelCase` (e.g., `mScene`)

## Architecture

- **Scene** — central orchestrator holding EntityManager, ComponentManager, SystemManager, EventManager, TaskManager
- **Archetype** — group of entities sharing the same component set, divided into fixed-size **chunks**
- **Chunk** — unit of parallel work distribution; iteration happens per-chunk
- Entry point: `include/Freyr/Freyr.hpp`

## Dependencies

- **Skirnir** (v0.15.3) — fetched automatically via FetchContent
- **Perfetto** — submodule (`vendor/perfetto`), enables profiling when `FREYR_PROFILING=ON`
- **Google Test** (v1.17.0) — test framework

## Testing

- Tests live in `test/src/` organized by module (`Core/`, `Builders/`, `Containers/`, etc.)
- Tests use Google Test with `SceneSpec` parameterized by `FreyrOptions` (execution strategy)
- Tests follow Arrange-Act-Assert (AAA) pattern
- `FREYR_BUILDING_TESTS` macro is defined when building tests

## Documentation

- Docs in `docs/` built with MkDocs Material theme
- Deploy: `mkdocs gh-deploy --force --clean --verbose` (handled by CI on main push)
