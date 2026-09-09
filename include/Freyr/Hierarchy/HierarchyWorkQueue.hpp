#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/UnboundedMPMCQueue.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace FREYR_NAMESPACE
{
    class HierarchyWorkQueue
    {
      public:
        static constexpr std::size_t ChunkSize = 512;

        using Batch = std::vector<Entity>;

        void SendBatches(std::vector<Entity>& outbox)
        {
            if (outbox.empty())
                return;

            for (std::size_t offset = 0; offset < outbox.size(); offset += ChunkSize)
            {
                const std::size_t count = std::min(ChunkSize, outbox.size() - offset);
                Batch             batch(outbox.begin() + static_cast<std::ptrdiff_t>(offset),
                            outbox.begin() + static_cast<std::ptrdiff_t>(offset + count));

                mPublished.fetch_add(1, std::memory_order_release);
                mTasks.push(std::move(batch));
            }
            outbox.clear();
        }

        enum class ClaimResult
        {
            GotWork,
            IdleRetry,
            Done
        };

        ClaimResult TryClaim(std::vector<Entity>& batch)
        {
            Batch claimed;
            if (mTasks.try_pop(claimed))
            {
                mBusy.fetch_add(1, std::memory_order_acq_rel);
                mPublished.fetch_sub(1, std::memory_order_acq_rel);
                batch = std::move(claimed);
                return ClaimResult::GotWork;
            }

            if (mBusy.load(std::memory_order_acquire) == 0 &&
                mPublished.load(std::memory_order_acquire) == 0)
                return ClaimResult::Done;

            return ClaimResult::IdleRetry;
        }

        void FinishBatch()
        {
            mBusy.fetch_sub(1, std::memory_order_acq_rel);
        }

        [[nodiscard]] bool HasPendingTasks() const
        {
            return mPublished.load(std::memory_order_acquire) != 0 ||
                   mBusy.load(std::memory_order_acquire) != 0;
        }

        void Reset()
        {
            Batch discarded;
            while (mTasks.try_pop(discarded))
            {
            }
            mPublished.store(0, std::memory_order_release);
            mBusy.store(0, std::memory_order_release);
        }

      private:
        rigtorp::UnboundedMPMCQueue<Batch> mTasks;
        alignas(64) std::atomic<std::int32_t> mPublished { 0 };
        alignas(64) std::atomic<std::int32_t> mBusy { 0 };
    };
} // namespace FREYR_NAMESPACE
