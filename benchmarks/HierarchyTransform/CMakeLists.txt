add_executable(HierarchyTransform src/main.cpp)

target_link_libraries(HierarchyTransform PUBLIC benchmark::benchmark freyr::freyr)
