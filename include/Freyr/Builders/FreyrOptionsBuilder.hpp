#pragma once

#include <memory>

#include "Freyr/Core/FreyrOptions.hpp"

namespace FREYR_NAMESPACE
{
    class FreyrOptionsBuilder
    {
      public:
        FreyrOptionsBuilder() : mOptions(std::make_shared<FreyrOptions>()) {}

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

        std::shared_ptr<FreyrOptions> Build() { return mOptions; }

      private:
        std::shared_ptr<FreyrOptions> mOptions;
    };
} // namespace FREYR_NAMESPACE