# Installation

## CMake FetchContent (recommended)

Add the following to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.29)
project(my_project CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    freyr
    GIT_REPOSITORY https://github.com/gilmar-sales/Freyr.git
    GIT_TAG        main  # pin to a tag or commit SHA for reproducibility
)

FetchContent_MakeAvailable(freyr)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE Freyr)
```

Then include the single umbrella header:

```cpp
#include <Freyr/Freyr.hpp>
```

---

## Building from source

```bash
git clone --recurse-submodules https://github.com/gilmar-sales/Freyr.git
cd Freyr
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

!!! tip "Clone with submodules"
    Freyr requires Skirnir (fetched automatically via FetchContent by default) and optionally
    Perfetto (as a git submodule). Use `--recurse-submodules` to clone the full source tree.

### Tests

```bash
cd build
ctest --output-on-failure
```

### Benchmarks

```bash
./build/benchmarks/ComponentArray/freyr_component_array_benchmark
./build/benchmarks/ExecutionStrategy/freyr_execution_strategy_benchmark
```

---

## Build options

| Option                  | Default  | Description                                         |
|-------------------------|----------|-----------------------------------------------------|
| `FREYR_BUILD_TESTS`     | `ON`     | Build the test suite                                |
| `FREYR_BUILD_BENCHMARKS`| `OFF`    | Build benchmarks                                    |
| `FREYR_BUILD_EXAMPLES`  | `OFF`    | Build example applications                          |
| `FREYR_ASSERTIONS`      | `OFF`    | Enable runtime assertions (`FREYR_ASSERT` macro)     |
| `FREYR_PROFILING`       | `OFF`    | Enable Perfetto tracing support                     |

Pass options via `-D` on the CMake command line:

```bash
cmake -B build -DFREYR_ASSERTIONS=ON -DFREYR_BUILD_BENCHMARKS=ON
```

---

## Optional: Perfetto profiling

Freyr integrates with [Perfetto](https://perfetto.dev) for trace-based profiling. Enable it with the
`FREYR_PROFILING` option. Perfetto is pulled as a git submodule from `vendor/perfetto`.

```bash
cmake -B build -DFREYR_PROFILING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

!!! warning "Profiling build type"
    Use `RelWithDebInfo` or `Release` for profiling. Debug builds have significantly higher overhead
    and produce distorted timing data.

See the [Profiling guide](../guides/profiling.md) for usage details.

---

## Optional: Enable assertions

Runtime assertions help catch programming errors during development:

```cmake
target_compile_definitions(my_app PRIVATE FREYR_ASSERTIONS)
```

Alternatively, pass `-DFREYR_ASSERTIONS=ON` when configuring Freyr itself. Assertions check for:

- Component registration before use
- Entity ID range validity
- Archetype chunk capacity violations

---

## Using with an existing project

If you already have a `CMakeLists.txt`, add Freyr as a dependency:

```cmake
# In your project's CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
    freyr
    GIT_REPOSITORY https://github.com/gilmar-sales/Freyr.git
    GIT_TAG v1.0.0  # use a specific tag for stability
)

FetchContent_MakeAvailable(freyr)

target_link_libraries(your_target PRIVATE Freyr)
```

Freyr will automatically find or fetch its own dependencies (Skirnir, Google Test).

---

## Troubleshooting

### "C++23 not supported"

Ensure your compiler is recent enough:

- **GCC** ≥ 13
- **Clang** ≥ 16
- **MSVC** ≥ 19.37 (Visual Studio 2022 17.7+)

### "Skirnir not found"

Freyr fetches Skirnir via `FetchContent` automatically. If you have a local copy, set
`FETCHCONTENT_SOURCE_DIR_SKIRNIR`:

```bash
cmake -B build -DFETCHCONTENT_SOURCE_DIR_SKIRNIR=/path/to/skirnir
```

### Build is slow

- Use `ccache` to cache compilations
- Use `-j` / `--parallel` with your build tool
- For development, build only Freyr: `cmake --build build --target Freyr`
