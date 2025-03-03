#pragma once

#ifdef FREYR_ASSERTIONS
    #include <cassert>

    #define FREYR_ASSERT(assetion) assert(assertion)
#else
    #define FREYR_ASSERT(assetion)
#endif