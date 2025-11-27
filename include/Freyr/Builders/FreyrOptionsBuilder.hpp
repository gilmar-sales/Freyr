#pragma once

#include "Freyr/Core/FreyrOptions.hpp"

namespace FREYR_NAMESPACE
{
    class FreyrOptionsBuilder
    {
      public:
        FreyrOptionsBuilder() {}

        FreyrOptionsBuilder& SetMaxEntities(const size_t maxEntities)
        {
            mMaxEntities = maxEntities;

            return *this;
        }

        FreyrOptionsBuilder& SetArchetypeChunkCapacity(const size_t archetypeChunkCapacity)
        {
            mArchetypeChunkCapacity = archetypeChunkCapacity;

            return *this;
        }

        FreyrOptionsBuilder& SetThreadCount(const size_t threadCount)
        {
            mThreadCount = threadCount;

            return *this;
        }

        Ref<FreyrOptions> Build() const
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

            return options;
        }

      private:
        std::optional<size_t> mMaxEntities;
        std::optional<size_t> mArchetypeChunkCapacity;
        std::optional<size_t> mThreadCount;
    };
} // namespace FREYR_NAMESPACE