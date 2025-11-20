add_executable(ComponentArrayBench src/main.cpp)

target_link_libraries(ComponentArrayBench PUBLIC benchmark::benchmark freyr::freyr)