# Freyr Agent Guidelines

## Build Commands

```bash
# Configure (from repo root)
cmake -B build

# Build
cmake --build build --config Debug

# Run tests
ctest --build-config Debug --rerun-failed --output-on-failure

# Run a single test binary
./build/test/Tests_run --gtest_filter="SceneSpec.*"
```

## Build Options

- `FREYR_ASSERTIONS=ON` — enable runtime assertions

## Code Style

- Format: `.clang-format` (Microsoft-based, column limit 120)
- Lint: `.clang-tidy` (extensive checks enabled)
- C++ standard: C++23
- No comments unless requested

## Dependencies

- **Skirnir** (v0.15.3) — fetched automatically via FetchContent
- **Perfetto** — included as a submodule (`vendor/perfetto`), enables profiling when `FREYR_PROFILING=ON`
- **Google Test** (v1.17.0) — test framework

## Key Conventions

- Components must inherit from `fr::Component` (data only, no logic)
- Systems must inherit from `fr::System`, override lifecycle hooks (`PreUpdate`, `Update`, `PostUpdate`, `FixedUpdate`, etc.)
- Register components via `FreyrExtension::WithComponent<T>()` before use
- `scene->DestroyEntity(e)` is **deferred** — processed at end of `Update`
- Entity IDs are stable; component access is always deferred
- Entity iteration: `ForEach<Ts...>(fn)`, `ForEachAsync<Ts...>(fn)`

## Architecture

- **Scene** — central orchestrator holding EntityManager, ComponentManager, SystemManager, EventManager, TaskManager
- **Archetype** — group of entities sharing the same component set, divided into fixed-size **chunks**
- **Chunk** — unit of parallel work distribution; iteration happens per-chunk
- Entry point: `include/Freyr/Freyr.hpp`

## Testing

- Tests live in `test/src/` organized by module (`Core/`, `Builders/`, `Containers/`, etc.)
- Tests use Google Test with `SceneSpec` parameterized by `FreyrOptions` (execution strategy)
- Tests follow the Arrange-Act-Assert (AAA) pattern
- `FREYR_BUILDING_TESTS` macro is defined when building tests

## Documentation

- Docs in `docs/` built with MkDocs Material theme
- Deploy: `mkdocs gh-deploy --force --clean --verbose` (handled by CI on main push)

## CI

- **cmake-multi-platform.yml**: Builds on Windows (MSVC), Ubuntu (GCC/Clang), macOS (Clang)
- Runs `ctest --rerun-failed --output-on-failure`
- Triggered on push/PR to `main` for `src/`, `include/`, `test/`, `examples/`, `benchmarks/`, `CMakeLists.txt`
