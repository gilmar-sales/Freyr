#pragma once

namespace FREYR_NAMESPACE
{
    struct FreyrOptions
    {
        size_t MaxEntities            = 256 * 1024;
        size_t ArchetypeChunkCapacity = 1024;
        size_t MaxSystems             = 1024;
        size_t ThreadCount            = 2;
        float  FixedDeltaTime         = 1.0f / 60.0f;
    };
} // namespace FREYR_NAMESPACE