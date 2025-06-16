#pragma once

#include <latch>

#include <Skirnir/Skirnir.hpp>

namespace FREYR_NAMESPACE
{
    using Task      = std::move_only_function<void()>;
    using TaskQueue = std::queue<Task>;

    class TaskManager
    {
      public:
        TaskManager(const Ref<FreyrOptions>&             freyrOptions,
                    const Ref<skr::Logger<TaskManager>>& logger) :
            mLogger(logger), mReservedTasks(0), mRunning(true), mWaiting(false),
            mProfiling(false), mThreadCount(1)
        {
            Resize(freyrOptions->ThreadCount);
        }

        static thread_local size_t ThreadId;

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

        void BeginProfiling();

      private:
        void workerLoop();

        Ref<skr::Logger<TaskManager>> mLogger;
        std::vector<std::string>      mWorkersDescriptions;
        std::vector<std::thread>      mWorkers;
        std::atomic<int>              mThreadCount;
        TaskQueue                     mAvaiableTasks;
        unsigned long                 mReservedTasks;

        std::mutex              mMutex;
        std::condition_variable mCondition;
        Ref<std::latch>         mTasksCompleted;

        bool mWaiting;
        bool mProfiling;
        bool mRunning;
    };
} // namespace FREYR_NAMESPACE