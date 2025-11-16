#include "Freyr/Core/TaskManager.hpp"

#include "Freyr/Core/Profiling.hpp"

#include <format>
#include <iostream>

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
    }

    void TaskManager::Resize(std::uint32_t threadCount)
    {
        // Atomically transition to Resizing state
        State expected = mState.load();
        while (!mState.compare_exchange_weak(expected, State::Resizing))
        {
            expected = mState.load();
            if (expected == State::Resizing)
                return;
        }

        NotifyWorkers(); // Wake up any waiting workers

        for (auto& worker : mWorkers)
        {
            if (worker.joinable())
                worker.join();
        }

        mWorkers.clear();
        mThreadCount.store(1);

        mState.store(State::Running);
        for (uint32_t i = 0; i < threadCount; ++i)
        {
            mWorkers.emplace_back([this] { workerLoop(); });
        }
        NotifyWorkers();
    }

    void TaskManager::StartWorkers()
    {
        while (true)
        {
            if (mState.load() == State::Running)
                return;

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

            auto idle = State::Running;
            if (mState.compare_exchange_strong(idle, State::Idle))
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

    void TaskManager::workerLoop()
    {
        ThreadId = mThreadCount.fetch_add(1);

        int attempt = 0;
        while (true)
        {
            State currentState = mState.load();

            if (currentState == State::Resizing)
            {
                return; // Exit thread
            }

            if (currentState == State::Idle)
            {
                std::unique_lock lock(mMutex);
                mCondition.wait(lock, [this]() {
                    State s = mState.load();
                    return s != State::Idle;
                });
                continue;
            }

            if (Task task; mAvaiableTasks.try_pop(task))
                task();

            if (attempt++ > 32)
            {
                attempt = 0;
                std::this_thread::yield();
            }
        }
    }

} // namespace FREYR_NAMESPACE