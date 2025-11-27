#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Containers/MPMCQueue.hpp"
#include "Freyr/Core/TaskCounter.hpp"

namespace FREYR_NAMESPACE
{
    using Task      = std::function<void()>;
    using TaskQueue = rigtorp::mpmc::Queue<Task>;

    class TaskManager
    {
        enum class State : unsigned
        {
            Empty,
            Resizing,
            Running,
            Idle
        };

      public:
        TaskManager(const Ref<FreyrOptions>& freyrOptions, const Ref<skr::Logger<TaskManager>>& logger,
                    const Ref<TaskCounter>& taskCounter) :
            mLogger(logger), mThreadLane(1), mFreyrOptions(freyrOptions), mState(State::Empty), mQueueIndex(0),
            mTaskCounter(taskCounter)
        {
            Resize(freyrOptions->ThreadCount);
        }

        static thread_local size_t ThreadId;

        ~TaskManager();

        void AddTask(auto&& func)
        {
            const auto nextQueue = mQueueIndex.fetch_add(1) % mWorkerQueues.size();

            mTaskCounter->addTasks(1);
            mWorkerQueues[nextQueue]->push(std::forward<decltype(func)>(func));
        }

        void Resize(std::uint32_t threadCount);

        void StartWorkers();
        void StopWorkers();
        void WaitForAllTasks()
        {
            mTaskCounter->waitForCompletion();
            StopWorkers();
        }

        void NotifyWorker() { mCondition.notify_one(); }

        void NotifyWorkers() { mCondition.notify_all(); }

        void BeginProfiling();

        bool IsRunning() const { return mState.load() == State::Running; }

      private:
        void workerLoop(TaskQueue* workerQueue);

        Ref<skr::Logger<TaskManager>> mLogger;
        Ref<FreyrOptions>             mFreyrOptions;
        Ref<TaskCounter>              mTaskCounter;

        std::vector<std::string> mWorkersDescriptions;
        std::vector<std::thread> mWorkers;
        std::atomic<int>         mThreadLane;

        std::atomic<std::uint32_t> mQueueIndex;
        std::vector<TaskQueue*>    mWorkerQueues;

        std::mutex              mMutex;
        std::condition_variable mCondition;
        std::atomic<State>      mState;
    };
} // namespace FREYR_NAMESPACE