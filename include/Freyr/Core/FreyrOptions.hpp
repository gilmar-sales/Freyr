#pragma once

namespace FREYR_NAMESPACE
{
    struct FreyrOptions
    {
        size_t MaxEntities;
        size_t ArchetypeChunkCapacity;
        size_t MaxSystems;
        size_t ThreadCount;
    };
} // namespace FREYR_NAMESPACE