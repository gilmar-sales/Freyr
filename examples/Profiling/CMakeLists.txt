
set(CMAKE_CXX_STANDARD, 23)

file(GLOB_RECURSE FREYR_PROFILING_SOURCE
        src/*.hpp
        src/*.cpp
        )

add_executable(freyr_profiling ${FREYR_PROFILING_SOURCE})

target_link_libraries(freyr_profiling freyr)