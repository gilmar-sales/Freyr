#pragma once

namespace FREYR_NAMESPACE
{
    struct FreyrOptions
    {
        size_t MaxEntities            = 1024 * 1024;
        size_t ArchetypeChunkCapacity = 4 * 1024;
        size_t MaxSystems             = 1024;
        size_t ThreadCount            = 2;
        float  FixedDeltaTime         = 1.0f / 50.0f;
    };
} // namespace FREYR_NAMESPACE