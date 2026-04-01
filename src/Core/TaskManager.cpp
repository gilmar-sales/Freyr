#include "Freyr/Core/TaskManager.hpp"

#include "Freyr/Core/Profiling.hpp"

#include <format>
#include <iostream>
#include <ranges>

namespace FREYR_NAMESPACE
{

    thread_local size_t TaskManager::ThreadId = 0;

    TaskManager::~TaskManager()
    {
        mState.store(State::Resizing);
        NotifyWorkers();

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

    void TaskManager::Resize(std::uint32_t threadCount)
    {
        State expected = mState.load();

        while (!mState.compare_exchange_weak(expected, State::Resizing))
        {
            if (const auto currentState = mState.load(); currentState == State::Resizing)
            {
                return;
            }
        }

        NotifyWorkers();

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
                1024, mFreyrOptions->MaxEntities / mFreyrOptions->ArchetypeChunkCapacity / threadCount + 1)));
        }

        mState.store(State::Idle);
        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkers.emplace_back([this, workerQueue = mWorkerQueues[i]] { workerLoop(workerQueue); });
        }

        mState.store(expected != State::Empty ? expected : State::Idle);
        NotifyWorkers();
    }

    void TaskManager::StartWorkers()
    {
        while (true)
        {
            if (mState.load() == State::Resizing)
            {
                continue;
            }

            if (mState.load() == State::Running)
            {
                NotifyWorkers();
                return;
            }

            auto idle = State::Idle;
            if (mState.compare_exchange_strong(idle, State::Running))
            {
                NotifyWorkers();
                return;
            }
        }
    }

    void TaskManager::StopWorkers()
    {
        while (true)
        {
            if (mState.load() == State::Idle)
                return;

            auto running = State::Running;
            if (mState.compare_exchange_strong(running, State::Idle))
                return;
        }
    }

    void TaskManager::BeginProfiling()
    {
        mWorkersDescriptions.clear();

        for (size_t i = 1; i <= mWorkers.size(); ++i)
        {
            FREYR_PROFILING_BEGIN("FREYR", mWorkersDescriptions.emplace_back(std::format("Thread: {:0>2}", i)).c_str(),
                                  perfetto::Track(i));

            FREYR_PROFILING_END("FREYR", perfetto::Track(i));
        }
    }

    void TaskManager::workerLoop(TaskQueue* workerQueue)
    {
        ThreadId = mThreadLane.fetch_add(1);

        std::vector<TaskQueue*> stolenQueues;

        auto buildStolenQueues = [&] {
            stolenQueues = std::ranges::to<std::vector>(
                std::views::filter(mWorkerQueues, [workerQueue](auto queue) { return queue != workerQueue; }));

            std::rotate(stolenQueues.begin(),
                        stolenQueues.begin() + (ThreadId % stolenQueues.size()),
                        stolenQueues.end());
        };
        buildStolenQueues();

        while (true)
        {
            while (true)
            {
                Task task;

                if (workerQueue->try_pop(task))
                {
                    task();
                    mTaskCounter->TaskCompleted();
                    continue;
                }

                bool stolen = false;
                for (const auto queue : stolenQueues)
                {
                    if (queue->try_pop(task))
                    {
                        task();
                        mTaskCounter->TaskCompleted();
                        stolen = true;
                        break;
                    }
                }

                if (!stolen)
                    break;
            }

            const auto currentState = mState.load(std::memory_order_acquire);

            if (currentState == State::Resizing)
                return;

            if (currentState == State::Idle)
            {
                std::unique_lock lock(mMutex);
                mCondition.wait(lock, [this]() { return mState.load() != State::Idle; });
                buildStolenQueues();
                continue;
            }

            std::this_thread::yield();
        }
    }

} // namespace FREYR_NAMESPACE