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

#include "Freyr/Core/FreyrOptions.hpp"
