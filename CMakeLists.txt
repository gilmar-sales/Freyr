cmake_minimum_required(VERSION 3.29)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FREYR_ASSERTIONS
"Enables freyr assertions for debugging purposes (Always off on Release builds)" OFF)
option(FREYR_COVERAGE "Enable gcov coverage instrumentation (GCC/Clang)" OFF)

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  option(FREYR_PROFILING "Enables perfetto profiling" OFF)
  option(FREYR_BUILD_EXAMPLES "Build freyr examples" ON)
  option(FREYR_BUILD_TESTS "Build freyr tests" ON)
  option(FREYR_BUILD_BENCHMARKS "Build freyr benchmarks" ON)

  find_program(FREYR_CCACHE_PROGRAM ccache)
  if (FREYR_CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${FREYR_CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${FREYR_CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${FREYR_CCACHE_PROGRAM}")
  else ()
    message(STATUS "ccache not found; compiling without cache")
  endif ()
else()
  option(FREYR_PROFILING "Enables perfetto profiling" OFF)
  option(FREYR_BUILD_EXAMPLES "Build freyr examples" OFF)
  option(FREYR_BUILD_TESTS "Build freyr tests" OFF)
  option(FREYR_BUILD_BENCHMARKS "Build freyr benchmarks" OFF)
endif ()


if (FREYR_COVERAGE AND FREYR_PROFILING)
  message(WARNING "FREYR_COVERAGE=ON with FREYR_PROFILING=ON inflates untested branches; prefer -DFREYR_PROFILING=OFF")
endif ()

include(FetchContent)

fetchcontent_declare(
        skirnir
        GIT_REPOSITORY "https://github.com/gilmar-sales/Skirnir.git"
        GIT_TAG "v0.23.1"
        GIT_SHALLOW TRUE
)
fetchcontent_makeavailable(skirnir)

if (FREYR_PROFILING)
  add_subdirectory(vendor/perfetto)
endif (FREYR_PROFILING)

project(freyr LANGUAGES C CXX VERSION 0.37.0)

if (FREYR_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif (FREYR_BUILD_EXAMPLES)

if (FREYR_BUILD_TESTS)
  enable_testing()
  include(CTest)

  add_subdirectory(test)
endif (FREYR_BUILD_TESTS)

if (FREYR_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif (FREYR_BUILD_BENCHMARKS)

file(GLOB_RECURSE FREYR_SOURCE
        src/**.cpp
        include/**.hpp
)

add_library(freyr ${FREYR_SOURCE})
add_library(freyr::freyr ALIAS freyr)

target_link_libraries(freyr PUBLIC skirnir)

target_include_directories(freyr PUBLIC
        $<BUILD_INTERFACE:${freyr_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(freyr PUBLIC cxx_std_26)

if (FREYR_ASSERTIONS)
  target_compile_definitions(freyr PUBLIC FREYR_ASSERTIONS=1)
endif (FREYR_ASSERTIONS)

if (FREYR_PROFILING)
  target_compile_definitions(freyr PUBLIC FREYR_PROFILING=1)
  target_link_libraries(freyr PUBLIC perfetto)
endif (FREYR_PROFILING)

if (FREYR_COVERAGE)
  if (NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "FREYR_COVERAGE requires GCC or Clang")
  endif ()

  if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "FREYR_COVERAGE requires CMAKE_BUILD_TYPE=Debug")
  endif ()

  target_compile_options(freyr PUBLIC
    $<$<CONFIG:Debug>:--coverage>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g>)
  target_link_options(freyr PUBLIC $<$<CONFIG:Debug>:--coverage>)
endif (FREYR_COVERAGE)

file(GLOB_RECURSE FREYR_TYPES
        src/Types/*.hpp
)

target_precompile_headers(
        freyr
        PRIVATE
        <Freyr/Pch.hpp>
)
