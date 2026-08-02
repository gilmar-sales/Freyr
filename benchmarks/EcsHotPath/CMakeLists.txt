add_executable(EcsHotPathBench src/main.cpp)

target_link_libraries(EcsHotPathBench PUBLIC benchmark::benchmark freyr::freyr)
