add_executable(ExecutionStrategyBench src/main.cpp)

target_link_libraries(ExecutionStrategyBench PUBLIC benchmark::benchmark freyr::freyr)