cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD, 23)
set(CMAKE_CXX_CLANG_TIDY, 1)
add_subdirectory(vendor/perfetto)

project(freyr LANGUAGES C CXX)

add_subdirectory(examples)

file(GLOB_RECURSE FREYR_SOURCE
        src/**.cpp
        src/**.hpp
        )

add_library(freyr ${FREYR_SOURCE})
add_library(freyr::freyr ALIAS freyr)

target_include_directories(freyr PUBLIC
        $<BUILD_INTERFACE:${freyr_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/src>
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

target_compile_features(freyr PUBLIC cxx_std_23)


if (WIN32)
  # The perfetto library contains many symbols, so it needs the big object
  # format.
  # Disable legacy features in windows.h.
  add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
  # On Windows we should link to WinSock2.
  target_link_libraries(freyr PUBLIC ws2_32)
endif (WIN32)
target_link_libraries(freyr PUBLIC perfetto)

file(GLOB_RECURSE FREYR_TYPES
        src/Types/*.hpp
        )

target_precompile_headers(
        freyr
        PUBLIC
        <cstdint>
        <cassert>
        <bitset>
        <any>
        <functional>
        <unordered_map>
        <list>
        <vector>
        <queue>
        <mutex>
        <memory>
        <thread>
        <condition_variable>
        <utility>
        src/Core/Macro.hpp
)