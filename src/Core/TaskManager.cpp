#include "Freyr/Core/TaskManager.hpp"

#include "Freyr/Core/Profiling.hpp"

#include <format>
#include <iostream>

namespace FREYR_NAMESPACE
{

    TaskManager::TaskManager(
        const std::shared_ptr<FreyrOptions>& freyrOptions) :
        mReservedTasks(0), mRunning(true)
    {
        Resize(freyrOptions->ThreadCount);
    }

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

        mTasksCompleted = std::make_shared<std::latch>(taskCount);

        mCondition.notify_all();

        if (!mTasksCompleted->try_wait())
        {
            mTasksCompleted->wait();
        }
    }

    void TaskManager::StartThreadProfiling()
    {
        const auto id =
            std::hash<std::thread::id> {}(std::this_thread::get_id());

        const auto label = std::format("Thread: {}", id);

        FREYR_PROFILING_BEGIN("FREYR",
                              label.c_str(),
                              perfetto::Track(id),
                              "ThreadId",
                              id);
    }

    void TaskManager::EndThreadProfiling()
    {
        {
            const auto id =
                std::hash<std::thread::id> {}(std::this_thread::get_id());

            FREYR_PROFILING_END("FREYR", perfetto::Track(id));
        }
    }

    void TaskManager::workerLoop()
    {
        StartThreadProfiling();
        while (true)
        {
            Task task;
            {
                const auto id =
                    std::hash<std::thread::id> {}(std::this_thread::get_id());

                FREYR_PROFILING_BEGIN("FREYR",
                                      "Idle Lock",
                                      perfetto::Track(id),
                                      "ThreadId",
                                      id);
                std::unique_lock lock(mMutex);
                FREYR_PROFILING_END("FREYR", perfetto::Track(id));

                FREYR_PROFILING_BEGIN("FREYR",
                                      "Idle Wait",
                                      perfetto::Track(id),
                                      "ThreadId",
                                      id);
                mCondition.wait(lock, [this] {
                    return !mRunning || !mAvaiableTasks.empty();
                });
                FREYR_PROFILING_END("FREYR", perfetto::Track(id));

                if (!mRunning)
                {
                    EndThreadProfiling();
                    return;
                }

                task = std::move(mAvaiableTasks.front());
                mAvaiableTasks.pop();
            }

            task();

            mTasksCompleted->count_down();
            mCondition.notify_one();
        }
    }

} // namespace FREYR_NAMESPACE