add_executable(HierarchyChildrenBench src/main.cpp)

target_include_directories(HierarchyChildrenBench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../HierarchyScenarios/src)
target_link_libraries(HierarchyChildrenBench PUBLIC benchmark::benchmark freyr::freyr)
