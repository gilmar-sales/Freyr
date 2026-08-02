add_executable(SparseSetBench src/main.cpp)

target_link_libraries(SparseSetBench PUBLIC benchmark::benchmark freyr::freyr)
