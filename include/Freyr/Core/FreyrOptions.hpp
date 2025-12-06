#pragma once

namespace
FREYR_NAMESPACE
{
    struct FreyrOptions
    {
        size_t MaxEntities            = 512 * 1024;
        size_t ArchetypeChunkCapacity = 4 * 1024;
        size_t MaxSystems             = 1024;
        size_t ThreadCount            = 2;
        float  FixedDeltaTime         = 0.0167f;
    };
} // namespace FREYR_NAMESPACE