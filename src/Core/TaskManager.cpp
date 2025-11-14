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
        auto resizing = State::Resizing;

        while (true)
        {
            auto expected = State::Empty;
            if (mState.compare_exchange_strong(expected, State::Resizing))
            {
                for (int i = 0; i < threadCount; ++i)
                {
                    mWorkers.emplace_back([this] { workerLoop(); });
                }

                mState.compare_exchange_strong(resizing, State::Running);
                NotifyWorkers();
                return;
            }

            auto running = State::Running;
            if (mState.compare_exchange_strong(running, State::Resizing))
            {
                for (auto& worker : mWorkers)
                {
                    if (worker.joinable())
                        worker.join();
                }

                mWorkers.clear();

                for (int i = 0; i < threadCount; ++i)
                {
                    mWorkers.emplace_back([this] { workerLoop(); });
                }
                mState.compare_exchange_strong(resizing, State::Running);
                return;
            }

            auto idle = State::Idle;
            if (mState.compare_exchange_strong(idle, State::Resizing))
            {
                NotifyWorkers();

                for (auto& worker : mWorkers)
                {
                    if (worker.joinable())
                        worker.join();
                }

                mWorkers.clear();

                for (int i = 0; i < threadCount; ++i)
                {
                    mWorkers.emplace_back([this] { workerLoop(); });
                }
                mState.compare_exchange_strong(resizing, State::Idle);
            }
        }
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
            if (mState.load() == State::Resizing)
            {
                return;
            }

            if (mState.load() == State::Idle)
            {
                auto lock = std::unique_lock(mMutex);
                mCondition.wait(lock, [this]() { return mState.load() == State::Running; });
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