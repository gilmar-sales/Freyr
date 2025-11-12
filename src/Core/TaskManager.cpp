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
        if (mRunning)
        {
            mRunning = false;
            for (int i = 0; i < threadCount; ++i)
            {
                mWorkers.emplace_back([this] { workerLoop(); });
            }
            mRunning = true;
        }
        else
        {
            for (int i = 0; i < threadCount; ++i)
            {
                mWorkers.emplace_back([this] { workerLoop(); });
            }
        }
    }

    void TaskManager::BeginProfiling()
    {
        mWorkersDescriptions.clear();

        for (size_t i = 1; i <= mWorkers.size(); ++i)
        {
            const auto& threadLabel = mWorkersDescriptions.emplace_back(std::format("Thread: {:0>2}", i));

            FREYR_PROFILING_BEGIN("FREYR", threadLabel.c_str(), perfetto::Track(i));

            FREYR_PROFILING_END("FREYR", perfetto::Track(i));
        }
    }

    void TaskManager::workerLoop()
    {
        ThreadId = mThreadCount.fetch_add(1);

        int attempt = 0;
        while (true)
        {
            if (!mRunning)
            {
                return;
            }

            if (NewTask task; mAvaiableTasks.try_pop(task))
                task();

            if (attempt++ > 32) {
                attempt = 0;
                std::this_thread::yield();
            }
        }
    }

} // namespace FREYR_NAMESPACE