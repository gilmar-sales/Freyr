cmake_minimum_required(VERSION 3.29)

option(FREYR_PROFILING "Enables perfetto profiling" OFF)
option(FREYR_BUILD_EXAMPLES "Build freyr examples" OFF)
option(FREYR_BUILD_TESTS "Build freyr tests" OFF)

add_subdirectory(vendor/boost/array)
add_subdirectory(vendor/boost/algorithm)
add_subdirectory(vendor/boost/atomic)
add_subdirectory(vendor/boost/align)
add_subdirectory(vendor/boost/bind)
add_subdirectory(vendor/boost/describe)
add_subdirectory(vendor/boost/core)
add_subdirectory(vendor/boost/concept_check)
add_subdirectory(vendor/boost/config)
add_subdirectory(vendor/boost/exception)
add_subdirectory(vendor/boost/assert)
add_subdirectory(vendor/boost/function)
add_subdirectory(vendor/boost/static_assert)
add_subdirectory(vendor/boost/throw_exception)
add_subdirectory(vendor/boost/move)
add_subdirectory(vendor/boost/intrusive)
add_subdirectory(vendor/boost/conversion)
add_subdirectory(vendor/boost/function_types)
add_subdirectory(vendor/boost/filesystem)
add_subdirectory(vendor/boost/container)
add_subdirectory(vendor/boost/container_hash)
add_subdirectory(vendor/boost/iterator)
add_subdirectory(vendor/boost/mp11)
add_subdirectory(vendor/boost/integer)
add_subdirectory(vendor/boost/predef)
add_subdirectory(vendor/boost/io)
add_subdirectory(vendor/boost/context)
add_subdirectory(vendor/boost/scope)
add_subdirectory(vendor/boost/fusion)
add_subdirectory(vendor/boost/pool)
add_subdirectory(vendor/boost/smart_ptr)
add_subdirectory(vendor/boost/preprocessor)
add_subdirectory(vendor/boost/tuple)
add_subdirectory(vendor/boost/type_traits)
add_subdirectory(vendor/boost/typeof)
add_subdirectory(vendor/boost/mpl)
add_subdirectory(vendor/boost/format)
add_subdirectory(vendor/boost/functional)
add_subdirectory(vendor/boost/range)
add_subdirectory(vendor/boost/system)
add_subdirectory(vendor/boost/variant2)
add_subdirectory(vendor/boost/optional)
add_subdirectory(vendor/boost/utility)
add_subdirectory(vendor/boost/regex)
add_subdirectory(vendor/boost/fiber)
add_subdirectory(vendor/boost/detail)
add_subdirectory(vendor/boost/unordered)
add_subdirectory(vendor/boost/winapi)
add_subdirectory(vendor/Skirnir)

if (FREYR_PROFILING)
    add_subdirectory(vendor/perfetto)
endif (FREYR_PROFILING)

project(freyr LANGUAGES C CXX)

if (FREYR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif (FREYR_BUILD_EXAMPLES)

if (FREYR_BUILD_TESTS)
    add_subdirectory(test)
endif (FREYR_BUILD_TESTS)

file(GLOB_RECURSE FREYR_SOURCE
        src/**.cpp
        include/**.hpp
)

add_library(freyr ${FREYR_SOURCE})
add_library(freyr::freyr ALIAS freyr)

find_package(TBB QUIET)

if (${TBB_FOUND})
    target_link_libraries(freyr PUBLIC TBB::tbb)
endif ()

target_link_libraries(freyr PUBLIC Boost::container Boost::fiber skirnir)

target_include_directories(freyr PUBLIC
        $<BUILD_INTERFACE:${freyr_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(freyr PUBLIC cxx_std_23)

if (FREYR_PROFILING)
    target_compile_definitions(freyr PUBLIC FREYR_PROFILING=1)
    if (WIN32)
        # The perfetto library contains many symbols, so it needs the big object
        # format.
        # Disable legacy features in windows.h.
        add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
        # On Windows we should link to WinSock2.
        target_link_libraries(freyr PUBLIC ws2_32)
    endif (WIN32)

    target_link_libraries(freyr PUBLIC perfetto)

endif (FREYR_PROFILING)

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
        <type_traits>
        <condition_variable>
        <utility>
        <execution>
        <Freyr/Macro.hpp>
)