#pragma once

#include "Freyr/Core/FreyrOptions.hpp"

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
         * @param archetypeChunkCapacity  Entities per chunk (affects memory layout and cache behavior)
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
         * @brief Sets the execution strategy for system updates.
         *
         * @param executionStrategy  Strategy type (DispatchOrder or ChunkAffinity)
         * @return Reference to this builder for chaining
         */
        FreyrOptionsBuilder& WithExecutionStrategy(const FreyrExecutionStategy executionStrategy)
        {
            mExecutionStrategy = executionStrategy;

            return *this;
        }

        /**
         * @brief Constructs a FreyrOptions object from the configured settings.
         *
         * @return FreyrOptions shared pointer with all specified values applied
         *
         * @note Only settings that were explicitly set are applied; defaults are used otherwise.
         */
        [[nodiscard]] Ref<FreyrOptions> Build() const
        {
            auto options = skr::MakeRef<FreyrOptions>();

            if (mThreadCount.has_value() && mThreadCount.value() > 0)
                options->ThreadCount = mThreadCount.value();

            if (mMaxEntities.has_value())
                options->MaxEntities = mMaxEntities.value();

            if (mArchetypeChunkCapacity.has_value())
                options->ArchetypeChunkCapacity = mArchetypeChunkCapacity.value();

            if (mExecutionStrategy.has_value())
                options->ExecutionStrategy = mExecutionStrategy.value();

            return options;
        }

      private:
        std::optional<size_t>                mMaxEntities;
        std::optional<size_t>                mArchetypeChunkCapacity;
        std::optional<size_t>                mThreadCount;
        std::optional<FreyrExecutionStategy> mExecutionStrategy;
    };
} // namespace FREYR_NAMESPACE