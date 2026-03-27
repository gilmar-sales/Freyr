#pragma once

#include "Freyr/Core/FreyrOptions.hpp"

namespace FREYR_NAMESPACE
{
    class FreyrOptionsBuilder
    {
      public:
        FreyrOptionsBuilder() = default;

        FreyrOptionsBuilder& WithMaxEntities(const size_t maxEntities)
        {
            mMaxEntities = maxEntities;

            return *this;
        }

        FreyrOptionsBuilder& WithArchetypeChunkCapacity(const size_t archetypeChunkCapacity)
        {
            mArchetypeChunkCapacity = archetypeChunkCapacity;

            return *this;
        }

        FreyrOptionsBuilder& WithFixedDeltaTime(const float fixedDeltaTime)
        {
            mFixedDeltaTime = fixedDeltaTime;

            return *this;
        }

        FreyrOptionsBuilder& WithThreadCount(const size_t threadCount)
        {
            mThreadCount = threadCount;

            return *this;
        }

        FreyrOptionsBuilder& WithExecutionStrategy(const FreyrExecutionStategy executionStrategy)
        {
            mExecutionStrategy = executionStrategy;

            return *this;
        }

        [[nodiscard]] Ref<FreyrOptions> Build() const
        {
            auto options = skr::MakeRef<FreyrOptions>();

            if (mThreadCount.has_value())
                options->ThreadCount = mThreadCount.value();

            if (mMaxEntities.has_value())
                options->MaxEntities = mMaxEntities.value();

            if (mArchetypeChunkCapacity.has_value())
                options->ArchetypeChunkCapacity = mArchetypeChunkCapacity.value();

            if (mFixedDeltaTime.has_value())
                options->FixedDeltaTime = mFixedDeltaTime.value();

            if (mExecutionStrategy.has_value())
                options->ExecutionStrategy = mExecutionStrategy.value();

            return options;
        }

      private:
        std::optional<size_t>                mMaxEntities;
        std::optional<size_t>                mArchetypeChunkCapacity;
        std::optional<float>                 mFixedDeltaTime;
        std::optional<size_t>                mThreadCount;
        std::optional<FreyrExecutionStategy> mExecutionStrategy;
    };
} // namespace FREYR_NAMESPACE