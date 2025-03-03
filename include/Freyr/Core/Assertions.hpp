#pragma once

#ifdef FREYR_ASSERTIONS
    #include <cassert>

    #define FREYR_ASSERT(assertion) assert(assertion)
#else
    #define FREYR_ASSERT(assertion)
#endif