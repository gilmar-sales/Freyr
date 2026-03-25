#pragma once

namespace FREYR_NAMESPACE
{
    enum class FreyrExecutionStategy
    {
        Default,
        ChunkAffinity
    };

    struct FreyrOptions
    {
        size_t                MaxEntities            = 1024 * 1024;
        size_t                ArchetypeChunkCapacity = 4 * 1024;
        size_t                MaxSystems             = 1024;
        size_t                ThreadCount            = 2;
        float                 FixedDeltaTime         = 1.0f / 50.0f;
        FreyrExecutionStategy ExecutionStategy       = FreyrExecutionStategy::Default;
    };
} // namespace FREYR_NAMESPACE