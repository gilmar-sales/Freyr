#include "Freyr/Core/ThreadPool.hpp"

#include "Freyr/Core/Processor.hpp"

namespace FREYR_NAMESPACE
{

    thread_local size_t        ThreadPool::ThreadId       = 0;
    thread_local std::uint32_t ThreadPool::mQueueLcgState = 0;

    void ThreadPool::publishState(State state)
    {
        {
            std::lock_guard lock(mMutex);
            mState.store(state, std::memory_order_release);
        }
        mCondition.notify_all();
    }

    ThreadPool::~ThreadPool()
    {
        publishState(State::Resizing);

        for (auto& worker : mWorkers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        for (auto& queue : mWorkerQueues)
        {
            delete queue;
        }
    }

    void ThreadPool::Resize(std::uint32_t threadCount)
    {
        State previous = State::Empty;
        {
            std::lock_guard lock(mMutex);
            previous = mState.load(std::memory_order_acquire);

            do
            {
                if (previous == State::Resizing || previous == State::Spawning)
                {
                    return;
                }
            } while (!mState.compare_exchange_weak(
                previous, State::Resizing, std::memory_order_acq_rel, std::memory_order_acquire));
        }
        mCondition.notify_all();

        for (auto& worker : mWorkers)
        {
            if (worker.joinable())
                worker.join();
        }

        mWorkers.clear();
        mThreadLane.store(1);

        for (const auto queue : mWorkerQueues)
        {
            delete queue;
        }

        mWorkerQueues.clear();
        mWorkerQueues.reserve(threadCount);

        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkerQueues.push_back(new TaskQueue(std::max<size_t>(
                1024,
                mFreyrOptions->MaxEntities / mFreyrOptions->ArchetypeChunkCapacity / threadCount +
                    1)));
        }

        {
            std::lock_guard lock(mMutex);
            mState.store(State::Spawning, std::memory_order_release);
        }
        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkers.emplace_back([this, workerQueue = mWorkerQueues[i]] {
                workerLoop(workerQueue);
            });
        }

        publishState(previous != State::Empty ? previous : State::Idle);
    }

    void ThreadPool::StartWorkers()
    {
        while (true)
        {
            if (const auto state = mState.load(std::memory_order_acquire);
                state == State::Resizing || state == State::Spawning)
            {
                continue;
            }

            {
                std::lock_guard lock(mMutex);
                const auto      state = mState.load(std::memory_order_acquire);

                if (state == State::Resizing || state == State::Spawning)
                {
                    continue;
                }

                if (state != State::Running)
                {
                    if (auto idle = State::Idle; !mState.compare_exchange_strong(
                            idle, State::Running, std::memory_order_acq_rel,
                            std::memory_order_acquire))
                    {
                        continue;
                    }
                }
            }

            mCondition.notify_all();
            return;
        }
    }

    void ThreadPool::StopWorkers()
    {
        while (true)
        {
            {
                std::lock_guard lock(mMutex);
                const auto      state = mState.load(std::memory_order_acquire);

                if (state == State::Idle)
                {
                    return;
                }

                if (auto running = State::Running; !mState.compare_exchange_strong(
                        running, State::Idle, std::memory_order_acq_rel, std::memory_order_acquire))
                {
                    continue;
                }
            }

            return;
        }
    }

    void ThreadPool::workerLoop(TaskQueue* workerQueue)
    {
        ThreadId       = mThreadLane.fetch_add(1);
        mQueueLcgState = static_cast<std::uint32_t>(ThreadId) + 1;

        const uint32_t stealStart = static_cast<std::uint32_t>(ThreadId);

        std::vector<TaskQueue*> stolenQueues;
        stolenQueues.reserve(mWorkerQueues.size() > 0 ? mWorkerQueues.size() - 1 : 0);
        for (auto* q : mWorkerQueues)
            if (q != workerQueue)
                stolenQueues.push_back(q);

        if (!stolenQueues.empty())
        {
            std::rotate(stolenQueues.begin(),
                        stolenQueues.begin() + (ThreadId % stolenQueues.size()),
                        stolenQueues.end());
        }

        while (true)
        {
            Task task;

            while (workerQueue->try_pop(task))
            {
                task();
                mTaskCounter->TaskCompleted();
            }

            bool madeProgress = false;
            for (std::uint32_t attempt = 0; attempt < 8; ++attempt)
            {
                for (std::size_t i = 0; i < stolenQueues.size(); ++i)
                {
                    auto* queue = stolenQueues[i];
                    while (queue->try_pop(task))
                    {
                        task();
                        mTaskCounter->TaskCompleted();
                        madeProgress = true;
                    }
                }
                for (int pause = 0; pause < 16; ++pause)
                {
                    Processor::Pause();
                }
            }

            if (!madeProgress)
            {
                std::this_thread::yield();
            }

            const auto currentState = mState.load(std::memory_order_acquire);

            if (currentState == State::Resizing)
                return;

            if (currentState == State::Idle)
            {
                std::unique_lock lock(mMutex);
                mCondition.wait(lock, [this]() {
                    return mState.load(std::memory_order_acquire) != State::Idle;
                });
            }
        }
    }

} // namespace FREYR_NAMESPACE
