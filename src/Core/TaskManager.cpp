#include "Freyr/Core/TaskManager.hpp"
#include "Freyr/Core/Profiling.hpp"
#include <format>

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__clang__) || defined(__GNUC__)
    #include <immintrin.h>
#endif
namespace FREYR_NAMESPACE
{
    inline void cpu_pause() noexcept
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
        __asm__ volatile("yield" ::: "memory");
#else
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }

    thread_local size_t TaskManager::ThreadId = 0;

    TaskManager::~TaskManager()
    {
        mState.store(State::Resizing);
        NotifyWorkers();
        for (auto& w : mWorkers)
            if (w.joinable())
                w.join();
        for (auto* q : mWorkerQueues)
            delete q;
    }

    void TaskManager::Resize(std::uint32_t threadCount)
    {
        State expected = mState.load();
        while (!mState.compare_exchange_weak(expected, State::Resizing))
        {
            if (mState.load() == State::Resizing)
                return;
        }

        NotifyWorkers();
        for (auto& w : mWorkers)
            if (w.joinable())
                w.join();

        mWorkers.clear();
        mThreadLane.store(1);

        for (auto* q : mWorkerQueues)
            delete q;
        mWorkerQueues.clear();
        mWorkerQueues.reserve(threadCount);

        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkerQueues.push_back(new TaskQueue(std::max<size_t>(
                1024,
                mFreyrOptions->MaxEntities / mFreyrOptions->ArchetypeChunkCapacity / threadCount + 1)));
        }

        mState.store(State::Idle);
        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkers.emplace_back([this, q = mWorkerQueues[i]] { workerLoop(q); });
        }

        mState.store(expected != State::Empty ? expected : State::Idle);
        NotifyWorkers();
    }

    void TaskManager::StartWorkers()
    {
        while (true)
        {
            if (mState.load() == State::Resizing)
                continue;
            if (mState.load() == State::Running)
            {
                NotifyWorkers();
                return;
            }
            if (auto idle = State::Idle; mState.compare_exchange_strong(idle, State::Running))
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
            if (auto running = State::Running; mState.compare_exchange_strong(running, State::Idle))
                return;
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

    void TaskManager::workerLoop(TaskQueue* workerQueue)
    {
        ThreadId = mThreadLane.fetch_add(1);

        std::vector<TaskQueue*> stolenQueues;

        auto buildStolenQueues = [&] {
            stolenQueues.clear();
            stolenQueues.reserve(mWorkerQueues.size() - 1);
            for (auto* q : mWorkerQueues)
                if (q != workerQueue)
                    stolenQueues.push_back(q);

            if (!stolenQueues.empty())
                std::rotate(stolenQueues.begin(),
                            stolenQueues.begin() + (ThreadId % stolenQueues.size()),
                            stolenQueues.end());
        };
        buildStolenQueues();

        uint32_t           emptySpins  = 0;
        constexpr uint32_t kPauseLimit = 64;  // nível 1: _mm_pause
        constexpr uint32_t kYieldLimit = 256; // nível 2: yield
                                              // nível 3: wait_for (sleep curto)

        while (true)
        {
            Task task;

        checkOwnQueue:
            if (workerQueue->try_pop(task))
            {
                task();
                mTaskCounter->TaskCompleted();
                emptySpins = 0;
                continue;
            }

            bool foundWork = false;
            for (auto* queue : stolenQueues)
            {
                while (queue->try_pop(task))
                {
                    task();
                    mTaskCounter->TaskCompleted();
                    foundWork  = true;
                    emptySpins = 0;

                    if (!workerQueue->empty())
                        goto checkOwnQueue;
                }
            }

            if (foundWork)
                continue;

            const auto state = mState.load(std::memory_order_acquire);

            if (state == State::Resizing)
                return;

            if (state == State::Idle)
            {
                std::unique_lock lock(mMutex);
                mCondition.wait(lock, [this] { return mState.load() != State::Idle; });

                buildStolenQueues();
                emptySpins = 0;
                continue;
            }

            ++emptySpins;

            if (emptySpins < kPauseLimit)
            {
                cpu_pause();
            }
            else if (emptySpins < kYieldLimit)
            {
                std::this_thread::yield();
            }
            else
            {
                std::unique_lock lock(mMutex);
                mCondition.wait_for(lock, std::chrono::microseconds(50), [this] {
                    return mState.load() != State::Running;
                });
                emptySpins = 0;
            }
        }
    }

} // namespace FREYR_NAMESPACE