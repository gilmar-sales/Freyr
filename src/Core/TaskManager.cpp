#include "Freyr/Core/TaskManager.hpp"

#include "Freyr/Core/Profiling.hpp"

#include <format>
#include <iostream>

namespace FREYR_NAMESPACE
{

    TaskManager::TaskManager(const Ref<FreyrOptions>& freyrOptions) :
        mReservedTasks(0), mRunning(true), mWaiting(false)
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
        for (auto& worker : mWorkers)
        {
            const auto id = std::hash<std::thread::id> {}(worker.get_id());

            const auto label = std::format("Thread: {}", id);

            FREYR_PROFILING_BEGIN("FREYR",
                                  label.c_str(),
                                  perfetto::Track(id),
                                  "ThreadId",
                                  id);
        }
    }

    void TaskManager::EndProfiling()
    {
        for (auto& worker : mWorkers)
        {
            const auto id = std::hash<std::thread::id> {}(worker.get_id());

            FREYR_PROFILING_END("FREYR", perfetto::Track(id));
        }
    }

    void TaskManager::BeginThreadTrace()
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

    void TaskManager::EndThreadTrace()
    {
        {
            const auto id =
                std::hash<std::thread::id> {}(std::this_thread::get_id());

            FREYR_PROFILING_END("FREYR", perfetto::Track(id));
        }
    }

    void TaskManager::workerLoop()
    {
        BeginThreadTrace();
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
                    EndThreadTrace();
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