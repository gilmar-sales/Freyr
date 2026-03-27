#pragma once

namespace FREYR_NAMESPACE
{
    enum class FreyrExecutionStategy
    {
        DispatchOrder,
        ChunkAffinity
    };

    struct FreyrOptions
    {
        std::uint64_t         MaxEntities            = 16 * 1024 * 1024;
        std::uint64_t         ArchetypeChunkCapacity = 512;
        std::uint64_t         MaxSystems             = 1024;
        std::uint64_t         ThreadCount            = 4;
        float                 FixedDeltaTime         = 1.0f / 50.0f;
        FreyrExecutionStategy ExecutionStrategy       = FreyrExecutionStategy::ChunkAffinity;
    };
} // namespace FREYR_NAMESPACE