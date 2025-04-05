#pragma once

namespace FREYR_NAMESPACE
{
    struct FreyrOptions
    {
        size_t MaxEntities            = 256 * 1024;
        size_t ArchetypeChunkCapacity = 1024;
        size_t MaxSystems             = 1024;
        size_t ThreadCount            = std::thread::hardware_concurrency();
    };
} // namespace FREYR_NAMESPACE