#pragma once

namespace FREYR_NAMESPACE
{
    /**
     * @brief Defines the execution strategy for system scheduling.
     */
    enum class FreyrExecutionStategy
    {
        DispatchOrder, ///< Execute systems in registration order
        ChunkAffinity  ///< Optimize for cache locality with chunk-based scheduling
    };

    /**
     * @brief Runtime configuration options for a Freyr scene.
     *
     * Defaults are designed for general use; adjust based on hardware and workload characteristics.
     */
    struct FreyrOptions
    {
        std::uint64_t         MaxEntities            = 16 * 1024 * 1024; ///< Maximum entities allowed in scene
        std::uint64_t         ArchetypeChunkCapacity = 512;              ///< Entities per archetype chunk
        std::uint64_t         MaxSystems             = 1024;             ///< Maximum registered systems
        std::uint64_t         ThreadCount            = 4;                ///< Worker threads for parallel execution
        FreyrExecutionStategy ExecutionStrategy = FreyrExecutionStategy::ChunkAffinity; ///< System scheduling strategy
    };
} // namespace FREYR_NAMESPACE