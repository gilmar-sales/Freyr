#pragma once

#include <Skirnir/Skirnir.hpp>
#include <memory>

#include "Freyr/Core/FreyrOptions.hpp"

namespace FREYR_NAMESPACE
{
    class FreyrOptionsBuilder
    {
      public:
        FreyrOptionsBuilder() : mOptions(std::make_shared<FreyrOptions>())
        {
            mOptions->ThreadCount     = std::thread::hardware_concurrency();
            mOptions->InitialCapacity = 10'000;
        }

        FreyrOptionsBuilder& SetInitialCapacity(const size_t initialCapacity)
        {
            mOptions->InitialCapacity = initialCapacity;

            return *this;
        }

        FreyrOptionsBuilder& SetThreadCount(const size_t threadCount)
        {
            mOptions->ThreadCount = threadCount;

            return *this;
        }

        Ref<FreyrOptions> Build() { return mOptions; }

      private:
        Ref<FreyrOptions> mOptions;
    };
} // namespace FREYR_NAMESPACE