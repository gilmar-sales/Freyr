#pragma once

#include "Freyr/Core/FreyrOptions.hpp"

namespace FREYR_NAMESPACE
{
    class FreyrOptionsBuilder
    {
      public:
        FreyrOptionsBuilder() : mOptions(skr::MakeRef<FreyrOptions>()) {}

        FreyrOptionsBuilder& SetMaxEntities(const size_t maxEntities)
        {
            mOptions->MaxEntities = maxEntities;

            return *this;
        }

        FreyrOptionsBuilder& SetArchetypeChunkCapacity(
            const size_t initialCapacity)
        {
            mOptions->ArchetypeChunkCapacity = initialCapacity;

            return *this;
        }

        FreyrOptionsBuilder& SetThreadCount(const size_t threadCount)
        {
            mOptions->ThreadCount = threadCount;

            return *this;
        }

        Ref<FreyrOptions> Build() const { return mOptions; }

      private:
        Ref<FreyrOptions> mOptions;
    };
} // namespace FREYR_NAMESPACE