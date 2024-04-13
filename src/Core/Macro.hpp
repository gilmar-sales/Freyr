#pragma once

#include <atomic>
#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("ECS")
        .SetDescription("Events from the graphics subsystem"),
    perfetto::Category("Update")
        .SetDescription("Network upload and download statistics"));

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

static std::atomic<unsigned> thread_id = 1;

