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

Then include the single umbrella header in your source:

```cpp
#include <Freyr/Freyr.hpp>
```

---

## Optional features

### Perfetto profiling

Enable trace-based profiling with Perfetto:

```cmake
target_compile_definitions(my_app PRIVATE FREYR_PROFILING)
```

See the [Profiling guide](../guides/profiling.md) for usage.

---

## Building from source

```bash
git clone --recurse-submodules https://github.com/gilmar-sales/Freyr.git
cd Freyr
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Running tests

```bash
cd build
ctest --output-on-failure
```

### Running benchmarks

```bash
./build/benchmarks/ComponentArray/freyr_component_array_benchmark
./build/benchmarks/ExecutionStrategy/freyr_execution_strategy_benchmark
```
