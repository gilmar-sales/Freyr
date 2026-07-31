#pragma once

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

template <typename T>
using Ref = skr::Arc<T>;

template <typename T>
using WeakRef = skr::WeakArc<T>;

namespace skr
{
    template <typename T, typename... TArgs>
        requires(std::is_constructible_v<T, TArgs...>)
    inline Arc<T> MakeRef(TArgs&&... args)
    {
        return MakeArc<T>(std::forward<TArgs>(args)...);
    }

    template <typename T>
    constexpr std::string_view type_name()
    {
        return ::refl::type_name<T>();
    }
} // namespace skr

#include "Freyr/Core/Assertions.hpp"
#include "Freyr/Core/FreyrOptions.hpp"