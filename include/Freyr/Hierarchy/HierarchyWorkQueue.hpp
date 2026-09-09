#pragma once

#include "Freyr/Base/Entity.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

namespace FREYR_NAMESPACE
{
    class HierarchyWorkQueue
    {
      public:
        static constexpr std::size_t ChunkSize = 512;

        void SendBatches(std::vector<Entity>& outbox)
        {
            if (outbox.empty())
                return;

            std::lock_guard lock(mMutex);
            for (std::size_t offset = 0; offset < outbox.size(); offset += ChunkSize)
            {
                const std::size_t count = std::min(ChunkSize, outbox.size() - offset);
                mTasks.emplace_back(outbox.begin() + static_cast<std::ptrdiff_t>(offset),
                                    outbox.begin() + static_cast<std::ptrdiff_t>(offset + count));
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
            std::lock_guard lock(mMutex);
            if (mTasks.empty())
            {
                if (mBusyThreads.load(std::memory_order_relaxed) == 0)
                    return ClaimResult::Done;
                return ClaimResult::IdleRetry;
            }

            batch = std::move(mTasks.front());
            mTasks.pop_front();
            mBusyThreads.fetch_add(1, std::memory_order_relaxed);
            return ClaimResult::GotWork;
        }

        void FinishBatch()
        {
            mBusyThreads.fetch_sub(1, std::memory_order_relaxed);
        }

        [[nodiscard]] bool HasPendingTasks() const
        {
            std::lock_guard lock(mMutex);
            return !mTasks.empty() || mBusyThreads.load(std::memory_order_relaxed) != 0;
        }

        void Reset()
        {
            std::lock_guard lock(mMutex);
            mTasks.clear();
            mBusyThreads.store(0, std::memory_order_relaxed);
        }

      private:
        mutable std::mutex              mMutex;
        std::deque<std::vector<Entity>> mTasks;
        std::atomic<std::int32_t>       mBusyThreads { 0 };
    };
} // namespace FREYR_NAMESPACE
