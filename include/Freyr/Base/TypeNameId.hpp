#pragma once

#include <cstdint>
#include <string_view>

#ifndef FREYR_NAMESPACE
    #define FREYR_NAMESPACE fr
#endif

#ifdef BUILD_SHARED_LIBRARIES
    #ifndef FREYR_API
        #define FREYR_API __declspec(dllexport)
    #endif
#else
    #ifndef FREYR_API
        #define FREYR_API
    #endif
#endif

namespace FREYR_NAMESPACE
{
    enum class TypeIdKind : std::uint8_t
    {
        Component = 0,
        Event     = 1,
        System    = 2,
    };

    [[nodiscard]] FREYR_API std::uint64_t RegisterTypeName(TypeIdKind kind, std::string_view name);
    [[nodiscard]] FREYR_API std::uint64_t TypeNameCount(TypeIdKind kind);
    [[nodiscard]] FREYR_API std::string_view TypeNameOf(TypeIdKind kind, std::uint64_t id);
} // namespace FREYR_NAMESPACE
