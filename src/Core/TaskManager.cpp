#include "Freyr/Core/TaskManager.hpp"

#include "Freyr/Core/Profiling.hpp"

#include <format>
#include <iostream>

namespace FREYR_NAMESPACE
{

    thread_local size_t TaskManager::ThreadId = 0;

    TaskManager::~TaskManager()
    {
        mRunning = false;

        mCondition.notify_all();

        for (auto& worker : mWorkers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    void TaskManager::Resize(const std::uint32_t threadCount)
    {
        std::unique_lock lock(mMutex);

        if (mRunning)
        {
            WaitTasks(mAvaiableTasks.size());

            for (int i = 0; i < threadCount; ++i)
            {
                mWorkers.emplace_back([this] { workerLoop(); });
            }
        }
        else
        {
            for (int i = 0; i < threadCount; ++i)
            {
                mWorkers.emplace_back([this] { workerLoop(); });
            }
        }
    }

    void TaskManager::WaitTasks(size_t taskCount)
    {
        if (!taskCount)
            return;

        {
            std::unique_lock lock(mMutex);
            mTasksCompleted = skr::MakeRef<std::latch>(taskCount);
            mWaiting        = true;
        }

        mCondition.notify_all();

        if (!mTasksCompleted->try_wait())
        {
            mTasksCompleted->wait();
        }

        {
            std::unique_lock lock(mMutex);
            mWaiting = false;
        }
    }

    void TaskManager::BeginProfiling()
    {
        std::unique_lock lock(mMutex);
        mWorkersDescriptions.clear();

        for (size_t i = 1; i <= mWorkers.size(); ++i)
        {
            const auto& threadLabel = mWorkersDescriptions.emplace_back(
                std::format("Thread: {:0>2}", i));

            FREYR_PROFILING_BEGIN("FREYR",
                                  threadLabel.c_str(),
                                  perfetto::Track(i));

            FREYR_PROFILING_END("FREYR", perfetto::Track(i));
        }

        mProfiling = true;
    }

    void TaskManager::workerLoop()
    {
        ThreadId = mThreadCount.fetch_add(1);

        while (true)
        {
            Task task;
            {
                FREYR_PROFILING_BEGIN("FREYR",
                                      "Idle Lock",
                                      perfetto::Track(ThreadId),
                                      "ThreadId",
                                      ThreadId);
                std::unique_lock lock(mMutex);

                if (mProfiling)
                    FREYR_PROFILING_END("FREYR", perfetto::Track(ThreadId));

                FREYR_PROFILING_BEGIN("FREYR",
                                      "Idle Wait",
                                      perfetto::Track(ThreadId),
                                      "ThreadId",
                                      ThreadId);
                mCondition.wait(lock, [this] {
                    return !mRunning || !mAvaiableTasks.empty();
                });

                if (mProfiling)
                    FREYR_PROFILING_END("FREYR", perfetto::Track(ThreadId));

                if (!mRunning)
                {
                    return;
                }

                task = std::move(mAvaiableTasks.front());
                mAvaiableTasks.pop();
            }

            if (task)
                task();

            {
                std::lock_guard lock(mMutex);
                if (mWaiting)
                    mTasksCompleted->count_down();
            }

            mCondition.notify_one();
        }
    }

} // namespace FREYR_NAMESPACE