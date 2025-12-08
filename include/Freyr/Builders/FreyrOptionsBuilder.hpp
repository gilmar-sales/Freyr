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

        [[nodiscard]] Ref<FreyrOptions> Build() const
        {
            auto options = skr::MakeRef<FreyrOptions>();

            if (mThreadCount.has_value())
                options->ThreadCount = mThreadCount.value();

            if (mMaxEntities.has_value())
                options->MaxEntities = mMaxEntities.value();

            if (mArchetypeChunkCapacity.has_value())
                options->ArchetypeChunkCapacity = mArchetypeChunkCapacity.value();
            else
                options->ArchetypeChunkCapacity = options->MaxEntities / (options->ThreadCount * options->ThreadCount);

            if (mFixedDeltaTime.has_value())
                options->FixedDeltaTime = mFixedDeltaTime.value();

            return options;
        }

      private:
        std::optional<size_t> mMaxEntities;
        std::optional<size_t> mArchetypeChunkCapacity;
        std::optional<float> mFixedDeltaTime;
        std::optional<size_t> mThreadCount;
    };
} // namespace FREYR_NAMESPACE