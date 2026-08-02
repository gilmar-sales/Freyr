#pragma once

#include <bitset>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#define FREYR_NAMESPACE fr

#ifdef BUILD_SHARED_LIBRARIES
    #define FREYR_API __declspec(dllexport)
#else
    #define FREYR_API
#endif

#ifdef FREYR_BUILDING_TESTS
    #define FREYR_SPEC virtual
#else
    #define FREYR_SPEC inline
#endif

namespace FREYR_NAMESPACE
{
#ifdef __cpp_lib_move_only_function
    template <typename T>
    using function = std::move_only_function<T>;
#else
    template <typename T>
    using function = std::function<T>;
#endif

    template <typename T>
    using Action = function<void(T&)>;

} // namespace FREYR_NAMESPACE

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Core/Assertions.hpp"
#include "Freyr/Core/FreyrOptions.hpp"
