#pragma once

#include <Skirnir/Skirnir.hpp>
#include <latch>

namespace FREYR_NAMESPACE
{
    class TaskManager
    {
      public:
        explicit TaskManager(Ref<FreyrOptions> freyrOptions) :
            mRunning(true), mThreadCount(freyrOptions->ThreadCount)
        {
            Resize(mThreadCount);
        }

        ~TaskManager()
        {

            mRunning = false;
            mCondition.notify_all();

            for (int i = 0; i < mThreadCount; ++i)
            {
                if (mWorkers[i].joinable())
                {
                    mWorkers[i].join();
                }
            }
        }

        template <typename Func>
        void AddTask(Func func)
        {
            std::lock_guard lock(mMutex);
            mTasks.push(func);
        }

        void Resize(const std::uint32_t threadCount)
        {
            std::unique_lock lock(mMutex);
            mThreadCount = threadCount;

            if (mRunning)
            {
                WaitTasks();

                for (int i = 0; i < mThreadCount; ++i)
                {
                    mWorkers.emplace_back([this] { workerLoop(); });
                }
            }
            else
            {
                for (int i = 0; i < mThreadCount; ++i)
                {
                    mWorkers.emplace_back([this] { workerLoop(); });
                }
            }
        }

        void WaitTasks()
        {
            if (mTasks.empty())
                return;

            mTasksCompleted = skr::MakeRef<std::latch>(std::ssize(mTasks));

            mCondition.notify_all();
            if (!mTasksCompleted->try_wait())
            {
                mTasksCompleted->wait();
            }
        }

        static void StartProfiling()
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

        static void EndProfiling()
        {
            const auto id =
                std::hash<std::thread::id> {}(std::this_thread::get_id());

            FREYR_PROFILING_END("FREYR", perfetto::Track(id));
        }

      private:
        void workerLoop()
        {
            StartProfiling();
            while (true)
            {
                std::move_only_function<void()> task;
                {
                    std::unique_lock lock(mMutex);
                    mCondition.wait(lock, [this] {
                        return !mRunning || !mTasks.empty();
                    });

                    if (!mRunning)
                    {
                        return;
                    }

                    task = std::move(mTasks.front());
                    mTasks.pop();
                }

                task();

                mTasksCompleted->count_down();
                mCondition.notify_one();
            }

            EndProfiling();
        }

        std::vector<std::thread>                    mWorkers;
        std::queue<std::move_only_function<void()>> mTasks;
        std::mutex                                  mMutex;
        std::condition_variable                     mCondition;
        std::condition_variable                     mConditionWaitTasksComplete;
        Ref<std::latch>                             mTasksCompleted;

        bool          mRunning;
        std::uint32_t mThreadCount;
    };
} // namespace FREYR_NAMESPACE