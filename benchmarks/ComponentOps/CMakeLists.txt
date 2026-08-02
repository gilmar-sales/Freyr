add_executable(ComponentOpsBench src/main.cpp)

target_link_libraries(ComponentOpsBench PUBLIC benchmark::benchmark freyr::freyr)
