#pragma once

#include <latch>

namespace FREYR_NAMESPACE
{
    class TaskManager
    {
      public:
        explicit TaskManager(const std::uint32_t threadCount =
                                 std::thread::hardware_concurrency()) :
            mRunning(true), mThreadCount(threadCount)
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

            mTasksCompleted = std::make_shared<std::latch>(std::ssize(mTasks));

            mCondition.notify_all();
            if (!mTasksCompleted->try_wait())
            {
                mTasksCompleted->wait();
            }
        }

      private:
        void workerLoop()
        {
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
        }

        std::vector<std::thread>                    mWorkers;
        std::queue<std::move_only_function<void()>> mTasks;
        std::mutex                                  mMutex;
        std::condition_variable                     mCondition;
        std::condition_variable                     mConditionWaitTasksComplete;
        std::shared_ptr<std::latch>                 mTasksCompleted;

        bool          mRunning;
        std::uint32_t mThreadCount;
    };
} // namespace FREYR_NAMESPACE