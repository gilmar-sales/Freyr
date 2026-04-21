#pragma once

namespace FREYR_NAMESPACE
{
    /**
     * @brief Runtime configuration options for a Freyr scene.
     *
     * Defaults are designed for general use; adjust based on hardware and workload characteristics.
     */
    struct FreyrOptions
    {
        std::uint64_t MaxEntities            = 16 * 1024 * 1024; ///< Maximum entities allowed in scene
        std::uint64_t ArchetypeChunkCapacity = 512;              ///< Entities per archetype chunk
        std::uint64_t MaxSystems             = 1024;             ///< Maximum registered systems
        std::uint64_t ThreadCount            = 4;                ///< Worker threads for parallel execution
    };
} // namespace FREYR_NAMESPACE