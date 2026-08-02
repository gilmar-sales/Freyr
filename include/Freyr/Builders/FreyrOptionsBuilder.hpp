#pragma once

#include "Freyr/Pch.hpp"
#include "Freyr/Core/FreyrOptions.hpp"
#include "Freyr/Core/Processor.hpp"

namespace FREYR_NAMESPACE
{
    /**
     * @brief Builder for configuring Freyr runtime options.
     *
     * Use FreyrExtension::WithOptions() to configure via this builder.
     * All settings are optional; defaults are used if not specified.
     */
    class FreyrOptionsBuilder
    {
      public:
        FreyrOptionsBuilder() = default;

        /**
         * @brief Sets the maximum number of entities allowed in the scene.
         *
         * @param maxEntities  Upper bound for entity creation
         * @return Reference to this builder for chaining
         */
        FreyrOptionsBuilder& WithMaxEntities(const size_t maxEntities)
        {
            mMaxEntities = maxEntities;

            return *this;
        }

        /**
         * @brief Sets the number of entities per archetype chunk.
         *
         * @param archetypeChunkCapacity  Entities per chunk (affects memory layout and cache
         * behavior)
         * @return Reference to this builder for chaining
         */
        FreyrOptionsBuilder& WithArchetypeChunkCapacity(const size_t archetypeChunkCapacity)
        {
            mArchetypeChunkCapacity = archetypeChunkCapacity;

            return *this;
        }

        /**
         * @brief Sets the number of worker threads for parallel execution.
         *
         * @param threadCount  Number of threads (0 = auto-detect based on hardware)
         * @return Reference to this builder for chaining
         */
        FreyrOptionsBuilder& WithThreadCount(const size_t threadCount)
        {
            mThreadCount = threadCount;

            return *this;
        }

        /**
         * @brief Configures the engine to utilize all available physical CPU cores.
         *
         * This method queries the hardware topology to determine the number of physical
         * processing units, excluding logical processors created by SMT (Hyper-Threading).
         *
         * @note Utilizing physical cores instead of logical ones can reduce resource
         * contention and cache misses in high-throughput or latency-sensitive workloads.
         *
         * @return FreyrOptionsBuilder& A reference to this builder instance for method chaining.
         *
         * @see fr::Processor::GetPhysicalCoreCount()
         */
        FreyrOptionsBuilder& WithAllPhysicalCores()
        {
            mThreadCount = Processor::GetPhysicalCoreCount();

            return *this;
        }

        /**
         * @brief Constructs a FreyrOptions object from the configured settings.
         *
         * @return FreyrOptions shared pointer with all specified values applied
         *
         * @note Only settings that were explicitly set are applied; defaults are used otherwise.
         */
        [[nodiscard]] skr::Arc<FreyrOptions> Build() const
        {
            auto options = skr::MakeArc<FreyrOptions>();

            if (mThreadCount.has_value() && mThreadCount.value() > 0)
                options->ThreadCount = mThreadCount.value();

            if (mMaxEntities.has_value())
                options->MaxEntities = mMaxEntities.value();

            if (mArchetypeChunkCapacity.has_value())
                options->ArchetypeChunkCapacity = mArchetypeChunkCapacity.value();

            return options;
        }

      private:
        std::optional<size_t> mMaxEntities;
        std::optional<size_t> mArchetypeChunkCapacity;
        std::optional<size_t> mThreadCount;
    };
} // namespace FREYR_NAMESPACE