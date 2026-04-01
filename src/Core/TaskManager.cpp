#include "Freyr/Core/TaskManager.hpp"
#include "Freyr/Core/Profiling.hpp"

#include <random>
#include <thread>

namespace FREYR_NAMESPACE
{
    struct SpinPolicy
    {
        static constexpr int kPureSpinCount  = 16;
        static constexpr int kYieldCount     = 8;
        static constexpr int kSleepMicrosecs = 50;
    };

    thread_local size_t TaskManager::ThreadId = 0;

    TaskManager::~TaskManager()
    {
        mState.store(State::Resizing, std::memory_order_release);
        NotifyWorkers();

        for (auto& worker : mWorkers)
            if (worker.joinable())
                worker.join();
    }

    bool TaskManager::Resize(std::uint32_t threadCount)
    {
        State expected = mState.load(std::memory_order_acquire);

        while (!mState.compare_exchange_weak(
            expected,
            State::Resizing,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
        {
            if (expected == State::Resizing)
                return false;
        }

        NotifyWorkers();

        for (auto& worker : mWorkers)
            if (worker.joinable())
                worker.join();

        mWorkers.clear();

        mThreadLane.store(1, std::memory_order_relaxed);

        mWorkerQueues.clear();
        mWorkerQueues.reserve(threadCount);

        mWorkerStates.clear();
        mWorkerStates.reserve(threadCount);

        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkerQueues.push_back(std::make_unique<TaskQueue>(std::max<size_t>(
                1024,
                mFreyrOptions->MaxEntities / mFreyrOptions->ArchetypeChunkCapacity / threadCount + 1)));
            mWorkerStates.push_back(std::make_unique<WorkerState>());
        }

        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkers.emplace_back([this, workerQueue = mWorkerQueues[i].get(), workerState = mWorkerStates[i].get()] {
                workerLoop(workerQueue, workerState);
            });
        }

        const State nextState = (expected != State::Empty) ? expected : State::Idle;
        mState.store(nextState, std::memory_order_release);

        NotifyWorkers();

        return true;
    }

    void TaskManager::StartWorkers()
    {
        while (true)
        {
            auto currentState = mState.load(std::memory_order_acquire);

            if (currentState == State::Resizing)
            {
                std::this_thread::yield();
                continue;
            }

            if (currentState == State::Running)
            {
                NotifyWorkers();
                return;
            }

            auto idle = State::Idle;
            if (mState.compare_exchange_strong(idle,
                                               State::Running,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire))
            {
                NotifyWorkers();
                return;
            }

            std::this_thread::yield();
        }
    }

    void TaskManager::StopWorkers()
    {
        while (true)
        {
            if (mState.load(std::memory_order_acquire) == State::Idle)
                return;

            auto running = State::Running;
            if (mState.compare_exchange_strong(running,
                                               State::Idle,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire))
                return;

            std::this_thread::yield();
        }
    }

    void TaskManager::BeginProfiling()
    {
        mWorkersDescriptions.clear();

        for (size_t i = 1; i <= mWorkers.size(); ++i)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  mWorkersDescriptions.emplace_back(std::format("Thread: {:0>2}", i)).c_str(),
                                  perfetto::Track(i));
            FREYR_PROFILING_END("FREYR", perfetto::Track(i));
        }
    }

    void TaskManager::workerLoop(TaskQueue* myQueue, WorkerState* myState)
    {
        ThreadId = mThreadLane.fetch_add(1, std::memory_order_relaxed);

        thread_local std::mt19937 rng { std::random_device {}() };

        std::vector<TaskQueue*> stolenQueues;
        stolenQueues.reserve(mWorkerQueues.size());
        auto refreshStolenQueues = [&]() {
            stolenQueues.clear();
            for (auto& q : mWorkerQueues)
                if (q.get() != myQueue)
                    stolenQueues.push_back(q.get());
        };
        refreshStolenQueues();

        int idleCount = 0;

        while (true)
        {
            bool didWork = false;

            while (true)
            {
                Task task;

                if (myQueue->try_pop(task))
                {
                    task();
                    mTaskCounter->TaskCompleted();
                    idleCount = 0;
                    didWork   = true;
                    continue;
                }

                if (stolenQueues.empty())
                    break;

                bool         stolen   = false;
                const size_t startIdx = std::uniform_int_distribution<size_t>(0, stolenQueues.size() - 1)(rng);

                for (size_t i = 0; i < stolenQueues.size(); ++i)
                {
                    const size_t idx = (startIdx + i) % stolenQueues.size();
                    if (stolenQueues[idx]->try_pop(task))
                    {
                        task();
                        mTaskCounter->TaskCompleted();
                        idleCount = 0;
                        stolen    = true;
                        didWork   = true;
                        break;
                    }
                }

                if (!stolen)
                    break;
            }

            const auto currentState = mState.load(std::memory_order_relaxed);

            if (currentState == State::Resizing)
                return;

            if (currentState == State::Idle)
            {
                idleCount = 0;

                std::atomic_thread_fence(std::memory_order_acquire);
                std::unique_lock lock(myState->mutex);
                myState->cv.wait(lock, [this]() { return mState.load(std::memory_order_acquire) != State::Idle; });
                refreshStolenQueues();
                continue;
            }

            if (!didWork)
            {
                ++idleCount;

                if (idleCount <= SpinPolicy::kPureSpinCount)
                {
#if defined(__x86_64__) || defined(_M_X64)
                    _mm_pause();
#elif defined(__aarch64__)
                    __asm__ volatile("yield");
#endif
                }
                else if (idleCount <= SpinPolicy::kPureSpinCount + SpinPolicy::kYieldCount)
                {
                    std::this_thread::yield();
                }
                else
                {
                    std::unique_lock lock(myState->mutex);
                    myState->cv.wait_for(lock, std::chrono::microseconds(SpinPolicy::kSleepMicrosecs), [this]() {
                        return mState.load(std::memory_order_acquire) != State::Running;
                    });
                    idleCount = 0;
                }
            }
        }
    }
} // namespace FREYR_NAMESPACE