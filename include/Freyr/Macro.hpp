#pragma once

#ifdef FREYR_PROFILING

#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("ECS")
        .SetDescription("Events from the graphics subsystem"),
    perfetto::Category("Update")
        .SetDescription("Network upload and download statistics"));

#define FREYR_PROFILING_BEGIN(category, name, ...) TRACE_EVENT_BEGIN(category, name, __VA_ARGS__)
    #define FREYR_PROFILING_END(category, ...)         TRACE_EVENT_END(category, __VA_ARGS__)

#else

#define FREYR_PROFILING_BEGIN(category, name, ...)
#define FREYR_PROFILING_END(category, ...)

#endif // FREYR_PROFILING

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
