add_executable(MPMCQueueBench src/main.cpp)

target_link_libraries(MPMCQueueBench PUBLIC benchmark::benchmark freyr::freyr)
