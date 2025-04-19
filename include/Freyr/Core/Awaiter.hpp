#pragma once

#include <latch>

namespace FREYR_NAMESPACE

{
    class Awaiter
    {
      public:
        Awaiter(const std::vector<Ref<std::latch>>& latches) :
            mLatches(std::move(latches))
        {
        }

        void Await()
        {
            for (const auto& latch : mLatches)
            {
                if (!latch->try_wait())
                    latch->wait();
            }
        }

        friend class ArchetypeChunk;

      private:
        std::vector<Ref<std::latch>> mLatches;
    };
} // namespace FREYR_NAMESPACE
