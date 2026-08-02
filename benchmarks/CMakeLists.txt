set(BENCHMARK_ENABLE_TESTING OFF)

FetchContent_Declare(
        google-benchmark
        GIT_REPOSITORY "https://github.com/google/benchmark.git"
        GIT_TAG "v1.9.5"
)
FetchContent_MakeAvailable(google-benchmark)

add_subdirectory(ComponentArray)
add_subdirectory(EcsHotPath)
add_subdirectory(SparseSet)
