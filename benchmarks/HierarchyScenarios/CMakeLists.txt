add_executable(HierarchyScenariosBench src/main.cpp)

target_link_libraries(HierarchyScenariosBench PUBLIC benchmark::benchmark freyr::freyr)
