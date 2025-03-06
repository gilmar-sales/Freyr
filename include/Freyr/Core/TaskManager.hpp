#pragma once

#include <latch>

namespace FREYR_NAMESPACE
{
    using Task      = std::move_only_function<void()>;
    using TaskQueue = std::queue<Task>;

    class TaskManager
    {
      public:
        explicit TaskManager(const std::shared_ptr<FreyrOptions>& freyrOptions);

        ~TaskManager();

        void AddTask(auto&& func)
        {
            std::lock_guard lock(mMutex);
            mAvaiableTasks.push(std::move(func));
            NotifyWorker();
        }

        void Resize(const std::uint32_t threadCount);

        void WaitTasks(size_t taskCount);

        void NotifyWorker() { mCondition.notify_one(); }

        void NotifyWorkers() { mCondition.notify_all(); }

        static void StartThreadProfiling();
        static void EndThreadProfiling();

      private:
        void workerLoop();

        std::vector<std::thread> mWorkers;
        TaskQueue                mAvaiableTasks;
        unsigned long            mReservedTasks;

        std::mutex                  mMutex;
        std::condition_variable     mCondition;
        std::shared_ptr<std::latch> mTasksCompleted;
        bool                        mWaiting {};

        bool mRunning;
    };
} // namespace FREYR_NAMESPACE